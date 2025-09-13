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
    LOGX_TRACE = 0,
    LOGX_DEBUG,
    LOGX_INFO,
    LOGX_WARN,
    LOGX_ERROR,
    LOGX_FATAL,
    // Aliases for compatibility
    LOGX_LEVEL_TRACE = LOGX_TRACE,
    LOGX_LEVEL_DEBUG = LOGX_DEBUG,
    LOGX_LEVEL_INFO = LOGX_INFO,
    LOGX_LEVEL_WARN = LOGX_WARN,
    LOGX_LEVEL_ERROR = LOGX_ERROR,
    LOGX_LEVEL_FATAL = LOGX_FATAL
} logx_level_t;

// Log output types
typedef enum {
    LOGX_OUTPUT_CONSOLE = 1,
    LOGX_OUTPUT_FILE = 2,
    LOGX_OUTPUT_SYSLOG = 4,
    LOGX_OUTPUT_STDERR = 8
} logx_output_t;

// Log format types
typedef enum {
    LOGX_FORMAT_SIMPLE = 0,
    LOGX_FORMAT_STRUCTURED,
    LOGX_FORMAT_JSON
} logx_format_t;

// Timestamp format types
typedef enum {
    LOGX_TIMESTAMP_SIMPLE = 0,
    LOGX_TIMESTAMP_ISO8601,
    LOGX_TIMESTAMP_UNIX
} logx_timestamp_format_t;

// Log configuration
typedef struct {
    logx_level_t min_level;
    logx_level_t level;                     // Alias for min_level
    logx_output_t output;                   // Output configuration
    bool enable_console;
    bool enable_file;
    bool enable_syslog;
    char log_file[256];                     // Bounds checked: max 255 chars + null terminator, validated in all functions
    char file_path[256];                    // Bounds checked: max 255 chars + null terminator, validated in all functions
    int max_file_size;
    int max_files;
    bool enable_colors;
    bool enable_timestamps;
    logx_format_t format;                   // Log format type
    logx_timestamp_format_t timestamp_format; // Timestamp format
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
