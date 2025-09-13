#include "memory_corruption_detector.h"
#include "../logging/logx.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <signal.h>
#ifdef __GLIBC__
#include <execinfo.h>
#endif

// Maximum number of monitored globals
#define MAX_MONITORED_GLOBALS 32

// Static data
static monitored_global_t g_monitored_globals[MAX_MONITORED_GLOBALS];
static int g_monitored_count = 0;
static memory_corruption_stats_t g_corruption_stats = {0};
static pthread_mutex_t g_corruption_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_detector_initialized = false;

// Stack overflow detection
static void *g_stack_base = NULL;
static size_t g_stack_size = 0;

// Initialize memory corruption detector
int memory_corruption_detector_init(void) {
    if (g_detector_initialized) {
        return 0;
    }
    
    LOGX_DEBUG_MSG("Initializing memory corruption detector");
    
    // Initialize monitored globals array
    memset(g_monitored_globals, 0, sizeof(g_monitored_globals));
    g_monitored_count = 0;
    
    // Initialize statistics
    memset(&g_corruption_stats, 0, sizeof(g_corruption_stats));
    
    // Get stack information for overflow detection
    FILE *maps = fopen("/proc/self/maps", "r");
    if (maps) {
        char line[256];
        while (fgets(line, sizeof(line), maps)) {
            if (strstr(line, "[stack]")) {
                unsigned long start, end;
                if (sscanf(line, "%lx-%lx", &start, &end) == 2) {
                    g_stack_base = (void*)start;
                    g_stack_size = end - start;
                    LOGX_DEBUG_MSG("Stack detected: base=%p, size=%zu", g_stack_base, g_stack_size);
                }
                break;
            }
        }
        fclose(maps);
    }
    
    // If stack detection failed, disable stack overflow detection
    if (!g_stack_base) {
        LOGX_DEBUG_MSG("Stack detection failed, disabling stack overflow detection");
    }
    
    g_detector_initialized = true;
    LOGX_DEBUG_MSG("Memory corruption detector initialized successfully");
    return 0;
}

// Cleanup memory corruption detector
void memory_corruption_detector_cleanup(void) {
    if (!g_detector_initialized) {
        return;
    }
    
    LOGX_DEBUG_MSG("Cleaning up memory corruption detector");
    
    pthread_mutex_lock(&g_corruption_mutex);
    
    // Clear all monitored globals
    for (int i = 0; i < g_monitored_count; i++) {
        g_monitored_globals[i].is_monitored = false;
    }
    g_monitored_count = 0;
    
    pthread_mutex_unlock(&g_corruption_mutex);
    
    g_detector_initialized = false;
    LOGX_DEBUG_MSG("Memory corruption detector cleaned up");
}

// Monitor a global variable
int monitor_global_variable(void *address, size_t size, const char *name, uint32_t magic) {
    if (!g_detector_initialized) {
        LOGX_ERROR_MSG("Memory corruption detector not initialized");
        return -1;
    }
    
    if (g_monitored_count >= MAX_MONITORED_GLOBALS) {
        LOGX_ERROR_MSG("Maximum number of monitored globals reached");
        return -1;
    }
    
    pthread_mutex_lock(&g_corruption_mutex);
    
    monitored_global_t *monitor = &g_monitored_globals[g_monitored_count];
    monitor->address = address;
    monitor->size = size;
    monitor->magic_number = magic;
    monitor->is_monitored = true;
    strncpy(monitor->name, name, sizeof(monitor->name) - 1);
    monitor->name[sizeof(monitor->name) - 1] = '\0';
    
    // Set up canaries
    monitor->canary_front = MEMORY_CANARY_VALUE;
    monitor->canary_back = MEMORY_CANARY_VALUE;
    
    g_monitored_count++;
    
    pthread_mutex_unlock(&g_corruption_mutex);
    
    LOGX_DEBUG_MSG("Monitoring global variable %s at %p (size %zu, magic 0x%x)", 
            name, address, size, magic);
    
    return 0;
}

// Unmonitor a global variable
void unmonitor_global_variable(void *address) {
    pthread_mutex_lock(&g_corruption_mutex);
    
    for (int i = 0; i < g_monitored_count; i++) {
        if (g_monitored_globals[i].address == address) {
            g_monitored_globals[i].is_monitored = false;
            LOGX_DEBUG_MSG("Unmonitoring global variable %s at %p", 
                    g_monitored_globals[i].name, address);
            break;
        }
    }
    
    pthread_mutex_unlock(&g_corruption_mutex);
}

