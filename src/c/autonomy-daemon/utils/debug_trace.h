#ifndef DEBUG_TRACE_H
#define DEBUG_TRACE_H

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include "../shared/logging/logx.h"

// Debug trace levels
typedef enum {
    DEBUG_TRACE_OFF = 0,
    DEBUG_TRACE_ERROR = 1,
    DEBUG_TRACE_WARN = 2,
    DEBUG_TRACE_INFO = 3,
    DEBUG_TRACE_DEBUG = 4,
    DEBUG_TRACE_TRACE = 5
} debug_trace_level_t;

// Global debug trace level
extern debug_trace_level_t g_debug_trace_level;

// Initialize debug trace system
void debug_trace_init(debug_trace_level_t level);

// Get current debug trace level
debug_trace_level_t debug_trace_get_level(void);

// Set debug trace level
void debug_trace_set_level(debug_trace_level_t level);

// Debug trace macros
#define DEBUG_TRACE_ERROR(fmt, ...) \
    do { \
        if (g_debug_trace_level >= DEBUG_TRACE_ERROR) { \
            debug_trace_print("ERROR", __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); \
        } \
    } while(0)

#define DEBUG_TRACE_WARN(fmt, ...) \
    do { \
        if (g_debug_trace_level >= DEBUG_TRACE_WARN) { \
            debug_trace_print("WARN ", __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); \
        } \
    } while(0)

#define DEBUG_TRACE_INFO(fmt, ...) \
    do { \
        if (g_debug_trace_level >= DEBUG_TRACE_INFO) { \
            debug_trace_print("INFO ", __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); \
        } \
    } while(0)

#define DEBUG_TRACE_DEBUG(fmt, ...) \
    do { \
        if (g_debug_trace_level >= DEBUG_TRACE_DEBUG) { \
            debug_trace_print("DEBUG", __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); \
        } \
    } while(0)

#define DEBUG_TRACE_TRACE(fmt, ...) \
    do { \
        if (g_debug_trace_level >= DEBUG_TRACE_TRACE) { \
            debug_trace_print("TRACE", __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); \
        } \
    } while(0)

// Function entry/exit tracing
#define DEBUG_TRACE_ENTER() \
    DEBUG_TRACE_TRACE(">>> ENTERING %s", __func__)

#define DEBUG_TRACE_EXIT() \
    DEBUG_TRACE_TRACE("<<< EXITING %s", __func__)

#define DEBUG_TRACE_EXIT_WITH_RETURN(ret) \
    DEBUG_TRACE_TRACE("<<< EXITING %s with return value: %d", __func__, ret)

// Enhanced function tracing with parameters
#define DEBUG_TRACE_ENTER_WITH_PARAMS(fmt, ...) \
    DEBUG_TRACE_TRACE(">>> ENTERING %s(" fmt ")", __func__, ##__VA_ARGS__)

#define DEBUG_TRACE_EXIT_WITH_PARAMS(fmt, ...) \
    DEBUG_TRACE_TRACE("<<< EXITING %s(" fmt ")", __func__, ##__VA_ARGS__)

// Critical function tracing (always enabled)
#define DEBUG_TRACE_CRITICAL_ENTER() \
    LOGX_ERROR_MSG("[CRITICAL] >>> ENTERING %s", __func__)

#define DEBUG_TRACE_CRITICAL_EXIT() \
    LOGX_ERROR_MSG("[CRITICAL] <<< EXITING %s", __func__)

#define DEBUG_TRACE_CRITICAL_EXIT_WITH_RETURN(ret) \
    LOGX_ERROR_MSG("[CRITICAL] <<< EXITING %s with return: %d", __func__, ret)

// Function call tracing
#define DEBUG_TRACE_CALLING(target_func) \
    DEBUG_TRACE_TRACE("CALLING: %s from %s", target_func, __func__)

#define DEBUG_TRACE_CALLED_BY(caller_func) \
    DEBUG_TRACE_TRACE("CALLED BY: %s", caller_func)

// Step-by-step tracing for complex operations
#define DEBUG_TRACE_STEP(step, fmt, ...) \
    DEBUG_TRACE_TRACE("STEP %d: " fmt, step, ##__VA_ARGS__)

// Memory allocation tracing
#define DEBUG_TRACE_MALLOC(size) \
    DEBUG_TRACE_TRACE("MALLOC: allocating %zu bytes", size)

#define DEBUG_TRACE_FREE(ptr) \
    DEBUG_TRACE_TRACE("FREE: freeing pointer %p", ptr)

// Function pointer tracing
#define DEBUG_TRACE_FUNCTION_CALL(func_name, ...) \
    DEBUG_TRACE_TRACE("CALLING: %s", func_name)

// Internal function for printing debug traces
void debug_trace_print(const char *level, const char *file, int line, const char *func, const char *fmt, ...);

// Get current timestamp string
const char* debug_trace_get_timestamp(void);

// Get process ID for tracing
pid_t debug_trace_get_pid(void);

#endif // DEBUG_TRACE_H
