#include "debugging_utilities.h"
#include "../logging/logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>

// Global debug level
debug_level_t g_debug_level = DEBUG_LEVEL_INFO;

// Global debug stack
debug_context_t g_debug_stack[MAX_DEBUG_STACK_DEPTH];
int g_debug_stack_depth = 0;

// Mutex for thread-safe debugging
static pthread_mutex_t debug_mutex = PTHREAD_MUTEX_INITIALIZER;

// Set debug level
void debug_set_level(debug_level_t level) {
    pthread_mutex_lock(&debug_mutex);
    g_debug_level = level;
    pthread_mutex_unlock(&debug_mutex);
    LOGX_INFO_MSG("Debug level set to %d", (int)level);
}

// Get debug level
debug_level_t debug_get_level(void) {
    pthread_mutex_lock(&debug_mutex);
    debug_level_t level = g_debug_level;
    pthread_mutex_unlock(&debug_mutex);
    return level;
}

// Dump buffer contents in hex format
void debug_dump_buffer(const unsigned char *buffer, size_t size) {
    if (!buffer || size == 0) {
        LOGX_DEBUG_MSG("BUFFER: (null or empty)");
        return;
    }
    
    const size_t max_dump_size = 256; // Limit dump size to prevent log spam
    size_t dump_size = (size > max_dump_size) ? max_dump_size : size;
    
    char hex_line[80];
    char ascii_line[20];
    
    for (size_t i = 0; i < dump_size; i += 16) {
        // Build hex representation
        int hex_pos = 0;
        int ascii_pos = 0;
        
        for (size_t j = 0; j < 16 && (i + j) < dump_size; j++) {
            unsigned char byte = buffer[i + j];
            hex_pos += snprintf(hex_line + hex_pos, sizeof(hex_line) - hex_pos, "%02x ", byte);
            ascii_line[ascii_pos++] = (byte >= 32 && byte <= 126) ? byte : '.';
        }
        
        // Pad hex line if needed
        while (hex_pos < 48) {
            hex_line[hex_pos++] = ' ';
        }
        hex_line[hex_pos] = '\0';
        ascii_line[ascii_pos] = '\0';
        
        LOGX_DEBUG_MSG("BUFFER: %04zx: %s |%s|", i, hex_line, ascii_line);
    }
    
    if (size > max_dump_size) {
        LOGX_DEBUG_MSG("BUFFER: ... (%zu more bytes truncated)", size - max_dump_size);
    }
}

// Print stack trace (if available)
void debug_dump_stack_trace(void) {
#ifdef __GLIBC__
    void *array[20];
    size_t size;
    char **strings;
    
    size = backtrace(array, 20);
    strings = backtrace_symbols(array, size);
    
    if (strings != NULL) {
        LOGX_DEBUG_MSG("STACK TRACE: %zu frames", size);
        for (size_t i = 0; i < size; i++) {
            LOGX_DEBUG_MSG("STACK[%zu]: %s", i, strings[i]);
        }
        free(strings);
    } else {
        LOGX_DEBUG_MSG("STACK TRACE: Unable to get symbols");
    }
#else
    LOGX_DEBUG_MSG("STACK TRACE: Not available on this platform");
#endif
}

// Print memory information
void debug_print_memory_info(void) {
    FILE *status = fopen("/proc/self/status", "r");
    if (status) {
        char line[256];
        LOGX_DEBUG_MSG("MEMORY INFO:");
        while (fgets(line, sizeof(line), status)) {
            if (strstr(line, "VmSize") || strstr(line, "VmRSS") || 
                strstr(line, "VmPeak") || strstr(line, "VmHWM") ||
                strstr(line, "VmData") || strstr(line, "VmStk") ||
                strstr(line, "VmExe") || strstr(line, "VmLib")) {
                // Remove newline
                char *newline = strchr(line, '\n');
                if (newline) *newline = '\0';
                LOGX_DEBUG_MSG("MEMORY: %s", line);
            }
        }
        fclose(status);
    } else {
        LOGX_DEBUG_MSG("MEMORY INFO: Unable to read /proc/self/status");
    }
}

// Print thread information
void debug_print_thread_info(void) {
    LOGX_DEBUG_MSG("THREAD INFO:");
    LOGX_DEBUG_MSG("THREAD: PID=%d, TID=%ld", getpid(), (long)pthread_self());
    
    // Try to read thread info from /proc
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/task", getpid());
    
    FILE *tasks = fopen(path, "r");
    if (tasks) {
        LOGX_DEBUG_MSG("THREAD: Task directory accessible");
        fclose(tasks);
    } else {
        LOGX_DEBUG_MSG("THREAD: Unable to access task directory");
    }
}

