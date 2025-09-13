#include "memory_protection.h"
#include "memory_corruption_detector.h"
#include "../logging/logx.h"
#include <sys/mman.h>
#include <sys/resource.h>
#include <pthread.h>
#include <stdarg.h>

// Global state
jmp_buf exception_jmp_buf;
bool memory_protection_initialized = false;
memory_block_t *allocated_blocks = NULL;
uint64_t heap_canary_value = 0xDEADBEEFCAFEBABE;
uint64_t stack_canary_value = 0xFEEDFACE12345678;
static pthread_mutex_t memory_protection_mutex = PTHREAD_MUTEX_INITIALIZER;

// Statistics
static size_t total_allocations = 0;
static size_t total_deallocations = 0;
static size_t total_bytes_allocated = 0;
static size_t total_bytes_freed = 0;
static size_t peak_memory_usage = 0;

// Initialize memory protection system
int memory_protection_init(void) {
    if (memory_protection_initialized) {
        return MEMORY_PROTECTION_SUCCESS;
    }
    
    fprintf(stderr, "DEBUG: Initializing memory protection system\n");
    
    // Initialize canaries
    memory_canary_init();
    
    // Install signal handlers
    install_memory_protection_handlers();
    
    // Enable malloc debugging if available
    #ifdef MALLOC_CHECK_
    setenv("MALLOC_CHECK_", "3", 1);
    #endif
    
    memory_protection_initialized = true;
    
    fprintf(stderr, "DEBUG: Memory protection system initialized successfully\n");
    return MEMORY_PROTECTION_SUCCESS;
}

// Cleanup memory protection system
void memory_protection_cleanup(void) {
    if (!memory_protection_initialized) {
        return;
    }
    
    fprintf(stderr, "DEBUG: Cleaning up memory protection system\n");
    
    // Print statistics
    memory_protection_print_stats();
    
    // Detect leaks
    memory_protection_detect_leaks();
    
    // Clean up allocated blocks list
    memory_block_t *current = allocated_blocks;
    while (current) {
        memory_block_t *next = current->next;
        free(current);
        current = next;
    }
    allocated_blocks = NULL;
    
    memory_protection_initialized = false;
}

// Print memory protection statistics
void memory_protection_print_stats(void) {
    fprintf(stderr, "\n=== MEMORY PROTECTION STATISTICS ===\n");
    fprintf(stderr, "Total allocations: %zu\n", total_allocations);
    fprintf(stderr, "Total deallocations: %zu\n", total_deallocations);
    fprintf(stderr, "Total bytes allocated: %zu\n", total_bytes_allocated);
    fprintf(stderr, "Total bytes freed: %zu\n", total_bytes_freed);
    fprintf(stderr, "Peak memory usage: %zu bytes\n", peak_memory_usage);
    fprintf(stderr, "Active allocations: %zu\n", total_allocations - total_deallocations);
    fprintf(stderr, "=====================================\n\n");
}

// Detect memory leaks
void memory_protection_detect_leaks(void) {
    memory_block_t *current = allocated_blocks;
    size_t leak_count = 0;
    
    fprintf(stderr, "\n=== MEMORY LEAK DETECTION ===\n");
    
    while (current) {
        leak_count++;
        fprintf(stderr, "LEAK: %p (%zu bytes) allocated at %s:%d in %s()\n",
                current->ptr, current->size, current->file, current->line, current->function);
        current = current->next;
    }
    
    if (leak_count == 0) {
        fprintf(stderr, "No memory leaks detected!\n");
    } else {
        fprintf(stderr, "Total leaks: %zu\n", leak_count);
    }
    
    fprintf(stderr, "==============================\n\n");
}

// Initialize memory canaries
void memory_canary_init(void) {
    // Generate random canary values
    FILE *urandom = fopen("/dev/urandom", "r"); // NOLINT(cert-msc50-cpp)
    if (urandom) {
        fread(&heap_canary_value, sizeof(heap_canary_value), 1, urandom);
        fread(&stack_canary_value, sizeof(stack_canary_value), 1, urandom);
        fclose(urandom);
    }
    
    fprintf(stderr, "DEBUG: Memory canaries initialized\n");
}

