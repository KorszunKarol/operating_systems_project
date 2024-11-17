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
#include "shared.h"
#include <pthread.h>

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
static WorkerMonitorData* gpWorkerMonitors = NULL;
static size_t gnMonitorCount = 0;
static pthread_t* gpWorkerThreads = NULL;

static pthread_mutex_t gWorkersMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t gClientsMutex = PTHREAD_MUTEX_INITIALIZER;

/* Function declarations */
void vHandleWorkerRegistration(Connection* pConn, const char* psInitialMsg);
void vHandleFleckConnection(Connection* pConn);
void vHandleFleckRequest(FleckClient* pClient, const char* psType);
void vHandleWorkerDisconnection(Worker* pWorker);
void vHandleShutdown(void);
void* vMonitorWorker(void* pvArg);
void vHandleSigInt(int nSigNum);

/*************************************************
* @Name: main
* @Def: Entry point for Gotham server
* @Arg: In: nArgc = Argument count
*       In: psArgv = Argument vector
* @Ret: 0 on success, 1 on failure
*************************************************/
int main(int nArgc, char* psArgv[]) {
    /* Check arguments */
    if (2 != nArgc) {
        vWriteLog("Usage: Gotham <config_file>\n");
        return 1;
    }

    /* Initialize */
    signal(SIGINT, vHandleSigInt);
    vWriteLog("Reading configuration file\n");
    load_gotham_config(psArgv[1], &gConfig);

    /* Create server socket */
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
    gpWorkerMonitors = malloc(sizeof(WorkerMonitorData));
    gpWorkerThreads = malloc(sizeof(pthread_t));

    if (!gpWorkers || !gpClients || !gpWorkerMonitors || !gpWorkerThreads) {
        vWriteLog("Failed to allocate memory\n");
        return 1;
    }

    /* Main server loop */
    while (1 == gnIsRunning) {
        /* Accept new connection */
        int nClientFd = accept_connection(gpServerConn);
        if (nClientFd < 0) {
            continue;
        }

        /* Create connection object */
        Connection* pConn = malloc(sizeof(Connection));
        if (!pConn) {
            close(nClientFd);
            continue;
        }
        pConn->fd = nClientFd;

        /* Read initial message */
        char sBuffer[256];
        ssize_t nBytes = receive_data(pConn, sBuffer, sizeof(sBuffer)-1);
        if (nBytes <= 0) {
            close_connection(pConn);
            continue;
        }
        sBuffer[nBytes] = '\0';

        /* Handle connection based on type */
        if (0 == strncmp(sBuffer, "WORKER", 6)) {
            vHandleWorkerRegistration(pConn, sBuffer);
        } else if (0 == strncmp(sBuffer, "FLECK", 5)) {
            vHandleFleckConnection(pConn);
        } else {
            close_connection(pConn);
        }
    }

    /* Cleanup */
    vHandleShutdown();
    return 0;
}

/*************************************************
* @Name: vHandleWorkerRegistration
* @Def: Handles new worker registration
* @Arg: In: pConn = Connection from new worker
*       In: psInitialMsg = Initial message read from the worker
* @Ret: None
*************************************************/
void vHandleWorkerRegistration(Connection* pConn, const char* psInitialMsg) {
    /* Local variables */
    Worker* pWorker;

    vWriteLog("Starting worker registration process...\n");

    /* Extract type from "WORKER <TYPE>" message that was already read */
    char* psType = strchr(psInitialMsg, ' ');
    if (!psType) {
        vWriteLog("Invalid worker registration format\n");
        close_connection(pConn);
        return;
    }
    psType++; // Skip space

    /* Remove newline */
    char* psNewline = strchr(psType, '\n');
    if (psNewline) {
        *psNewline = '\0';
    }

    /* Create new worker */
    pWorker = (Worker*)malloc(sizeof(Worker));
    if (!pWorker) {
        close_connection(pConn);
        return;
    }

    pWorker->pConn = pConn;
    pWorker->psType = strdup(psType);
    pWorker->nIsMain = 0;
    pWorker->nIsBusy = 0;

    pthread_mutex_lock(&gWorkersMutex);

    /* Check if this should be main worker */
    int nHasMain = 0;
    for (size_t i = 0; i < gnWorkerCount; i++) {
        if (gpWorkers[i] && strcmp(gpWorkers[i]->psType, pWorker->psType) == 0) {
            nHasMain = 1;
            break;
        }
    }
    pWorker->nIsMain = !nHasMain;

    /* Add to workers array */
    gpWorkers = realloc(gpWorkers, (gnWorkerCount + 1) * sizeof(Worker*));
    gpWorkers[gnWorkerCount++] = pWorker;

    pthread_mutex_unlock(&gWorkersMutex);

    /* Log connection with proper format */
    if (0 == strcmp(pWorker->psType, "Text")) {
        vWriteLog("New Enigma worker connected - ready to distort!\n");
    } else {
        vWriteLog("New Harley worker connected - ready to distort!\n");
    }

    /* Send confirmation if main worker */
    if (pWorker->nIsMain) {
        send_data(pWorker->pConn, "MAIN_WORKER\n", 12);
    }
}

