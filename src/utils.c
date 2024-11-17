#include "utils.h"

char *read_until(int fd, char end) {
    int i = 0;
    ssize_t chars_read;
    char c = 0;
    char *buffer = NULL;

    while (1) {
        chars_read = read(fd, &c, sizeof(char));
        if (chars_read == 0) {
            if (i == 0) {
                return NULL;
            }
            break;
        } else if (chars_read < 0) {
            free(buffer);
            return NULL;
        }

        if (c == end) {
            break;
        }
        buffer = (char *)realloc(buffer, i + 2);
        if (!buffer) {
            return NULL;
        }
        buffer[i++] = c;
    }

    if (buffer) {
        buffer[i] = '\0';
    }
    return buffer;
}

void verify_directory(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        char *error;
        asprintf(&error, "Error: Directory %s does not exist\n", path);
        printF(error);
        free(error);
        exit(1);
    }
    closedir(dir);
}

void to_uppercase(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = toupper((unsigned char)str[i]);
    }
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

void setup_signal_handlers(void) {
    signal(SIGINT, SIG_IGN);
}

void load_fleck_config(const char *filename, FleckConfig *config) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        printF(ERROR_MSG_CONFIG);
        exit(1);
    }

    char *line;

    line = read_until(fd, '\n');
    if (line) {
        strncpy(config->username, line, MAX_USERNAME_LENGTH - 1);
        sanitize_username(config->username);
        free(line);
    }

    line = read_until(fd, '\n');
    if (line) {
        strncpy(config->folder_path, line, MAX_PATH_LENGTH - 1);
        free(line);
    }

    line = read_until(fd, '\n');
    if (line) {
        strncpy(config->gotham_ip, line, MAX_IP_LENGTH - 1);
        free(line);
    }

    line = read_until(fd, '\n');
    if (line) {
        strncpy(config->gotham_port, line, MAX_PORT_LENGTH - 1);
        free(line);
    }

    close(fd);
}

void nothing(int signum) {
    signal(SIGINT, nothing);
    (void)signum;
}

void load_worker_config(const char *filename, WorkerConfig *config) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        printF(ERROR_MSG_CONFIG);
        exit(1);
    }

    char *line;

    line = read_until(fd, '\n');
    if (line) {
        strncpy(config->gotham_ip, line, MAX_IP_LENGTH - 1);
        free(line);
    }

    line = read_until(fd, '\n');
    if (line) {
        strncpy(config->gotham_port, line, MAX_PORT_LENGTH - 1);
        free(line);
    }

    line = read_until(fd, '\n');
    if (line) {
        strncpy(config->fleck_ip, line, MAX_IP_LENGTH - 1);
        free(line);
    }

    line = read_until(fd, '\n');
    if (line) {
        strncpy(config->fleck_port, line, MAX_PORT_LENGTH - 1);
        free(line);
    }

    line = read_until(fd, '\n');
    if (line) {
        strncpy(config->save_folder, line, MAX_PATH_LENGTH - 1);
        free(line);
    }

    line = read_until(fd, '\n');
    if (line) {
        strncpy(config->worker_type, line, MAX_TYPE_LENGTH - 1);
        free(line);
    }

    close(fd);
}
