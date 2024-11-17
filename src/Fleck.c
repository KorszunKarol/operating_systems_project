#include "common.h"
#include "config.h"
#include "network.h"
#include "utils.h"
#include <pthread.h>
#include <dirent.h>

#define ERROR_MSG_COMMAND "ERROR: Please input a valid command.\n"
#define ERROR_MSG_NOT_CONNECTED "Cannot distort, you are not connected to Mr. J System\n"
#define ERROR_MSG_DISTORT_USAGE "Usage: DISTORTED <file.xxx> <factor>\n"

static FleckConfig gConfig;
static int gnIsConnected = 0;
static Connection *gpGothamConn = NULL;
static Connection *gpWorkerConn = NULL;

void handle_connect(void);
void handle_logout(void);
void list_files(const char *type);
void handle_command(char *command);
int is_valid_factor(const char *factor);
void handle_distort(const char *file, const char *factor);
void handle_gotham_crash();
void handle_worker_crash();
void *monitor_gotham(void *arg);
void *monitor_worker(void *arg);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printF("Usage: Fleck <config_file>\n");
        return 1;
    }
    signal(SIGINT, nothing);

    load_fleck_config(argv[1], &gConfig);
    verify_directory(gConfig.sFolderPath);

    char *message;
    asprintf(&message, "%s user initialized\n", gConfig.sUsername);
    printF(message);
    free(message);

    char *command = malloc(MAX_COMMAND_LENGTH);
    if (!command) {
        printF("Memory allocation failed\n");
        return 1;
    }
    while (1) {
        printF("$ ");
        int bytes_read = read(STDIN_FILENO, command, MAX_COMMAND_LENGTH - 1);
        if (bytes_read <= 0) break;

        command[bytes_read - 1] = '\0';
        handle_command(command);
    }
    free(command);

    return 0;
}

void handle_connect(void) {
    gpGothamConn = connect_to_server(gConfig.sGothamIP, atoi(gConfig.sGothamPort));
    if (!gpGothamConn) {
        printF("Failed to connect to Gotham\n");
        return;
    }

    // Send registration message
    char *psMsg = "FLECK\n";
    if (send_data(gpGothamConn, psMsg, strlen(psMsg)) <= 0) {
        close_connection(gpGothamConn);
        gpGothamConn = NULL;
        return;
    }

    // Start monitoring thread for Gotham crashes
    pthread_t pMonitorThread;
    pthread_create(&pMonitorThread, NULL, monitor_gotham, NULL);
    pthread_detach(pMonitorThread);

    gnIsConnected = 1;
    char *psMessage;
    asprintf(&psMessage, "%s connected to Mr. J System. Let the chaos begin!:)\n",
             gConfig.sUsername);
    printF(psMessage);
    free(psMessage);
}

void handle_logout(void) {
    gnIsConnected = 0;
    printF("Thanks for using Mr. J System, see you soon, chaos lover :)\n");
    exit(0);
}

void handle_command(char *psCommand) {
    char *psCmdCopy = strdup(psCommand);
    if (!psCmdCopy) {
        printF("Memory allocation failed\n");
        return;
    }
    to_uppercase(psCmdCopy);

    char *psToken = strtok(psCmdCopy, " \n");
    if (!psToken) {
        free(psCmdCopy);
        return;
    }

    if (strcmp(psToken, "CONNECT") == 0) {
        handle_connect();
    } else if (strcmp(psToken, "LOGOUT") == 0) {
        handle_logout();
    } else if (strcmp(psToken, "LIST") == 0) {
        psToken = strtok(NULL, " \n");
        if (!psToken) {
            printF(ERROR_MSG_COMMAND);
            free(psCmdCopy);
            return;
        }

        if (strcmp(psToken, "TEXT") == 0 || strcmp(psToken, "MEDIA") == 0) {
            list_files(psToken);
        } else {
            printF(ERROR_MSG_COMMAND);
        }
    } else if (strcmp(psToken, "DISTORT") == 0) {
        if (!gnIsConnected) {
            printF(ERROR_MSG_NOT_CONNECTED);
            free(psCmdCopy);
            return;
        }

        char *psFile = strtok(NULL, " \n");
        char *psFactor = strtok(NULL, " \n");

        if (!psFile || !psFactor || !is_valid_factor(psFactor)) {
            printF(ERROR_MSG_DISTORT_USAGE);
        } else {
            handle_distort(psFile, psFactor);
        }
    } else {
        printF("Unknown command\n");
    }

    free(psCmdCopy);
}

void list_files(const char *type) {
    DIR *dir;
    struct dirent *entry;
    int count = 0;
    char *message;

    dir = opendir(gConfig.sFolderPath);
    if (dir == NULL) {
        asprintf(&message, "Error opening directory %s\n", gConfig.sFolderPath);
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
        asprintf(&message, "No %s files found in %s\n", type, gConfig.sFolderPath);
    } else {
        asprintf(&message, "There are %d %s files available.\n", count, type);
    }
    printF(message);
    free(message);
}

int is_valid_factor(const char *factor) {
    char *endptr;
    double value = strtod(factor, &endptr);
    return *endptr == '\0' && value > 0 && value <= 10;
}

void handle_distort(const char *psFile, const char *psFactor) {
    if (!gnIsConnected) {
        printF(ERROR_MSG_NOT_CONNECTED);
        return;
    }

    // Request worker from Gotham
    char *psMsg;
    asprintf(&psMsg, "DISTORT %s %s\n", psFile, psFactor);
    if (send_data(gpGothamConn, psMsg, strlen(psMsg)) <= 0) {
        free(psMsg);
        handle_gotham_crash();
        return;
    }
    free(psMsg);

    // Receive worker info
    char psBuffer[256];
    ssize_t nBytes = receive_data(gpGothamConn, psBuffer, sizeof(psBuffer)-1);
    if (nBytes <= 0) {
        handle_gotham_crash();
        return;
    }
    psBuffer[nBytes] = '\0';

    // Handle worker unavailable case
    if (strncmp(psBuffer, "ERROR", 5) == 0) {
        printF("No available worker for this type of distortion\n");
        return;
    }

    // Connect to worker and handle distortion
    // ... rest of the function
}

void handle_gotham_crash() {
    printF("Lost connection to Gotham. Shutting down...\n");
    if (gpWorkerConn) {
        close_connection(gpWorkerConn);
    }
    if (gpGothamConn) {
        close_connection(gpGothamConn);
    }
    exit(1);
}

void handle_worker_crash() {
    printF("Lost connection to worker. Canceling distortion...\n");
    if (gpWorkerConn) {
        close_connection(gpWorkerConn);
        gpWorkerConn = NULL;
    }
}

void *monitor_gotham(void *arg) {
    (void)arg;
    char buffer[2];
    while (1) {
        if (receive_data(gpGothamConn, buffer, 1) <= 0) {
            handle_gotham_crash();
            break;
        }
        sleep(1);
    }
    return NULL;
}

void *monitor_worker(void *arg) {
    (void)arg;
    char buffer[2];
    while (gpWorkerConn) {
        if (receive_data(gpWorkerConn, buffer, 1) <= 0) {
            handle_worker_crash();
            break;
        }
        sleep(1);
    }
    return NULL;
}
