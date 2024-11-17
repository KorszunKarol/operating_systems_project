/*********************************
*
* @File: Harley.c
* @Purpose: Media distortion worker implementation for Gotham system
* @Author: Karol Korszun
* @Date: 2024-03-19
*
*********************************/

#include "common.h"
#include "config.h"
#include "network.h"
#include "utils.h"
#include "shared.h"
#include <pthread.h>

/* Add after includes */
/* Function declarations */
void vNotifyGothamDisconnection(void);
void vLogInitialization(const WorkerConfig* pConfig);

/* Global variables */
static WorkerConfig gConfig;
static Connection* gpGothamConn = NULL;
static Connection* gpClientConn = NULL;
static volatile int gnIsRunning = 1;
static volatile int gnIsProcessingDistortion = 0;
static int gnIsMainWorker = 0;  // Track if this is the main worker

/*************************************************
* @Name: vHandleGothamCrash
* @Def: Handles cleanup when Gotham connection is lost
* @Arg: None
* @Ret: None
*************************************************/
void vHandleGothamCrash(void) {
    /* Local variables */
    int nRetryCount = 0;
    const int MAX_RETRY = 3;

    vWriteLog("Lost connection to Gotham. Finishing current work...");
    gnIsRunning = 0;

    /* Wait for current distortion to complete */
    while (gnIsProcessingDistortion && nRetryCount < MAX_RETRY) {
        sleep(1);
        nRetryCount++;
    }

    /* Cleanup connections */
    if (gpClientConn != NULL) {
        close_connection(gpClientConn);
    }
    if (gpGothamConn != NULL) {
        close_connection(gpGothamConn);
    }
    exit(0);
}

/*************************************************
* @Name: vMonitorGotham
* @Def: Thread function monitoring Gotham connection
* @Arg: pvArg - Thread argument (unused)
* @Ret: NULL
*************************************************/
void* vMonitorGotham(void* pvArg) {
    (void)pvArg;
    /* Local variables */
    char cBuffer[2];
    int nBytesRead;

    while (gnIsRunning == 1) {
        nBytesRead = receive_data(gpGothamConn, cBuffer, 1);
        if (nBytesRead <= 0) {
            vHandleGothamCrash();
            break;
        }
        sleep(1);
    }
    return NULL;
}

/*************************************************
* @Name: vHandleSigInt
* @Def: SIGINT signal handler for graceful shutdown
* @Arg: nSigNum - Signal number received
* @Ret: None
*************************************************/
void vHandleSigInt(int nSigNum) {
    (void)nSigNum;
    vWriteLog("Received shutdown signal. Cleaning up...");
    gnIsRunning = 0;

    if (gnIsProcessingDistortion) {
        vWriteLog("Waiting for current distortion to complete");
        while (gnIsProcessingDistortion) {
            sleep(1);
        }
    }

    vNotifyGothamDisconnection();

    if (gpClientConn != NULL) {
        close_connection(gpClientConn);
    }
    if (gpGothamConn != NULL) {
        close_connection(gpGothamConn);
    }
    exit(0);
}

/*************************************************
* @Name: vHandleWorkerRegistration
* @Def: Handles initial worker registration with Gotham
* @Arg: None
* @Ret: None
*************************************************/
void vHandleWorkerRegistration(void) {
    /* Local variables */
    char sResponse[256];
    int nBytes;

    /* Send registration message */
    char* psMsg;
    asprintf(&psMsg, "REGISTER HARLEY\n");
    send_data(gpGothamConn, psMsg, strlen(psMsg));
    free(psMsg);

    /* Wait for main worker status */
    nBytes = receive_data(gpGothamConn, sResponse, sizeof(sResponse) - 1);
    if (nBytes > 0) {
        sResponse[nBytes] = '\0';
        if (0 == strcmp(sResponse, "MAIN_WORKER\n")) {
            gnIsMainWorker = 1;
            vWriteLog("Assigned as main Harley worker");
        }
    }
}

