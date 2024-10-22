#ifndef UTILS_H
#define UTILS_H

#include "common.h"
#include "config.h"

// File operations
char *read_until(int fd, char end);
void verify_directory(const char *path);

// String operations
void to_uppercase(char *str);
void sanitize_username(char *username);

// Signal handlers
void setup_signal_handlers(void);

// Config operations
void load_fleck_config(const char *filename, FleckConfig *config);
void load_gotham_config(const char *filename, GothamConfig *config);
void load_worker_config(const char *filename, WorkerConfig *config);

#endif
