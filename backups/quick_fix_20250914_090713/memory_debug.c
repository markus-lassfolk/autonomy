#include "memory_debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/mman.h>
#ifdef __GLIBC__
#include <execinfo.h>
#endif
#include <inttypes.h>

// Global variables
memory_debug_stats_t g_memory_debug_stats = {0};
static memory_debug_block_t *g_memory_blocks = NULL;
static pthread_mutex_t g_memory_mutex = PTHREAD_MUTEX_INITIALIZER;
static stack_protection_t g_stack_protection = {0};
static bool g_memory_debug_initialized = false;

// Internal functions
static void memory_debug_add_block(memory_debug_block_t *block);
static void memory_debug_remove_block(memory_debug_block_t *block);
static memory_debug_block_t* memory_debug_find_block(void *ptr);
static void memory_debug_print_backtrace(void);
static void memory_debug_corruption_handler(const char *message);

void memory_debug_init(void) {
    if (g_memory_debug_initialized) {
        return;
    }
    
    memset(&g_memory_debug_stats, 0, sizeof(memory_debug_stats_t));
    g_memory_blocks = NULL;
    
    // Initialize stack protection
    memory_debug_init_stack_protection();
    
    g_memory_debug_initialized = true;
    
    fprintf(stderr, "MEMORY_DEBUG: Memory debugging system initialized\n");
    fprintf(stderr, "MEMORY_DEBUG: Stack protection: %s\n", MEMORY_DEBUG_STACK_PROTECTION ? "enabled" : "disabled");
    fprintf(stderr, "MEMORY_DEBUG: Heap protection: %s\n", MEMORY_DEBUG_HEAP_PROTECTION ? "enabled" : "disabled");
    fprintf(stderr, "MEMORY_DEBUG: Null pointer checks: %s\n", MEMORY_DEBUG_NULL_POINTER_CHECKS ? "enabled" : "disabled");
}

void memory_debug_cleanup(void) {
    if (!g_memory_debug_initialized) {
        return;
    }
    
    pthread_mutex_lock(&g_memory_mutex);
    
    // Detect and report memory leaks
    memory_debug_detect_leaks();
    
    // Print final statistics
    memory_debug_print_stats();
    
    // Clean up memory blocks (but don't free them as they might be leaked)
    g_memory_blocks = NULL;
    
    pthread_mutex_unlock(&g_memory_mutex);
    
    g_memory_debug_initialized = false;
    fprintf(stderr, "MEMORY_DEBUG: Memory debugging system cleaned up\n");
}

void memory_debug_print_stats(void) {
    fprintf(stderr, "\n=== MEMORY DEBUG STATISTICS ===\n");
    fprintf(stderr, "Total allocations: %" PRIu64 "\n", g_memory_debug_stats.total_allocations);
    fprintf(stderr, "Total deallocations: %" PRIu64 "\n", g_memory_debug_stats.total_deallocations);
    fprintf(stderr, "Current allocations: %" PRIu64 "\n", g_memory_debug_stats.current_allocations);
    fprintf(stderr, "Peak allocations: %" PRIu64 "\n", g_memory_debug_stats.peak_allocations);
    fprintf(stderr, "Total bytes allocated: %" PRIu64 "\n", g_memory_debug_stats.total_bytes_allocated);
    fprintf(stderr, "Total bytes freed: %" PRIu64 "\n", g_memory_debug_stats.total_bytes_freed);
    fprintf(stderr, "Current bytes allocated: %" PRIu64 "\n", g_memory_debug_stats.current_bytes_allocated);
    fprintf(stderr, "Peak bytes allocated: %" PRIu64 "\n", g_memory_debug_stats.peak_bytes_allocated);
    fprintf(stderr, "Memory leaks detected: %" PRIu64 "\n", g_memory_debug_stats.memory_leaks_detected);
    fprintf(stderr, "Corruption detected: %" PRIu64 "\n", g_memory_debug_stats.corruption_detected);
    fprintf(stderr, "Null pointer accesses: %" PRIu64 "\n", g_memory_debug_stats.null_pointer_accesses);
    fprintf(stderr, "Stack overflows detected: %" PRIu64 "\n", g_memory_debug_stats.stack_overflows_detected);
    fprintf(stderr, "===============================\n\n");
}