/*************************************************
* @Name: vHandleDistortionRequest
* @Def: Processes media distortion request from Fleck
* @Arg: pClientConn - Connection to Fleck client
* @Ret: None
*************************************************/
void vHandleDistortionRequest(Connection* pClientConn) {
    gnIsProcessingDistortion = 1;

    /* Local variables */
    char sBuffer[1024];
    int nBytes;

    /* Receive distortion request details */
    nBytes = receive_data(pClientConn, sBuffer, sizeof(sBuffer) - 1);
    if (nBytes <= 0) {
        vWriteLog("Failed to receive distortion request\n");
        gnIsProcessingDistortion = 0;
        return;
    }
    sBuffer[nBytes] = '\0';

    /* Process the distortion request */
    if (gnIsMainWorker) {
        vWriteLog("New request - client wants to distort media, with factor X\n");
        vWriteLog("Receiving original media...\n");
        vWriteLog("Distorting...\n");
        vWriteLog("Sending distorted media to client...\n");

        // Simulated processing
        sleep(2);

        const char* psComplete = "DISTORTION_COMPLETE\n";
        send_data(pClientConn, psComplete, strlen(psComplete));
    } else {
        const char* psError = "ERROR: Not main worker\n";
        send_data(pClientConn, psError, strlen(psError));
    }

    gnIsProcessingDistortion = 0;
}

/*************************************************
* @Name: vHandleShutdown
* @Def: Handles system shutdown gracefully
* @Arg: None
* @Ret: None
*************************************************/
void vHandleShutdown(void) {
    /* Local variables */
    int nRetryCount = 0;
    const int MAX_RETRY = 3;

    vWriteLog("Received shutdown command from Gotham");
    gnIsRunning = 0;

    /* Notify any connected Fleck clients */
    if (gpClientConn != NULL) {
        const char* psMsg = "WORKER_SHUTDOWN\n";
        send_data(gpClientConn, psMsg, strlen(psMsg));
    }

    /* Wait for current distortion to complete */
    while (gnIsProcessingDistortion && nRetryCount < MAX_RETRY) {
        sleep(1);
        nRetryCount++;
    }

    /* Send acknowledgment to Gotham */
    const char* psAck = "SHUTDOWN_ACK\n";
    send_data(gpGothamConn, psAck, strlen(psAck));

    /* Cleanup connections */
    if (gpClientConn != NULL) {
        close_connection(gpClientConn);
    }
    if (gpGothamConn != NULL) {
        close_connection(gpGothamConn);
    }
}

/*************************************************
* @Name: vNotifyGothamDisconnection
* @Def: Notifies Gotham before disconnecting
* @Arg: None
* @Ret: None
*************************************************/
void vNotifyGothamDisconnection(void) {
    const char* psMsg = "WORKER_DISCONNECT\n";
    send_data(gpGothamConn, psMsg, strlen(psMsg));

    /* Wait for acknowledgment */
    char sBuffer[256];
    int nBytes = receive_data(gpGothamConn, sBuffer, sizeof(sBuffer) - 1);
    if (nBytes > 0) {
        sBuffer[nBytes] = '\0';
        if (0 == strcmp(sBuffer, "ACK\n")) {
            vWriteLog("Gotham acknowledged disconnection");
        }
    }
}

/*************************************************
* @Name: vLogInitialization
* @Def: Logs worker initialization details
* @Arg: In: pConfig = Worker configuration
* @Ret: None
*************************************************/
void vLogInitialization(const WorkerConfig* pConfig) {
    char* psMsg;
    asprintf(&psMsg, "Harley initialized with Gotham at %s:%s, saving to %s as %s worker\n",
             pConfig->sGothamIP, pConfig->sGothamPort,
             pConfig->sSaveFolder, pConfig->sWorkerType);
    vWriteLog(psMsg);
    free(psMsg);
}

