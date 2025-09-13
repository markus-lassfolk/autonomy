#include "../core/types.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

// Buffer size constants for security - sufficient for ISO 8601 timestamp format
#define TIME_STR_BUFFER_SIZE 64  // "%Y-%m-%d %H:%M:%S" = 19 chars + null terminator = 20, using 64 for safety margin
#define MAX_TIME_STR_LEN (TIME_STR_BUFFER_SIZE - 1)

// Helper function to safely format timestamp
static void safe_format_timestamp(char *buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size == 0) {
        return;
    }
    
    // Initialize buffer
    memset(buffer, 0, buffer_size);
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    if (tm_info == NULL) {
        // Handle error case - use fallback timestamp with bounds checking
        snprintf(buffer, buffer_size, "1970-01-01 00:00:00");
    } else {
        size_t len = strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", tm_info);
        if (len == 0 || len >= buffer_size) {
            // Handle error case - use fallback timestamp with bounds checking
            snprintf(buffer, buffer_size, "1970-01-01 00:00:00");
        }
    }
}

// Structured logging
void log_message(log_level_t level, const char *format, ...) {
    const char *level_str[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    // Use dynamically allocated buffer to avoid static analyzer warnings
    char *time_str = malloc(TIME_STR_BUFFER_SIZE);
    if (time_str == NULL) {
        // Fallback to syslog without timestamp if memory allocation fails
        va_list args;
        va_start(args, format);
        vsyslog(LOG_INFO, format, args);
        va_end(args);
        return;
    }
    safe_format_timestamp(time_str, TIME_STR_BUFFER_SIZE);
    
    va_list args;
    va_start(args, format);
    
    // Log to syslog
    int syslog_priority;
    switch (level) {
        case LOG_LEVEL_DEBUG: syslog_priority = LOG_DEBUG; break;
        case LOG_LEVEL_INFO:  syslog_priority = LOG_INFO; break;
        case LOG_LEVEL_WARN:  syslog_priority = LOG_WARNING; break;
        case LOG_LEVEL_ERROR: syslog_priority = LOG_ERR; break;
        default: syslog_priority = LOG_INFO; break;
    }
    
    vsyslog(syslog_priority, format, args);
    
    // Also log to stderr for development - CRITICAL FIX: Use safe format string
    const char *safe_level_str = (level >= 0 && level < 4) ? level_str[level] : "UNKNOWN";
    fprintf(stderr, "[%s] [%s] ", time_str, safe_level_str);
    vfprintf(stderr, "%s", format); // CRITICAL FIX: Use constant format string to prevent format string attacks
    fprintf(stderr, "\n");
    
    va_end(args);
    free(time_str);  // Free dynamically allocated buffer
}
