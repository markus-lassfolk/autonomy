#ifndef DEBUGGING_UTILITIES_H
#define DEBUGGING_UTILITIES_H

#include "../logging/logx.h"
#include "error_handling_macros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// Comprehensive debugging utilities for troubleshooting evasive problems

// Debug levels for fine-grained control
typedef enum {
    DEBUG_LEVEL_NONE = 0,
    DEBUG_LEVEL_ERROR = 1,
    DEBUG_LEVEL_WARN = 2,
    DEBUG_LEVEL_INFO = 3,
    DEBUG_LEVEL_DEBUG = 4,
    DEBUG_LEVEL_TRACE = 5,
    DEBUG_LEVEL_VERBOSE = 6
} debug_level_t;

// Global debug level
extern debug_level_t g_debug_level;

// Debug context for tracking execution flow
typedef struct {
    char function_name[64];
    char file_name[128];
    int line_number;
    struct timespec entry_time;
    int depth;
} debug_context_t;

// Maximum call stack depth for debugging
#define MAX_DEBUG_STACK_DEPTH 32

// Global debug stack
extern debug_context_t g_debug_stack[MAX_DEBUG_STACK_DEPTH];
extern int g_debug_stack_depth;

// Function call tracing macros
#define DEBUG_ENTER() \
    do { \
        if (g_debug_level >= DEBUG_LEVEL_TRACE && g_debug_stack_depth < MAX_DEBUG_STACK_DEPTH) { \
            debug_context_t *ctx = &g_debug_stack[g_debug_stack_depth]; \
            strncpy(ctx->function_name, __func__, sizeof(ctx->function_name) - 1); \
            strncpy(ctx->file_name, __FILE__, sizeof(ctx->file_name) - 1); \
            ctx->line_number = __LINE__; \
            ctx->depth = g_debug_stack_depth; \
            clock_gettime(CLOCK_MONOTONIC, &ctx->entry_time); \
            g_debug_stack_depth++; \
            LOGX_DEBUG_MSG("%*s>>> ENTERING %s() [depth=%d]", \
                          ctx->depth * 2, "", __func__, ctx->depth); \
        } \
    } while(0)

#define DEBUG_EXIT() \
    do { \
        if (g_debug_level >= DEBUG_LEVEL_TRACE && g_debug_stack_depth > 0) { \
            g_debug_stack_depth--; \
            debug_context_t *ctx = &g_debug_stack[g_debug_stack_depth]; \
            struct timespec exit_time; \
            clock_gettime(CLOCK_MONOTONIC, &exit_time); \
            long duration_ns = (exit_time.tv_sec - ctx->entry_time.tv_sec) * 1000000000L + \
                              (exit_time.tv_nsec - ctx->entry_time.tv_nsec); \
            LOGX_DEBUG_MSG("%*s<<< EXITING %s() [depth=%d, duration=%ld.%06ld ms]", \
                          ctx->depth * 2, "", ctx->function_name, ctx->depth, \
                          duration_ns / 1000000L, (duration_ns % 1000000L) / 1000L); \
        } \
    } while(0)

#define DEBUG_EXIT_WITH_RETURN(ret) \
    do { \
        if (g_debug_level >= DEBUG_LEVEL_TRACE && g_debug_stack_depth > 0) { \
            g_debug_stack_depth--; \
            debug_context_t *ctx = &g_debug_stack[g_debug_stack_depth]; \
            struct timespec exit_time; \
            clock_gettime(CLOCK_MONOTONIC, &exit_time); \
            long duration_ns = (exit_time.tv_sec - ctx->entry_time.tv_sec) * 1000000000L + \
                              (exit_time.tv_nsec - ctx->entry_time.tv_nsec); \
            LOGX_DEBUG_MSG("%*s<<< EXITING %s() [depth=%d, duration=%ld.%06ld ms, return=%d]", \
                          ctx->depth * 2, "", ctx->function_name, ctx->depth, \
                          duration_ns / 1000000L, (duration_ns % 1000000L) / 1000L, (int)(ret)); \
        } \
        return ret; \
    } while(0)

