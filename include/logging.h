#ifndef __LOGGING_H__
#define __LOGGING_H__

#include "protocol.h"
#include <stdarg.h>

// Log levels
typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
} LogLevel;

// Initialize logging
void init_logging(const char* log_file);
void close_logging(void);

// Logging functions
void log_message(LogLevel level, const char* module, const char* format, ...);
void log_frame(LogLevel level, const char* module, const Frame* frame);
void log_error(const char* module, const char* message);

// Helper macros
#define LOG_DEBUG(module, ...) log_message(LOG_DEBUG, module, __VA_ARGS__)
#define LOG_INFO(module, ...) log_message(LOG_INFO, module, __VA_ARGS__)
#define LOG_WARNING(module, ...) log_message(LOG_WARNING, module, __VA_ARGS__)
#define LOG_ERROR(module, ...) log_message(LOG_ERROR, module, __VA_ARGS__)

#endif