#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_IP_LENGTH 16
#define MAX_PORT_LENGTH 6

#define printF(x) write(1, x, strlen(x))
#define ERROR_MSG_CONFIG "Error opening Gotham config file\n"
#define ERROR_MSG_USAGE "Usage: Gotham <config_file>\n"

typedef struct {
    char fleck_ip[MAX_IP_LENGTH];
    char fleck_port[MAX_PORT_LENGTH];
    char worker_ip[MAX_IP_LENGTH];
    char worker_port[MAX_PORT_LENGTH];
} GothamConfig;

GothamConfig config;

void load_config(const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        printF(ERROR_MSG_CONFIG);
        exit(1);
    }

    char buffer[256];
    int bytes_read;

    bytes_read = read(fd, buffer, sizeof(buffer));
    if (bytes_read > 0) {
        buffer[bytes_read - 1] = '\0';
        strncpy(config.fleck_ip, buffer, MAX_IP_LENGTH - 1);
    }

    bytes_read = read(fd, buffer, sizeof(buffer));
    if (bytes_read > 0) {
        buffer[bytes_read - 1] = '\0';
        strncpy(config.fleck_port, buffer, MAX_PORT_LENGTH - 1);
    }

    bytes_read = read(fd, buffer, sizeof(buffer));
    if (bytes_read > 0) {
        buffer[bytes_read - 1] = '\0';
        strncpy(config.worker_ip, buffer, MAX_IP_LENGTH - 1);
    }

    bytes_read = read(fd, buffer, sizeof(buffer));
    if (bytes_read > 0) {
        buffer[bytes_read - 1] = '\0';
        strncpy(config.worker_port, buffer, MAX_PORT_LENGTH - 1);
    }

    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printF(ERROR_MSG_USAGE);
        return 1;
    }

    load_config(argv[1]);

    char *message;
    asprintf(&message, "Gotham initialized with Fleck at %s:%s and Worker at %s:%s\n",
             config.fleck_ip, config.fleck_port, config.worker_ip, config.worker_port);
    printF(message);
    free(message);

    return 0;
}
