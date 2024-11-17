#include "common.h"
#include "config.h"
#include "utils.h"

#define ERROR_MSG_TYPE_ENIGMA "Error: Invalid worker type. Must be 'Text' or 'Media'\n"
#define ERROR_MSG_USAGE_ENIGMA "Usage: Enigma <config_file>\n"

static WorkerConfig config;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printF(ERROR_MSG_USAGE_ENIGMA);
        return 1;
    }

    signal(SIGINT, nothing);

    load_worker_config(argv[1], &config);

    verify_directory(config.save_folder);

    if (strcmp(config.worker_type, "Text") != 0 && strcmp(config.worker_type, "Media") != 0) {
        printF(ERROR_MSG_TYPE_ENIGMA);
        return 1;
    }

    char *message;
    asprintf(&message, "Enigma initialized with Gotham at %s:%s, Fleck at %s:%s\n"
             "Working directory: %s\nWorker type: %s\n",
             config.gotham_ip, config.gotham_port,
             config.fleck_ip, config.fleck_port,
             config.save_folder, config.worker_type);
    printF(message);
    free(message);

    while (1) {
        pause();
    }

    return 0;
}