void* memory_debug_malloc(size_t size, const char *file, int line, const char *function) {
    if (!g_memory_debug_initialized) {
        memory_debug_init();
    }
    
    // Add guard bytes for corruption detection
    size_t total_size = size + (2 * sizeof(uint32_t));
    void *ptr = malloc(total_size);
    
    if (!ptr) {
        fprintf(stderr, "MEMORY_DEBUG: malloc failed for %zu bytes in %s:%d (%s)\n", 
                size, file, line, function);
        return NULL;
    }
    
    // Set up guard patterns
    uint32_t *guard_before = (uint32_t*)ptr;
    uint32_t *guard_after = (uint32_t*)((char*)ptr + sizeof(uint32_t) + size);
    *guard_before = MEMORY_GUARD_PATTERN;
    *guard_after = MEMORY_GUARD_PATTERN;
    
    // Get the actual user pointer
    void *user_ptr = (char*)ptr + sizeof(uint32_t);
    
    // Create tracking block
    memory_debug_block_t *block = (memory_debug_block_t*)malloc(sizeof(memory_debug_block_t));
    if (!block) {
        free(ptr);
        return NULL;
    }
    
    block->ptr = user_ptr;
    block->size = size;
    block->file = file;
    block->line = line;
    block->function = function;
    block->guard_before = MEMORY_GUARD_PATTERN;
    block->guard_after = MEMORY_GUARD_PATTERN;
    block->allocation_id = g_memory_debug_stats.total_allocations + 1;
    block->is_freed = false;
    
    pthread_mutex_lock(&g_memory_mutex);
    
    // Update statistics
    g_memory_debug_stats.total_allocations++;
    g_memory_debug_stats.current_allocations++;
    g_memory_debug_stats.total_bytes_allocated += size;
    g_memory_debug_stats.current_bytes_allocated += size;
    
    if (g_memory_debug_stats.current_allocations > g_memory_debug_stats.peak_allocations) {
        g_memory_debug_stats.peak_allocations = g_memory_debug_stats.current_allocations;
    }
    
    if (g_memory_debug_stats.current_bytes_allocated > g_memory_debug_stats.peak_bytes_allocated) {
        g_memory_debug_stats.peak_bytes_allocated = g_memory_debug_stats.current_bytes_allocated;
    }
    
    // Add to tracking list
    memory_debug_add_block(block);
    
    pthread_mutex_unlock(&g_memory_mutex);
    
    if (MEMORY_DEBUG_VERBOSE) {
        fprintf(stderr, "MEMORY_DEBUG: malloc(%zu) = %p in %s:%d (%s) [ID: %" PRIu64 "]\n", 
                size, user_ptr, file, line, function, block->allocation_id);
    }
    
    return user_ptr;
}

void* memory_debug_calloc(size_t num, size_t size, const char *file, int line, const char *function) {
    size_t total_size = num * size;
    void *ptr = memory_debug_malloc(total_size, file, line, function);
    
    if (ptr) {
        memset(ptr, 0, total_size);
    }
    
    return ptr;
}

void* memory_debug_realloc(void *ptr, size_t size, const char *file, int line, const char *function) {
    if (!ptr) {
        return memory_debug_malloc(size, file, line, function);
    }
    
    if (size == 0) {
        memory_debug_free(ptr, file, line, function);
        return NULL;
    }
    
    // Find the original block
    pthread_mutex_lock(&g_memory_mutex);
    memory_debug_block_t *block = memory_debug_find_block(ptr);
    pthread_mutex_unlock(&g_memory_mutex);
    
    if (!block) {
        fprintf(stderr, "MEMORY_DEBUG: realloc called on untracked pointer %p in %s:%d (%s)\n", 
                ptr, file, line, function);
        return NULL;
    }
    
    // Validate the block before reallocating
    if (!memory_debug_validate_memory_block(block)) {
        memory_debug_corruption_handler("Memory corruption detected before realloc");
        return NULL;
    }
    
    // Calculate new total size with guards
    size_t new_total_size = size + (2 * sizeof(uint32_t));
    void *new_ptr = realloc((char*)ptr - sizeof(uint32_t), new_total_size);
    
    if (!new_ptr) {
        return NULL;
    }
    
    // Set up new guard patterns
    uint32_t *guard_before = (uint32_t*)new_ptr;
    uint32_t *guard_after = (uint32_t*)((char*)new_ptr + sizeof(uint32_t) + size);
    *guard_before = MEMORY_GUARD_PATTERN;
    *guard_after = MEMORY_GUARD_PATTERN;
    
    void *new_user_ptr = (char*)new_ptr + sizeof(uint32_t);
    
    // Update the block
    pthread_mutex_lock(&g_memory_mutex);
    
    block->ptr = new_user_ptr;
    block->size = size;
    block->file = file;
    block->line = line;
    block->function = function;
    
    // Update statistics
    g_memory_debug_stats.total_bytes_freed += block->size;
    g_memory_debug_stats.current_bytes_allocated -= block->size;
    g_memory_debug_stats.total_bytes_allocated += size;
    g_memory_debug_stats.current_bytes_allocated += size;
    
    if (g_memory_debug_stats.current_bytes_allocated > g_memory_debug_stats.peak_bytes_allocated) {
        g_memory_debug_stats.peak_bytes_allocated = g_memory_debug_stats.current_bytes_allocated;
    }
    
    pthread_mutex_unlock(&g_memory_mutex);
    
    if (MEMORY_DEBUG_VERBOSE) {
        fprintf(stderr, "MEMORY_DEBUG: realloc(%p, %zu) = %p in %s:%d (%s) [ID: %" PRIu64 "]\n", 
                ptr, size, new_user_ptr, file, line, function, block->allocation_id);
    }
    
    return new_user_ptr;
}

