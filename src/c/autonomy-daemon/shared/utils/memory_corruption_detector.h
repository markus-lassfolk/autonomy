#ifndef MEMORY_CORRUPTION_DETECTOR_H
#define MEMORY_CORRUPTION_DETECTOR_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>

// Memory corruption detection system
#define MEMORY_CANARY_VALUE 0xDEADBEEF
#define MEMORY_CANARY_SIZE 4

// Global variable monitoring
typedef struct {
    void *address;
    size_t size;
    uint32_t canary_front;
    uint32_t canary_back;
    uint32_t magic_number;
    bool is_monitored;
    char name[64];
} monitored_global_t;

// Memory corruption statistics
typedef struct {
    uint32_t corruption_detected;
    uint32_t stack_overflows;
    uint32_t heap_corruptions;
    uint32_t global_corruptions;
    uint32_t canary_violations;
    uint32_t magic_violations;
    uint32_t total_checks;
    uint32_t successful_checks;
} memory_corruption_stats_t;

// Function declarations
int memory_corruption_detector_init(void);
void memory_corruption_detector_cleanup(void);

// Global variable monitoring
int monitor_global_variable(void *address, size_t size, const char *name, uint32_t magic);
void unmonitor_global_variable(void *address);
bool check_global_variable_integrity(void *address);
void check_all_monitored_globals(void);

// Memory protection
bool validate_memory_region(void *ptr, size_t size);
bool detect_stack_overflow(void);
bool detect_heap_corruption(void);

// Statistics and reporting
memory_corruption_stats_t* get_memory_corruption_stats(void);
void print_memory_corruption_report(void);
void reset_memory_corruption_stats(void);

// Defensive programming macros
#define DEFENSIVE_POINTER_CHECK(ptr, name) \
    do { \
        if (!validate_memory_region((void*)(ptr), sizeof(*(ptr)))) { \
            LOGX_ERROR_MSG("DEFENSIVE: Invalid pointer %s at %p", name, ptr); \
            return -1; \
        } \
    } while(0)

#define DEFENSIVE_ARRAY_CHECK(ptr, size, name) \
    do { \
        if (!validate_memory_region((void*)(ptr), (size))) { \
            LOGX_ERROR_MSG("DEFENSIVE: Invalid array %s at %p, size %zu", name, ptr, size); \
            return -1; \
        } \
    } while(0)

#define DEFENSIVE_GLOBAL_CHECK(ptr, name) \
    do { \
        if (!check_global_variable_integrity(ptr)) { \
            LOGX_ERROR_MSG("DEFENSIVE: Global variable %s corrupted at %p", name, ptr); \
            return -1; \
        } \
    } while(0)

// Stack overflow detection with rate limiting
#define STACK_OVERFLOW_CHECK() \
    do { \
        static time_t last_stack_error = 0; \
        time_t now = time(NULL); \
        if (detect_stack_overflow()) { \
            if (now - last_stack_error > 5) { /* Rate limit to once per 5 seconds */ \
                LOGX_ERROR_MSG("DEFENSIVE: Stack overflow detected!"); \
                last_stack_error = now; \
            } \
            return -1; \
        } \
    } while(0)

#endif // MEMORY_CORRUPTION_DETECTOR_H