/*************************************************
* @Name: vHandleFleckConnection
* @Def: Handles new Fleck client connection
* @Arg: In: pConn = Connection from new Fleck
* @Ret: None
*************************************************/
void vHandleFleckConnection(Connection* pConn) {
    /* Local variables */
    FleckClient* pClient;
    char sBuffer[256];
    ssize_t nBytes;

    /* Create new client structure */
    pClient = (FleckClient*)malloc(sizeof(FleckClient));
    if (!pClient) {
        close_connection(pConn);
        return;
    }

    pClient->pConn = pConn;
    pClient->pCurrentWorker = NULL;

    /* Add to clients list */
    pthread_mutex_lock(&gClientsMutex);
    gpClients = realloc(gpClients, (gnClientCount + 1) * sizeof(FleckClient*));
    gpClients[gnClientCount++] = pClient;
    pthread_mutex_unlock(&gClientsMutex);

    /* Read username and log */
    nBytes = receive_data(pConn, sBuffer, sizeof(sBuffer)-1);
    if (nBytes > 0) {
        sBuffer[nBytes] = '\0';
        char* psUsername = strchr(sBuffer, ' ');
        if (psUsername) {
            psUsername++; // Skip space
            char* psNewline = strchr(psUsername, '\n');
            if (psNewline) *psNewline = '\0';
            pClient->psUsername = strdup(psUsername);

            char* psMsg;
            asprintf(&psMsg, "New user connected: %s.\n", pClient->psUsername);
            vWriteLog(psMsg);
            free(psMsg);
        }
    }
}

/*************************************************
* @Name: vHandleSigInt
* @Def: SIGINT signal handler
* @Arg: In: nSigNum = Signal number
* @Ret: None
*************************************************/
void vHandleSigInt(int nSigNum) {
    (void)nSigNum;
    vWriteLog("Received shutdown signal. Cleaning up...\n");
    gnIsRunning = 0;
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
    /* Notify all clients */
    pthread_mutex_lock(&gClientsMutex);
    for (size_t i = 0; i < gnClientCount; i++) {
        if (gpClients[i]) {
            send_data(gpClients[i]->pConn, "SHUTDOWN\n", 9);
            close_connection(gpClients[i]->pConn);
            free(gpClients[i]->psUsername);
            free(gpClients[i]);
        }
    }
    free(gpClients);
    pthread_mutex_unlock(&gClientsMutex);

    /* Notify all workers */
    pthread_mutex_lock(&gWorkersMutex);
    for (size_t i = 0; i < gnWorkerCount; i++) {
        if (gpWorkers[i]) {
            send_data(gpWorkers[i]->pConn, "SHUTDOWN\n", 9);
            close_connection(gpWorkers[i]->pConn);
            free(gpWorkers[i]->psType);
            free(gpWorkers[i]);
        }
    }
    free(gpWorkers);
    pthread_mutex_unlock(&gWorkersMutex);

    /* Cleanup server */
    if (gpServerConn) {
        close_connection(gpServerConn);
    }
}

/*************************************************
* @Name: vHandleFleckRequest
* @Def: Handles distortion request from Fleck client
* @Arg: In: pClient = Requesting client
*       In: psType = Type of distortion requested
* @Ret: None
*************************************************/
void vHandleFleckRequest(FleckClient* pClient, const char* psType) {
    /* Local variables */
    Worker* pWorker = NULL;
    char* psMsg;

    pthread_mutex_lock(&gWorkersMutex);

    /* Find available main worker */
    for (size_t i = 0; i < gnWorkerCount; i++) {
        if (gpWorkers[i] && !gpWorkers[i]->nIsBusy &&
            gpWorkers[i]->nIsMain &&
            0 == strcmp(gpWorkers[i]->psType, psType)) {
            pWorker = gpWorkers[i];
            pWorker->nIsBusy = 1;
            break;
        }
    }

    pthread_mutex_unlock(&gWorkersMutex);

    if (!pWorker) {
        /* No worker available */
        asprintf(&psMsg, "ERROR: No %s worker available\n", psType);
        send_data(pClient->pConn, psMsg, strlen(psMsg));
        free(psMsg);
        return;
    }

    /* Log distortion request with proper format */
    if (0 == strcmp(psType, "Text")) {
        asprintf(&psMsg, "%s has sent a text distortion petition - redirecting %s to Enigma worker\n",
                pClient->psUsername, pClient->psUsername);
    } else {
        asprintf(&psMsg, "%s has sent a media distortion petition - redirecting %s to Harley worker\n",
                pClient->psUsername, pClient->psUsername);
    }
    vWriteLog(psMsg);
    free(psMsg);

    /* Send worker details to client */
    asprintf(&psMsg, "WORKER %s %s %s\n",
             pWorker->psType,
             gConfig.sWorkerIP,
             gConfig.sWorkerPort);
    send_data(pClient->pConn, psMsg, strlen(psMsg));
    free(psMsg);

    pClient->pCurrentWorker = pWorker;
}
