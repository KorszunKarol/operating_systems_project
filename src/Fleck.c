#include "common.h"
#include "config.h"
#include "utils.h"

#define ERROR_MSG_COMMAND "ERROR: Please input a valid command.\n"
#define ERROR_MSG_NOT_CONNECTED "Cannot distort, you are not connected to Mr. J System\n"
#define ERROR_MSG_DISTORT_USAGE "Usage: DISTORTED <file.xxx> <factor>\n"

static FleckConfig config;
static int is_connected = 0;

void handle_connect(void);
void handle_logout(void);
void list_files(const char *type);
void handle_command(char *command);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printF("Usage: Fleck <config_file>\n");
        return 1;
    }
    signal(SIGINT, nothing);

    load_fleck_config(argv[1], &config);

    verify_directory(config.folder_path);

    char *message;
    asprintf(&message, "%s user initialized\n", config.username);
    printF(message);
    free(message);

    char command[MAX_COMMAND_LENGTH];
    while (1) {
        printF("$ ");
        int bytes_read = read(STDIN_FILENO, command, MAX_COMMAND_LENGTH - 1);
        if (bytes_read <= 0) break;

        command[bytes_read - 1] = '\0';
        handle_command(command);
    }

    return 0;
}

void handle_connect(void) {
    is_connected = 1;
    char *message;
    asprintf(&message, "%s connected to Mr. J System. Let the chaos begin!:)\n",
             config.username);
    printF(message);
    free(message);
}

void handle_logout(void) {
    is_connected = 0;
    printF("Thanks for using Mr. J System, see you soon, chaos lover :)\n");
    exit(0);
}

void handle_command(char *command) {
    char cmd_copy[MAX_COMMAND_LENGTH];
    strncpy(cmd_copy, command, MAX_COMMAND_LENGTH - 1);
    cmd_copy[MAX_COMMAND_LENGTH - 1] = '\0';
    to_uppercase(cmd_copy);

    char *token = strtok(cmd_copy, " \n");
    if (!token) return;

    if (strcmp(token, "CONNECT") == 0) {
        handle_connect();
    } else if (strcmp(token, "LOGOUT") == 0) {
        handle_logout();
    } else if (strcmp(token, "LIST") == 0) {
        token = strtok(NULL, " \n");
        if (token && strcmp(token, "MEDIA") == 0) {
            list_files("MEDIA");
        } else {
            printF(ERROR_MSG_COMMAND);
        }
    } else if (strcmp(token, "TEXT") == 0) {
        token = strtok(NULL, " \n");
        if (token && strcmp(token, "LIST") == 0) {
            list_files("TEXT");
        } else {
            printF(ERROR_MSG_COMMAND);
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
        printF("Command OK\n");
    } else if (strcmp(token, "CHECK") == 0) {
        token = strtok(NULL, " \n");
        if (token && strcmp(token, "STATUS") == 0) {
            printF("Command OK\n");
        } else {
            printF(ERROR_MSG_COMMAND);
        }
    } else {
        printF("Unknown command\n");
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
