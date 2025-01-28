/*********************************
*
* @File: Gotham.c
* @Purpose: Central server managing worker and client connections
* @Author: Karol Korszun
* @Date: 2024-03-19
*
*********************************/

#include "common.h"
#include "config.h"
#include "network.h"
#include "logging.h"

#include "shared.h"
#include <pthread.h>
#include <arpa/inet.h>

/* Global variables */
static GothamConfig gConfig;
static Connection* gpServerConn = NULL;
static volatile int gnIsRunning = 1;

/* Worker management */
typedef struct {
    Connection* pConn;
    char* psType;        // "Media" or "Text"
    int nIsMain;         // Is this the main worker of its type
    int nIsBusy;        // Currently processing a request
} Worker;

typedef struct {
    Connection* pConn;
    char* psUsername;
    Worker* pCurrentWorker;
} FleckClient;

/* Thread management */
typedef struct {
    Worker* pWorker;
    int nActive;
} WorkerMonitorData;

/* Global arrays and mutexes */
static Worker** gpWorkers = NULL;
static size_t gnWorkerCount = 0;
static FleckClient** gpClients = NULL;
static size_t gnClientCount = 0;

static pthread_mutex_t gWorkersMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t gClientsMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t gShutdownMutex = PTHREAD_MUTEX_INITIALIZER;
static volatile int gnShutdownInProgress = 0;

/* Function declarations */
void vHandleWorkerRegistration(Connection* pConn, Frame* pFrame);
void vHandleFleckConnection(Connection* pConn, Frame* pFrame);
void vHandleFleckRequest(FleckClient* pClient, Frame* pRequestFrame);
void vHandleWorkerDisconnection(Worker* pWorker);
void vHandleShutdown(void);
void* vMonitorWorker(void* pvArg);
void vHandleSigInt(int nSigNum);
void vHandleFleckDisconnection(Connection* pConn);
void vHandleWorkerCrash(Worker* pWorker);
void vCompactWorkerArray();
void vCheckMainWorkers();
void vHandleDistortRequest(FleckClient* pClient, Frame* pFrame);
void vHandleFrame(Connection* pConn, Frame* pFrame);

/*************************************************
* @Name: main
* @Def: Entry point for Gotham server
* @Arg: In: nArgc = Argument count
*       In: psArgv = Argument vector
* @Ret: 0 on success, 1 on failure
*************************************************/
int main(int nArgc, char* psArgv[]) {
    if (2 != nArgc) {
        vWriteLog("Usage: Gotham <config_file>\n");
        return 1;
    }

    /* Initialize signal handling first */
    struct sigaction sa;
    sa.sa_handler = vHandleSigInt;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    /* Initialize */
    vWriteLog("Reading configuration file\n");
    load_gotham_config(psArgv[1], &gConfig);

    /* Create server socket */
    char debug[256];
    snprintf(debug, sizeof(debug), "Creating server on %s:%s\n",
             gConfig.sWorkerIP, gConfig.sWorkerPort);
    vWriteLog(debug);

    gpServerConn = create_server(gConfig.sWorkerIP,
                                nStringToInt(gConfig.sWorkerPort));
    if (!gpServerConn) {
        vWriteLog("Failed to create server\n");
        return 1;
    }

    vWriteLog("Gotham server initialized\n");
    vWriteLog("Waiting for connections...\n");

    /* Initialize arrays */
    gpWorkers = malloc(sizeof(Worker*));
    gpClients = malloc(sizeof(FleckClient*));

    if (!gpWorkers || !gpClients) {
        vWriteLog("Failed to allocate memory\n");
        return 1;
    }

    /* Main server loop */
    while (1 == gnIsRunning) {
        // Use select to monitor all active connections
        fd_set readfds;
        struct timeval tv;
        FD_ZERO(&readfds);

        // Add server socket
        FD_SET(gpServerConn->fd, &readfds);
        int max_fd = gpServerConn->fd;

        // Add all client connections
        pthread_mutex_lock(&gClientsMutex);
        for (size_t i = 0; i < gnClientCount; i++) {
            if (gpClients[i] && gpClients[i]->pConn) {
                FD_SET(gpClients[i]->pConn->fd, &readfds);
                if (gpClients[i]->pConn->fd > max_fd) {
                    max_fd = gpClients[i]->pConn->fd;
                }
            }
        }
        pthread_mutex_unlock(&gClientsMutex);

        tv.tv_sec = SOCKET_TIMEOUT_SEC;
        tv.tv_usec = 0;

        int ready = select(max_fd + 1, &readfds, NULL, NULL, &tv);
        if (ready < 0) {
            continue;
        }

        // Check for new connections
        if (FD_ISSET(gpServerConn->fd, &readfds)) {
            int nClientFd = accept_connection(gpServerConn);
            if (nClientFd >= 0) {
                Connection* pConn = malloc(sizeof(Connection));
                if (pConn) {
                    pConn->fd = nClientFd;
                    Frame* frame = receive_frame(pConn);
                    if (frame) {
                        vHandleFrame(pConn, frame);
                        free_frame(frame);
                    }
                }
            }
        }

        // Check existing client connections
        pthread_mutex_lock(&gClientsMutex);
        for (size_t i = 0; i < gnClientCount; i++) {
            if (gpClients[i] && gpClients[i]->pConn &&
                FD_ISSET(gpClients[i]->pConn->fd, &readfds)) {
                Frame* frame = receive_frame(gpClients[i]->pConn);
                if (frame) {
                    vHandleFrame(gpClients[i]->pConn, frame);
                    free_frame(frame);
                } else {
                    // Connection lost
                    vHandleFleckDisconnection(gpClients[i]->pConn);
                }
            }
        }
        pthread_mutex_unlock(&gClientsMutex);
    }

    /* Cleanup */
    vHandleShutdown();

    /* Cleanup mutexes before exit */
    pthread_mutex_destroy(&gWorkersMutex);
    pthread_mutex_destroy(&gClientsMutex);
    pthread_mutex_destroy(&gShutdownMutex);

    return 0;
}

