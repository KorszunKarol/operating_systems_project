/*********************************
*
* @File: worker.c
* @Purpose: Base implementation for Enigma/Harley workers
* @Author: Karol Korszun
* @Date: 2024-03-19
*
*********************************/

#include "worker.h"
#include "utils.h"
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <sys/select.h>

#define SOCKET_TIMEOUT_SEC 3  // 3 second timeout for sockets

/* Global variables */
static volatile int gnShutdownInProgress = 0;
static pthread_mutex_t gShutdownMutex = PTHREAD_MUTEX_INITIALIZER;

/* Forward declarations */
static void* vMonitorGotham(void* pvArg);
static void* vHandleClient(void* pvArg);
static void vHandleGothamCrash(Worker* pWorker);
static void vHandleSigInt(int nSigNum);
static void vHandleRegistration(Worker* pWorker);
static void vSimulateDistortion(Worker* pWorker);

/*************************************************
* @Name: create_worker
* @Def: Creates and initializes a worker
* @Arg: In: psConfigFile = path to config file
* @Ret: Worker pointer or NULL on failure
*************************************************/
Worker* create_worker(const char* psConfigFile) {
    Worker* pWorker = malloc(sizeof(Worker));
    if (!pWorker) return NULL;

    /* Initialize worker */
    pWorker->pGothamConn = NULL;
    pWorker->pClientConn = NULL;
    pWorker->pServerConn = NULL;
    pWorker->nIsRunning = 1;
    pWorker->nIsProcessing = 0;
    pWorker->nIsMainWorker = 0;
    pWorker->nIsRegistered = 0;

    /* Load configuration */
    load_worker_config(psConfigFile, &pWorker->config);
    verify_directory(pWorker->config.sSaveFolder);

    /* Set worker type and connection info */
    pWorker->psType = strdup(pWorker->config.sWorkerType);
    strncpy(pWorker->sIP, pWorker->config.sFleckIP, MAX_IP_LENGTH - 1);
    strncpy(pWorker->sPort, pWorker->config.sFleckPort, MAX_PORT_LENGTH - 1);
    pWorker->sIP[MAX_IP_LENGTH - 1] = '\0';
    pWorker->sPort[MAX_PORT_LENGTH - 1] = '\0';

    /* Set up signal handlers */
    struct sigaction sa;
    sa.sa_handler = vHandleSigInt;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    return pWorker;
}

