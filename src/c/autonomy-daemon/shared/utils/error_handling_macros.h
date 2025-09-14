#ifndef ERROR_HANDLING_MACROS_H
#define ERROR_HANDLING_MACROS_H

#include "../logging/logx.h"
#include <errno.h>
#include <string.h>

// Enhanced error handling macros for better debugging and troubleshooting

// Parameter validation macros
#define VALIDATE_PARAM(param, error_code) \
    do { \
        if (!(param)) { \
            LOGX_ERROR_MSG("Parameter validation failed: %s is NULL at %s:%d in %s()", \
                          #param, __FILE__, __LINE__, __func__); \
            return error_code; \
        } \
    } while(0)

#define VALIDATE_PARAM_NOT_NULL(param) \
    VALIDATE_PARAM(param, -1)

#define VALIDATE_PARAM_RANGE(param, min, max, error_code) \
    do { \
        if ((param) < (min) || (param) > (max)) { \
            LOGX_ERROR_MSG("Parameter validation failed: %s (%d) out of range [%d, %d] at %s:%d in %s()", \
                          #param, (int)(param), (int)(min), (int)(max), __FILE__, __LINE__, __func__); \
            return error_code; \
        } \
    } while(0)

#define VALIDATE_BUFFER_SIZE(buffer, size, min_size, error_code) \
    do { \
        if (!(buffer) || (size) < (min_size)) { \
            LOGX_ERROR_MSG("Buffer validation failed: buffer=%p, size=%zu, min_size=%zu at %s:%d in %s()", \
                          (void*)(buffer), (size_t)(size), (size_t)(min_size), __FILE__, __LINE__, __func__); \
            return error_code; \
        } \
    } while(0)

// Function call error handling macros
#define CHECK_RESULT(call, expected, error_code) \
    do { \
        int _result = (call); \
        if (_result != (expected)) { \
            LOGX_ERROR_MSG("Function call failed: %s returned %d, expected %d at %s:%d in %s()", \
                          #call, _result, (int)(expected), __FILE__, __LINE__, __func__); \
            return error_code; \
        } \
    } while(0)

#define CHECK_RESULT_SUCCESS(call) \
    CHECK_RESULT(call, 0, -1)

#define CHECK_RESULT_NOT_NULL(call, error_code) \
    do { \
        void *_result = (call); \
        if (!_result) { \
            LOGX_ERROR_MSG("Function call failed: %s returned NULL at %s:%d in %s()", \
                          #call, __FILE__, __LINE__, __func__); \
            return error_code; \
        } \
    } while(0)

// System call error handling macros
#define CHECK_SYSCALL(call, error_code) \
    do { \
        if ((call) < 0) { \
            LOGX_ERROR_MSG("System call failed: %s returned %d, errno=%d (%s) at %s:%d in %s()", \
                          #call, (int)(call), errno, strerror(errno), __FILE__, __LINE__, __func__); \
            return error_code; \
        } \
    } while(0)

#define CHECK_SYSCALL_SUCCESS(call) \
    CHECK_SYSCALL(call, -1)

// Memory allocation error handling macros
#define CHECK_MALLOC(ptr, size, error_code) \
    do { \
        if (!(ptr)) { \
            LOGX_ERROR_MSG("Memory allocation failed: malloc(%zu) returned NULL at %s:%d in %s()", \
                          (size_t)(size), __FILE__, __LINE__, __func__); \
            return error_code; \
        } \
    } while(0)

#define SAFE_MALLOC_CHECK(ptr, size) \
    CHECK_MALLOC(ptr, size, NULL)

// File operation error handling macros
#define CHECK_FILE_OPEN(file, filename, mode, error_code) \
    do { \
        if (!(file)) { \
            LOGX_ERROR_MSG("File open failed: fopen(\"%s\", \"%s\") returned NULL, errno=%d (%s) at %s:%d in %s()", \
                          filename, mode, errno, strerror(errno), __FILE__, __LINE__, __func__); \
            return error_code; \
        } \
    } while(0)

// Threading error handling macros
#define CHECK_PTHREAD(call, error_code) \
    do { \
        int _result = (call); \
        if (_result != 0) { \
            LOGX_ERROR_MSG("Pthread call failed: %s returned %d (%s) at %s:%d in %s()", \
                          #call, _result, strerror(_result), __FILE__, __LINE__, __func__); \
            return error_code; \
        } \
    } while(0)

#define CHECK_PTHREAD_SUCCESS(call) \
    CHECK_PTHREAD(call, -1)

// Resource cleanup macros
#define SAFE_FREE(ptr) \
    do { \
        if (ptr) { \
            free(ptr); \
            ptr = NULL; \
        } \
    } while(0)

#define SAFE_CLOSE(fd) \
    do { \
        if ((fd) >= 0) { \
            close(fd); \
            fd = -1; \
        } \
    } while(0)

#define SAFE_FCLOSE(file) \
    do { \
        if (file) { \
            fclose(file); \
            file = NULL; \
        } \
    } while(0)

// Debugging and tracing macros
#define FUNCTION_ENTRY() \
    LOGX_DEBUG_MSG(">>> ENTERING %s", __func__)

#define FUNCTION_EXIT() \
    LOGX_DEBUG_MSG("<<< EXITING %s", __func__)

#define FUNCTION_EXIT_WITH_RETURN(ret) \
    do { \
        LOGX_DEBUG_MSG("<<< EXITING %s with return: %d", __func__, (int)(ret)); \
        return ret; \
    } while(0)

#define TRACE_VARIABLE(var, format) \
    LOGX_DEBUG_MSG("TRACE: %s = " format " at %s:%d in %s()", \
                   #var, var, __FILE__, __LINE__, __func__)

#define TRACE_POINTER(ptr) \
    LOGX_DEBUG_MSG("TRACE: %s = %p at %s:%d in %s()", \
                   #ptr, (void*)(ptr), __FILE__, __LINE__, __func__)

// Performance monitoring macros
#define PERFORMANCE_START(name) \
    struct timespec _perf_start_##name; \
    clock_gettime(CLOCK_MONOTONIC, &_perf_start_##name); \
    LOGX_DEBUG_MSG("PERFORMANCE: Starting %s", #name)

#define PERFORMANCE_END(name) \
    do { \
        struct timespec _perf_end_##name; \
        clock_gettime(CLOCK_MONOTONIC, &_perf_end_##name); \
        long _perf_diff_ns = (_perf_end_##name.tv_sec - _perf_start_##name.tv_sec) * 1000000000L + \
                            (_perf_end_##name.tv_nsec - _perf_start_##name.tv_nsec); \
        LOGX_DEBUG_MSG("PERFORMANCE: %s took %ld.%06ld ms", #name, \
                       _perf_diff_ns / 1000000L, (_perf_diff_ns % 1000000L) / 1000L); \
    } while(0)

// Assertion macros for debugging
#ifdef DEBUG
#define ASSERT(condition) \
    do { \
        if (!(condition)) { \
            LOGX_FATAL_MSG("ASSERTION FAILED: %s at %s:%d in %s()", \
                          #condition, __FILE__, __LINE__, __func__); \
            abort(); \
        } \
    } while(0)
#else
#define ASSERT(condition) ((void)0)
#endif

// Warning macros for potential issues
#define WARN_IF(condition, message) \
    do { \
        if (condition) { \
            LOGX_WARN_MSG("WARNING: %s - %s at %s:%d in %s()", \
                         #condition, message, __FILE__, __LINE__, __func__); \
        } \
    } while(0)

#define WARN_DEPRECATED(function_name, replacement) \
    LOGX_WARN_MSG("DEPRECATED: %s is deprecated, use %s instead. Called at %s:%d in %s()", \
                  function_name, replacement, __FILE__, __LINE__, __func__)

// Error recovery macros
#define RETRY_ON_ERROR(call, max_retries, delay_ms) \
    do { \
        int _retry_count = 0; \
        int _retry_result; \
        while ((_retry_result = (call)) != 0 && _retry_count < (max_retries)) { \
            _retry_count++; \
            LOGX_WARN_MSG("Retry %d/%d for %s failed with result %d", \
                         _retry_count, (int)(max_retries), #call, _retry_result); \
            if (_retry_count < (max_retries)) { \
                usleep((delay_ms) * 1000); \
            } \
        } \
        if (_retry_result != 0) { \
            LOGX_ERROR_MSG("All retries failed for %s, final result: %d", #call, _retry_result); \
            return _retry_result; \
        } \
    } while(0)

#endif // ERROR_HANDLING_MACROS_H
