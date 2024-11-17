#ifndef UTILS_H
#define UTILS_H

#include "common.h"
#include "config.h"

char *read_until(int fd, char end);
void verify_directory(const char *path);

void to_uppercase(char *str);
void sanitize_username(char *username);

void setup_signal_handlers(void);

void load_fleck_config(const char *filename, FleckConfig *config);
void load_gotham_config(const char *filename, GothamConfig *config);
void load_worker_config(const char *filename, WorkerConfig *config);

#endif
