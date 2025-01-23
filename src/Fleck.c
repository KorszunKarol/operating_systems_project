/*********************************
*
* @File: Fleck.c
* @Purpose: Client process for distortion system
* @Author: Karol Korszun
* @Date: 2024-03-19
*
*********************************/

#include "common.h"
#include "config.h"
#include "network.h"
#include "utils.h"
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>

#define ERROR_MSG_COMMAND "ERROR: Please input a valid command.\n"
#define ERROR_MSG_NOT_CONNECTED "Cannot distort, you are not connected to Mr. J System\n"
#define ERROR_MSG_DISTORT_USAGE "Usage: DISTORT <file.xxx> <factor>\n"

static FleckConfig gConfig;
static int gnIsConnected = 0;
static Connection *gpGothamConn = NULL;
static Connection *gpWorkerConn = NULL;

/* Function declarations */
void vHandleConnect(void);
void vHandleLogout(void);
void vListFiles(const char *psType);
void vHandleCommand(char *psCommand);
void vHandleDistort(const char *psFile, const char *psFactor);
void vHandleGothamCrash(void);
void vHandleWorkerCrash(void);
void vConnectToWorker(const char* psIP, const char* psPort, const char* psFile, const char* psFactor);
void vSimulateFileTransfer(void);
void *vMonitorGotham(void *pvArg);
void *vMonitorWorker(void *pvArg);
void vHandleSigInt(int nSigNum);

/*************************************************
* @Name: main
* @Def: Entry point for Fleck client
* @Arg: In: nArgc = argument count
*       In: psArgv = argument vector
* @Ret: 0 on success, 1 on failure
*************************************************/
int main(int nArgc, char *psArgv[]) {
    if (nArgc != 2) {
        printF("Usage: Fleck <config_file>\n");
        return 1;
    }

    signal(SIGINT, vHandleSigInt);
    load_fleck_config(psArgv[1], &gConfig);
    verify_directory(gConfig.sFolderPath);

    char *psMessage;
    asprintf(&psMessage, "%s user initialized\n", gConfig.sUsername);
    printF(psMessage);
    free(psMessage);

    char *psCommand = malloc(MAX_COMMAND_LENGTH);
    if (!psCommand) {
        printF("Memory allocation failed\n");
        return 1;
    }

    while (1) {
        printF("$ ");
        int nBytesRead = read(STDIN_FILENO, psCommand, MAX_COMMAND_LENGTH - 1);
        if (nBytesRead <= 0) break;

        psCommand[nBytesRead - 1] = '\0';  // Remove newline
        vHandleCommand(psCommand);
    }

    free(psCommand);
    return 0;
}

/*************************************************
* @Name: vHandleCommand
* @Def: Processes user commands
* @Arg: In: psCommand = command string to process
* @Ret: None
*************************************************/
void vHandleCommand(char *psCommand) {
    char *psCmdCopy = strdup(psCommand);
    if (!psCmdCopy) {
        printF("Memory allocation failed\n");
        return;
    }
    to_uppercase(psCmdCopy);

    char *psToken = strtok(psCmdCopy, " \n");
    if (!psToken) {
        free(psCmdCopy);
        return;
    }

    if (strcmp(psToken, "CONNECT") == 0) {
        vHandleConnect();
    } else if (strcmp(psToken, "LOGOUT") == 0) {
        vHandleLogout();
    } else if (strcmp(psToken, "LIST") == 0) {
        psToken = strtok(NULL, " \n");
        if (!psToken) {
            printF(ERROR_MSG_COMMAND);
            free(psCmdCopy);
            return;
        }

        if (strcmp(psToken, "TEXT") == 0 || strcmp(psToken, "MEDIA") == 0) {
            vListFiles(psToken);
        } else {
            printF(ERROR_MSG_COMMAND);
        }
    } else if (strcmp(psToken, "DISTORT") == 0) {
        char *psFile = strtok(NULL, " \n");
        char *psFactor = strtok(NULL, " \n");
        if (!psFile || !psFactor) {
            printF(ERROR_MSG_DISTORT_USAGE);
        } else {
            vHandleDistort(psFile, psFactor);
        }
    } else {
        printF(ERROR_MSG_COMMAND);
    }

    free(psCmdCopy);
}

