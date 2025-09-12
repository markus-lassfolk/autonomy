#ifndef SHARED_ERROR_UTILS_H
#define SHARED_ERROR_UTILS_H

#include "../core/common_types.h"
#include <stdbool.h>

// Shared error handling utilities
// Consolidates common error handling patterns

// Common error logging patterns
#define LOG_INIT_ERROR(module_name) \
    LOGX_ERROR_MSG("Failed to initialize %s", module_name)

#define LOG_CLEANUP_ERROR(module_name) \
    LOGX_ERROR_MSG("Failed to cleanup %s", module_name)

#define LOG_ALREADY_INITIALIZED(module_name) \
    LOGX_WARN_MSG("%s already initialized", module_name)

#define LOG_NOT_INITIALIZED(module_name) \
    LOGX_ERROR_MSG("%s not initialized", module_name)

#define LOG_INVALID_PARAM(function_name) \
    LOGX_ERROR_MSG("Invalid parameters for %s", function_name)

#define LOG_MEMORY_ERROR(operation) \
    LOGX_ERROR_MSG("Memory allocation failed for %s", operation)

#define LOG_MUTEX_ERROR(operation) \
    LOGX_ERROR_MSG("Mutex operation failed: %s", operation)

#define LOG_NETWORK_ERROR(operation, details) \
    LOGX_ERROR_MSG("Network error in %s: %s", operation, details)

// Common initialization check pattern
#define CHECK_INITIALIZED(initialized_flag, module_name) \
    do { \
        if (initialized_flag) { \
            LOG_ALREADY_INITIALIZED(module_name); \
            return AUTONOMY_SUCCESS; \
        } \
    } while(0)

#define CHECK_NOT_INITIALIZED(initialized_flag, module_name) \
    do { \
        if (!initialized_flag) { \
            LOG_NOT_INITIALIZED(module_name); \
            return AUTONOMY_ERROR_NOT_INITIALIZED; \
        } \
    } while(0)

// Common parameter validation
#define VALIDATE_PARAM(param, function_name) \
    do { \
        if (!param) { \
            LOG_INVALID_PARAM(function_name); \
            return AUTONOMY_ERROR_INVALID_PARAM; \
        } \
    } while(0)

#define VALIDATE_PARAMS(param1, param2, function_name) \
    do { \
        if (!param1 || !param2) { \
            LOG_INVALID_PARAM(function_name); \
            return AUTONOMY_ERROR_INVALID_PARAM; \
        } \
    } while(0)

// Common memory allocation pattern
#define SAFE_MALLOC(ptr, size, error_msg) \
    do { \
        ptr = malloc(size); \
        if (!ptr) { \
            LOG_MEMORY_ERROR(error_msg); \
            return AUTONOMY_ERROR_NO_MEMORY; \
        } \
        memset(ptr, 0, size); \
    } while(0)

// Common mutex operations
#define SAFE_MUTEX_INIT(mutex, error_msg) \
    do { \
        if (pthread_mutex_init(mutex, NULL) != 0) { \
            LOG_MUTEX_ERROR(error_msg); \
            return AUTONOMY_ERROR_SYSTEM; \
        } \
    } while(0)

#define SAFE_MUTEX_LOCK(mutex) \
    do { \
        if (pthread_mutex_lock(mutex) != 0) { \
            LOGX_ERROR_MSG("Failed to lock mutex"); \
            return AUTONOMY_ERROR_SYSTEM; \
        } \
    } while(0)

#define SAFE_MUTEX_UNLOCK(mutex) \
    do { \
        if (pthread_mutex_unlock(mutex) != 0) { \
            LOGX_ERROR_MSG("Failed to unlock mutex"); \
        } \
    } while(0)

// Include logging macros
#include "../logging/logx.h"

#endif // SHARED_ERROR_UTILS_H