// Save debug state to file
void debug_save_state_to_file(const char *filename) {
    if (!filename) {
        LOGX_ERROR_MSG("DEBUG: Cannot save state - filename is NULL");
        return;
    }
    
    FILE *file = fopen(filename, "w");
    if (!file) {
        LOGX_ERROR_MSG("DEBUG: Cannot open file %s for writing", filename);
        return;
    }
    
    fprintf(file, "=== DEBUG STATE DUMP ===\n");
    fprintf(file, "Timestamp: %lld\n", (long long)time(NULL));
    fprintf(file, "PID: %d\n", getpid());
    fprintf(file, "Debug Level: %d\n", (int)g_debug_level);
    fprintf(file, "Stack Depth: %d\n", g_debug_stack_depth);
    
    fprintf(file, "\n=== CALL STACK ===\n");
    for (int i = 0; i < g_debug_stack_depth; i++) {
        debug_context_t *ctx = &g_debug_stack[i];
        fprintf(file, "[%d] %s() at %s:%d\n", 
                ctx->depth, ctx->function_name, ctx->file_name, ctx->line_number);
    }
    
    fprintf(file, "\n=== END DEBUG STATE ===\n");
    fclose(file);
    
    LOGX_INFO_MSG("DEBUG: State saved to %s", filename);
}

// Load debug state from file
void debug_load_state_from_file(const char *filename) {
    if (!filename) {
        LOGX_ERROR_MSG("DEBUG: Cannot load state - filename is NULL");
        return;
    }
    
    FILE *file = fopen(filename, "r");
    if (!file) {
        LOGX_ERROR_MSG("DEBUG: Cannot open file %s for reading", filename);
        return;
    }
    
    char line[256];
    LOGX_INFO_MSG("DEBUG: Loading state from %s", filename);
    
    while (fgets(line, sizeof(line), file)) {
        // Remove newline
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        LOGX_DEBUG_MSG("DEBUG_LOAD: %s", line);
    }
    
    fclose(file);
    LOGX_INFO_MSG("DEBUG: State loaded from %s", filename);
}

// Initialize debugging utilities
int debug_utilities_init(void) {
    LOGX_INFO_MSG("DEBUG: Initializing debugging utilities");
    
    // Initialize debug stack
    memset(g_debug_stack, 0, sizeof(g_debug_stack));
    g_debug_stack_depth = 0;
    
    // Set default debug level from environment if available
    const char *debug_env = getenv("AUTONOMY_DEBUG_LEVEL");
    if (debug_env) {
        int level = atoi(debug_env);
        if (level >= DEBUG_LEVEL_NONE && level <= DEBUG_LEVEL_VERBOSE) {
            g_debug_level = (debug_level_t)level;
            LOGX_INFO_MSG("DEBUG: Level set from environment: %d", level);
        }
    }
    
    LOGX_INFO_MSG("DEBUG: Debugging utilities initialized (level=%d)", (int)g_debug_level);
    return 0;
}

// Cleanup debugging utilities
void debug_utilities_cleanup(void) {
    LOGX_INFO_MSG("DEBUG: Cleaning up debugging utilities");
    
    pthread_mutex_lock(&debug_mutex);
    
    // Clear debug stack
    memset(g_debug_stack, 0, sizeof(g_debug_stack));
    g_debug_stack_depth = 0;
    
    pthread_mutex_unlock(&debug_mutex);
    
    LOGX_INFO_MSG("DEBUG: Debugging utilities cleaned up");
}

// Emergency debug dump
void debug_emergency_dump(const char *reason) {
    LOGX_FATAL_MSG("=== EMERGENCY DEBUG DUMP ===");
    LOGX_FATAL_MSG("Reason: %s", reason ? reason : "Unknown");
    LOGX_FATAL_MSG("PID: %d", getpid());
    LOGX_FATAL_MSG("Time: %ld", time(NULL));
    
    // Dump call stack
    LOGX_FATAL_MSG("Call stack depth: %d", g_debug_stack_depth);
    for (int i = 0; i < g_debug_stack_depth; i++) {
        debug_context_t *ctx = &g_debug_stack[i];
        LOGX_FATAL_MSG("Stack[%d]: %s() at %s:%d", 
                      i, ctx->function_name, ctx->file_name, ctx->line_number);
    }
    
    // Dump memory info
    debug_print_memory_info();
    
    // Dump stack trace
    debug_dump_stack_trace();
    
    // Save to emergency file
    char emergency_file[128];
    snprintf(emergency_file, sizeof(emergency_file), "/tmp/autonomy_emergency_%d_%lld.log",
              getpid(), (long long)time(NULL));
    debug_save_state_to_file(emergency_file);
    
    LOGX_FATAL_MSG("=== END EMERGENCY DUMP ===");
}

// Force core dump for debugging
void debug_force_core_dump(void) {
    LOGX_FATAL_MSG("DEBUG: Forcing core dump for debugging");
    debug_emergency_dump("Forced core dump");
    abort();
}

// Print all global variables (placeholder - would need to be customized per application)
void debug_print_all_globals(void) {
    LOGX_DEBUG_MSG("=== GLOBAL VARIABLES DUMP ===");
    LOGX_DEBUG_MSG("g_debug_level = %d", (int)g_debug_level);
    LOGX_DEBUG_MSG("g_debug_stack_depth = %d", g_debug_stack_depth);
    // Add more global variables as needed
    LOGX_DEBUG_MSG("=== END GLOBALS DUMP ===");
}