/*************************************************
* @Name: vListFiles
* @Def: Lists files of specified type in user's directory
* @Arg: In: psType = type of files to list ("TEXT" or "MEDIA")
* @Ret: None
*************************************************/
void vListFiles(const char *psType) {
    DIR *pDir;
    struct dirent *psEntry;
    int nCount = 0;
    char *psMessage;

    pDir = opendir(gConfig.sFolderPath);
    if (!pDir) {
        asprintf(&psMessage, "Error opening directory %s\n", gConfig.sFolderPath);
        printF(psMessage);
        free(psMessage);
        return;
    }

    if (strcmp(psType, "MEDIA") == 0) {
        printF("Media files available:\n");
        while ((psEntry = readdir(pDir)) != NULL) {
            char *psExt = strrchr(psEntry->d_name, '.');
            if (psExt != NULL &&
               (strcmp(psExt, ".wav") == 0 ||
                strcmp(psExt, ".jpg") == 0 ||
                strcmp(psExt, ".png") == 0)) {
                nCount++;
                asprintf(&psMessage, "%d. %s\n", nCount, psEntry->d_name);
                printF(psMessage);
                free(psMessage);
            }
        }
    } else if (strcmp(psType, "TEXT") == 0) {
        printF("Text files available:\n");
        while ((psEntry = readdir(pDir)) != NULL) {
            char *psExt = strrchr(psEntry->d_name, '.');
            if (psExt != NULL && strcmp(psExt, ".txt") == 0) {
                nCount++;
                asprintf(&psMessage, "%d. %s\n", nCount, psEntry->d_name);
                printF(psMessage);
                free(psMessage);
            }
        }
    }

    closedir(pDir);

    if (nCount == 0) {
        asprintf(&psMessage, "No %s files found in %s\n", psType, gConfig.sFolderPath);
    } else {
        asprintf(&psMessage, "There are %d %s files available.\n", nCount, psType);
    }
    printF(psMessage);
    free(psMessage);
}

/*************************************************
* @Name: vHandleConnect
* @Def: Handles connection to Gotham server
* @Arg: None
* @Ret: None
*************************************************/
void vHandleConnect(void) {
    vWriteLog("Reading configuration file\n");
    vWriteLog("Connecting to Mr. J System...\n");

    if (gnIsConnected) {
        printF("Already connected\n");
        return;
    }

    gpGothamConn = connect_to_server(gConfig.sGothamIP, atoi(gConfig.sGothamPort));
    if (!gpGothamConn) {
        printF("Failed to connect to Gotham\n");
        return;
    }

    // Format according to protocol: <userName>&<IP>&<Port>
    char sData[DATA_SIZE];
    snprintf(sData, sizeof(sData), "%s&%s&%s",
             gConfig.sUsername,
             gConfig.sGothamIP,
             gConfig.sGothamPort);

    // Send type 0x01 (FRAME_CONNECT_REQ)
    Frame* frame = create_frame(FRAME_CONNECT_REQ, sData, strlen(sData));
    if (send_frame(gpGothamConn, frame) != 0) {
        free_frame(frame);
        close_connection(gpGothamConn);
        return;
    }
    free_frame(frame);

    // Wait for response - should be type 0x01 with empty data for success
    Frame* response = receive_frame(gpGothamConn);
    if (!response) {
        close_connection(gpGothamConn);
        return;
    }

    if (response->type != FRAME_CONNECT_REQ ||
        (response->data_length > 0 && strcmp(response->data, "CON_KO") == 0)) {
        printF("Connection rejected\n");
        free_frame(response);
        close_connection(gpGothamConn);
        return;
    }
    free_frame(response);

    gnIsConnected = 1;
    printF("Connected successfully\n");

    if (gnIsConnected) {
        vWriteLog("Connected successfully to Mr. J System\n");
    }
}