/*************************************************
* @Name: vHandleWorkerRegistration
* @Def: Handles new worker registration
* @Arg: In: pConn = Connection from new worker
*       In: pFrame = Initial message read from the worker
* @Ret: None
*************************************************/
void vHandleWorkerRegistration(Connection* pConn, Frame* pFrame) {
    vWriteLog("Starting worker registration process...\n");

    // Parse <workerType>&<IP>&<Port>
    char sType[MAX_TYPE_LENGTH] = {0};
    char sIP[MAX_IP_LENGTH] = {0};
    char sPort[MAX_PORT_LENGTH] = {0};

    if (sscanf(pFrame->data, "%[^&]&%[^&]&%s", sType, sIP, sPort) != 3) {
        Frame* error = create_frame(FRAME_ERROR, NULL, 0);
        send_frame(pConn, error);
        free_frame(error);
        close_connection(pConn);
        return;
    }

    // Validate worker type
    if (strcmp(sType, "Text") != 0 && strcmp(sType, "Media") != 0) {
        Frame* error = create_frame(FRAME_ERROR, NULL, 0);
        send_frame(pConn, error);
        free_frame(error);
        close_connection(pConn);
        return;
    }

    Worker* pWorker = malloc(sizeof(Worker));
    if (!pWorker) {
        Frame* error = create_frame(FRAME_ERROR, NULL, 0);
        send_frame(pConn, error);
        free_frame(error);
        close_connection(pConn);
        return;
    }

    // Store worker's address info
    pWorker->pConn = pConn;
    memcpy(&pWorker->pConn->addr, &pConn->addr, sizeof(struct sockaddr_in));
    pWorker->psType = strdup(sType);
    pWorker->nIsMain = 0;
    pWorker->nIsBusy = 0;

    pthread_mutex_lock(&gWorkersMutex);

    // Check if should be main worker
    int nHasMain = 0;
    for (size_t i = 0; i < gnWorkerCount; i++) {
        if (gpWorkers[i] && strcmp(gpWorkers[i]->psType, sType) == 0 &&
            gpWorkers[i]->nIsMain) {
            nHasMain = 1;
            break;
        }
    }

    // Add to workers array
    gpWorkers = realloc(gpWorkers, (gnWorkerCount + 1) * sizeof(Worker*));
    gpWorkers[gnWorkerCount++] = pWorker;

    pthread_mutex_unlock(&gWorkersMutex);

    // Send appropriate response frame
    Frame* response;
    if (!nHasMain) {
        pWorker->nIsMain = 1;
        response = create_frame(FRAME_NEW_MAIN, NULL, 0);  // 0x08
    } else {
        response = create_frame(FRAME_WORKER_REG, NULL, 0); // 0x02
    }

    if (send_frame(pConn, response) != 0) {
        vWriteLog("Failed to send registration response\n");
        free_frame(response);
        // Cleanup...
        return;
    }

    free_frame(response);

    // Update logging messages
    if (strcmp(sType, "Text") == 0) {
        vWriteLog("New Enigma worker connected - ready to distort!\n");
    } else {
        vWriteLog("New Harley worker connected - ready to distort!\n");
    }
}

