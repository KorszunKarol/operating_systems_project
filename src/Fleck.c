#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#define MAX_IP_LENGTH 16
#define MAX_PORT_LENGTH 6
#define MAX_PATH_LENGTH 256
#define MAX_USERNAME_LENGTH 64
#define MAX_COMMAND_LENGTH 256

#define printF(x) write(1, x, strlen(x))
#define ERROR_MSG_CONFIG "Error opening config file\n"
#define ERROR_MSG_USAGE "Usage: Fleck <config_file>\n"
#define ERROR_MSG_COMMAND "ERROR: Please input a valid command.\n"
#define ERROR_MSG_NOT_CONNECTED "Cannot distort, you are not connected to Mr. J System\n"
#define ERROR_MSG_DISTORT_USAGE "Usage: DISTORTED <file.xxx> <factor>\n"

typedef struct {
    char username[MAX_USERNAME_LENGTH];
    char folder_path[MAX_PATH_LENGTH];
    char gotham_ip[MAX_IP_LENGTH];
    char gotham_port[MAX_PORT_LENGTH];
} FleckConfig;

FleckConfig config;
int is_connected = 0;

void nothing() {
    signal(SIGINT, nothing);
}

void sanitize_username(char *username) {
    char *src = username;
    char *dst = username;
    while (*src) {
        if (*src != '&') {
            *dst = *src;
            dst++;
        }
        src++;
    }
    *dst = '\0';
}

void verify_directory() {
    DIR *dir = opendir(config.folder_path);
    if (dir == NULL) {
        char *error;
        asprintf(&error, "Error: Directory %s does not exist\n", config.folder_path);
        printF(error);
        free(error);
        exit(1);
    }
    closedir(dir);
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
        strncpy(config.username, buffer, MAX_USERNAME_LENGTH - 1);
        sanitize_username(config.username);
    }

    bytes_read = read(fd, buffer, sizeof(buffer));
    if (bytes_read > 0) {
        buffer[bytes_read - 1] = '\0';
        strncpy(config.folder_path, buffer, MAX_PATH_LENGTH - 1);
    }

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

    close(fd);
}

void to_uppercase(char *str) {
    while (*str) {
        *str = toupper((unsigned char)*str);
        str++;
    }
}

void list_files(const char *type) {
    DIR *dir;
    struct dirent *entry;
    int count = 0;
    char *message;

    dir = opendir(config.folder_path);
    if (dir == NULL) {
        asprintf(&message, "Error opening directory %s\n", config.folder_path);
        printF(message);
        free(message);
        return;
    }

    if (strcmp(type, "MEDIA") == 0) {
        printF("Media files available:\n");
        while ((entry = readdir(dir)) != NULL) {
            char *ext = strrchr(entry->d_name, '.');
            if (ext != NULL &&
               (strcmp(ext, ".wav") == 0 ||
                strcmp(ext, ".jpg") == 0 ||
                strcmp(ext, ".png") == 0)) {
                count++;
                asprintf(&message, "%d. %s\n", count, entry->d_name);
                printF(message);
                free(message);
            }
        }
    } else if (strcmp(type, "TEXT") == 0) {
        printF("Text files available:\n");
        while ((entry = readdir(dir)) != NULL) {
            char *ext = strrchr(entry->d_name, '.');
            if (ext != NULL && strcmp(ext, ".txt") == 0) {
                count++;
                asprintf(&message, "%d. %s\n", count, entry->d_name);
                printF(message);
                free(message);
            }
        }
    } else {
        printF("Unknown file type. Use 'LIST MEDIA' or 'LIST TEXT'\n");
        closedir(dir);
        return;
    }

    closedir(dir);

    if (count == 0) {
        asprintf(&message, "No %s files found in %s\n", type, config.folder_path);
    } else {
        asprintf(&message, "There are %d %s files available.\n", count, type);
    }
    printF(message);
    free(message);
}

void handle_command(char *command) {
    char cmd_copy[MAX_COMMAND_LENGTH];
    strncpy(cmd_copy, command, MAX_COMMAND_LENGTH - 1);
    cmd_copy[MAX_COMMAND_LENGTH - 1] = '\0';
    to_uppercase(cmd_copy);

    char *token = strtok(cmd_copy, " \n");
    if (!token) return;

    if (strcmp(token, "CONNECT") == 0) {
        is_connected = 1;
        char *message;
        asprintf(&message, "%s connected to Mr. J System. Let the chaos begin!:)\n", config.username);
        printF(message);
        free(message);
    } else if (strcmp(token, "LOGOUT") == 0) {
        is_connected = 0;
        printF("Thanks for using Mr. J System, see you soon, chaos lover :)\n");
    } else if (strcmp(token, "LIST") == 0) {
        token = strtok(NULL, " \n");
        if (token) {
            list_files(token);
        }
    } else if (strcmp(token, "DISTORTED") == 0) {
        if (!is_connected) {
            printF(ERROR_MSG_NOT_CONNECTED);
            return;
        }
        char *filename = strtok(NULL, " \n");
        char *factor = strtok(NULL, " \n");
        if (!filename || !factor) {
            printF(ERROR_MSG_DISTORT_USAGE);
            return;
        }
        printF("Distortion started!\n");
    } else if (strcmp(token, "CHECK") == 0) {
        token = strtok(NULL, " \n");
        if (token && strcmp(token, "STATUS") == 0) {
            printF("You have no ongoing or finished distortions\n");
        } else {
            printF(ERROR_MSG_COMMAND);
        }
    } else {
        printF(ERROR_MSG_COMMAND);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printF(ERROR_MSG_USAGE);
        return 1;
    }

    signal(SIGINT, nothing);

    load_config(argv[1]);
    verify_directory();

    char *message;
    asprintf(&message, "%s user initialized\n", config.username);
    printF(message);
    free(message);

    char command[MAX_COMMAND_LENGTH];
    int bytes_read;

    while (1) {
        printF("$ ");
        bytes_read = read(STDIN_FILENO, command, MAX_COMMAND_LENGTH);
        if (bytes_read <= 0) break;

        if (command[bytes_read - 1] == '\n') {
            command[bytes_read - 1] = '\0';
        }

        handle_command(command);
    }

    return 0;
}