/*************************************************
* @Name: vHandleLogout
* @Def: Handles logout from system
* @Arg: None
* @Ret: None
*************************************************/
void vHandleLogout(void) {
    if (gnIsConnected) {
        // Create proper disconnect frame
        Frame* frame = create_frame(FRAME_DISCONNECT, gConfig.sUsername, strlen(gConfig.sUsername));
        if (send_frame(gpGothamConn, frame) == 0) {
            vWriteLog("Sent disconnect frame to Gotham\n");
        }
        free_frame(frame);

        // If connected to a worker, send disconnect there too
        if (gpWorkerConn) {
            frame = create_frame(FRAME_DISCONNECT, gConfig.sUsername, strlen(gConfig.sUsername));
            if (send_frame(gpWorkerConn, frame) == 0) {
                vWriteLog("Sent disconnect frame to worker\n");
            }
            free_frame(frame);
            close_connection(gpWorkerConn);
            gpWorkerConn = NULL;
        }

        close_connection(gpGothamConn);
        gpGothamConn = NULL;
        gnIsConnected = 0;
    }
    printF("Thanks for using Mr. J System, see you soon, chaos lover :)\n");
    exit(0);
}

/*************************************************
* @Name: vHandleSigInt
* @Def: Handles SIGINT signal (CTRL+C)
* @Arg: In: nSigNum = signal number
* @Ret: None
*************************************************/
void vHandleSigInt(int nSigNum) {
    (void)nSigNum;
    vHandleLogout();
}

/*************************************************
* @Name: nIsValidFactor
* @Def: Validates distortion factor
* @Arg: In: psFactor = factor string to validate
* @Ret: 1 if valid, 0 if invalid
*************************************************/
int nIsValidFactor(const char *psFactor) {
    char *psEndptr;
    double dValue = strtod(psFactor, &psEndptr);
    return *psEndptr == '\0' && dValue > 0 && dValue <= 10;
}