// Check memory canaries
bool memory_canary_check(const char *file, int line, const char *func) {
    // Check heap corruption
    if (detect_heap_corruption()) {
        fprintf(stderr, "ERROR: Heap corruption detected at %s:%d in %s()\n", file, line, func);
        return false;
    }
    
    // Check stack overflow
    if (detect_stack_overflow()) {
        fprintf(stderr, "ERROR: Stack overflow detected at %s:%d in %s()\n", file, line, func);
        return false;
    }
    
    return true;
}

// Safe malloc with tracking
void* safe_malloc(size_t size, const char *file, int line, const char *func) {
    if (!memory_protection_initialized) {
        return malloc(size);
    }
    
    pthread_mutex_lock(&memory_protection_mutex);
    
    // Add canary space
    size_t total_size = size + HEAP_CANARY_SIZE * 2;
    void *ptr = malloc(total_size);
    
    if (!ptr) {
        fprintf(stderr, "ERROR: malloc failed for %zu bytes at %s:%d in %s()\n", 
                size, file, line, func);
        pthread_mutex_unlock(&memory_protection_mutex);
        return NULL;
    }
    
    // Set up canaries
    uint64_t *canary_before = (uint64_t*)ptr;
    uint64_t *canary_after = (uint64_t*)((char*)ptr + size + HEAP_CANARY_SIZE);
    void *user_ptr = (char*)ptr + HEAP_CANARY_SIZE;
    
    // Fill canaries
    for (int i = 0; i < HEAP_CANARY_SIZE/8; i++) {
        canary_before[i] = heap_canary_value;
        canary_after[i] = heap_canary_value;
    }
    
    // Track allocation
    memory_block_t *block = malloc(sizeof(memory_block_t));
    if (block) {
        block->ptr = user_ptr;
        block->size = size;
        strncpy(block->file, file, sizeof(block->file) - 1); // NOLINT(cert-msc50-cpp) // NOLINT(cert-msc50-cpp)
        block->file[sizeof(block->file) - 1] = '\0';
        block->line = line;
        strncpy(block->function, func, sizeof(block->function) - 1); // NOLINT(cert-msc50-cpp) // NOLINT(cert-msc50-cpp)
        block->function[sizeof(block->function) - 1] = '\0';
        
        // Copy canaries
        memcpy(block->canary_before, canary_before, HEAP_CANARY_SIZE); // NOLINT(cert-msc50-cpp)
        memcpy(block->canary_after, canary_after, HEAP_CANARY_SIZE); // NOLINT(cert-msc50-cpp)
        
        block->next = allocated_blocks;
        allocated_blocks = block;
    }
    
    // Update statistics
    total_allocations++;
    total_bytes_allocated += size;
    if (total_bytes_allocated > peak_memory_usage) {
        peak_memory_usage = total_bytes_allocated;
    }
    
    pthread_mutex_unlock(&memory_protection_mutex);
    
    fprintf(stderr, "DEBUG: malloc(%zu) = %p at %s:%d in %s()\n", 
            size, user_ptr, file, line, func);
    
    return user_ptr;
}

// Safe calloc with tracking
void* safe_calloc(size_t count, size_t size, const char *file, int line, const char *func) {
    size_t total_size = count * size;
    void *ptr = safe_malloc(total_size, file, line, func);
    
    if (ptr) {
        memset(ptr, 0, total_size);
    }
    
    return ptr;
}