// Check integrity of a specific global variable
bool check_global_variable_integrity(void *address) {
    pthread_mutex_lock(&g_corruption_mutex);
    
    for (int i = 0; i < g_monitored_count; i++) {
        monitored_global_t *monitor = &g_monitored_globals[i];
        if (monitor->address == address && monitor->is_monitored) {
            // Check canaries
            if (monitor->canary_front != MEMORY_CANARY_VALUE || 
                monitor->canary_back != MEMORY_CANARY_VALUE) {
                g_corruption_stats.canary_violations++;
                g_corruption_stats.corruption_detected++;
                pthread_mutex_unlock(&g_corruption_mutex);
                LOGX_ERROR_MSG("Canary violation detected for global %s at %p", 
                        monitor->name, address);
                return false;
            }
            
            // Check magic number if it's a pointer to a struct
            if (monitor->magic_number != 0) {
                uint32_t *magic_ptr = (uint32_t*)address;
                if (*magic_ptr != monitor->magic_number) {
                    g_corruption_stats.magic_violations++;
                    g_corruption_stats.corruption_detected++;
                    pthread_mutex_unlock(&g_corruption_mutex);
                    LOGX_ERROR_MSG("Magic number violation for global %s at %p (expected 0x%x, got 0x%x)", 
                            monitor->name, address, monitor->magic_number, *magic_ptr);
                    return false;
                }
            }
            
            g_corruption_stats.successful_checks++;
            pthread_mutex_unlock(&g_corruption_mutex);
            return true;
        }
    }
    
    pthread_mutex_unlock(&g_corruption_mutex);
    return true; // Not monitored, assume OK
}

// Check all monitored globals
void check_all_monitored_globals(void) {
    pthread_mutex_lock(&g_corruption_mutex);
    
    for (int i = 0; i < g_monitored_count; i++) {
        monitored_global_t *monitor = &g_monitored_globals[i];
        if (monitor->is_monitored) {
            if (!check_global_variable_integrity(monitor->address)) {
                g_corruption_stats.global_corruptions++;
            }
        }
    }
    
    pthread_mutex_unlock(&g_corruption_mutex);
}

// Validate memory region
bool validate_memory_region(void *ptr, size_t size) {
    if (!ptr) {
        return false;
    }
    
    // Check if pointer is in valid memory range
    if ((uintptr_t)ptr < 0x1000) {
        return false;
    }
    
    // Try to read the memory region
    volatile char *test_ptr = (volatile char*)ptr;
    for (size_t i = 0; i < size; i += 4096) { // Check in 4KB chunks
        volatile char test = test_ptr[i];
        (void)test; // Suppress unused variable warning
    }
    
    return true;
}

// Detect stack overflow
bool detect_stack_overflow(void) {
    if (!g_stack_base) {
        return false; // Can't detect without stack info
    }
    
    // Get current stack pointer using a safer method
    volatile char stack_var;
    void *current_sp = (void*)&stack_var;
    
    // Check if stack pointer is getting close to stack base
    // Use a very conservative threshold (1MB instead of 64KB)
    if ((uintptr_t)current_sp < (uintptr_t)g_stack_base + 1048576) { // 1MB safety margin
        g_corruption_stats.stack_overflows++;
        g_corruption_stats.corruption_detected++;
        return true;
    }
    
    return false;
}

// Detect heap corruption
bool detect_heap_corruption(void) {
    // This is a simplified check - in a real implementation,
    // you'd want to use more sophisticated heap corruption detection
    return false;
}

// Get corruption statistics
memory_corruption_stats_t* get_memory_corruption_stats(void) {
    return &g_corruption_stats;
}

// Print corruption report
void print_memory_corruption_report(void) {
    pthread_mutex_lock(&g_corruption_mutex);
    
    LOGX_INFO_MSG("=== MEMORY CORRUPTION REPORT ===");
    LOGX_INFO_MSG("Total corruption events: %u", g_corruption_stats.corruption_detected);
    LOGX_INFO_MSG("Stack overflows: %u", g_corruption_stats.stack_overflows);
    LOGX_INFO_MSG("Heap corruptions: %u", g_corruption_stats.heap_corruptions);
    LOGX_INFO_MSG("Global corruptions: %u", g_corruption_stats.global_corruptions);
    LOGX_INFO_MSG("Canary violations: %u", g_corruption_stats.canary_violations);
    LOGX_INFO_MSG("Magic violations: %u", g_corruption_stats.magic_violations);
    LOGX_INFO_MSG("Total checks: %u", g_corruption_stats.total_checks);
    LOGX_INFO_MSG("Successful checks: %u", g_corruption_stats.successful_checks);
    LOGX_INFO_MSG("Monitored globals: %d", g_monitored_count);
    
    pthread_mutex_unlock(&g_corruption_mutex);
}

// Reset corruption statistics
void reset_memory_corruption_stats(void) {
    pthread_mutex_lock(&g_corruption_mutex);
    memset(&g_corruption_stats, 0, sizeof(g_corruption_stats));
    pthread_mutex_unlock(&g_corruption_mutex);
}
