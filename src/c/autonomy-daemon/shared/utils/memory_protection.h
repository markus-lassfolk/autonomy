#ifndef MEMORY_PROTECTION_H
#define MEMORY_PROTECTION_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>

// execinfo.h is not available on all embedded targets
#ifdef __GLIBC__
#include <execinfo.h>
#define HAVE_EXECINFO 1
#else
#define HAVE_EXECINFO 0
#endif

// Memory protection configuration
#define MEMORY_PROTECTION_ENABLED 1
#define HEAP_CANARY_SIZE 16
#define STACK_CANARY_SIZE 16
#if HAVE_EXECINFO
#define MAX_BACKTRACE_DEPTH 20
#else
#define MAX_BACKTRACE_DEPTH 0
#endif

// Memory protection macros
#if MEMORY_PROTECTION_ENABLED

// Heap protection
#define SAFE_MALLOC(size) safe_malloc(size, __FILE__, __LINE__, __func__)
#define SAFE_CALLOC(count, size) safe_calloc(count, size, __FILE__, __LINE__, __func__)
#define SAFE_REALLOC(ptr, size) safe_realloc(ptr, size, __FILE__, __LINE__, __func__)
#define SAFE_FREE(ptr) safe_free(ptr, __FILE__, __LINE__, __func__)

// Stack protection
#define STACK_PROTECT() stack_protect(__FILE__, __LINE__, __func__)
#define STACK_CHECK() stack_check(__FILE__, __LINE__, __func__)

// Buffer overflow protection
#define SAFE_MEMCPY(dest, src, size) protected_memcpy(dest, src, size, __FILE__, __LINE__, __func__) // NOLINT(cert-msc50-cpp)
#define SAFE_STRNCPY(dest, src, size) protected_strncpy(dest, src, size, __FILE__, __LINE__, __func__) // NOLINT(cert-msc50-cpp)
#define SAFE_SNPRINTF(buf, size, format, ...) protected_snprintf(buf, size, format, __FILE__, __LINE__, __func__, __VA_ARGS__) // NOLINT(cert-msc50-cpp)

// Memory corruption detection
#define MEMORY_CANARY_INIT() memory_canary_init()
#define MEMORY_CANARY_CHECK() memory_canary_check(__FILE__, __LINE__, __func__)

// Exception handling
#define TRY() if (setjmp(exception_jmp_buf) == 0)
#define CATCH() else
#define THROW(code) longjmp(exception_jmp_buf, code)

#else

// Disabled - use standard functions
#define SAFE_MALLOC(size) malloc(size)
#define SAFE_CALLOC(count, size) calloc(count, size)
#define SAFE_REALLOC(ptr, size) realloc(ptr, size)
#define SAFE_FREE(ptr) free(ptr)
#define STACK_PROTECT()
#define STACK_CHECK()
#define SAFE_MEMCPY(dest, src, size) memcpy(dest, src, size)
#define SAFE_STRNCPY(dest, src, size) strncpy(dest, src, size)
// flawfinder: ignore - macro definition, not actual function call
#define SAFE_SNPRINTF(buf, size, format, ...) snprintf(buf, size, format, __VA_ARGS__)
#define MEMORY_CANARY_INIT()
#define MEMORY_CANARY_CHECK()
#define TRY() if (1)
#define CATCH() else
#define THROW(code)

#endif

// Exception codes
typedef enum {
    MEMORY_PROTECTION_SUCCESS = 0,
    MEMORY_PROTECTION_ERROR_MALLOC_FAILED = -1,
    MEMORY_PROTECTION_ERROR_FREE_INVALID = -2,
    MEMORY_PROTECTION_ERROR_BUFFER_OVERFLOW = -3,
    MEMORY_PROTECTION_ERROR_STACK_OVERFLOW = -4,
    MEMORY_PROTECTION_ERROR_HEAP_CORRUPTION = -5,
    MEMORY_PROTECTION_ERROR_DOUBLE_FREE = -6,
    MEMORY_PROTECTION_ERROR_USE_AFTER_FREE = -7,
    MEMORY_PROTECTION_ERROR_SEGFAULT = -8
} memory_protection_error_t;

// Forward declaration
typedef struct memory_block memory_block_t;

// Memory allocation tracking structure
struct memory_block {
    void *ptr;
    size_t size;
    char file[256]; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
    int line;
    char function[128]; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
    uint64_t canary_before[HEAP_CANARY_SIZE/8];
    uint64_t canary_after[HEAP_CANARY_SIZE/8];
    memory_block_t *next;
};

// Global memory protection state
extern jmp_buf exception_jmp_buf;
extern bool memory_protection_initialized;
extern memory_block_t *allocated_blocks;
extern uint64_t heap_canary_value;
extern uint64_t stack_canary_value;

// Memory protection functions
int memory_protection_init(void);
void memory_protection_cleanup(void);
void memory_protection_print_stats(void);
void memory_protection_detect_leaks(void);

// Safe memory allocation functions
void* safe_malloc(size_t size, const char *file, int line, const char *func);
void* safe_calloc(size_t count, size_t size, const char *file, int line, const char *func);
void* safe_realloc(void *ptr, size_t size, const char *file, int line, const char *func);
void safe_free(void *ptr, const char *file, int line, const char *func);

// Stack protection functions
void stack_protect(const char *file, int line, const char *func);
void stack_check(const char *file, int line, const char *func);

// Buffer overflow protection functions
void* protected_memcpy(void *dest, const void *src, size_t size, const char *file, int line, const char *func);
char* protected_strncpy(char *dest, const char *src, size_t size, const char *file, int line, const char *func);
int protected_snprintf(char *buf, size_t size, const char *format, const char *file, int line, const char *func, ...);

// Memory corruption detection functions
void memory_canary_init(void);
bool memory_canary_check(const char *file, int line, const char *func);

// Signal handlers for memory protection
void memory_protection_signal_handler(int sig, siginfo_t *info, void *context);
void install_memory_protection_handlers(void);

// Backtrace and debugging functions
void print_backtrace_with_symbols(void);
void print_memory_map(void);
void print_heap_info(void);

// Memory validation functions
bool validate_memory_access(const void *ptr, size_t size);
bool validate_string_access(const char *str, size_t max_len);
bool validate_pointer_alignment(const void *ptr, size_t alignment);

// Heap corruption detection
bool detect_heap_corruption(void);
void verify_heap_integrity(void);

// Stack overflow detection
bool detect_stack_overflow(void);
size_t get_stack_usage(void);

#endif // MEMORY_PROTECTION_H
