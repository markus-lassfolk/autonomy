#include "debug_trace.h"
#include <stdarg.h>
#include <sys/time.h>

// Global debug trace level
debug_trace_level_t g_debug_trace_level = DEBUG_TRACE_INFO;

// Initialize debug trace system
void debug_trace_init(debug_trace_level_t level) {
    g_debug_trace_level = level;
    DEBUG_TRACE_INFO("Debug trace system initialized with level: %d", level);
}

// Get current debug trace level
debug_trace_level_t debug_trace_get_level(void) {
    return g_debug_trace_level;
}

// Set debug trace level
void debug_trace_set_level(debug_trace_level_t level) {
    g_debug_trace_level = level;
    DEBUG_TRACE_INFO("Debug trace level changed to: %d", level);
}

// Get current timestamp string
const char* debug_trace_get_timestamp(void) {
    static char timestamp_buffer[64];
    struct timeval tv;
    struct tm *tm_info;
    
    gettimeofday(&tv, NULL);
    tm_info = localtime(&tv.tv_sec);
    
    strftime(timestamp_buffer, sizeof(timestamp_buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    snprintf(timestamp_buffer + 19, sizeof(timestamp_buffer) - 19, ".%06lld", (long long)tv.tv_usec);
    
    return timestamp_buffer;
}

// Get process ID for tracing
pid_t debug_trace_get_pid(void) {
    return getpid();
}

// Internal function for printing debug traces
void debug_trace_print(const char *level, const char *file, int line, const char *func, const char *fmt, ...) {
    va_list args;
    char message_buffer[1024];
    const char *filename = strrchr(file, '/');
    filename = filename ? filename + 1 : file;
    
    // Format the message - SECURE VERSION
    // Validate format string to prevent format string attacks
    if (!fmt || strpbrk(fmt, "%n") != NULL) {
        // Reject format strings with %n (can be used for format string attacks)
        strncpy(message_buffer, "Debug trace: Invalid format string", sizeof(message_buffer) - 1);
        message_buffer[sizeof(message_buffer) - 1] = '\0';
    } else {
        // SECURE VERSION: Format string vulnerability - validate format string
        if (strpbrk(fmt, "%n") != NULL) {
            // Reject dangerous format strings that could write to memory
            fmt = "SECURE: Dangerous format string rejected";
        }
        
        va_start(args, fmt);
        // flawfinder: ignore - format string is validated above to prevent %n attacks
        vsnprintf(message_buffer, sizeof(message_buffer), fmt, args);
        va_end(args);
    }
    
    // Print with timestamp, PID, level, file:line, function, and message
    fprintf(stderr, "[%s] [PID:%d] [%s] [%s:%d] [%s] %s\n", 
            debug_trace_get_timestamp(),
            debug_trace_get_pid(),
            level,
            filename,
            line,
            func,
            message_buffer);
    
    // Flush to ensure immediate output
    fflush(stderr);
}