void memory_debug_free(void *ptr, const char *file, int line, const char *function) {
    if (!ptr) {
        return;
    }
    
    pthread_mutex_lock(&g_memory_mutex);
    
    memory_debug_block_t *block = memory_debug_find_block(ptr);
    if (!block) {
        pthread_mutex_unlock(&g_memory_mutex);
        fprintf(stderr, "MEMORY_DEBUG: free called on untracked pointer %p in %s:%d (%s)\n", 
                ptr, file, line, function);
        return;
    }
    
    if (block->is_freed) {
        pthread_mutex_unlock(&g_memory_mutex);
        fprintf(stderr, "MEMORY_DEBUG: double free detected for %p in %s:%d (%s) [ID: %" PRIu64 "]\n", 
                ptr, file, line, function, block->allocation_id);
        memory_debug_print_backtrace();
        return;
    }
    
    // Validate the block before freeing
    if (!memory_debug_validate_memory_block(block)) {
        memory_debug_corruption_handler("Memory corruption detected before free");
        pthread_mutex_unlock(&g_memory_mutex);
        return;
    }
    
    // Mark as freed
    block->is_freed = true;
    
    // Update statistics
    g_memory_debug_stats.total_deallocations++;
    g_memory_debug_stats.current_allocations--;
    g_memory_debug_stats.total_bytes_freed += block->size;
    g_memory_debug_stats.current_bytes_allocated -= block->size;
    
    // Remove from tracking list
    memory_debug_remove_block(block);
    
    pthread_mutex_unlock(&g_memory_mutex);
    
    if (MEMORY_DEBUG_VERBOSE) {
        fprintf(stderr, "MEMORY_DEBUG: free(%p) in %s:%d (%s) [ID: %" PRIu64 "]\n", 
                ptr, file, line, function, block->allocation_id);
    }
    
    // Free the actual memory (including guard bytes)
    free((char*)ptr - sizeof(uint32_t));
    
    // Free the tracking block
    free(block);
}

bool memory_debug_validate_pointer(void *ptr, const char *file, int line, const char *function) {
    if (!ptr) {
        g_memory_debug_stats.null_pointer_accesses++;
        fprintf(stderr, "MEMORY_DEBUG: null pointer access in %s:%d (%s)\n", 
                file, line, function);
        return false;
    }
    
    pthread_mutex_lock(&g_memory_mutex);
    memory_debug_block_t *block = memory_debug_find_block(ptr);
    pthread_mutex_unlock(&g_memory_mutex);
    
    if (!block) {
        fprintf(stderr, "MEMORY_DEBUG: access to untracked pointer %p in %s:%d (%s)\n", 
                ptr, file, line, function);
        return false;
    }
    
    if (block->is_freed) {
        fprintf(stderr, "MEMORY_DEBUG: access to freed pointer %p in %s:%d (%s) [ID: %" PRIu64 "]\n", 
                ptr, file, line, function, block->allocation_id);
        memory_debug_print_backtrace();
        return false;
    }
    
    return true;
}

bool memory_debug_validate_memory_block(memory_debug_block_t *block) {
    if (!block) {
        return false;
    }
    
    // Check guard patterns
    uint32_t *guard_before = (uint32_t*)((char*)block->ptr - sizeof(uint32_t));
    uint32_t *guard_after = (uint32_t*)((char*)block->ptr + block->size);
    
    if (*guard_before != MEMORY_GUARD_PATTERN) {
        fprintf(stderr, "MEMORY_DEBUG: guard before corruption detected for block %p [ID: %" PRIu64 "]\n", 
                block->ptr, block->allocation_id);
        g_memory_debug_stats.corruption_detected++;
        return false;
    }
    
    if (*guard_after != MEMORY_GUARD_PATTERN) {
        fprintf(stderr, "MEMORY_DEBUG: guard after corruption detected for block %p [ID: %" PRIu64 "]\n", 
                block->ptr, block->allocation_id);
        g_memory_debug_stats.corruption_detected++;
        return false;
    }
    
    return true;
}

