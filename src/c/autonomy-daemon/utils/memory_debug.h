#ifndef MEMORY_DEBUG_H
#define MEMORY_DEBUG_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

// Memory debugging configuration
#define MEMORY_DEBUG_ENABLED 1
#define MEMORY_DEBUG_VERBOSE 1
#define MEMORY_DEBUG_STACK_PROTECTION 1
#define MEMORY_DEBUG_HEAP_PROTECTION 1
#define MEMORY_DEBUG_NULL_POINTER_CHECKS 1

// Memory protection patterns
#define MEMORY_GUARD_PATTERN 0xDEADBEEF
#define MEMORY_FREED_PATTERN 0xFEEDDEAD
#define STACK_GUARD_PATTERN 0x57ACCEEE
#define HEAP_GUARD_PATTERN 0xEEAFCEED

// Memory debugging statistics
typedef struct {
    uint64_t total_allocations;
    uint64_t total_deallocations;
    uint64_t current_allocations;
    uint64_t peak_allocations;
    uint64_t total_bytes_allocated;
    uint64_t total_bytes_freed;
    uint64_t current_bytes_allocated;
    uint64_t peak_bytes_allocated;
    uint64_t memory_leaks_detected;
    uint64_t corruption_detected;
    uint64_t null_pointer_accesses;
    uint64_t stack_overflows_detected;
} memory_debug_stats_t;

// Memory allocation tracking
typedef struct memory_block {
    void *ptr;
    size_t size;
    const char *file;
    int line;
    const char *function;
    uint32_t guard_before;
    uint32_t guard_after;
    struct memory_block *next;
    struct memory_block *prev;
    uint64_t allocation_id;
    bool is_freed;
} memory_block_t;

// Stack protection
typedef struct {
    uint32_t guard_before;
    uint32_t guard_after;
    size_t stack_size;
    void *stack_base;
    void *stack_top;
} stack_protection_t;

// Function declarations
void memory_debug_init(void);
void memory_debug_cleanup(void);
void memory_debug_print_stats(void);

// Memory allocation tracking
void* memory_debug_malloc(size_t size, const char *file, int line, const char *function);
void* memory_debug_calloc(size_t num, size_t size, const char *file, int line, const char *function);
void* memory_debug_realloc(void *ptr, size_t size, const char *file, int line, const char *function);
void memory_debug_free(void *ptr, const char *file, int line, const char *function);

// Memory validation
bool memory_debug_validate_pointer(void *ptr, const char *file, int line, const char *function);
bool memory_debug_validate_memory_block(memory_block_t *block);
void memory_debug_check_all_allocations(void);
void memory_debug_detect_leaks(void);

// Stack protection
void memory_debug_init_stack_protection(void);
void memory_debug_check_stack_integrity(const char *file, int line, const char *function);
bool memory_debug_detect_stack_overflow(void);

// Null pointer protection
bool memory_debug_validate_pointer_access(void *ptr, size_t size, const char *file, int line, const char *function);

// Memory corruption detection
bool memory_debug_detect_corruption(void *ptr, size_t size);
void memory_debug_scan_memory_for_corruption(void);

// Utility functions
void memory_debug_print_memory_map(void);
void memory_debug_print_allocation_trace(void *ptr);
void memory_debug_force_garbage_collection(void);

// Macros for automatic tracking
#if MEMORY_DEBUG_ENABLED
#define MEMORY_DEBUG_MALLOC(size) memory_debug_malloc(size, __FILE__, __LINE__, __func__)
#define MEMORY_DEBUG_CALLOC(num, size) memory_debug_calloc(num, size, __FILE__, __LINE__, __func__)
#define MEMORY_DEBUG_REALLOC(ptr, size) memory_debug_realloc(ptr, size, __FILE__, __LINE__, __func__)
#define MEMORY_DEBUG_FREE(ptr) memory_debug_free(ptr, __FILE__, __LINE__, __func__)
#define MEMORY_DEBUG_VALIDATE_PTR(ptr) memory_debug_validate_pointer(ptr, __FILE__, __LINE__, __func__)
#define MEMORY_DEBUG_CHECK_STACK() memory_debug_check_stack_integrity(__FILE__, __LINE__, __func__)
#define MEMORY_DEBUG_VALIDATE_ACCESS(ptr, size) memory_debug_validate_pointer_access(ptr, size, __FILE__, __LINE__, __func__)
#else
#define MEMORY_DEBUG_MALLOC(size) malloc(size)
#define MEMORY_DEBUG_CALLOC(num, size) calloc(num, size)
#define MEMORY_DEBUG_REALLOC(ptr, size) realloc(ptr, size)
#define MEMORY_DEBUG_FREE(ptr) free(ptr)
#define MEMORY_DEBUG_VALIDATE_PTR(ptr) true
#define MEMORY_DEBUG_CHECK_STACK() 
#define MEMORY_DEBUG_VALIDATE_ACCESS(ptr, size) true
#endif

// Global statistics
extern memory_debug_stats_t g_memory_debug_stats;

#endif // MEMORY_DEBUG_H
