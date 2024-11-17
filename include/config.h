/*********************************
*
* @File: config.h
* @Purpose: Configuration structures and loading functions
* @Author: [Your Name]
*
*********************************/

#ifndef __CONFIG_H__
#define __CONFIG_H__

#define MAX_IP_LENGTH 16
#define MAX_PORT_LENGTH 6
#define MAX_PATH_LENGTH 256
#define MAX_USERNAME_LENGTH 64
#define MAX_TYPE_LENGTH 16
#define MAX_COMMAND_LENGTH 256

typedef struct {
    char sUsername[MAX_USERNAME_LENGTH];
    char sFolderPath[MAX_PATH_LENGTH];
    char sGothamIP[MAX_IP_LENGTH];
    char sGothamPort[MAX_PORT_LENGTH];
} FleckConfig;

typedef struct {
    char sFleckIP[MAX_IP_LENGTH];
    char sFleckPort[MAX_PORT_LENGTH];
    char sWorkerIP[MAX_IP_LENGTH];
    char sWorkerPort[MAX_PORT_LENGTH];
} GothamConfig;

typedef struct {
    char sGothamIP[MAX_IP_LENGTH];
    char sGothamPort[MAX_PORT_LENGTH];
    char sFleckIP[MAX_IP_LENGTH];
    char sFleckPort[MAX_PORT_LENGTH];
    char sSaveFolder[MAX_PATH_LENGTH];
    char sWorkerType[MAX_TYPE_LENGTH];
} WorkerConfig;

void load_fleck_config(const char *psFilename, FleckConfig *psConfig);
void load_gotham_config(const char *psFilename, GothamConfig *psConfig);
void load_worker_config(const char *psFilename, WorkerConfig *psConfig);

int validate_fleck_config(const FleckConfig *psConfig);
int validate_gotham_config(const GothamConfig *psConfig);
int validate_worker_config(const WorkerConfig *psConfig);

#endif