void memory_debug_check_all_allocations(void) {
    pthread_mutex_lock(&g_memory_mutex);
    
    memory_debug_block_t *block = g_memory_blocks;
    while (block) {
        if (!memory_debug_validate_memory_block(block)) {
            fprintf(stderr, "MEMORY_DEBUG: corruption in block %p allocated in %s:%d (%s) [ID: %" PRIu64 "]\n", 
                    block->ptr, block->file, block->line, block->function, block->allocation_id);
        }
        block = block->next;
    }
    
    pthread_mutex_unlock(&g_memory_mutex);
}

void memory_debug_detect_leaks(void) {
    pthread_mutex_lock(&g_memory_mutex);
    
    memory_debug_block_t *block = g_memory_blocks;
    while (block) {
        if (!block->is_freed) {
            g_memory_debug_stats.memory_leaks_detected++;
            fprintf(stderr, "MEMORY_DEBUG: memory leak detected: %p (%zu bytes) allocated in %s:%d (%s) [ID: %" PRIu64 "]\n", 
                    block->ptr, block->size, block->file, block->line, block->function, block->allocation_id);
        }
        block = block->next;
    }
    
    pthread_mutex_unlock(&g_memory_mutex);
}

void memory_debug_init_stack_protection(void) {
    if (!MEMORY_DEBUG_STACK_PROTECTION) {
        return;
    }
    
    // Get stack information
    FILE *maps = fopen("/proc/self/maps", "r");
    if (maps) {
        char line[256];
        while (fgets(line, sizeof(line), maps)) {
            if (strstr(line, "[stack]")) {
                unsigned long start, end;
                if (sscanf(line, "%lx-%lx", &start, &end) == 2) {
                    g_stack_protection.stack_base = (void*)start;
                    g_stack_protection.stack_top = (void*)end;
                    g_stack_protection.stack_size = end - start;
                    break;
                }
            }
        }
        fclose(maps);
    }
    
    // Set up stack guards
    g_stack_protection.guard_before = STACK_GUARD_PATTERN;
    g_stack_protection.guard_after = STACK_GUARD_PATTERN;
    
    fprintf(stderr, "MEMORY_DEBUG: Stack protection initialized (base: %p, top: %p, size: %zu)\n", 
            g_stack_protection.stack_base, g_stack_protection.stack_top, g_stack_protection.stack_size);
}

void memory_debug_check_stack_integrity(const char *file, int line, const char *function) {
    if (!MEMORY_DEBUG_STACK_PROTECTION) {
        return;
    }
    
    void *current_sp;
    asm volatile ("mov %0, sp" : "=r" (current_sp));
    
    if (current_sp < g_stack_protection.stack_base || current_sp > g_stack_protection.stack_top) {
        g_memory_debug_stats.stack_overflows_detected++;
        fprintf(stderr, "MEMORY_DEBUG: stack overflow detected in %s:%d (%s) - SP: %p, Stack: %p-%p\n", 
                file, line, function, current_sp, g_stack_protection.stack_base, g_stack_protection.stack_top);
        memory_debug_print_backtrace();
    }
}

bool memory_debug_detect_stack_overflow(void) {
    if (!MEMORY_DEBUG_STACK_PROTECTION) {
        return false;
    }
    
    void *current_sp;
    asm volatile ("mov %0, sp" : "=r" (current_sp));
    
    return (current_sp < g_stack_protection.stack_base || current_sp > g_stack_protection.stack_top);
}

bool memory_debug_validate_pointer_access(void *ptr, size_t size, const char *file, int line, const char *function) {
    if (!MEMORY_DEBUG_NULL_POINTER_CHECKS) {
        return true;
    }
    
    if (!memory_debug_validate_pointer(ptr, file, line, function)) {
        return false;
    }
    
    pthread_mutex_lock(&g_memory_mutex);
    memory_debug_block_t *block = memory_debug_find_block(ptr);
    pthread_mutex_unlock(&g_memory_mutex);
    
    if (block) {
        if ((char*)ptr + size > (char*)block->ptr + block->size) {
            fprintf(stderr, "MEMORY_DEBUG: buffer overflow detected: accessing %zu bytes at %p, but block is only %zu bytes in %s:%d (%s) [ID: %" PRIu64 "]\n", 
                    size, ptr, block->size, file, line, function, block->allocation_id);
            memory_debug_print_backtrace();
            return false;
        }
    }
    
    return true;
}

