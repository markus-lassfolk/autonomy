#ifndef LOGX_H
#define LOGX_H

#include <stdint.h>
#include <stdbool.h>

// Log levels
typedef enum {
    LOGX_LEVEL_TRACE = 0,
    LOGX_LEVEL_DEBUG,
    LOGX_LEVEL_INFO,
    LOGX_LEVEL_WARN,
    LOGX_LEVEL_ERROR,
    LOGX_LEVEL_FATAL
} logx_level_t;

// Output destinations
typedef enum {
    LOGX_OUTPUT_STDERR = 1 << 0,
    LOGX_OUTPUT_SYSLOG = 1 << 1,
    LOGX_OUTPUT_FILE = 1 << 2
} logx_output_t;

// Log format types
typedef enum {
    LOGX_FORMAT_SIMPLE = 0,
    LOGX_FORMAT_STRUCTURED
} logx_format_t;

// Timestamp formats
typedef enum {
    LOGX_TIMESTAMP_ISO8601 = 0,
    LOGX_TIMESTAMP_UNIX,
    LOGX_TIMESTAMP_SIMPLE
} logx_timestamp_t;

// Logging configuration
typedef struct {
    logx_level_t level;
    logx_output_t output;
    logx_format_t format;
    logx_timestamp_t timestamp_format;
    size_t max_file_size;
    int max_files;
    char file_path[256];
} logx_config_t;

// Core logging function
void logx_log(logx_level_t level, const char *file, int line, const char *func, const char *format, ...);

// Convenience logging functions
void logx_trace(const char *file, int line, const char *func, const char *format, ...);
void logx_debug(const char *file, int line, const char *func, const char *format, ...);
void logx_info(const char *file, int line, const char *func, const char *format, ...);
void logx_warn(const char *file, int line, const char *func, const char *format, ...);
void logx_error(const char *file, int line, const char *func, const char *format, ...);
void logx_fatal(const char *file, int line, const char *func, const char *format, ...);

// Configuration functions
int logx_init(const logx_config_t *config);
void logx_set_level(logx_level_t level);
logx_level_t logx_get_level(void);
void logx_set_output(logx_output_t output);
logx_output_t logx_get_output(void);
void logx_cleanup(void);

// Logging macros for convenience
#define LOGX_TRACE(fmt, ...) logx_trace(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOGX_DEBUG(fmt, ...) logx_debug(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOGX_INFO(fmt, ...)  logx_info(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOGX_WARN(fmt, ...)  logx_warn(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOGX_ERROR(fmt, ...) logx_error(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOGX_FATAL(fmt, ...) logx_fatal(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#endif // LOGX_H