/*************************************************
* @Name: vHandleDistort
* @Def: Handles distortion request
* @Arg: In: psFile = file to distort
*       In: psFactor = distortion factor
* @Ret: None
*************************************************/
void vHandleDistort(const char *psFile, const char *psFactor) {
    char sMsg[256];
    snprintf(sMsg, sizeof(sMsg), "\n=== Starting Distortion Request ===\n");
    vWriteLog(sMsg);

    snprintf(sMsg, sizeof(sMsg), "Sending distortion request for %s with factor %s\n",
             psFile, psFactor);
    vWriteLog(sMsg);

    if (!gnIsConnected || !gpGothamConn) {
        printF(ERROR_MSG_NOT_CONNECTED);
        return;
    }

    // Convert filename to lowercase for extension check
    char sLowerFile[256];
    strncpy(sLowerFile, psFile, sizeof(sLowerFile)-1);
    sLowerFile[sizeof(sLowerFile)-1] = '\0';
    for(int i = 0; sLowerFile[i]; i++) {
        sLowerFile[i] = tolower(sLowerFile[i]);
    }

    // Determine media type from extension
    char *psExt = strrchr(sLowerFile, '.');
    if (!psExt) {
        printF("Invalid file format\n");
        return;
    }

    char *psMediaType;
    if (strcmp(psExt, ".txt") == 0) {
        psMediaType = "Text";
    } else if (strcmp(psExt, ".wav") == 0 ||
               strcmp(psExt, ".jpg") == 0 ||
               strcmp(psExt, ".png") == 0) {
        psMediaType = "Media";
    } else {
        printF("Unsupported file format\n");
        return;
    }

    // Send distortion request to Gotham
    char data[DATA_SIZE];
    snprintf(data, sizeof(data), "%s&%s", psMediaType, psFile);
    Frame* frame = create_frame(FRAME_DISTORT_REQ, data, strlen(data));

    vWriteLog("Sending distortion request to Gotham\n");

    if (send_frame(gpGothamConn, frame) != 0) {
        vWriteLog("Failed to send distortion request\n");
        free_frame(frame);
        vHandleGothamCrash();
        return;
    }
    free_frame(frame);

    vWriteLog("Waiting for worker info from Gotham\n");

    // Wait for worker info from Gotham with increased timeout
    struct timeval tv;
    tv.tv_sec = SOCKET_TIMEOUT_SEC;
    tv.tv_usec = 0;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(gpGothamConn->fd, &readfds);

    int ready = select(gpGothamConn->fd + 1, &readfds, NULL, NULL, &tv);
    if (ready <= 0) {
        vWriteLog("Timeout/error waiting for Gotham response\n");
        vHandleGothamCrash();
        return;
    }

    Frame* response = receive_frame(gpGothamConn);
    if (!response) {
        vWriteLog("Failed to receive response from Gotham\n");
        vHandleGothamCrash();
        return;
    }

    vWriteLog("Received response from Gotham\n");

    if (response->type != FRAME_DISTORT_REQ) {
        vWriteLog("Received unexpected frame type\n");
        free_frame(response);
        vHandleGothamCrash();
        return;
    }

    // Check for error responses
    if (response->data_length == 9 && strcmp(response->data, "DISTORT_KO") == 0) {
        vWriteLog("No available worker for this media type\n");
        printF("Error: No available worker of this type is currently connected\n");
        free_frame(response);
        return;
    }

    if (response->data_length == 8 && strcmp(response->data, "MEDIA_KO") == 0) {
        vWriteLog("Invalid media type for request\n");
        printF("Error: Invalid media type\n");
        free_frame(response);
        return;
    }

    // Parse worker IP and port
    char sWorkerIP[MAX_IP_LENGTH];
    char sWorkerPort[MAX_PORT_LENGTH];
    if (sscanf(response->data, "%[^&]&%s", sWorkerIP, sWorkerPort) != 2) {
        vWriteLog("Failed to parse worker info\n");
        printF("Error: Invalid worker info received\n");
        free_frame(response);
        return;
    }

    // Log the worker info we received
    snprintf(sMsg, sizeof(sMsg), "Received worker info - IP: %s, Port: %s\n",
             sWorkerIP, sWorkerPort);
    vWriteLog(sMsg);

    // Connect to worker and handle distortion
    vConnectToWorker(sWorkerIP, sWorkerPort, psFile, psFactor);

    free_frame(response);
}

/*************************************************
* @Name: vHandleGothamCrash
* @Def: Handles Gotham server crash
* @Arg: None
* @Ret: None
*************************************************/
void vHandleGothamCrash(void) {
    printF("Lost connection to Gotham. Shutting down...\n");
    if (gpWorkerConn) {
        close_connection(gpWorkerConn);
        gpWorkerConn = NULL;
    }
    if (gpGothamConn) {
        close_connection(gpGothamConn);
        gpGothamConn = NULL;
    }
    gnIsConnected = 0;
    exit(1);
}

