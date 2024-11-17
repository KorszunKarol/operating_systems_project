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

    // Read worker IP
    line = read_until(fd, '\n');
    if (line) {
        strncpy(config->sWorkerIP, line, MAX_IP_LENGTH - 1);
        free(line);
    }

    // Read worker port
    line = read_until(fd, '\n');
    if (line) {
        strncpy(config->sWorkerPort, line, MAX_PORT_LENGTH - 1);
        free(line);
    }

    close(fd);
}

int validate_fleck_config(const FleckConfig *psConfig) {
    return (strlen(psConfig->sUsername) > 0 &&
            strlen(psConfig->sFolderPath) > 0 &&
            strlen(psConfig->sGothamIP) > 0 &&
            strlen(psConfig->sGothamPort) > 0);
}