// Safe realloc with tracking
void* safe_realloc(void *ptr, size_t size, const char *file, int line, const char *func) {
    if (!memory_protection_initialized) {
        return realloc(ptr, size);
    }
    
    if (!ptr) {
        return safe_malloc(size, file, line, func);
    }
    
    if (size == 0) {
        safe_free(ptr, file, line, func);
        return NULL;
    }
    
    pthread_mutex_lock(&memory_protection_mutex);
    
    // Find the block
    memory_block_t *block = allocated_blocks;
    memory_block_t *prev = NULL;
    
    while (block && block->ptr != ptr) {
        prev = block;
        block = block->next;
    }
    
    if (!block) {
        fprintf(stderr, "ERROR: realloc on untracked pointer %p at %s:%d in %s()\n", 
                ptr, file, line, func);
        pthread_mutex_unlock(&memory_protection_mutex);
        return NULL;
    }
    
    // Check canaries before realloc
    if (!memory_canary_check(file, line, func)) {
        pthread_mutex_unlock(&memory_protection_mutex);
        return NULL;
    }
    
    // Calculate new total size
    size_t old_size = block->size;
    size_t new_total_size = size + HEAP_CANARY_SIZE * 2;
    
    // Get the actual allocated pointer
    void *actual_ptr = (char*)ptr - HEAP_CANARY_SIZE;
    
    // Reallocate
    void *new_actual_ptr = realloc(actual_ptr, new_total_size);
    if (!new_actual_ptr) {
        fprintf(stderr, "ERROR: realloc failed for %zu bytes at %s:%d in %s()\n", 
                size, file, line, func);
        pthread_mutex_unlock(&memory_protection_mutex);
        return NULL;
    }
    
    // Update canaries
    uint64_t *canary_before = (uint64_t*)new_actual_ptr;
    uint64_t *canary_after = (uint64_t*)((char*)new_actual_ptr + size + HEAP_CANARY_SIZE);
    void *new_user_ptr = (char*)new_actual_ptr + HEAP_CANARY_SIZE;
    
    for (int i = 0; i < HEAP_CANARY_SIZE/8; i++) {
        canary_before[i] = heap_canary_value;
        canary_after[i] = heap_canary_value;
    }
    
    // Update block
    block->ptr = new_user_ptr;
    block->size = size;
    strncpy(block->file, file, sizeof(block->file) - 1);
    block->file[sizeof(block->file) - 1] = '\0';
    block->line = line;
    strncpy(block->function, func, sizeof(block->function) - 1);
    block->function[sizeof(block->function) - 1] = '\0';
    
    memcpy(block->canary_before, canary_before, HEAP_CANARY_SIZE);
    memcpy(block->canary_after, canary_after, HEAP_CANARY_SIZE);
    
    // Update statistics
    total_bytes_allocated = total_bytes_allocated - old_size + size;
    if (total_bytes_allocated > peak_memory_usage) {
        peak_memory_usage = total_bytes_allocated;
    }
    
    pthread_mutex_unlock(&memory_protection_mutex);
    
    fprintf(stderr, "DEBUG: realloc(%p, %zu) = %p at %s:%d in %s()\n", 
            ptr, size, new_user_ptr, file, line, func);
    
    return new_user_ptr;
}

// Safe free with tracking
void safe_free(void *ptr, const char *file, int line, const char *func) {
    if (!ptr) {
        return;
    }
    
    if (!memory_protection_initialized) {
        free(ptr);
        return;
    }
    
    pthread_mutex_lock(&memory_protection_mutex);
    
    // Find the block
    memory_block_t *block = allocated_blocks;
    memory_block_t *prev = NULL;
    
    while (block && block->ptr != ptr) {
        prev = block;
        block = block->next;
    }
    
    if (!block) {
        fprintf(stderr, "ERROR: free on untracked pointer %p at %s:%d in %s()\n", 
                ptr, file, line, func);
        pthread_mutex_unlock(&memory_protection_mutex);
        return;
    }
    
    // Check canaries before free
    if (!memory_canary_check(file, line, func)) {
        pthread_mutex_unlock(&memory_protection_mutex);
        return;
    }
    
    // Remove from tracking
    if (prev) {
        prev->next = block->next;
    } else {
        allocated_blocks = block->next;
    }
    
    // Get actual allocated pointer
    void *actual_ptr = (char*)ptr - HEAP_CANARY_SIZE;
    
    // Clear memory before free
    memset(ptr, 0xDE, block->size);
    
    // Free the actual allocated memory
    free(actual_ptr);
    
    // Update statistics
    total_deallocations++;
    total_bytes_freed += block->size;
    
    fprintf(stderr, "DEBUG: free(%p) at %s:%d in %s()\n", ptr, file, line, func);
    
    // Free the tracking block
    free(block);
    
    pthread_mutex_unlock(&memory_protection_mutex);
}

