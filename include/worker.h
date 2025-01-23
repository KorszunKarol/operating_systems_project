#ifndef __WORKER_H__
#define __WORKER_H__

#include "network.h"
#include "config.h"

#define MAX_IP_LENGTH 16
#define MAX_PORT_LENGTH 6

typedef struct {
    Connection* pGothamConn;    // Connection to Gotham
    Connection* pClientConn;    // Connection to current client
    Connection* pServerConn;    // Server socket for client connections
    WorkerConfig config;        // Worker configuration
    volatile int nIsRunning;    // Running flag
    volatile int nIsProcessing; // Currently processing a distortion
    int nIsMainWorker;         // Is this the main worker
    int nIsRegistered;         // Registration status with Gotham
    char* psType;              // Worker type (Text/Media)
    char sIP[MAX_IP_LENGTH];   // Worker IP
    char sPort[MAX_PORT_LENGTH]; // Worker port
} Worker;

Worker* create_worker(const char* psConfigFile);
void destroy_worker(Worker* pWorker);
int run_worker(Worker* pWorker);

#endif