/*************************************************
* @Name: run_worker
* @Def: Runs the worker main loop
* @Arg: In: pWorker = worker to run
* @Ret: 0 on success, -1 on failure
*************************************************/
int run_worker(Worker* pWorker) {
    vWriteLog("Reading configuration file\n");
    vWriteLog("Connecting worker to the system...\n");

    /* Connect to Gotham */
    pWorker->pGothamConn = connect_to_server(pWorker->config.sGothamIP,
                                            atoi(pWorker->config.sGothamPort));
    if (!pWorker->pGothamConn) {
        vWriteLog("Failed to connect to Gotham\n");
        return -1;
    }

    /* Register with Gotham */
    vHandleRegistration(pWorker);
    if (!pWorker->nIsRegistered) {
        vWriteLog("Registration failed\n");
        close_connection(pWorker->pGothamConn);
        return -1;
    }

    vWriteLog("Connected to Mr. J System, ready to listen to Fleck petitions\n");
    vWriteLog("Waiting for connections...\n");

    /* Start monitor thread */
    pthread_t monitor_thread;
    if (pthread_create(&monitor_thread, NULL, vMonitorGotham, pWorker) != 0) {
        vWriteLog("Failed to create monitor thread\n");
        close_connection(pWorker->pGothamConn);
        return -1;
    }
    pthread_detach(monitor_thread);

    /* Main worker loop */
    while (pWorker->nIsRunning) {
        // Use select() with timeout instead of blocking read
        fd_set readfds;
        struct timeval tv;
        FD_ZERO(&readfds);
        FD_SET(pWorker->pGothamConn->fd, &readfds);

        tv.tv_sec = SOCKET_TIMEOUT_SEC;
        tv.tv_usec = 0;

        int ready = select(pWorker->pGothamConn->fd + 1, &readfds, NULL, NULL, &tv);
        if (ready < 0) {
            if (errno != EINTR) {
                vWriteLog("Select error\n");
                break;
            }
            continue;
        }

        if (ready == 0) {
            // Timeout - continue loop
            continue;
        }

        // Data available - read frame
        Frame* frame = receive_frame(pWorker->pGothamConn);
        if (!frame) {
            if (pWorker->nIsRunning) {
                vWriteLog("Lost connection to Gotham\n");
                break;
            }
            continue;
        }

        // Handle frame
        switch (frame->type) {
            case FRAME_HEARTBEAT:
                // Heartbeat handled by monitor thread
                break;
            case FRAME_NEW_MAIN:
                vWriteLog("Promoted to main worker\n");
                pWorker->nIsMainWorker = 1;
                break;
            case FRAME_WORKER_CONNECT:
                vWriteLog("Received client connection request\n");
                // Handle client connection
                break;
            case FRAME_DISCONNECT:
                vWriteLog("Received disconnect request\n");
                pWorker->nIsRunning = 0;
                break;
            case FRAME_ERROR:
                vWriteLog("Received error frame from Gotham\n");
                break;
            default:
                vWriteLog("Received unknown frame type\n");
                break;
        }
        free_frame(frame);
    }

    /* Cleanup */
    if (pWorker->pGothamConn) {
        vWriteLog("Sending disconnect notification to Gotham\n");
        Frame* disconnect = create_frame(FRAME_DISCONNECT, pWorker->psType, strlen(pWorker->psType));
        send_frame(pWorker->pGothamConn, disconnect);
        free_frame(disconnect);
        close_connection(pWorker->pGothamConn);
    }

    return 0;
}

/*************************************************
* @Name: destroy_worker
* @Def: Cleans up worker resources
* @Arg: In: pWorker = worker to destroy
* @Ret: None
*************************************************/
void destroy_worker(Worker* pWorker) {
    if (!pWorker) return;

    /* Close connections if they exist */
    if (pWorker->pGothamConn) {
        close_connection(pWorker->pGothamConn);
        pWorker->pGothamConn = NULL;
    }
    if (pWorker->pClientConn) {
        close_connection(pWorker->pClientConn);
        pWorker->pClientConn = NULL;
    }
    if (pWorker->pServerConn) {
        close_connection(pWorker->pServerConn);
        pWorker->pServerConn = NULL;
    }

    /* Free allocated strings */
    if (pWorker->psType) {
        free(pWorker->psType);
    }

    /* Free worker structure */
    free(pWorker);
}

/*************************************************
* @Name: vMonitorGotham
* @Def: Monitors Gotham connection
* @Arg: In: pvArg = Worker pointer
* @Ret: NULL
*************************************************/
static void* vMonitorGotham(void* pvArg) {
    Worker* pWorker = (Worker*)pvArg;

    while (pWorker->nIsRunning) {
        // Use select() to wait for data with timeout
        fd_set readfds;
        struct timeval tv;
        FD_ZERO(&readfds);
        FD_SET(pWorker->pGothamConn->fd, &readfds);

        tv.tv_sec = SOCKET_TIMEOUT_SEC;
        tv.tv_usec = 0;

        int ready = select(pWorker->pGothamConn->fd + 1, &readfds, NULL, NULL, &tv);
        if (ready < 0) {
            // Error in select
            if (errno != EINTR) {  // Ignore interrupted system calls
                vHandleGothamCrash(pWorker);
                break;
            }
            continue;
        }

        if (ready == 0) {
            // Timeout - send heartbeat
            Frame* heartbeat = create_frame(FRAME_HEARTBEAT, "PING", 4);
            if (send_frame(pWorker->pGothamConn, heartbeat) < 0) {
                free_frame(heartbeat);
                vHandleGothamCrash(pWorker);
                break;
            }
            free_frame(heartbeat);
            continue;
        }

        // Data available - read frame
        Frame* frame = receive_frame(pWorker->pGothamConn);
        if (!frame) {
            vHandleGothamCrash(pWorker);
            break;
        }

        // Handle frame based on type
        switch (frame->type) {
            case FRAME_HEARTBEAT: {
                // Send heartbeat response
                Frame* response = create_frame(FRAME_HEARTBEAT, "PONG", 4);
                send_frame(pWorker->pGothamConn, response);
                free_frame(response);
                break;
            }
            case FRAME_NEW_MAIN:
                pWorker->nIsMainWorker = 1;
                break;
            case FRAME_ERROR:
                vWriteLog("Received error frame from Gotham\n");
                break;
            default:
                vWriteLog("Received unknown frame type\n");
                break;
        }

        free_frame(frame);
    }
    return NULL;
}

