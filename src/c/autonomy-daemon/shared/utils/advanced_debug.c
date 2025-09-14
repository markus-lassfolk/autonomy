#include "advanced_debug.h"
#include "../logging/logx.h"
#include <time.h>
#include <sys/time.h>

// Conditional includes for systems that support them
#ifdef __GLIBC__
#include <pthread.h>
#else
// Simple mutex implementation for systems without pthread
typedef int pthread_mutex_t;
#define PTHREAD_MUTEX_INITIALIZER 0
static inline int pthread_mutex_lock(pthread_mutex_t *mutex) { (void)mutex; return 0; }
static inline int pthread_mutex_unlock(pthread_mutex_t *mutex) { (void)mutex; return 0; }
#endif

// Global variables
call_stack_t g_call_stack = {0};
memory_access_t g_memory_accesses[MAX_MEMORY_ACCESSES] = {0};
int g_memory_access_count = 0;
ubus_call_t g_ubus_calls[MAX_UBUS_CALLS] = {0};
int g_ubus_call_count = 0;

// Thread safety
static pthread_mutex_t g_debug_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize advanced debugging
void advanced_debug_init(void) {
    memset(&g_call_stack, 0, sizeof(g_call_stack));
    memset(g_memory_accesses, 0, sizeof(g_memory_accesses));
    memset(g_ubus_calls, 0, sizeof(g_ubus_calls));
    
    g_memory_access_count = 0;
    g_ubus_call_count = 0;
    
    // Install signal handlers
    advanced_debug_install_signal_handlers();
    
    LOGX_INFO_MSG("Advanced debugging system initialized");
}

// Cleanup advanced debugging
void advanced_debug_cleanup(void) {
    advanced_debug_lock();
    
    // Save any remaining debug info
    if (g_call_stack.depth > 0) {
        LOGX_WARN_MSG("Call stack not empty at cleanup (depth: %d)", g_call_stack.depth);
    }
    
    advanced_debug_unlock();
    
    LOGX_INFO_MSG("Advanced debugging system cleaned up");
}

// Push function frame onto call stack
void advanced_debug_push_frame(const char *function, const char *file, int line) {
    advanced_debug_lock();
    
    if (g_call_stack.depth < MAX_CALL_DEPTH) {
        call_frame_t *frame = &g_call_stack.frames[g_call_stack.depth];
        
        strncpy(frame->function_name, function, sizeof(frame->function_name) - 1);
        frame->function_name[sizeof(frame->function_name) - 1] = '\0';
        
        strncpy(frame->file_name, file, sizeof(frame->file_name) - 1);
        frame->file_name[sizeof(frame->file_name) - 1] = '\0';
        
        frame->line_number = line;
#ifdef __GNUC__
        frame->return_address = __builtin_return_address(0);
        frame->frame_pointer = __builtin_frame_address(0);
#else
        frame->return_address = NULL;
        frame->frame_pointer = NULL;
#endif
        
        g_call_stack.depth++;
        if (g_call_stack.depth > g_call_stack.max_depth) {
            g_call_stack.max_depth = g_call_stack.depth;
        }
    }
    
    advanced_debug_unlock();
}

// Pop function frame from call stack
void advanced_debug_pop_frame(void) {
    advanced_debug_lock();
    
    if (g_call_stack.depth > 0) {
        g_call_stack.depth--;
    }
    
    advanced_debug_unlock();
}

// Print current call stack
void advanced_debug_print_stack(FILE *stream) {
    advanced_debug_lock();
    
    fprintf(stream, "\n=== ADVANCED CALL STACK ===\n");
    fprintf(stream, "Stack depth: %d (max: %d)\n", g_call_stack.depth, g_call_stack.max_depth);
    
    for (int i = g_call_stack.depth - 1; i >= 0; i--) {
        call_frame_t *frame = &g_call_stack.frames[i];
        fprintf(stream, "  #%d: %s() at %s:%d\n", 
                g_call_stack.depth - i, 
                frame->function_name, 
                frame->file_name, 
                frame->line_number);
        fprintf(stream, "       return_address: %p, frame_pointer: %p\n",
                frame->return_address, frame->frame_pointer);
    }
    
    fprintf(stream, "=== END CALL STACK ===\n\n");
    
    advanced_debug_unlock();
}

// Track memory access
void advanced_debug_track_memory_access(void *ptr, size_t size, const char *operation, 
                                       const char *function, const char *file, int line) {
    advanced_debug_lock();
    
    if (g_memory_access_count < MAX_MEMORY_ACCESSES) {
        memory_access_t *access = &g_memory_accesses[g_memory_access_count];
        
        access->address = ptr;
        access->size = size;
        strncpy(access->operation, operation, sizeof(access->operation) - 1);
        access->operation[sizeof(access->operation) - 1] = '\0';
        
        snprintf(access->location, sizeof(access->location), "%s:%s:%d", 
                function, file, line);
        
        access->timestamp = time(NULL);
        g_memory_access_count++;
    }
    
    advanced_debug_unlock();
}

