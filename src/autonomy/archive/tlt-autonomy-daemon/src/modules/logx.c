#include "logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <syslog.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// Global logging configuration
static logx_config_t g_logx_config = {
    .level = LOGX_LEVEL_INFO,
    .output = LOGX_OUTPUT_STDERR | LOGX_OUTPUT_SYSLOG,
    .format = LOGX_FORMAT_STRUCTURED,
    .timestamp_format = LOGX_TIMESTAMP_ISO8601,
    .max_file_size = 10 * 1024 * 1024,  // 10MB
    .max_files = 5,
    .file_path = "/var/log/autonomy.log"
};

// Log level names
static const char* LOGX_LEVEL_NAMES[] = {
    "TRACE",
    "DEBUG", 
    "INFO",
    "WARN",
    "ERROR",
    "FATAL"
};

// Log level syslog priorities
static const int LOGX_SYSLOG_PRIORITIES[] = {
    LOG_DEBUG,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERR,
    LOG_CRIT
};

// Log level colors for terminal output
static const char* LOGX_LEVEL_COLORS[] = {
    "\033[36m",  // Cyan for TRACE
    "\033[36m",  // Cyan for DEBUG
    "\033[32m",  // Green for INFO
    "\033[33m",  // Yellow for WARN
    "\033[31m",  // Red for ERROR
    "\033[35m"   // Magenta for FATAL
};

static const char* LOGX_RESET_COLOR = "\033[0m";

// Initialize logx system
int logx_init(const logx_config_t *config) {
    if (config) {
        memcpy(&g_logx_config, config, sizeof(logx_config_t));
    }
    
    // Open syslog if enabled
    if (g_logx_config.output & LOGX_OUTPUT_SYSLOG) {
        openlog("autonomy-daemon", LOG_PID | LOG_CONS, LOG_USER);
    }
    
    // Create log directory if file logging is enabled
    if (g_logx_config.output & LOGX_OUTPUT_FILE) {
        char *dir = strdup(g_logx_config.file_path);
        char *last_slash = strrchr(dir, '/');
        if (last_slash) {
            *last_slash = '\0';
            mkdir(dir, 0755);
        }
        free(dir);
    }
    
    return 0;
}

// Get current timestamp string
static void get_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    switch (g_logx_config.timestamp_format) {
        case LOGX_TIMESTAMP_ISO8601:
            strftime(buffer, size, "%Y-%m-%dT%H:%M:%S%z", tm_info);
            break;
        case LOGX_TIMESTAMP_UNIX:
            snprintf(buffer, size, "%ld", now);
            break;
        case LOGX_TIMESTAMP_SIMPLE:
            strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
            break;
        default:
            strftime(buffer, size, "%Y-%m-%dT%H:%M:%S%z", tm_info);
    }
}

// Write to log file with rotation
static void write_to_file(const char *message) {
    if (!(g_logx_config.output & LOGX_OUTPUT_FILE)) {
        return;
    }
    
    // Check if we need to rotate logs
    struct stat st;
    if (stat(g_logx_config.file_path, &st) == 0) {
        if (st.st_size > g_logx_config.max_file_size) {
            rotate_log_files();
        }
    }
    
    // Write to log file
    FILE *fp = fopen(g_logx_config.file_path, "a");
    if (fp) {
        fputs(message, fp);
        fputs("\n", fp);
        fclose(fp);
    }
}

// Rotate log files
static void rotate_log_files(void) {
    char old_name[256];
    char new_name[256];
    
    // Remove oldest log file
    snprintf(old_name, sizeof(old_name), "%s.%d", g_logx_config.file_path, g_logx_config.max_files - 1);
    unlink(old_name);
    
    // Shift existing log files
    for (int i = g_logx_config.max_files - 2; i >= 0; i--) {
        if (i == 0) {
            snprintf(old_name, sizeof(old_name), "%s", g_logx_config.file_path);
        } else {
            snprintf(old_name, sizeof(old_name), "%s.%d", g_logx_config.file_path, i);
        }
        snprintf(new_name, sizeof(new_name), "%s.%d", g_logx_config.file_path, i + 1);
        rename(old_name, new_name);
    }
}

