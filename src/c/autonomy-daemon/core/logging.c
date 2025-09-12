#include "../core/types.h"
#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include <time.h>

// Structured logging
void log_message(log_level_t level, const char *format, ...) {
    const char *level_str[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    time_t now = time(NULL);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
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
    
    // Also log to stderr for development
    fprintf(stderr, "[%s] [%s] ", time_str, level_str[level]);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    
    va_end(args);
}