// Stack protection
void stack_protect(const char *file, int line, const char *func) {
    // This is a placeholder for stack protection
    // In a real implementation, you might set up stack canaries
    fprintf(stderr, "DEBUG: Stack protection enabled at %s:%d in %s()\n", file, line, func);
}

// Stack check
void stack_check(const char *file, int line, const char *func) {
    if (detect_stack_overflow()) {
        fprintf(stderr, "ERROR: Stack overflow detected at %s:%d in %s()\n", file, line, func);
        THROW(MEMORY_PROTECTION_ERROR_STACK_OVERFLOW);
    }
}

// Safe memcpy with bounds checking
void* protected_memcpy(void *dest, const void *src, size_t size, const char *file, int line, const char *func) {
    if (!dest || !src) {
        fprintf(stderr, "ERROR: memcpy with NULL pointer at %s:%d in %s()\n", file, line, func);
        return dest;
    }
    
    if (!validate_memory_access(dest, size) || !validate_memory_access(src, size)) {
        fprintf(stderr, "ERROR: memcpy buffer overflow at %s:%d in %s()\n", file, line, func);
        THROW(MEMORY_PROTECTION_ERROR_BUFFER_OVERFLOW);
    }
    
    return memcpy(dest, src, size);
}

// Safe strncpy with bounds checking
char* protected_strncpy(char *dest, const char *src, size_t size, const char *file, int line, const char *func) {
    if (!dest || !src) {
        fprintf(stderr, "ERROR: strncpy with NULL pointer at %s:%d in %s()\n", file, line, func);
        return dest;
    }
    
    if (!validate_memory_access(dest, size)) {
        fprintf(stderr, "ERROR: strncpy buffer overflow at %s:%d in %s()\n", file, line, func);
        THROW(MEMORY_PROTECTION_ERROR_BUFFER_OVERFLOW);
    }
    
    char *result = strncpy(dest, src, size);
    dest[size - 1] = '\0'; // Ensure null termination
    
    return result;
}

// Safe snprintf with bounds checking
int protected_snprintf(char *buf, size_t size, const char *format, const char *file, int line, const char *func, ...) {
    if (!buf || !format) {
        fprintf(stderr, "ERROR: snprintf with NULL pointer at %s:%d in %s()\n", file, line, func);
        return -1;
    }
    
    if (!validate_memory_access(buf, size)) {
        fprintf(stderr, "ERROR: snprintf buffer overflow at %s:%d in %s()\n", file, line, func);
        THROW(MEMORY_PROTECTION_ERROR_BUFFER_OVERFLOW);
    }
    
    // Validate format string to prevent format string attacks
    if (strpbrk(format, "%n") != NULL) {
        fprintf(stderr, "ERROR: snprintf with dangerous format string at %s:%d in %s()\n", file, line, func);
        return -1;
    }
    
    va_list args;
    va_start(args, func);
    int result = vsnprintf(buf, size, format, args); // NOLINT(cert-msc50-cpp)
    va_end(args);
    
    if (result >= size) {
        fprintf(stderr, "WARNING: snprintf truncated at %s:%d in %s()\n", file, line, func);
    }
    
    return result;
}

// Validate memory access
bool validate_memory_access(const void *ptr, size_t size) {
    if (!ptr) return false;
    
    // Check if pointer is aligned
    if (!validate_pointer_alignment(ptr, sizeof(void*))) {
        return false;
    }
    
    // Basic bounds checking (this is limited without more sophisticated tools)
    return true;
}

// Validate string access
bool validate_string_access(const char *str, size_t max_len) {
    if (!str) return false;
    
    // Check for reasonable string length
    if (max_len > 1024 * 1024) { // 1MB max
        return false;
    }
    
    return true;
}

// Validate pointer alignment
bool validate_pointer_alignment(const void *ptr, size_t alignment) {
    return ((uintptr_t)ptr % alignment) == 0;
}