/*************************************************
* @Name: vHandleClient
* @Def: Handles client connection
* @Arg: In: pvArg = Worker pointer
* @Ret: NULL
*************************************************/
static void* vHandleClient(void* pvArg) {
    Worker* pWorker = (Worker*)pvArg;
    char sMsg[256];
    char sUsername[64] = "Unknown";  // Default username
    char sFileType[32] = "Unknown";
    int nFactor = 0;

    while (pWorker->nIsRunning && !pWorker->nIsProcessing) {
        Frame* frame = receive_frame(pWorker->pClientConn);
        if (!frame) break;

        switch (frame->type) {
            case FRAME_WORKER_CONNECT:
                {
                    // Parse connection info
                    char sFileName[256], sMD5[33], sFactor[32];
                    unsigned long nFileSize;
                    if (sscanf(frame->data, "%[^&]&%[^&]&%lu&%[^&]&%s",
                             sUsername, sFileName, &nFileSize, sMD5, sFactor) != 5) {
                        Frame* response = create_frame(FRAME_ERROR, "Invalid connection format", 22);
                        send_frame(pWorker->pClientConn, response);
                        free_frame(response);
                        break;
                    }

                    // Get file type from extension
                    char* psExt = strrchr(sFileName, '.');
                    if (psExt) {
                        if (strcmp(psExt, ".txt") == 0) {
                            strncpy(sFileType, "text", sizeof(sFileType)-1);
                        } else {
                            strncpy(sFileType, "media", sizeof(sFileType)-1);
                        }
                    }
                    nFactor = atoi(sFactor);

                    // Log connection
                    snprintf(sMsg, sizeof(sMsg), "New user connected: %s.\n", sUsername);
                    vWriteLog(sMsg);

                    // Log distortion request
                    snprintf(sMsg, sizeof(sMsg), "New request - %s wants to distort %s, with factor %d.\n",
                            sUsername, sFileType, nFactor);
                    vWriteLog(sMsg);

                    vWriteLog("Receiving original text...\n");
                    vWriteLog("Distorting...\n");
                    vWriteLog("Sending distorted text...\n");

                    // For Phase 1/2, just acknowledge
                    Frame* response = create_frame(FRAME_WORKER_CONNECT, NULL, 0);
                    send_frame(pWorker->pClientConn, response);
                    free_frame(response);
                }
                break;

            case FRAME_DISCONNECT:
                free_frame(frame);
                goto cleanup;

            default:
                {
                    Frame* response = create_frame(FRAME_ERROR, "Unknown frame type", 16);
                    send_frame(pWorker->pClientConn, response);
                    free_frame(response);
                }
        }

        free_frame(frame);
    }

cleanup:
    close_connection(pWorker->pClientConn);
    pWorker->pClientConn = NULL;
    return NULL;
}