// Format log message
static void format_message(char *buffer, size_t size, logx_level_t level, 
                          const char *file, int line, const char *func, 
                          const char *format, va_list args) {
    char timestamp[64];
    char message[1024];
    
    get_timestamp(timestamp, sizeof(timestamp));
    
    // Format the actual message
    vsnprintf(message, sizeof(message), format, args);
    
    if (g_logx_config.format == LOGX_FORMAT_STRUCTURED) {
        // Structured format: {"timestamp":"...","level":"...","file":"...","line":...,"func":"...","message":"..."}
        snprintf(buffer, size, 
                "{\"timestamp\":\"%s\",\"level\":\"%s\",\"file\":\"%s\",\"line\":%d,\"func\":\"%s\",\"message\":\"%s\"}",
                timestamp, LOGX_LEVEL_NAMES[level], file, line, func, message);
    } else {
        // Simple format: [timestamp] LEVEL file:line:func message
        snprintf(buffer, size, "[%s] %s %s:%d:%s %s",
                timestamp, LOGX_LEVEL_NAMES[level], file, line, func, message);
    }
}

// Core logging function
void logx_log(logx_level_t level, const char *file, int line, const char *func, const char *format, ...) {
    if (level < g_logx_config.level) {
        return;
    }
    
    char formatted_message[2048];
    va_list args;
    va_start(args, format);
    format_message(formatted_message, sizeof(formatted_message), level, file, line, func, format, args);
    va_end(args);
    
    // Output to stderr if enabled
    if (g_logx_config.output & LOGX_OUTPUT_STDERR) {
        if (isatty(STDERR_FILENO)) {
            // Colored output for terminal
            fprintf(stderr, "%s%s%s\n", 
                    LOGX_LEVEL_COLORS[level], 
                    formatted_message, 
                    LOGX_RESET_COLOR);
        } else {
            fprintf(stderr, "%s\n", formatted_message);
        }
    }
    
    // Output to syslog if enabled
    if (g_logx_config.output & LOGX_OUTPUT_SYSLOG) {
        syslog(LOGX_SYSLOG_PRIORITIES[level], "%s", formatted_message);
    }
    
    // Output to file if enabled
    if (g_logx_config.output & LOGX_OUTPUT_FILE) {
        write_to_file(formatted_message);
    }
}

// Convenience logging functions
void logx_trace(const char *file, int line, const char *func, const char *format, ...) {
    va_list args;
    va_start(args, format);
    logx_log(LOGX_LEVEL_TRACE, file, line, func, format, args);
    va_end(args);
}

void logx_debug(const char *file, int line, const char *func, const char *format, ...) {
    va_list args;
    va_start(args, format);
    logx_log(LOGX_LEVEL_DEBUG, file, line, func, format, args);
    va_end(args);
}

void logx_info(const char *file, int line, const char *func, const char *format, ...) {
    va_list args;
    va_start(args, format);
    logx_log(LOGX_LEVEL_INFO, file, line, func, format, args);
    va_end(args);
}

void logx_warn(const char *file, int line, const char *func, const char *format, ...) {
    va_list args;
    va_start(args, format);
    logx_log(LOGX_LEVEL_WARN, file, line, func, format, args);
    va_end(args);
}

void logx_error(const char *file, int line, const char *func, const char *format, ...) {
    va_list args;
    va_start(args, format);
    logx_log(LOGX_LEVEL_ERROR, file, line, func, format, args);
    va_end(args);
}

void logx_fatal(const char *file, int line, const char *func, const char *format, ...) {
    va_list args;
    va_start(args, format);
    logx_log(LOGX_LEVEL_FATAL, file, line, func, format, args);
    va_end(args);
    
    // Fatal errors should exit
    exit(1);
}

// Set log level
void logx_set_level(logx_level_t level) {
    g_logx_config.level = level;
}

// Get current log level
logx_level_t logx_get_level(void) {
    return g_logx_config.level;
}

// Set output destinations
void logx_set_output(logx_output_t output) {
    g_logx_config.output = output;
}

// Get current output configuration
logx_output_t logx_get_output(void) {
    return g_logx_config.output;
}

// Cleanup logx system
void logx_cleanup(void) {
    if (g_logx_config.output & LOGX_OUTPUT_SYSLOG) {
        closelog();
    }
}