/*************************************************
* @Name: vHandleWorkerCrash
* @Def: Handles worker crash during distortion
* @Arg: None
* @Ret: None
*************************************************/
void vHandleWorkerCrash(void) {
    printF("Lost connection to worker. Attempting to resume distortion...\n");

    if (gpWorkerConn) {
        close_connection(gpWorkerConn);
        gpWorkerConn = NULL;
    }

    // Send resume request to Gotham
    char data[DATA_SIZE];
    snprintf(data, sizeof(data), "%s&%s", psCurrentMediaType, psCurrentFile);
    Frame* frame = create_frame(FRAME_RESUME_REQ, data, strlen(data));

    if (send_frame(gpGothamConn, frame) != 0) {
        vWriteLog("Failed to send resume request\n");
        free_frame(frame);
        vHandleGothamCrash();
        return;
    }
    free_frame(frame);

    // Wait for new worker info
    Frame* response = receive_frame(gpGothamConn);
    if (!response) {
        vWriteLog("Failed to receive resume response\n");
        vHandleGothamCrash();
        return;
    }

    if (response->type != FRAME_RESUME_REQ) {
        vWriteLog("Received unexpected frame type for resume\n");
        free_frame(response);
        vHandleGothamCrash();
        return;
    }

    // Check for error responses
    if (strcmp(response->data, "DISTORT_KO") == 0) {
        vWriteLog("No available worker to resume distortion\n");
        printF("Error: No available worker to resume distortion\n");
        free_frame(response);
        return;
    }

    // Parse new worker info and reconnect
    char sWorkerIP[MAX_IP_LENGTH];
    char sWorkerPort[MAX_PORT_LENGTH];
    if (sscanf(response->data, "%[^&]&%s", sWorkerIP, sWorkerPort) != 2) {
        vWriteLog("Failed to parse new worker info\n");
        free_frame(response);
        return;
    }

    free_frame(response);

    // Connect to new worker
    vConnectToWorker(sWorkerIP, sWorkerPort, psCurrentFile, psCurrentFactor);
}

/*************************************************
* @Name: vMonitorGotham
* @Def: Monitors Gotham connection for crashes
* @Arg: In: pvArg = thread argument (unused)
* @Ret: NULL
*************************************************/
void *vMonitorGotham(void *pvArg) {
    (void)pvArg;

    while (gnIsConnected) {
        // Use timeout to avoid blocking forever
        Frame* frame = receive_frame(gpGothamConn);
        if (!frame) {
            // Only handle crash if we're still connected
            if (gnIsConnected) {
                vHandleGothamCrash();
            }
            break;
        }
        sleep(1);
    }
    return NULL;
}

/*************************************************
* @Name: vMonitorWorker
* @Def: Monitors worker connection for crashes
* @Arg: In: pvArg = thread argument (unused)
* @Ret: NULL
*************************************************/
void *vMonitorWorker(void *pvArg) {
    (void)pvArg;
    char sBuffer[2];

    while (gpWorkerConn) {
        if (receive_data(gpWorkerConn, sBuffer, 1) <= 0) {
            vHandleWorkerCrash();
            break;
        }
        sleep(1);
    }
    return NULL;
}