/*************************************************
* @Name: vHandleFleckConnection
* @Def: Handles new Fleck client connection
* @Arg: In: pConn = Connection from new Fleck
* @Ret: None
*************************************************/
void vHandleFleckConnection(Connection* pConn, Frame* pFrame) {
    // Parse username from frame data
    char sUsername[MAX_USERNAME_LENGTH];
    if (sscanf(pFrame->data, "%[^&]", sUsername) != 1) {
        Frame* error = create_frame(FRAME_ERROR, "Invalid connection format", 22);
        send_frame(pConn, error);
        free_frame(error);
        close_connection(pConn);
        return;
    }

    // Create new client
    FleckClient* pClient = malloc(sizeof(FleckClient));
    if (!pClient) {
        Frame* error = create_frame(FRAME_ERROR, "Internal error", 13);
        send_frame(pConn, error);
        free_frame(error);
        close_connection(pConn);
        return;
    }

    pClient->pConn = pConn;
    pClient->psUsername = strdup(sUsername);
    pClient->pCurrentWorker = NULL;

    // Add to clients array
    pthread_mutex_lock(&gClientsMutex);
    gpClients = realloc(gpClients, (gnClientCount + 1) * sizeof(FleckClient*));
    gpClients[gnClientCount++] = pClient;
    pthread_mutex_unlock(&gClientsMutex);

    // Send connection acknowledgment frame
    Frame* response = create_frame(FRAME_CONNECT_REQ, NULL, 0);  // Empty data means success
    if (send_frame(pConn, response) != 0) {
        free_frame(response);
        vHandleFleckDisconnection(pConn);
        return;
    }
    free_frame(response);

    char sMsg[256];
    snprintf(sMsg, sizeof(sMsg), "New user connected: %s.\n", sUsername);
    vWriteLog(sMsg);
}

/*************************************************
* @Name: vHandleSigInt
* @Def: SIGINT signal handler
* @Arg: In: nSigNum = Signal number
* @Ret: None
*************************************************/
void vHandleSigInt(int nSigNum) {
    (void)nSigNum;

    pthread_mutex_lock(&gShutdownMutex);
    if (gnShutdownInProgress) {
        pthread_mutex_unlock(&gShutdownMutex);
        return;
    }
    gnShutdownInProgress = 1;
    pthread_mutex_unlock(&gShutdownMutex);

    vWriteLog("\nReceived CTRL+C. Initiating system shutdown...\n");
    gnIsRunning = 0;

    /* Notify all workers first */
    pthread_mutex_lock(&gWorkersMutex);
    for (size_t i = 0; i < gnWorkerCount; i++) {
        if (gpWorkers[i]) {
            vWriteLog("Notifying worker of shutdown...\n");
            send_data(gpWorkers[i]->pConn, "SHUTDOWN\n", 9);
        }
    }
    pthread_mutex_unlock(&gWorkersMutex);

    /* Then notify all clients */
    pthread_mutex_lock(&gClientsMutex);
    for (size_t i = 0; i < gnClientCount; i++) {
        if (gpClients[i]) {
            vWriteLog("Notifying Fleck client of shutdown...\n");
            send_data(gpClients[i]->pConn, "SHUTDOWN\n", 9);
        }
    }
    pthread_mutex_unlock(&gClientsMutex);

    /* Wait briefly for notifications to be sent */
    sleep(1);

    /* Clean up and exit */
    vHandleShutdown();
    exit(0);
}

