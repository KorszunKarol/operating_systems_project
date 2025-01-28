#include "../include/logging.h"
#include "../include/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>

#define DEBUG 1

static int log_fd = -1;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static const char* level_strings[] = {
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR"
};

void init_logging(const char* log_file) {
    pthread_mutex_lock(&log_mutex);
    if (log_fd >= 0) {
        close(log_fd);
    }
    log_fd = open(log_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
    pthread_mutex_unlock(&log_mutex);
}

void close_logging(void) {
    pthread_mutex_lock(&log_mutex);
    if (log_fd >= 0) {
        close(log_fd);
        log_fd = -1;
    }
    pthread_mutex_unlock(&log_mutex);
}

static void write_log(const char* message) {
    if (log_fd >= 0) {
        write(log_fd, message, strlen(message));
    } else {
        write(STDOUT_FILENO, message, strlen(message));
    }
}

void log_error(const char* module, const char* message) {
    char buffer[256];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, sizeof(buffer), "[%Y-%m-%d %H:%M:%S]", t);

    dprintf(STDERR_FILENO, "%s ERROR [%s] %s\n", buffer, module, message);
}

void log_message(LogLevel level, const char* module, const char* format, ...) {
    char buffer[512];
    va_list args;
    va_start(args, format);

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, sizeof(buffer), "[%Y-%m-%d %H:%M:%S]", t);

    const char* level_str = "INFO";
    if(level == LOG_WARNING) level_str = "WARN";
    else if(level == LOG_ERROR) level_str = "ERROR";

    dprintf(STDOUT_FILENO, "%s %s [%s] ", buffer, level_str, module);
    vdprintf(STDOUT_FILENO, format, args);
    dprintf(STDOUT_FILENO, "\n");

    va_end(args);
}

void log_frame(LogLevel level, const char* module, const Frame* frame) {
    if (!frame) {
        log_message(level, module, "Attempted to log NULL frame");
        return;
    }

    char frame_info[512];
    snprintf(frame_info, sizeof(frame_info),
             "Frame{type=0x%02X, length=%u, checksum=0x%04X, timestamp=%u, data=%.*s}",
             frame->type, frame->data_length, frame->checksum, frame->timestamp,
             frame->data_length, frame->data);

    log_message(level, module, "%s", frame_info);
}