// Track UBUS method call
void advanced_debug_track_ubus_call(const char *method_name, const char *function, 
                                   const char *file, int line) {
    advanced_debug_lock();
    
    if (g_ubus_call_count < MAX_UBUS_CALLS) {
        ubus_call_t *call = &g_ubus_calls[g_ubus_call_count];
        
        strncpy(call->method_name, method_name, sizeof(call->method_name) - 1);
        call->method_name[sizeof(call->method_name) - 1] = '\0';
        
        snprintf(call->caller_info, sizeof(call->caller_info), "%s:%s:%d", 
                function, file, line);
        
        call->timestamp = time(NULL);
#ifdef __GNUC__
        call->context = __builtin_frame_address(0);
#else
        call->context = NULL;
#endif
        g_ubus_call_count++;
    }
    
    advanced_debug_unlock();
}

// Print memory accesses
void advanced_debug_print_memory_accesses(FILE *stream) {
    advanced_debug_lock();
    
    fprintf(stream, "\n=== MEMORY ACCESS LOG ===\n");
    fprintf(stream, "Total accesses: %d\n", g_memory_access_count);
    
    // Show last 20 accesses
    int start = (g_memory_access_count > 20) ? g_memory_access_count - 20 : 0;
    for (int i = start; i < g_memory_access_count; i++) {
        memory_access_t *access = &g_memory_accesses[i];
        fprintf(stream, "  %s: %p (size: %zu) at %s\n",
                access->operation, access->address, access->size, access->location);
    }
    
    fprintf(stream, "=== END MEMORY ACCESS LOG ===\n\n");
    
    advanced_debug_unlock();
}

// Print UBUS calls
void advanced_debug_print_ubus_calls(FILE *stream) {
    advanced_debug_lock();
    
    fprintf(stream, "\n=== UBUS CALL LOG ===\n");
    fprintf(stream, "Total calls: %d\n", g_ubus_call_count);
    
    // Show last 10 calls
    int start = (g_ubus_call_count > 10) ? g_ubus_call_count - 10 : 0;
    for (int i = start; i < g_ubus_call_count; i++) {
        ubus_call_t *call = &g_ubus_calls[i];
        fprintf(stream, "  %s() called from %s (context: %p)\n",
                call->method_name, call->caller_info, call->context);
    }
    
    fprintf(stream, "=== END UBUS CALL LOG ===\n\n");
    
    advanced_debug_unlock();
}

// Save crash information
void advanced_debug_save_crash_info(const char *crash_reason) {
    FILE *crash_file = fopen("/tmp/autonomy_crash_debug.log", "w");
    if (!crash_file) {
        return;
    }
    
    fprintf(crash_file, "=== AUTONOMY DAEMON CRASH DEBUG INFO ===\n");
    fprintf(crash_file, "Crash reason: %s\n", crash_reason);
    fprintf(crash_file, "Timestamp: %ld\n", time(NULL));
    fprintf(crash_file, "PID: %d\n", getpid());
    
    advanced_debug_print_stack(crash_file);
    advanced_debug_print_memory_accesses(crash_file);
    advanced_debug_print_ubus_calls(crash_file);
    
    fclose(crash_file);
}

// Install signal handlers
void advanced_debug_install_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = advanced_crash_handler;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sa.sa_mask);
    
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
}

// Enhanced crash handler
void advanced_crash_handler(int sig, siginfo_t *info, void *context) {
    fprintf(stderr, "\n=== ADVANCED CRASH DETECTION ===\n");
    fprintf(stderr, "Signal: %d (%s)\n", sig, strsignal(sig));
    fprintf(stderr, "Signal code: %d\n", info->si_code);
    fprintf(stderr, "Fault address: %p\n", info->si_addr);
    fprintf(stderr, "PID: %d\n", getpid());
    fprintf(stderr, "UID: %d\n", getuid());
    
    // Print advanced debug information
    advanced_debug_print_stack(stderr);
    advanced_debug_print_memory_accesses(stderr);
    advanced_debug_print_ubus_calls(stderr);
    
    // Save crash info
    char crash_reason[256];
    snprintf(crash_reason, sizeof(crash_reason), "Signal %d at address %p", sig, info->si_addr);
    advanced_debug_save_crash_info(crash_reason);
    
    fprintf(stderr, "=== END ADVANCED CRASH DETECTION ===\n");
    
    // Call original crash handler
    exit(1);
}

// Check for memory corruption
void advanced_debug_check_memory_corruption(void) {
    advanced_debug_lock();
    
    // Check for suspicious memory access patterns
    for (int i = 0; i < g_memory_access_count; i++) {
        memory_access_t *access = &g_memory_accesses[i];
        
        // Check for null pointer access
        if (access->address == NULL && access->size > 0) {
            LOGX_ERROR_MSG("SUSPICIOUS: NULL pointer access detected at %s", access->location);
        }
        
        // Check for very large allocations
        if (access->size > 1024 * 1024) { // 1MB
            LOGX_WARN_MSG("LARGE ALLOCATION: %zu bytes at %s", access->size, access->location);
        }
    }
    
    advanced_debug_unlock();
}

// Validate pointer
int advanced_debug_validate_pointer(void *ptr, size_t size) {
    if (!ptr) {
        LOGX_ERROR_MSG("NULL pointer validation failed");
        return 0;
    }
    
    // Try to read the first byte
    volatile char test = *(volatile char*)ptr;
    (void)test;
    
    // Try to read the last byte if size > 0
    if (size > 0) {
        volatile char test_end = *((volatile char*)ptr + size - 1);
        (void)test_end;
    }
    
    return 1;
}

// Thread safety functions
void advanced_debug_lock(void) {
    pthread_mutex_lock(&g_debug_mutex);
}

void advanced_debug_unlock(void) {
    pthread_mutex_unlock(&g_debug_mutex);
}
