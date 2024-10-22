#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_IP_LENGTH 16
#define MAX_PORT_LENGTH 6
#define MAX_PATH_LENGTH 256
#define MAX_TYPE_LENGTH 16

#define printF(x) write(1, x, strlen(x))
#define ERROR_MSG_CONFIG "Error opening Harley config file\n"
#define ERROR_MSG_USAGE "Usage: Harley <config_file>\n"

typedef struct {
    char gotham_ip[MAX_IP_LENGTH];
    char gotham_port[MAX_PORT_LENGTH];
    char fleck_ip[MAX_IP_LENGTH];
    char fleck_port[MAX_PORT_LENGTH];
    char save_folder[MAX_PATH_LENGTH];
    char worker_type[MAX_TYPE_LENGTH];
} WorkerConfig;

WorkerConfig config;

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
        strncpy(config.gotham_ip, buffer, MAX_IP_LENGTH - 1);
    }

    bytes_read = read(fd, buffer, sizeof(buffer));
    if (bytes_read > 0) {
        buffer[bytes_read - 1] = '\0';
        strncpy(config.gotham_port, buffer, MAX_PORT_LENGTH - 1);
    }

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
        strncpy(config.save_folder, buffer, MAX_PATH_LENGTH - 1);
    }

    bytes_read = read(fd, buffer, sizeof(buffer));
    if (bytes_read > 0) {
        buffer[bytes_read - 1] = '\0';
        strncpy(config.worker_type, buffer, MAX_TYPE_LENGTH - 1);
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
    asprintf(&message, "Harley initialized with Gotham at %.15s:%.5s, Fleck at %.15s:%.5s, saving to %.100s as %.15s worker\n",
             config.gotham_ip, config.gotham_port, config.fleck_ip, config.fleck_port,
             config.save_folder, config.worker_type);
    printF(message);
    free(message);

    return 0;
}