/*************************************************
* @Name: vConnectToWorker
* @Def: Connects to worker and handles distortion
* @Arg: In: psIP = worker IP
*       In: psPort = worker port
*       In: psFile = file to distort
*       In: psFactor = distortion factor
* @Ret: None
*************************************************/
void vConnectToWorker(const char* psIP, const char* psPort, const char* psFile, const char* psFactor) {
    // Connect to worker
    gpWorkerConn = connect_to_server(psIP, atoi(psPort));
    if (!gpWorkerConn) {
        vWriteLog("Failed to connect to worker\n");
        return;
    }

    // Calculate real file size and MD5
    char sFilePath[512];
    snprintf(sFilePath, sizeof(sFilePath), "%s/%s", gConfig.sFolderPath, psFile);

    // Get file size
    struct stat st;
    if (stat(sFilePath, &st) != 0) {
        vWriteLog("Failed to get file stats\n");
        vHandleWorkerCrash();
        return;
    }
    unsigned long nFileSize = st.st_size;

    // Calculate MD5 (simulated for now)
    char sMD5[33] = "d41d8cd98f00b204e9800998ecf8427e";

    // Send connection frame
    char sData[DATA_SIZE];
    snprintf(sData, sizeof(sData), "%s&%s&%lu&%s&%s",
             gConfig.sUsername, psFile, nFileSize, sMD5, psFactor);

    Frame* frame = create_frame(FRAME_WORKER_CONNECT, sData, strlen(sData));
    if (send_frame(gpWorkerConn, frame) != 0) {
        free_frame(frame);
        vHandleWorkerCrash();
        return;
    }
    free_frame(frame);

    // Wait for worker acknowledgment
    Frame* response = receive_frame(gpWorkerConn);
    if (!response || response->type != FRAME_WORKER_CONNECT) {
        if (response) free_frame(response);
        vHandleWorkerCrash();
        return;
    }
    free_frame(response);

    // Send file data in chunks
    int fd = open(sFilePath, O_RDONLY);
    if (fd < 0) {
        vWriteLog("Failed to open file\n");
        vHandleWorkerCrash();
        return;
    }

    char buffer[DATA_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, DATA_SIZE)) > 0) {
        frame = create_frame(FRAME_FILE_DATA, buffer, bytes_read);
        if (send_frame(gpWorkerConn, frame) != 0) {
            free_frame(frame);
            close(fd);
            vHandleWorkerCrash();
            return;
        }
        free_frame(frame);
    }
    close(fd);

    // Wait for distorted file info
    response = receive_frame(gpWorkerConn);
    if (!response || response->type != FRAME_FILE_INFO) {
        if (response) free_frame(response);
        vHandleWorkerCrash();
        return;
    }

    // Parse file info
    unsigned long nDistortedSize;
    char sDistortedMD5[33];
    if (sscanf(response->data, "%lu&%s", &nDistortedSize, sDistortedMD5) != 2) {
        free_frame(response);
        vHandleWorkerCrash();
        return;
    }
    free_frame(response);

    // Receive distorted file data
    char sDistortedPath[512];
    snprintf(sDistortedPath, sizeof(sDistortedPath), "%s/distorted_%s",
             gConfig.sFolderPath, psFile);

    fd = open(sDistortedPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        vWriteLog("Failed to create output file\n");
        vHandleWorkerCrash();
        return;
    }

    unsigned long nReceived = 0;
    while (nReceived < nDistortedSize) {
        response = receive_frame(gpWorkerConn);
        if (!response || response->type != FRAME_FILE_DATA) {
            if (response) free_frame(response);
            close(fd);
            vHandleWorkerCrash();
            return;
        }

        write(fd, response->data, response->data_length);
        nReceived += response->data_length;
        free_frame(response);
    }
    close(fd);

    // Send MD5 check
    frame = create_frame(FRAME_MD5_CHECK, "CHECK_OK", 8);
    send_frame(gpWorkerConn, frame);
    free_frame(frame);

    // Disconnect
    frame = create_frame(FRAME_DISCONNECT, gConfig.sUsername, strlen(gConfig.sUsername));
    send_frame(gpWorkerConn, frame);
    free_frame(frame);

    close_connection(gpWorkerConn);
    gpWorkerConn = NULL;
}

/*************************************************
* @Name: vSimulateFileTransfer
* @Def: Simulates file transfer for Phase 1
* @Arg: None
* @Ret: None
*************************************************/
void vSimulateFileTransfer(void) {
    // Simulate sending file data
    Frame* dataFrame = create_frame(FRAME_FILE_DATA, "SIMULATED_FILE_DATA", 17);
    send_frame(gpWorkerConn, dataFrame);
    free_frame(dataFrame);

    // Wait for distortion completion
    Frame* response = receive_frame(gpWorkerConn);
    if (!response || response->type != FRAME_FILE_INFO) {
        if (response) free_frame(response);
        vHandleWorkerCrash();
        return;
    }
    free_frame(response);

    // Simulate receiving distorted data
    response = receive_frame(gpWorkerConn);
    if (!response || response->type != FRAME_FILE_DATA) {
        if (response) free_frame(response);
        vHandleWorkerCrash();
        return;
    }
    free_frame(response);

    // Send MD5 check acknowledgment
    Frame* ack = create_frame(FRAME_MD5_CHECK, "CHECK_OK", 8);
    send_frame(gpWorkerConn, ack);
    free_frame(ack);
}
