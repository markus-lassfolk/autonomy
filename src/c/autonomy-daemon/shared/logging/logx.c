#include "../logging/logx.h"
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

// Forward declarations
static void rotate_log_files(void);

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
    struct tm tm_info;
    
    if (localtime_r(&now, &tm_info) == NULL) {
        snprintf(buffer, size, "1970-01-01T00:00:00+0000");
        return;
    }
    
    switch (g_logx_config.timestamp_format) {
        case LOGX_TIMESTAMP_ISO8601:
            strftime(buffer, size, "%Y-%m-%dT%H:%M:%S%z", &tm_info);
            break;
        case LOGX_TIMESTAMP_UNIX:
            snprintf(buffer, size, "%lld", (long long)now);
            break;
        case LOGX_TIMESTAMP_SIMPLE:
            strftime(buffer, size, "%Y-%m-%d %H:%M:%S", &tm_info);
            break;
        default:
            strftime(buffer, size, "%Y-%m-%dT%H:%M:%S%z", &tm_info);
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
void rotate_log_files(void) {
    char old_name[512];  // Increased buffer size
    char new_name[512];  // Increased buffer size
    
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
    // Use smaller stack buffers to reduce stack pressure
    char timestamp[64];
    char message[512];  // Reduced from 1024 to 512
    
    get_timestamp(timestamp, sizeof(timestamp));
    
    // Format the actual message - SECURE VERSION
    // Validate format string to prevent format string attacks
    if (strpbrk(format, "%n") != NULL) {
        // Reject format strings with %n (can be used for format string attacks)
        strncpy(message, "LOGX: Invalid format string (contains %n)", sizeof(message) - 1);
        message[sizeof(message) - 1] = '\0';
    } else {
        // Create a copy of va_list to avoid reuse issues
        va_list args_copy;
        va_copy(args_copy, args);
        vsnprintf(message, sizeof(message), format, args_copy);
        va_end(args_copy);
    }
    
    // Bounds check for level to prevent buffer overflow
    const char* level_name = "UNKNOWN";
    if (level >= 0 && level < (sizeof(LOGX_LEVEL_NAMES) / sizeof(LOGX_LEVEL_NAMES[0]))) {
        level_name = LOGX_LEVEL_NAMES[level];
    }
    
    // Null pointer checks for safety
    if (!file) file = "unknown";
    if (!func) func = "unknown";
    if (!format) format = "no message";
    
    if (g_logx_config.format == LOGX_FORMAT_STRUCTURED) {
        // Structured format: {"timestamp":"...","level":"...","file":"...","line":...,"func":"...","message":"..."}
        snprintf(buffer, size, 
                "{\"timestamp\":\"%s\",\"level\":\"%s\",\"file\":\"%s\",\"line\":%d,\"func\":\"%s\",\"message\":\"%s\"}",
                timestamp, level_name, file, line, func, message);
    } else {
        // Simple format: [timestamp] LEVEL file:line:func message
        snprintf(buffer, size, "[%s] %s %s:%d:%s %s",
                timestamp, level_name, file, line, func, message);
    }
}

// Core logging function
void logx_log(logx_level_t level, const char *file, int line, const char *func, const char *format, ...) {
    // Safety checks to prevent crashes
    if (!format) {
        fprintf(stderr, "LOGX: NULL format string provided\n");
        return;
    }
    
    if (level < g_logx_config.level) {
        return;
    }
    
    // Use dynamic allocation to avoid stack overflow
    char *formatted_message = malloc(2048);
    if (!formatted_message) {
        // Fallback to simple output if allocation fails
        fprintf(stderr, "LOGX: Memory allocation failed for log message\n");
        return;
    }
    
    va_list args;
    va_start(args, format);
    format_message(formatted_message, 2048, level, file, line, func, format, args);
    va_end(args);
    
    // Output to stderr if enabled
    if (g_logx_config.output & LOGX_OUTPUT_STDERR) {
        if (isatty(STDERR_FILENO)) {
            // Colored output for terminal - with bounds checking
            const char* color = (level >= 0 && level < (sizeof(LOGX_LEVEL_COLORS) / sizeof(LOGX_LEVEL_COLORS[0]))) 
                               ? LOGX_LEVEL_COLORS[level] : "";
            fprintf(stderr, "%s%s%s\n", 
                    color, 
                    formatted_message, 
                    LOGX_RESET_COLOR);
        } else {
            fprintf(stderr, "%s\n", formatted_message);
        }
    }
    
    // Output to syslog if enabled
    if (g_logx_config.output & LOGX_OUTPUT_SYSLOG) {
        int priority = (level >= 0 && level < (sizeof(LOGX_SYSLOG_PRIORITIES) / sizeof(LOGX_SYSLOG_PRIORITIES[0]))) 
                      ? LOGX_SYSLOG_PRIORITIES[level] : LOG_INFO;
        syslog(priority, "%s", formatted_message);
    }
    
    // Output to file if enabled
    if (g_logx_config.output & LOGX_OUTPUT_FILE) {
        write_to_file(formatted_message);
    }
    
    // Free the allocated memory
    free(formatted_message);
}

// Convenience logging functions - REMOVED to fix va_list issue
// These functions were causing stack corruption by passing va_list to logx_log
// which expects variadic arguments. The macros in logx.h are used instead.

// Removed logx_error and logx_fatal convenience functions to fix va_list issue
// These functions were causing stack corruption by passing va_list to logx_log
// which expects variadic arguments. The macros in logx.h are used instead.

// Set log level
static void logx_set_level(logx_level_t level) {
    g_logx_config.level = level;
}

// Get current log level
static logx_level_t logx_get_level(void) {
    return g_logx_config.level;
}

// Set output destinations
static void logx_set_output(logx_output_t output) {
    g_logx_config.output = output;
}

// Get current output configuration
static logx_output_t logx_get_output(void) {
    return g_logx_config.output;
}

// Cleanup logx system
void logx_cleanup(void) {
    if (g_logx_config.output & LOGX_OUTPUT_SYSLOG) {
        closelog();
    }
}