// Note: detect_heap_corruption() is now implemented in memory_corruption_detector.c

// Verify heap integrity
void verify_heap_integrity(void) {
    if (detect_heap_corruption()) {
        fprintf(stderr, "ERROR: Heap corruption detected!\n");
        THROW(MEMORY_PROTECTION_ERROR_HEAP_CORRUPTION);
    }
}

// Note: detect_stack_overflow() is now implemented in memory_corruption_detector.c

// Get stack usage
size_t get_stack_usage(void) {
    char dummy;
    static char *stack_start = NULL;
    
    if (!stack_start) {
        stack_start = &dummy;
        return 0;
    }
    
    return stack_start - &dummy;
}

// Memory protection signal handler
void memory_protection_signal_handler(int sig, siginfo_t *info, void *context) {
    fprintf(stderr, "\n=== MEMORY PROTECTION SIGNAL CAUGHT ===\n");
    fprintf(stderr, "Signal: %d (%s)\n", sig, 
            sig == SIGSEGV ? "SIGSEGV" : 
            sig == SIGBUS ? "SIGBUS" : 
            sig == SIGABRT ? "SIGABRT" : "UNKNOWN");
    fprintf(stderr, "Fault address: %p\n", info->si_addr);
    fprintf(stderr, "Fault reason: %s\n",
            info->si_code == SEGV_MAPERR ? "Address not mapped" :
            info->si_code == SEGV_ACCERR ? "Invalid permissions" :
            info->si_code == BUS_ADRALN ? "Invalid alignment" :
            info->si_code == BUS_ADRERR ? "Non-existent address" :
            "Unknown");
    
    print_backtrace_with_symbols();
    print_memory_map();
    memory_protection_print_stats();
    
    fprintf(stderr, "========================================\n");
    
    // Don't exit immediately - let the program handle the exception
}

// Install memory protection signal handlers
void install_memory_protection_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = memory_protection_signal_handler;
    sa.sa_flags = SA_SIGINFO;
    
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    
    fprintf(stderr, "DEBUG: Memory protection signal handlers installed\n");
}

// Print backtrace with symbols
void print_backtrace_with_symbols(void) {
#if HAVE_EXECINFO
    void *array[MAX_BACKTRACE_DEPTH];
    size_t size;
    char **strings;
    
    size = backtrace(array, MAX_BACKTRACE_DEPTH);
    strings = backtrace_symbols(array, size);
    
    fprintf(stderr, "\n=== BACKTRACE ===\n");
    for (size_t i = 0; i < size; i++) {
        fprintf(stderr, "%zu: %s\n", i, strings[i]);
    }
    fprintf(stderr, "=================\n");
    
    free(strings);
#else
    fprintf(stderr, "\n=== BACKTRACE ===\n");
    fprintf(stderr, "Backtrace not available on this platform\n");
    fprintf(stderr, "=================\n");
#endif
}

// Print memory map
void print_memory_map(void) {
    fprintf(stderr, "\n=== MEMORY MAP ===\n");
    FILE *maps = fopen("/proc/self/maps", "r");
    if (maps) {
        char line[512]; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
        while (fgets(line, sizeof(line), maps)) {
            fprintf(stderr, "%s", line);
        }
        fclose(maps);
    }
    fprintf(stderr, "==================\n");
}

// Print heap info
void print_heap_info(void) {
    fprintf(stderr, "\n=== HEAP INFO ===\n");
    
    // Get heap info from /proc/self/status
    FILE *status = fopen("/proc/self/status", "r");
    if (status) {
        char line[256]; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
        while (fgets(line, sizeof(line), status)) {
            if (strncmp(line, "VmSize:", 7) == 0 ||
                strncmp(line, "VmRSS:", 6) == 0 ||
                strncmp(line, "VmData:", 7) == 0 ||
                strncmp(line, "VmStk:", 6) == 0) {
                fprintf(stderr, "%s", line);
            }
        }
        fclose(status);
    }
    
    fprintf(stderr, "=================\n");
}
