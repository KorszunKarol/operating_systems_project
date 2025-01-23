#include "config.h"
#include "common.h"
#include "utils.h"

void load_gotham_config(const char *filename, GothamConfig *config) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        printF(ERROR_MSG_CONFIG);
        exit(1);
    }

    char *line;

    // Read Fleck IP
    line = read_until(fd, '\n');
    if (line) {
        strncpy(config->sFleckIP, line, MAX_IP_LENGTH - 1);
        config->sFleckIP[MAX_IP_LENGTH - 1] = '\0';
        free(line);
    }

    // Read Fleck Port
    line = read_until(fd, '\n');
    if (line) {
        strncpy(config->sFleckPort, line, MAX_PORT_LENGTH - 1);
        config->sFleckPort[MAX_PORT_LENGTH - 1] = '\0';
        free(line);
    }

    // Read Worker IP
    line = read_until(fd, '\n');
    if (line) {
        strncpy(config->sWorkerIP, line, MAX_IP_LENGTH - 1);
        config->sWorkerIP[MAX_IP_LENGTH - 1] = '\0';
        free(line);
    }

    // Read Worker Port
    line = read_until(fd, '\n');
    if (line) {
        strncpy(config->sWorkerPort, line, MAX_PORT_LENGTH - 1);
        config->sWorkerPort[MAX_PORT_LENGTH - 1] = '\0';
        free(line);
    }

    close(fd);

    // Debug log
    char debug[256];
    snprintf(debug, sizeof(debug),
             "Loaded config:\nFleck IP: %s\nFleck Port: %s\nWorker IP: %s\nWorker Port: %s\n",
             config->sFleckIP, config->sFleckPort,
             config->sWorkerIP, config->sWorkerPort);
    vWriteLog(debug);
}

int validate_fleck_config(const FleckConfig *psConfig) {
    return (strlen(psConfig->sUsername) > 0 &&
            strlen(psConfig->sFolderPath) > 0 &&
            strlen(psConfig->sGothamIP) > 0 &&
            strlen(psConfig->sGothamPort) > 0);
}

