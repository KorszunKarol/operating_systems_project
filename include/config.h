#ifndef CONFIG_H
#define CONFIG_H

#define MAX_IP_LENGTH 16
#define MAX_PORT_LENGTH 6
#define MAX_PATH_LENGTH 256
#define MAX_USERNAME_LENGTH 64
#define MAX_TYPE_LENGTH 16
#define MAX_COMMAND_LENGTH 256

typedef struct {
    char username[MAX_USERNAME_LENGTH];
    char folder_path[MAX_PATH_LENGTH];
    char gotham_ip[MAX_IP_LENGTH];
    char gotham_port[MAX_PORT_LENGTH];
} FleckConfig;

typedef struct {
    char fleck_ip[MAX_IP_LENGTH];
    char fleck_port[MAX_PORT_LENGTH];
    char worker_ip[MAX_IP_LENGTH];
    char worker_port[MAX_PORT_LENGTH];
} GothamConfig;

typedef struct {
    char gotham_ip[MAX_IP_LENGTH];
    char gotham_port[MAX_PORT_LENGTH];
    char fleck_ip[MAX_IP_LENGTH];
    char fleck_port[MAX_PORT_LENGTH];
    char save_folder[MAX_PATH_LENGTH];
    char worker_type[MAX_TYPE_LENGTH];
} WorkerConfig;

#endif