/*************************************************
* @Name: vHandleShutdown
* @Def: Handles system shutdown
* @Arg: None
* @Ret: None
*************************************************/
void vHandleShutdown(void) {
    pthread_mutex_lock(&gShutdownMutex);
    if (gnShutdownInProgress) {
        pthread_mutex_unlock(&gShutdownMutex);
        return;
    }
    gnShutdownInProgress = 1;
    pthread_mutex_unlock(&gShutdownMutex);

    /* Close all client connections */
    pthread_mutex_lock(&gClientsMutex);
    for (size_t i = 0; i < gnClientCount; i++) {
        if (gpClients[i]) {
            close_connection(gpClients[i]->pConn);
            free(gpClients[i]->psUsername);
            free(gpClients[i]);
        }
    }
    free(gpClients);
    gnClientCount = 0;
    pthread_mutex_unlock(&gClientsMutex);

    /* Close all worker connections */
    pthread_mutex_lock(&gWorkersMutex);
    for (size_t i = 0; i < gnWorkerCount; i++) {
        if (gpWorkers[i]) {
            close_connection(gpWorkers[i]->pConn);
            free(gpWorkers[i]->psType);
            free(gpWorkers[i]);
        }
    }
    free(gpWorkers);
    gnWorkerCount = 0;
    pthread_mutex_unlock(&gWorkersMutex);

    /* Close server connection */
    if (gpServerConn) {
        close_connection(gpServerConn);
        gpServerConn = NULL;
    }

    vWriteLog("System shutdown complete\n");
}

/*************************************************
* @Name: vHandleDistortRequest
* @Def: Handles FRAME_DISTORT_REQ (0x10) frames
* @Arg: In: pClient = Requesting client
*       In: pFrame = Received frame
* @Ret: None
*************************************************/
void vHandleDistortRequest(FleckClient* pClient, Frame* pFrame) {
    char sMediaType[16], sFileName[256];
    char sLogMsg[512];

    // Parse request data
    if(sscanf(pFrame->data, "%15[^&]&%255s", sMediaType, sFileName) != 2) {
        vWriteLog("Invalid distort request format\n");
        Frame* error = create_frame(FRAME_ERROR, "INVALID_FORMAT", 14);
        send_frame(pClient->pConn, error);
        free_frame(error);
        return;
    }

    // Validate media type
    if(strcasecmp(sMediaType, "media") != 0 && strcasecmp(sMediaType, "text") != 0) {
        vWriteLog("Invalid media type received\n");
        Frame* response = create_frame(FRAME_DISTORT_REQ, "MEDIA_KO", 8);
        send_frame(pClient->pConn, response);
        free_frame(response);
        return;
    }

    // Worker selection logic
    Worker* pSelectedWorker = NULL;
    pthread_mutex_lock(&gWorkersMutex);
    for(size_t i = 0; i < gnWorkerCount; i++) {
        if(gpWorkers[i] && !gpWorkers[i]->nIsBusy &&
           strcasecmp(gpWorkers[i]->psType, sMediaType) == 0) {
            pSelectedWorker = gpWorkers[i];
            pSelectedWorker->nIsBusy = 1;
            break;
        }
    }
    pthread_mutex_unlock(&gWorkersMutex);

    // Prepare response
    if(pSelectedWorker) {
        struct sockaddr_in* pAddr = &pSelectedWorker->pConn->addr;
        char sIP[INET_ADDRSTRLEN];
        char sPort[8];

        inet_ntop(AF_INET, &pAddr->sin_addr, sIP, INET_ADDRSTRLEN);
        snprintf(sPort, sizeof(sPort), "%d", ntohs(pAddr->sin_port));

        char sResponseData[256];
        snprintf(sResponseData, sizeof(sResponseData), "%s&%s", sIP, sPort);

        Frame* response = create_frame(FRAME_DISTORT_REQ, sResponseData, strlen(sResponseData));
        send_frame(pClient->pConn, response);
        free_frame(response);

        snprintf(sLogMsg, sizeof(sLogMsg), "Assigned %s worker %s:%s for %s\n",
                sMediaType, sIP, sPort, sFileName);
        vWriteLog(sLogMsg);
    } else {
        Frame* response = create_frame(FRAME_DISTORT_REQ, "DISTORT_KO", 10);
        send_frame(pClient->pConn, response);
        free_frame(response);
        vWriteLog("No available workers for request\n");
    }
}