// Memory debugging macros
#define DEBUG_MALLOC(size) \
    ({ \
        void *_ptr = malloc(size); \
        if (g_debug_level >= DEBUG_LEVEL_DEBUG) { \
            LOGX_DEBUG_MSG("MALLOC: %p = malloc(%zu) at %s:%d in %s()", \
                          _ptr, (size_t)(size), __FILE__, __LINE__, __func__); \
        } \
        _ptr; \
    })

#define DEBUG_FREE(ptr) \
    do { \
        if (g_debug_level >= DEBUG_LEVEL_DEBUG) { \
            LOGX_DEBUG_MSG("FREE: free(%p) at %s:%d in %s()", \
                          (void*)(ptr), __FILE__, __LINE__, __func__); \
        } \
        free(ptr); \
    } while(0)

#define DEBUG_CALLOC(count, size) \
    ({ \
        void *_ptr = calloc(count, size); \
        if (g_debug_level >= DEBUG_LEVEL_DEBUG) { \
            LOGX_DEBUG_MSG("CALLOC: %p = calloc(%zu, %zu) at %s:%d in %s()", \
                          _ptr, (size_t)(count), (size_t)(size), __FILE__, __LINE__, __func__); \
        } \
        _ptr; \
    })

// Variable state debugging macros
#define DEBUG_VAR_INT(var) \
    do { \
        if (g_debug_level >= DEBUG_LEVEL_VERBOSE) { \
            LOGX_DEBUG_MSG("VAR: %s = %d at %s:%d in %s()", \
                          #var, (int)(var), __FILE__, __LINE__, __func__); \
        } \
    } while(0)

#define DEBUG_VAR_STR(var) \
    do { \
        if (g_debug_level >= DEBUG_LEVEL_VERBOSE) { \
            LOGX_DEBUG_MSG("VAR: %s = \"%s\" at %s:%d in %s()", \
                          #var, (var) ? (var) : "(null)", __FILE__, __LINE__, __func__); \
        } \
    } while(0)

#define DEBUG_VAR_PTR(var) \
    do { \
        if (g_debug_level >= DEBUG_LEVEL_VERBOSE) { \
            LOGX_DEBUG_MSG("VAR: %s = %p at %s:%d in %s()", \
                          #var, (void*)(var), __FILE__, __LINE__, __func__); \
        } \
    } while(0)

#define DEBUG_VAR_FLOAT(var) \
    do { \
        if (g_debug_level >= DEBUG_LEVEL_VERBOSE) { \
            LOGX_DEBUG_MSG("VAR: %s = %.6f at %s:%d in %s()", \
                          #var, (double)(var), __FILE__, __LINE__, __func__); \
        } \
    } while(0)

// Conditional debugging macros
#define DEBUG_IF(condition, message) \
    do { \
        if ((condition) && g_debug_level >= DEBUG_LEVEL_DEBUG) { \
            LOGX_DEBUG_MSG("DEBUG_IF(%s): %s at %s:%d in %s()", \
                          #condition, message, __FILE__, __LINE__, __func__); \
        } \
    } while(0)

#define DEBUG_CHECKPOINT(name) \
    do { \
        if (g_debug_level >= DEBUG_LEVEL_DEBUG) { \
            LOGX_DEBUG_MSG("CHECKPOINT: %s at %s:%d in %s()", \
                          name, __FILE__, __LINE__, __func__); \
        } \
    } while(0)

// State machine debugging
#define DEBUG_STATE_CHANGE(old_state, new_state) \
    do { \
        if (g_debug_level >= DEBUG_LEVEL_INFO) { \
            LOGX_INFO_MSG("STATE: %s -> %s at %s:%d in %s()", \
                         #old_state, #new_state, __FILE__, __LINE__, __func__); \
        } \
    } while(0)

// Network debugging macros
#define DEBUG_NETWORK_CALL(call, result) \
    do { \
        if (g_debug_level >= DEBUG_LEVEL_DEBUG) { \
            LOGX_DEBUG_MSG("NETWORK: %s = %d at %s:%d in %s()", \
                          #call, (int)(result), __FILE__, __LINE__, __func__); \
        } \
    } while(0)

// File I/O debugging macros
#define DEBUG_FILE_OPEN(filename, mode, result) \
    do { \
        if (g_debug_level >= DEBUG_LEVEL_DEBUG) { \
            LOGX_DEBUG_MSG("FILE: fopen(\"%s\", \"%s\") = %p at %s:%d in %s()", \
                          filename, mode, (void*)(result), __FILE__, __LINE__, __func__); \
        } \
    } while(0)

#define DEBUG_FILE_READ(filename, bytes_read) \
    do { \
        if (g_debug_level >= DEBUG_LEVEL_DEBUG) { \
            LOGX_DEBUG_MSG("FILE: read from \"%s\": %zu bytes at %s:%d in %s()", \
                          filename, (size_t)(bytes_read), __FILE__, __LINE__, __func__); \
        } \
    } while(0)

#define DEBUG_FILE_WRITE(filename, bytes_written) \
    do { \
        if (g_debug_level >= DEBUG_LEVEL_DEBUG) { \
            LOGX_DEBUG_MSG("FILE: write to \"%s\": %zu bytes at %s:%d in %s()", \
                          filename, (size_t)(bytes_written), __FILE__, __LINE__, __func__); \
        } \
    } while(0)

// Threading debugging macros
#define DEBUG_THREAD_CREATE(thread_id, function_name) \
    do { \
        if (g_debug_level >= DEBUG_LEVEL_INFO) { \
            LOGX_INFO_MSG("THREAD: Created thread %p for %s at %s:%d in %s()", \
                         (void*)(thread_id), function_name, __FILE__, __LINE__, __func__); \
        } \
    } while(0)

#define DEBUG_MUTEX_LOCK(mutex_name) \
    do { \
        if (g_debug_level >= DEBUG_LEVEL_VERBOSE) { \
            LOGX_DEBUG_MSG("MUTEX: Locking %s at %s:%d in %s()", \
                          mutex_name, __FILE__, __LINE__, __func__); \
        } \
    } while(0)

#define DEBUG_MUTEX_UNLOCK(mutex_name) \
    do { \
        if (g_debug_level >= DEBUG_LEVEL_VERBOSE) { \
            LOGX_DEBUG_MSG("MUTEX: Unlocking %s at %s:%d in %s()", \
                          mutex_name, __FILE__, __LINE__, __func__); \
        } \
    } while(0)

// Buffer debugging macros
#define DEBUG_BUFFER_DUMP(buffer, size, name) \
    do { \
        if (g_debug_level >= DEBUG_LEVEL_VERBOSE && (buffer) && (size) > 0) { \
            LOGX_DEBUG_MSG("BUFFER: %s (%zu bytes) at %s:%d in %s():", \
                          name, (size_t)(size), __FILE__, __LINE__, __func__); \
            debug_dump_buffer((const unsigned char*)(buffer), (size_t)(size)); \
        } \
    } while(0)

// Function declarations for debugging utilities
void debug_set_level(debug_level_t level);
debug_level_t debug_get_level(void);
void debug_dump_buffer(const unsigned char *buffer, size_t size);
void debug_dump_stack_trace(void);
void debug_print_memory_info(void);
void debug_print_thread_info(void);
void debug_save_state_to_file(const char *filename);
void debug_load_state_from_file(const char *filename);

// Debugging initialization and cleanup
int debug_utilities_init(void);
void debug_utilities_cleanup(void);

// Emergency debugging functions
void debug_emergency_dump(const char *reason);
void debug_force_core_dump(void);
void debug_print_all_globals(void);

#endif // DEBUGGING_UTILITIES_H
