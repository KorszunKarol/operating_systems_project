#include "common.h"
#include "config.h"
#include "network.h"
#include "utils.h"
#include <pthread.h>

#define ERROR_MSG_TYPE_ENIGMA "Error: Invalid worker type. Must be 'Text' or 'Media'\n"
#define ERROR_MSG_USAGE_ENIGMA "Usage: Enigma <config_file>\n"

static WorkerConfig gConfig;
static Connection *gpGothamConn = NULL;
static Connection *gpClientConn = NULL;
static volatile int gnRunning = 1;
static volatile int gnProcessingDistortion = 0;

/*************************************************
*
* @Name: handleGothamCrash
* @Def: Handles the event when the connection to Gotham crashes.
* @Arg: None
* @Ret: None
*
*************************************************/

void handle_gotham_crash() {
    printF("Lost connection to Gotham. Finishing current work...\n");
    gnRunning = 0;

    // If we're processing a distortion, wait for it to finish
    while (gnProcessingDistortion) {
        sleep(1);
    }

    if (gpClientConn) {
        close_connection(gpClientConn);
    }
    if (gpGothamConn) {
        close_connection(gpGothamConn);
    }
    exit(0);
}

void *monitor_gotham(void *arg) {
    (void)arg;  // Suppress unused parameter warning
    char buffer[2];
    while (gnRunning) {
        if (receive_data(gpGothamConn, buffer, 1) <= 0) {
            handle_gotham_crash();
            break;
        }
        sleep(1);
    }
    return NULL;
}

void handle_client_crash(Connection *conn) {
    printF("Client connection lost. Cleaning up...\n");
    if (gnProcessingDistortion) {
        gnProcessingDistortion = 0;
    }
    close_connection(conn);
    gpClientConn = NULL;
}

void *handle_client(void *arg) {
    Connection *conn = (Connection *)arg;
    char buffer[256];

    while (gnRunning && !gnProcessingDistortion) {
        ssize_t n = receive_data(conn, buffer, sizeof(buffer)-1);
        if (n <= 0) {
            handle_client_crash(conn);
            break;
        }
        buffer[n] = '\0';

        if (strncmp(buffer, "DISTORT", 7) == 0) {
            if (!gnRunning) {
                char *msg = "SERVER_SHUTDOWN\n";
                send_data(conn, msg, strlen(msg));
                break;
            }

            gnProcessingDistortion = 1;
            // Simulate distortion process
            sleep(2);
            char *msg = "DISTORTION_COMPLETE\n";
            send_data(conn, msg, strlen(msg));
            gnProcessingDistortion = 0;
        }
    }

    return NULL;
}

void handle_sigint(int signum) {
    (void)signum;  // Suppress unused parameter warning
    printF("Received shutdown signal. Finishing current work...\n");
    gnRunning = 0;

    // Wait for current distortion to finish
    while (gnProcessingDistortion) {
        sleep(1);
    }

    if (gpClientConn) {
        close_connection(gpClientConn);
    }
    if (gpGothamConn) {
        close_connection(gpGothamConn);
    }
    exit(0);
}

/*************************************************
*
* @Name: main
* @Def: Entry point of the Enigma application.
* @Arg: In: argc = argument count
*       In: argv = argument vector (expects config file)
* @Ret: Returns 0 on successful execution.
*
*************************************************/

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printF(ERROR_MSG_USAGE_ENIGMA);
        return 1;
    }

    signal(SIGINT, handle_sigint);
    vWriteLog("Reading configuration file\n");
    load_worker_config(argv[1], &gConfig);

    // Create directory if it doesn't exist
    char *psCmd;
    asprintf(&psCmd, "mkdir -p %s", gConfig.sSaveFolder);
    system(psCmd);
    free(psCmd);

    verify_directory(gConfig.sSaveFolder);

    if (strcmp(gConfig.sWorkerType, "Text") != 0 &&
        strcmp(gConfig.sWorkerType, "Media") != 0) {
        printF(ERROR_MSG_TYPE_ENIGMA);
        return 1;
    }

    // Connect to Gotham
    vWriteLog("Connecting Enigma worker to the system...\n");
    gpGothamConn = connect_to_server(gConfig.sGothamIP, atoi(gConfig.sGothamPort));
    if (!gpGothamConn) {
        vWriteLog("Failed to connect to Gotham\n");
        return 1;
    }

    vWriteLog("Connected to Mr. J System, ready to listen to Fleck petitions\n");
    vWriteLog("Waiting for connections...\n");

    // Register with Gotham
    vWriteLog("Sending registration to Gotham...\n");
    char *msg;
    asprintf(&msg, "WORKER %s\n", gConfig.sWorkerType);
    ssize_t bytes_sent = send_data(gpGothamConn, msg, strlen(msg));
    if (bytes_sent <= 0) {
        vWriteLog("Failed to send registration message\n");
        free(msg);
        close_connection(gpGothamConn);
        return 1;
    }
    free(msg);
    vWriteLog("Registration message sent successfully\n");

    // Start Gotham monitoring thread
    pthread_t monitor_thread;
    pthread_create(&monitor_thread, NULL, monitor_gotham, NULL);
    pthread_detach(monitor_thread);

    // Create server for client connections
    Connection *server_conn = create_server(gConfig.sFleckIP, atoi(gConfig.sFleckPort));
    if (!server_conn) {
        printF("Failed to create server\n");
        close_connection(gpGothamConn);
        return 1;
    }

    printF("Worker initialized and ready for connections\n");

    // Main server loop
    while (gnRunning) {
        int client_fd = accept_connection(server_conn);
        if (client_fd < 0) continue;

        if (!gnRunning) {
            close(client_fd);
            break;
        }

        Connection *conn = malloc(sizeof(Connection));
        if (!conn) {
            close(client_fd);
            continue;
        }
        conn->fd = client_fd;
        gpClientConn = conn;

        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, handle_client, conn) != 0) {
            close_connection(conn);
            continue;
        }
        pthread_detach(client_thread);
    }

    // Cleanup
    if (server_conn) {
        close_connection(server_conn);
    }
    if (gpGothamConn) {
        close_connection(gpGothamConn);
    }
    if (gpClientConn) {
        close_connection(gpClientConn);
    }

    return 0;
}
