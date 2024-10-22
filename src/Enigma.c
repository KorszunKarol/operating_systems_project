#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#define MAX_IP_LENGTH 16
#define MAX_PORT_LENGTH 6
#define MAX_PATH_LENGTH 256
#define MAX_TYPE_LENGTH 16

#define printF(x) write(1, x, strlen(x))
#define ERROR_MSG_CONFIG "Error opening Enigma config file\n"
#define ERROR_MSG_USAGE "Usage: Enigma <config_file>\n"
#define ERROR_MSG_TYPE "Error: Invalid worker type. Must be 'Text' or 'Media'\n"
#define ERROR_MSG_DIR "Error: Directory does not exist\n"

typedef struct {
    char gotham_ip[MAX_IP_LENGTH];
    char gotham_port[MAX_PORT_LENGTH];
    char fleck_ip[MAX_IP_LENGTH];
    char fleck_port[MAX_PORT_LENGTH];
    char save_folder[MAX_PATH_LENGTH];
    char worker_type[MAX_TYPE_LENGTH];
} WorkerConfig;

WorkerConfig config;

void nothing() {
    signal(SIGINT, nothing);
}

void verify_directory() {
    DIR *dir = opendir(config.save_folder);
    if (dir == NULL) {
        char *error;
        asprintf(&error, "Error: Directory %s does not exist\n", config.save_folder);
        printF(error);
        free(error);
        exit(1);
    }
    closedir(dir);
}

void validate_worker_type() {
    if (strcmp(config.worker_type, "Text") != 0 && strcmp(config.worker_type, "Media") != 0) {
        printF(ERROR_MSG_TYPE);
        exit(1);
    }
}

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

    signal(SIGINT, nothing);

    load_config(argv[1]);
    validate_worker_type();
    verify_directory();

    char *message;
    asprintf(&message, "Enigma initialized with Gotham at %.15s:%.5s, Fleck at %.15s:%.5s, saving to %.100s as %.15s worker\n",
             config.gotham_ip, config.gotham_port, config.fleck_ip, config.fleck_port,
             config.save_folder, config.worker_type);
    printF(message);
    free(message);

    while(1) {
        pause();
    }

    return 0;
}