bool memory_debug_detect_corruption(void *ptr, size_t size) {
    if (!ptr) {
        return false;
    }
    
    // Check for common corruption patterns
    uint8_t *bytes = (uint8_t*)ptr;
    for (size_t i = 0; i < size; i++) {
        if (bytes[i] == 0xDE || bytes[i] == 0xAD || bytes[i] == 0xBE || bytes[i] == 0xEF) {
            return true;
        }
    }
    
    return false;
}

void memory_debug_scan_memory_for_corruption(void) {
    pthread_mutex_lock(&g_memory_mutex);
    
    memory_debug_block_t *block = g_memory_blocks;
    while (block) {
        if (memory_debug_detect_corruption(block->ptr, block->size)) {
            fprintf(stderr, "MEMORY_DEBUG: corruption detected in block %p [ID: %" PRIu64 "]\n", 
                    block->ptr, block->allocation_id);
            g_memory_debug_stats.corruption_detected++;
        }
        block = block->next;
    }
    
    pthread_mutex_unlock(&g_memory_mutex);
}

void memory_debug_print_memory_map(void) {
    fprintf(stderr, "MEMORY_DEBUG: Memory map:\n");
    FILE *maps = fopen("/proc/self/maps", "r");
    if (maps) {
        char line[256];
        while (fgets(line, sizeof(line), maps)) {
            fprintf(stderr, "MEMORY_DEBUG: %s", line);
        }
        fclose(maps);
    }
}

void memory_debug_print_allocation_trace(void *ptr) {
    pthread_mutex_lock(&g_memory_mutex);
    
    memory_debug_block_t *block = memory_debug_find_block(ptr);
    if (block) {
        fprintf(stderr, "MEMORY_DEBUG: Allocation trace for %p [ID: %" PRIu64 "]:\n", ptr, block->allocation_id);
        fprintf(stderr, "  Allocated in: %s:%d (%s)\n", block->file, block->line, block->function);
        fprintf(stderr, "  Size: %zu bytes\n", block->size);
        fprintf(stderr, "  Freed: %s\n", block->is_freed ? "yes" : "no");
    } else {
        fprintf(stderr, "MEMORY_DEBUG: No allocation trace found for %p\n", ptr);
    }
    
    pthread_mutex_unlock(&g_memory_mutex);
}

void memory_debug_force_garbage_collection(void) {
    pthread_mutex_lock(&g_memory_mutex);
    
    memory_debug_block_t *block = g_memory_blocks;
    while (block) {
        if (block->is_freed) {
            memory_debug_block_t *next = block->next;
            memory_debug_remove_block(block);
            free(block);
            block = next;
        } else {
            block = block->next;
        }
    }
    
    pthread_mutex_unlock(&g_memory_mutex);
}

// Internal helper functions
static void memory_debug_add_block(memory_debug_block_t *block) {
    block->next = g_memory_blocks;
    block->prev = NULL;
    
    if (g_memory_blocks) {
        g_memory_blocks->prev = block;
    }
    
    g_memory_blocks = block;
}

static void memory_debug_remove_block(memory_debug_block_t *block) {
    if (block->prev) {
        block->prev->next = block->next;
    } else {
        g_memory_blocks = block->next;
    }
    
    if (block->next) {
        block->next->prev = block->prev;
    }
}

static memory_debug_block_t* memory_debug_find_block(void *ptr) {
    memory_debug_block_t *block = g_memory_blocks;
    while (block) {
        if (block->ptr == ptr) {
            return block;
        }
        block = block->next;
    }
    return NULL;
}

static void memory_debug_print_backtrace(void) {
#ifdef __GLIBC__
    void *array[10];
    size_t size = backtrace(array, 10);
    char **strings = backtrace_symbols(array, size);
    
    fprintf(stderr, "MEMORY_DEBUG: Backtrace:\n");
    for (size_t i = 0; i < size; i++) {
        fprintf(stderr, "  %zu: %s\n", i, strings[i]);
    }
    
    free(strings);
#else
    fprintf(stderr, "MEMORY_DEBUG: Backtrace not available (not using GNU libc)\n");
#endif
}

static void memory_debug_corruption_handler(const char *message) {
    fprintf(stderr, "MEMORY_DEBUG: %s\n", message);
    memory_debug_print_backtrace();
    memory_debug_print_stats();
}
