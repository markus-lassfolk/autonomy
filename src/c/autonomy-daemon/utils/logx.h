#ifndef LOGX_H
#define LOGX_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <pthread.h>

// Extended logging system for RUTOS

// Log levels
typedef enum {
    LOGX_DEBUG = 0,
    LOGX_INFO,
    LOGX_WARN,
    LOGX_ERROR,
    LOGX_FATAL
} logx_level_t;

// Log configuration
typedef struct {
    logx_level_t min_level;
    bool enable_console;
    bool enable_file;
    bool enable_syslog;
    char log_file[256];
    int max_file_size;
    int max_files;
    bool enable_colors;
    bool enable_timestamps;
} logx_config_t;

// Function declarations
int logx_init(const logx_config_t *config);
void logx_cleanup(void);
void logx_log(logx_level_t level, const char *file, int line, const char *function, const char *format, ...);

// Convenience macros
#define LOGX_DEBUG_MSG(fmt, ...) logx_log(LOGX_DEBUG, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOGX_INFO_MSG(fmt, ...)  logx_log(LOGX_INFO, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOGX_WARN_MSG(fmt, ...)  logx_log(LOGX_WARN, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOGX_ERROR_MSG(fmt, ...) logx_log(LOGX_ERROR, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOGX_FATAL_MSG(fmt, ...) logx_log(LOGX_FATAL, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#endif // LOGX_H