/*************************************************
* @Name: vHandleFleckDisconnection
* @Def: Handles Fleck client disconnection
* @Arg: In: pConn = Connection from disconnected Fleck
* @Ret: None
*************************************************/
void vHandleFleckDisconnection(Connection* pConn) {
    vWriteLog("Fleck disconnecting from system\n");

    pthread_mutex_lock(&gClientsMutex);
    for (size_t i = 0; i < gnClientCount; i++) {
        if (gpClients[i] && gpClients[i]->pConn->fd == pConn->fd) {
            char* psMsg;
            asprintf(&psMsg, "User %s disconnected from the system\n",
                    gpClients[i]->psUsername);
            vWriteLog(psMsg);
            free(psMsg);

            free(gpClients[i]->psUsername);
            free(gpClients[i]);
            gpClients[i] = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&gClientsMutex);

    close_connection(pConn);
}

/*************************************************
* @Name: vHandleWorkerCrash
* @Def: Handles worker crash
* @Arg: In: pWorker = Crashed worker
* @Ret: None
*************************************************/
void vHandleWorkerCrash(Worker* pWorker) {
    if (!pWorker) return;

    pthread_mutex_lock(&gWorkersMutex);

    // Store worker info before cleanup
    char* psWorkerType = strdup(pWorker->psType);
    int nWasMain = pWorker->nIsMain;

    // Find and remove worker from array
    for (size_t i = 0; i < gnWorkerCount; i++) {
        if (gpWorkers[i] == pWorker) {
            gpWorkers[i] = NULL;
            break;
        }
    }

    // Log disconnection
    if (strcmp(psWorkerType, "Text") == 0) {
        vWriteLog("Enigma worker disconnected from the system\n");
    } else {
        vWriteLog("Harley worker disconnected from the system\n");
    }

    // Clean up worker resources
    close_connection(pWorker->pConn);
    free(pWorker->psType);
    free(pWorker);

    // Compact array and check for remaining workers
    vCompactWorkerArray();

    // Check if we need to assign a new main worker
    if (nWasMain) {
        for (size_t i = 0; i < gnWorkerCount; i++) {
            if (gpWorkers[i] && strcmp(gpWorkers[i]->psType, psWorkerType) == 0) {
                gpWorkers[i]->nIsMain = 1;
                Frame* mainFrame = create_frame(FRAME_NEW_MAIN, NULL, 0);
                send_frame(gpWorkers[i]->pConn, mainFrame);
                free_frame(mainFrame);
                vWriteLog("Assigned new main worker\n");
                break;
            }
        }
    }

    pthread_mutex_unlock(&gWorkersMutex);
    free(psWorkerType);
}

/*************************************************
* @Name: vMonitorWorker
* @Def: Monitors worker connection
* @Arg: In: pvArg = Worker pointer
* @Ret: NULL
*************************************************/
void* vMonitorWorker(void* pvArg) {
    WorkerMonitorData* pData = (WorkerMonitorData*)pvArg;
    Worker* pWorker = pData->pWorker;
    time_t tLastHeartbeat = time(NULL);

    while (pData->nActive) {
        // Check if too much time has passed since last heartbeat
        if (time(NULL) - tLastHeartbeat > SOCKET_TIMEOUT_SEC * 2) {
            vHandleWorkerCrash(pWorker);
            break;
        }

        // Send heartbeat
        Frame* heartbeat = create_frame(FRAME_HEARTBEAT, "PING", 4);
        if (send_frame(pWorker->pConn, heartbeat) < 0) {
            free_frame(heartbeat);
            vHandleWorkerCrash(pWorker);
            break;
        }
        free_frame(heartbeat);

        // Wait for response with timeout
        Frame* response = receive_frame_timeout(pWorker->pConn, SOCKET_TIMEOUT_SEC);
        if (response) {
            if (response->type == FRAME_HEARTBEAT) {
                tLastHeartbeat = time(NULL);
            }
            free_frame(response);
        }

        sleep(SOCKET_TIMEOUT_SEC);  // Wait before next heartbeat
    }

    return NULL;
}

/*************************************************
* @Name: vCompactWorkerArray
* @Def: Compacts and manages workers array
* @Arg: None
* @Ret: None
*************************************************/
void vCompactWorkerArray() {
    pthread_mutex_lock(&gWorkersMutex);

    size_t nNewCount = 0;
    Worker** pNewWorkers = malloc(sizeof(Worker*) * gnWorkerCount);

    // Copy non-null workers to new array
    for (size_t i = 0; i < gnWorkerCount; i++) {
        if (gpWorkers[i] != NULL) {
            pNewWorkers[nNewCount++] = gpWorkers[i];
        }
    }

    // Update array
    free(gpWorkers);
    gpWorkers = pNewWorkers;
    gnWorkerCount = nNewCount;

    pthread_mutex_unlock(&gWorkersMutex);
}

/*************************************************
* @Name: vCheckMainWorkers
* @Def: Checks and assigns main workers
* @Arg: None
* @Ret: None
*************************************************/
void vCheckMainWorkers() {
    pthread_mutex_lock(&gWorkersMutex);

    int nHasTextMain = 0;
    int nHasMediaMain = 0;

    // Check current main workers
    for (size_t i = 0; i < gnWorkerCount; i++) {
        if (gpWorkers[i] && gpWorkers[i]->nIsMain) {
            if (strcmp(gpWorkers[i]->psType, "Text") == 0) {
                nHasTextMain = 1;
            } else if (strcmp(gpWorkers[i]->psType, "Media") == 0) {
                nHasMediaMain = 1;
            }
        }
    }

    // Assign new main workers if needed
    if (!nHasTextMain) {
        for (size_t i = 0; i < gnWorkerCount; i++) {
            if (gpWorkers[i] && strcmp(gpWorkers[i]->psType, "Text") == 0) {
                gpWorkers[i]->nIsMain = 1;
                Frame* mainFrame = create_frame(FRAME_NEW_MAIN, NULL, 0);
                send_frame(gpWorkers[i]->pConn, mainFrame);
                free_frame(mainFrame);
                vWriteLog("New main Enigma worker assigned\n");
                break;
            }
        }
    }

    if (!nHasMediaMain) {
        for (size_t i = 0; i < gnWorkerCount; i++) {
            if (gpWorkers[i] && strcmp(gpWorkers[i]->psType, "Media") == 0) {
                gpWorkers[i]->nIsMain = 1;
                Frame* mainFrame = create_frame(FRAME_NEW_MAIN, NULL, 0);
                send_frame(gpWorkers[i]->pConn, mainFrame);
                free_frame(mainFrame);
                vWriteLog("New main Harley worker assigned\n");
                break;
            }
        }
    }

    pthread_mutex_unlock(&gWorkersMutex);
}

/*************************************************
* @Name: vHandleFrame
* @Def: Handles incoming frames
* @Arg: In: pConn = Connection from client
*       In: pFrame = Received frame
* @Ret: None
*************************************************/
void vHandleFrame(Connection* pConn, Frame* pFrame) {
    char sLogMsg[512];
    snprintf(sLogMsg, sizeof(sLogMsg),
             "Processing frame - Type: 0x%02X, Length: %d, Checksum: 0x%04X\n",
             pFrame->type, pFrame->data_length, pFrame->checksum);
    vWriteLog(sLogMsg);

    uint16_t nCalculatedChecksum = calculate_checksum(pFrame);
    if (nCalculatedChecksum != pFrame->checksum) {
        snprintf(sLogMsg, sizeof(sLogMsg),
                "Checksum mismatch - Expected: 0x%04X, Got: 0x%04X\n",
                pFrame->checksum, nCalculatedChecksum);
        vWriteLog(sLogMsg);
        return;
    }

    switch (pFrame->type) {
        case FRAME_WORKER_REG:
            vHandleWorkerRegistration(pConn, pFrame);
            break;

        case FRAME_CONNECT_REQ:
            vHandleFleckConnection(pConn, pFrame);
            break;

        case FRAME_DISTORT_REQ:
            vWriteLog("Received distortion request\n");
            FleckClient* pClient = NULL;
            pthread_mutex_lock(&gClientsMutex);
            for (size_t i = 0; i < gnClientCount; i++) {
                if (gpClients[i] && gpClients[i]->pConn == pConn) {
                    pClient = gpClients[i];
                    break;
                }
            }
            pthread_mutex_unlock(&gClientsMutex);

            if (pClient) {
                vHandleDistortRequest(pClient, pFrame);
            } else {
                vWriteLog("Error: Distortion request from unregistered client\n");
            }
            break;

        case FRAME_HEARTBEAT:
            pthread_mutex_lock(&gWorkersMutex);
            for (size_t i = 0; i < gnWorkerCount; i++) {
                if (gpWorkers[i] && gpWorkers[i]->pConn == pConn) {
                    Frame* response = create_frame(FRAME_HEARTBEAT, NULL, 0);
                    send_frame(pConn, response);
                    free_frame(response);
                    break;
                }
            }
            pthread_mutex_unlock(&gWorkersMutex);
            break;

        case FRAME_DISCONNECT:
            vHandleFleckDisconnection(pConn);
            break;

        default:
            snprintf(sLogMsg, sizeof(sLogMsg),
                    "Unhandled frame type: 0x%02X\n", pFrame->type);
            vWriteLog(sLogMsg);
            close_connection(pConn);
            free(pConn);
            break;
    }
}