/*************************************************
* @Name: vSendWorkerRegistration
* @Def: Sends worker registration to Gotham
* @Arg: In: pConn = Connection to Gotham
*       In: psType = Worker type
* @Ret: None
*************************************************/
void vSendWorkerRegistration(Connection* pConn, const char* psType) {
    char* psMsg;
    asprintf(&psMsg, "WORKER %s\n", psType);
    send_data(pConn, psMsg, strlen(psMsg));
    free(psMsg);
}

/*************************************************
* @Name: main
* @Def: Entry point for Harley worker process
* @Arg: nArgc - Argument count
*       psArgv - Argument vector
* @Ret: 0 on success, 1 on failure
*************************************************/
int main(int nArgc, char* psArgv[]) {
    /* Local variables */
    pthread_t tMonitorThread;  // Added declaration
    char sBuffer[256];        // Added declaration
    ssize_t nBytes;          // Added declaration

    /* Check arguments */
    if (2 != nArgc) {
        vWriteLog("Usage: Harley <config_file>\n");
        return 1;
    }

    /* Initialize system */
    signal(SIGINT, vHandleSigInt);
    vWriteLog("Reading configuration file\n");
    load_worker_config(psArgv[1], &gConfig);

    /* Create save directory */
    if (0 != nCreateDirectory(gConfig.sSaveFolder)) {
        vWriteLog("Failed to create save directory\n");
        return 1;
    }

    /* Connect to Gotham */
    gpGothamConn = connect_to_server(gConfig.sGothamIP,
                                   nStringToInt(gConfig.sGothamPort));
    if (NULL == gpGothamConn) {
        vWriteLog("Failed to connect to Gotham\n");
        return 1;
    }

    /* Log initialization with proper format */
    vWriteLog("Connecting Harley worker to the system...\n");
    vWriteLog("Connected to Mr. J System, ready to listen to Fleck petitions\n");
    vWriteLog("Waiting for connections...\n");

    /* Register as worker */
    char *psMsg;
    asprintf(&psMsg, "WORKER %s\n", gConfig.sWorkerType);
    send_data(gpGothamConn, psMsg, strlen(psMsg));
    free(psMsg);

    /* Start monitoring thread */
    if (0 != pthread_create(&tMonitorThread, NULL, vMonitorGotham, NULL)) {
        vWriteLog("Failed to create monitor thread");
        return 1;
    }
    pthread_detach(tMonitorThread);

    /* Initialize server socket for Fleck connections */
    Connection* pServerConn = create_server(gConfig.sFleckIP,
                                          atoi(gConfig.sFleckPort));
    if (!pServerConn) {
        vWriteLog("Failed to initialize server socket");
        return 1;
    }

    /* Main processing loop */
    while (1 == gnIsRunning) {
        nBytes = receive_data(gpGothamConn, sBuffer, sizeof(sBuffer) - 1);
        if (nBytes <= 0) {
            vHandleGothamCrash();
            break;
        }
        sBuffer[nBytes] = '\0';

        if (0 == strcmp(sBuffer, "SHUTDOWN\n")) {
            break;
        } else if (0 == strcmp(sBuffer, "NEW_MAIN\n")) {
            /* Promoted to main worker */
            gnIsMainWorker = 1;
            vWriteLog("Promoted to main Harley worker");
        } else if (0 == strncmp(sBuffer, "DISTORT", 7)) {
            /* Handle new client connection */
            int nClientFd = accept_connection(pServerConn);
            if (nClientFd >= 0) {
                Connection* pClientConn = malloc(sizeof(Connection));
                if (pClientConn) {
                    pClientConn->fd = nClientFd;
                    vHandleDistortionRequest(pClientConn);
                    close_connection(pClientConn);
                } else {
                    close(nClientFd);
                }
            }
        }
    }

    /* Cleanup */
    if (NULL != gpGothamConn) {
        close_connection(gpGothamConn);
    }

    return 0;
}
