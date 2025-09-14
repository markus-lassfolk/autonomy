#ifndef ADVANCED_DEBUG_H
#define ADVANCED_DEBUG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <execinfo.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// Advanced debugging utilities for crash analysis

// Function call tracing
#define MAX_CALL_DEPTH 50
#define MAX_FUNCTION_NAME 256

typedef struct {
    char function_name[MAX_FUNCTION_NAME];
    char file_name[MAX_FUNCTION_NAME];
    int line_number;
    void *return_address;
    void *frame_pointer;
} call_frame_t;

typedef struct {
    call_frame_t frames[MAX_CALL_DEPTH];
    int depth;
    int max_depth;
} call_stack_t;

// Global call stack for tracking
extern call_stack_t g_call_stack;

// Function call tracing macros
#define ADVANCED_DEBUG_ENTER() \
    do { \
        advanced_debug_push_frame(__func__, __FILE__, __LINE__); \
    } while(0)

#define ADVANCED_DEBUG_EXIT() \
    do { \
        advanced_debug_pop_frame(); \
    } while(0)

// Memory access tracking
typedef struct {
    void *address;
    size_t size;
    char operation[16]; // "read", "write", "alloc", "free"
    char location[256]; // function:file:line
    time_t timestamp;
} memory_access_t;

#define MAX_MEMORY_ACCESSES 1000
extern memory_access_t g_memory_accesses[MAX_MEMORY_ACCESSES];
extern int g_memory_access_count;

// Memory access tracking macros
#define ADVANCED_DEBUG_MEMORY_READ(ptr, size) \
    advanced_debug_track_memory_access(ptr, size, "read", __func__, __FILE__, __LINE__)

#define ADVANCED_DEBUG_MEMORY_WRITE(ptr, size) \
    advanced_debug_track_memory_access(ptr, size, "write", __func__, __FILE__, __LINE__)

#define ADVANCED_DEBUG_MEMORY_ALLOC(ptr, size) \
    advanced_debug_track_memory_access(ptr, size, "alloc", __func__, __FILE__, __LINE__)

#define ADVANCED_DEBUG_MEMORY_FREE(ptr) \
    advanced_debug_track_memory_access(ptr, 0, "free", __func__, __FILE__, __LINE__)

// UBUS method call tracking
typedef struct {
    char method_name[128];
    char caller_info[256];
    time_t timestamp;
    void *context;
} ubus_call_t;

#define MAX_UBUS_CALLS 100
extern ubus_call_t g_ubus_calls[MAX_UBUS_CALLS];
extern int g_ubus_call_count;

#define ADVANCED_DEBUG_UBUS_CALL(method_name) \
    advanced_debug_track_ubus_call(method_name, __func__, __FILE__, __LINE__)

// Function prototypes
void advanced_debug_init(void);
void advanced_debug_cleanup(void);
void advanced_debug_push_frame(const char *function, const char *file, int line);
void advanced_debug_pop_frame(void);
void advanced_debug_print_stack(FILE *stream);
void advanced_debug_track_memory_access(void *ptr, size_t size, const char *operation, 
                                       const char *function, const char *file, int line);
void advanced_debug_track_ubus_call(const char *method_name, const char *function, 
                                   const char *file, int line);
void advanced_debug_print_memory_accesses(FILE *stream);
void advanced_debug_print_ubus_calls(FILE *stream);
void advanced_debug_save_crash_info(const char *crash_reason);
void advanced_debug_install_signal_handlers(void);

// Enhanced crash handler
void advanced_crash_handler(int sig, siginfo_t *info, void *context);

// Memory corruption detection
void advanced_debug_check_memory_corruption(void);
int advanced_debug_validate_pointer(void *ptr, size_t size);

// Thread safety
void advanced_debug_lock(void);
void advanced_debug_unlock(void);

#endif // ADVANCED_DEBUG_H