/*************************************************
* @Name: vHandleGothamCrash
* @Def: Handles Gotham server crash
* @Arg: In: pWorker = Worker pointer
* @Ret: None
*************************************************/
static void vHandleGothamCrash(Worker* pWorker) {
    vWriteLog("Lost connection to Gotham. Finishing current work...\n");
    pWorker->nIsRunning = 0;

    /* Wait for current distortion to complete */
    while (pWorker->nIsProcessing) {
        sleep(1);
    }

    /* Cleanup connections */
    if (pWorker->pClientConn) {
        close_connection(pWorker->pClientConn);
        pWorker->pClientConn = NULL;
    }
    if (pWorker->pGothamConn) {
        close_connection(pWorker->pGothamConn);
        pWorker->pGothamConn = NULL;
    }
}

/*************************************************
* @Name: vHandleSigInt
* @Def: Handles SIGINT signal
* @Arg: In: nSigNum = Signal number
* @Ret: None
*************************************************/
static void vHandleSigInt(int nSigNum) {
    (void)nSigNum;

    pthread_mutex_lock(&gShutdownMutex);
    if (gnShutdownInProgress) {
        pthread_mutex_unlock(&gShutdownMutex);
        return;
    }
    gnShutdownInProgress = 1;
    pthread_mutex_unlock(&gShutdownMutex);

    vWriteLog("Received CTRL+C. Initiating worker shutdown...\n");
    exit(0);
}

/*************************************************
* @Name: vHandleRegistration
* @Def: Handles worker registration
* @Arg: In: pWorker = Worker pointer
* @Ret: None
*************************************************/
static void vHandleRegistration(Worker* pWorker) {
    char sData[DATA_SIZE];
    snprintf(sData, sizeof(sData), "%s&%s&%s",
             pWorker->psType,
             pWorker->sIP,
             pWorker->sPort);

    vWriteLog("Sending registration frame to Gotham\n");

    Frame* frame = create_frame(FRAME_WORKER_REG, sData, strlen(sData));
    if (!frame) {
        vWriteLog("Failed to create registration frame\n");
        return;
    }

    if (send_frame(pWorker->pGothamConn, frame) != 0) {
        vWriteLog("Failed to send registration frame\n");
        free_frame(frame);
        return;
    }
    free_frame(frame);

    Frame* response = receive_frame(pWorker->pGothamConn);
    if (!response) {
        vWriteLog("Failed to receive response\n");
        return;
    }

    switch (response->type) {
        case FRAME_WORKER_REG:
            if (response->data_length == 0) {
                vWriteLog("Registration successful as backup worker\n");
                pWorker->nIsRegistered = 1;
            }
            break;
        case FRAME_NEW_MAIN:
            vWriteLog("Registration successful as main worker\n");
            pWorker->nIsMainWorker = 1;
            pWorker->nIsRegistered = 1;
            break;
        case FRAME_ERROR:
            vWriteLog("Registration failed - error frame received\n");
            break;
        default:
            vWriteLog("Received unknown frame type\n");
    }
    free_frame(response);
}

/*************************************************
* @Name: vSimulateDistortion
* @Def: Simulates distortion
* @Arg: In: pWorker = Worker pointer
* @Ret: None
*************************************************/
static void vSimulateDistortion(Worker* pWorker) {
    vWriteLog("Receiving original file...\n");

    // Simulate receiving file
    Frame* frame = receive_frame(pWorker->pClientConn);
    if (!frame || frame->type != FRAME_FILE_DATA) {
        if (frame) free_frame(frame);
        return;
    }
    free_frame(frame);

    vWriteLog("Distorting...\n");
    sleep(1); // Simulate processing

    // Send completion info
    Frame* info = create_frame(FRAME_FILE_INFO, "DONE", 4);
    send_frame(pWorker->pClientConn, info);
    free_frame(info);

    // Send distorted data
    Frame* data = create_frame(FRAME_FILE_DATA, "DISTORTED_DATA", 13);
    send_frame(pWorker->pClientConn, data);
    free_frame(data);

    vWriteLog("Sending distorted file...\n");
}