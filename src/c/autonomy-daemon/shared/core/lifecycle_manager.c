#include "lifecycle_manager.h"
#include "../logging/logx.h"
#include "../utils/string_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

// Maximum number of modules
#define MAX_MODULES 64

// Global lifecycle manager state
static struct {
    bool initialized;
    lifecycle_manager_config_t config;
    module_lifecycle_t* modules[MAX_MODULES];
    int module_count;
    pthread_mutex_t mutex;
    lifecycle_manager_stats_t stats;
} g_lifecycle_manager = {0};

// Helper function to get current time in milliseconds
static double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)(tv.tv_sec * 1000) + (double)(tv.tv_usec / 1000);
}

// Helper function to find module by name
static module_lifecycle_t* find_module(const char* module_name) {
    if (!module_name) return NULL;
    
    for (int i = 0; i < g_lifecycle_manager.module_count; i++) {
        if (g_lifecycle_manager.modules[i] && 
            strcmp(g_lifecycle_manager.modules[i]->module_name, module_name) == 0) {
            return g_lifecycle_manager.modules[i];
        }
    }
    return NULL;
}

// Initialize lifecycle manager
int lifecycle_manager_init(const lifecycle_manager_config_t* config) {
    if (g_lifecycle_manager.initialized) {
        LOGX_WARN_MSG("Lifecycle manager already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&g_lifecycle_manager.mutex, NULL) != 0) {
        LOGX_ERROR_MSG("Failed to initialize lifecycle manager mutex");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Copy configuration
    if (config) {
        memcpy(&g_lifecycle_manager.config, config, sizeof(lifecycle_manager_config_t));
    } else {
        // Default configuration
        g_lifecycle_manager.config.enable_dependency_checking = true;
        g_lifecycle_manager.config.enable_auto_restart = false;
        g_lifecycle_manager.config.max_restart_attempts = 3;
        g_lifecycle_manager.config.restart_delay_ms = 1000;
        g_lifecycle_manager.config.enable_status_monitoring = true;
        g_lifecycle_manager.config.status_check_interval_ms = 30000;
        g_lifecycle_manager.config.enable_performance_monitoring = true;
        safe_strncpy(g_lifecycle_manager.config.log_level, "INFO", sizeof(g_lifecycle_manager.config.log_level));
    }
    
    // Initialize statistics
    memset(&g_lifecycle_manager.stats, 0, sizeof(lifecycle_manager_stats_t));
    g_lifecycle_manager.stats.manager_start_time = time(NULL);
    
    g_lifecycle_manager.initialized = true;
    
    LOGX_INFO_MSG("Lifecycle manager initialized with %d max modules", MAX_MODULES);
    return AUTONOMY_SUCCESS;
}

// Cleanup lifecycle manager
void lifecycle_manager_cleanup(void) {
    if (!g_lifecycle_manager.initialized) {
        return;
    }
    
    pthread_mutex_lock(&g_lifecycle_manager.mutex);
    
    // Cleanup all modules
    for (int i = 0; i < g_lifecycle_manager.module_count; i++) {
        if (g_lifecycle_manager.modules[i]) {
            module_lifecycle_t* module = g_lifecycle_manager.modules[i];
            if (module->status == MODULE_STATUS_RUNNING && module->stop_func) {
                LOGX_INFO_MSG("Stopping module: %s", module->module_name);
                module->stop_func();
            }
            if (module->status != MODULE_STATUS_UNINITIALIZED && module->cleanup_func) {
                LOGX_INFO_MSG("Cleaning up module: %s", module->module_name);
                module->cleanup_func();
            }
        }
    }
    
    g_lifecycle_manager.module_count = 0;
    g_lifecycle_manager.initialized = false;
    
    pthread_mutex_unlock(&g_lifecycle_manager.mutex);
    pthread_mutex_destroy(&g_lifecycle_manager.mutex);
    
    LOGX_INFO_MSG("Lifecycle manager cleaned up");
}

// Register module
int lifecycle_register_module(const module_lifecycle_t* module) {
    if (!g_lifecycle_manager.initialized) {
        LOGX_ERROR_MSG("Lifecycle manager not initialized");
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    if (!module || !module->module_name) {
        LOGX_ERROR_MSG("Invalid module parameter");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_lifecycle_manager.mutex);
    
    // Check if module already exists
    if (find_module(module->module_name)) {
        pthread_mutex_unlock(&g_lifecycle_manager.mutex);
        LOGX_ERROR_MSG("Module already registered: %s", module->module_name);
        return AUTONOMY_ERROR_ALREADY_EXISTS;
    }
    
    // Check capacity
    if (g_lifecycle_manager.module_count >= MAX_MODULES) {
        pthread_mutex_unlock(&g_lifecycle_manager.mutex);
        LOGX_ERROR_MSG("Maximum number of modules reached: %d", MAX_MODULES);
        return AUTONOMY_ERROR_NO_RESOURCES;
    }
    
    // Allocate and copy module
    module_lifecycle_t* new_module = malloc(sizeof(module_lifecycle_t));
    if (!new_module) {
        pthread_mutex_unlock(&g_lifecycle_manager.mutex);
        LOGX_ERROR_MSG("Failed to allocate memory for module: %s", module->module_name);
        return AUTONOMY_ERROR_NO_MEMORY;
    }
    
    memcpy(new_module, module, sizeof(module_lifecycle_t));
    new_module->status = MODULE_STATUS_UNINITIALIZED;
    new_module->init_time = 0;
    new_module->start_time = 0;
    new_module->stop_time = 0;
    new_module->error_count = 0;
    memset(new_module->last_error, 0, sizeof(new_module->last_error));
    
    g_lifecycle_manager.modules[g_lifecycle_manager.module_count] = new_module;
    g_lifecycle_manager.module_count++;
    g_lifecycle_manager.stats.total_modules++;
    
    pthread_mutex_unlock(&g_lifecycle_manager.mutex);
    
    LOGX_INFO_MSG("Registered module: %s (%s)", module->module_name, 
                  module->module_description ? module->module_description : "No description");
    
    return AUTONOMY_SUCCESS;
}

// Initialize module
int lifecycle_init_module(const char* module_name) {
    if (!g_lifecycle_manager.initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_lifecycle_manager.mutex);
    
    module_lifecycle_t* module = find_module(module_name);
    if (!module) {
        pthread_mutex_unlock(&g_lifecycle_manager.mutex);
        LOGX_ERROR_MSG("Module not found: %s", module_name);
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    if (module->status != MODULE_STATUS_UNINITIALIZED) {
        pthread_mutex_unlock(&g_lifecycle_manager.mutex);
        LOGX_WARN_MSG("Module already initialized: %s", module_name);
        return AUTONOMY_SUCCESS;
    }
    
    if (!module->init_func) {
        pthread_mutex_unlock(&g_lifecycle_manager.mutex);
        LOGX_ERROR_MSG("Module has no init function: %s", module_name);
        return AUTONOMY_ERROR_NOT_SUPPORTED;
    }
    
    pthread_mutex_unlock(&g_lifecycle_manager.mutex);
    
    LOGX_INFO_MSG("Initializing module: %s", module_name);
    
    double start_time = get_time_ms();
    int result = module->init_func();
    double end_time = get_time_ms();
    double duration = end_time - start_time;
    
    pthread_mutex_lock(&g_lifecycle_manager.mutex);
    
    if (result == AUTONOMY_SUCCESS) {
        module->status = MODULE_STATUS_INITIALIZED;
        module->init_time = time(NULL);
        module->init_count++;
        
        // Update statistics
        g_lifecycle_manager.stats.initialized_modules++;
        g_lifecycle_manager.stats.total_init_operations++;
        g_lifecycle_manager.stats.avg_init_time_ms = 
            (g_lifecycle_manager.stats.avg_init_time_ms * (g_lifecycle_manager.stats.total_init_operations - 1) + duration) /
            g_lifecycle_manager.stats.total_init_operations;
        
        LOGX_INFO_MSG("Module initialized successfully: %s (%.2f ms)", module_name, duration);
        
        // Auto-start if configured
        if (module->auto_start && module->start_func) {
            pthread_mutex_unlock(&g_lifecycle_manager.mutex);
            return lifecycle_start_module(module_name);
        }
    } else {
        module->status = MODULE_STATUS_ERROR;
        module->error_count++;
        g_lifecycle_manager.stats.total_errors++;
        snprintf(module->last_error, sizeof(module->last_error), 
                "Initialization failed with code: %d", result);
        
        LOGX_ERROR_MSG("Module initialization failed: %s (code: %d, %.2f ms)", 
                      module_name, result, duration);
    }
    
    pthread_mutex_unlock(&g_lifecycle_manager.mutex);
    return result;
}

// Start module
int lifecycle_start_module(const char* module_name) {
    if (!g_lifecycle_manager.initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_lifecycle_manager.mutex);
    
    module_lifecycle_t* module = find_module(module_name);
    if (!module) {
        pthread_mutex_unlock(&g_lifecycle_manager.mutex);
        LOGX_ERROR_MSG("Module not found: %s", module_name);
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    if (module->status != MODULE_STATUS_INITIALIZED) {
        pthread_mutex_unlock(&g_lifecycle_manager.mutex);
        LOGX_WARN_MSG("Module not initialized: %s (status: %d)", module_name, module->status);
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    if (!module->start_func) {
        pthread_mutex_unlock(&g_lifecycle_manager.mutex);
        LOGX_DEBUG_MSG("Module has no start function: %s", module_name);
        return AUTONOMY_SUCCESS; // Not an error, just no start function
    }
    
    pthread_mutex_unlock(&g_lifecycle_manager.mutex);
    
    LOGX_INFO_MSG("Starting module: %s", module_name);
    
    double start_time = get_time_ms();
    int result = module->start_func();
    double end_time = get_time_ms();
    double duration = end_time - start_time;
    
    pthread_mutex_lock(&g_lifecycle_manager.mutex);
    
    if (result == AUTONOMY_SUCCESS) {
        module->status = MODULE_STATUS_RUNNING;
        module->start_time = time(NULL);
        module->start_count++;
        
        // Update statistics
        g_lifecycle_manager.stats.running_modules++;
        g_lifecycle_manager.stats.total_start_operations++;
        g_lifecycle_manager.stats.avg_start_time_ms = 
            (g_lifecycle_manager.stats.avg_start_time_ms * (g_lifecycle_manager.stats.total_start_operations - 1) + duration) /
            g_lifecycle_manager.stats.total_start_operations;
        
        LOGX_INFO_MSG("Module started successfully: %s (%.2f ms)", module_name, duration);
    } else {
        module->status = MODULE_STATUS_ERROR;
        module->error_count++;
        g_lifecycle_manager.stats.total_errors++;
        snprintf(module->last_error, sizeof(module->last_error), 
                "Start failed with code: %d", result);
        
        LOGX_ERROR_MSG("Module start failed: %s (code: %d, %.2f ms)", 
                      module_name, result, duration);
    }
    
    pthread_mutex_unlock(&g_lifecycle_manager.mutex);
    return result;
}

// Stop module
int lifecycle_stop_module(const char* module_name) {
    if (!g_lifecycle_manager.initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_lifecycle_manager.mutex);
    
    module_lifecycle_t* module = find_module(module_name);
    if (!module) {
        pthread_mutex_unlock(&g_lifecycle_manager.mutex);
        LOGX_ERROR_MSG("Module not found: %s", module_name);
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    if (module->status != MODULE_STATUS_RUNNING) {
        pthread_mutex_unlock(&g_lifecycle_manager.mutex);
        LOGX_WARN_MSG("Module not running: %s (status: %d)", module_name, module->status);
        return AUTONOMY_SUCCESS;
    }
    
    if (!module->stop_func) {
        pthread_mutex_unlock(&g_lifecycle_manager.mutex);
        LOGX_DEBUG_MSG("Module has no stop function: %s", module_name);
        module->status = MODULE_STATUS_STOPPED;
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_unlock(&g_lifecycle_manager.mutex);
    
    LOGX_INFO_MSG("Stopping module: %s", module_name);
    
    double start_time = get_time_ms();
    int result = module->stop_func();
    double end_time = get_time_ms();
    double duration = end_time - start_time;
    
    pthread_mutex_lock(&g_lifecycle_manager.mutex);
    
    if (result == AUTONOMY_SUCCESS) {
        module->status = MODULE_STATUS_STOPPED;
        module->stop_time = time(NULL);
        module->stop_count++;
        
        // Update statistics
        if (g_lifecycle_manager.stats.running_modules > 0) {
            g_lifecycle_manager.stats.running_modules--;
        }
        g_lifecycle_manager.stats.total_stop_operations++;
        g_lifecycle_manager.stats.avg_stop_time_ms = 
            (g_lifecycle_manager.stats.avg_stop_time_ms * (g_lifecycle_manager.stats.total_stop_operations - 1) + duration) /
            g_lifecycle_manager.stats.total_stop_operations;
        
        LOGX_INFO_MSG("Module stopped successfully: %s (%.2f ms)", module_name, duration);
    } else {
        module->status = MODULE_STATUS_ERROR;
        module->error_count++;
        g_lifecycle_manager.stats.total_errors++;
        snprintf(module->last_error, sizeof(module->last_error), 
                "Stop failed with code: %d", result);
        
        LOGX_ERROR_MSG("Module stop failed: %s (code: %d, %.2f ms)", 
                      module_name, result, duration);
    }
    
    pthread_mutex_unlock(&g_lifecycle_manager.mutex);
    return result;
}

// Cleanup module
int lifecycle_cleanup_module(const char* module_name) {
    if (!g_lifecycle_manager.initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_lifecycle_manager.mutex);
    
    module_lifecycle_t* module = find_module(module_name);
    if (!module) {
        pthread_mutex_unlock(&g_lifecycle_manager.mutex);
        LOGX_ERROR_MSG("Module not found: %s", module_name);
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    // Stop module first if running
    if (module->status == MODULE_STATUS_RUNNING) {
        pthread_mutex_unlock(&g_lifecycle_manager.mutex);
        int stop_result = lifecycle_stop_module(module_name);
        if (stop_result != AUTONOMY_SUCCESS) {
            LOGX_WARN_MSG("Failed to stop module before cleanup: %s", module_name);
        }
        pthread_mutex_lock(&g_lifecycle_manager.mutex);
    }
    
    if (module->cleanup_func) {
        pthread_mutex_unlock(&g_lifecycle_manager.mutex);
        
        LOGX_INFO_MSG("Cleaning up module: %s", module_name);
        module->cleanup_func();
        
        pthread_mutex_lock(&g_lifecycle_manager.mutex);
    }
    
    module->status = MODULE_STATUS_UNINITIALIZED;
    if (g_lifecycle_manager.stats.initialized_modules > 0) {
        g_lifecycle_manager.stats.initialized_modules--;
    }
    
    pthread_mutex_unlock(&g_lifecycle_manager.mutex);
    
    LOGX_INFO_MSG("Module cleaned up: %s", module_name);
    return AUTONOMY_SUCCESS;
}

// Initialize all modules
int lifecycle_init_all_modules(void) {
    if (!g_lifecycle_manager.initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    LOGX_INFO_MSG("Initializing all modules (%d total)", g_lifecycle_manager.module_count);
    
    int success_count = 0;
    int error_count = 0;
    
    for (int i = 0; i < g_lifecycle_manager.module_count; i++) {
        if (g_lifecycle_manager.modules[i]) {
            int result = lifecycle_init_module(g_lifecycle_manager.modules[i]->module_name);
            if (result == AUTONOMY_SUCCESS) {
                success_count++;
            } else {
                error_count++;
                if (g_lifecycle_manager.modules[i]->critical) {
                    LOGX_ERROR_MSG("Critical module failed to initialize: %s", 
                                  g_lifecycle_manager.modules[i]->module_name);
                    return result;
                }
            }
        }
    }
    
    LOGX_INFO_MSG("Module initialization complete: %d success, %d errors", 
                  success_count, error_count);
    
    return (error_count == 0) ? AUTONOMY_SUCCESS : AUTONOMY_ERROR_SYSTEM;
}

// Start all modules
int lifecycle_start_all_modules(void) {
    if (!g_lifecycle_manager.initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    LOGX_INFO_MSG("Starting all modules");
    
    int success_count = 0;
    int error_count = 0;
    
    for (int i = 0; i < g_lifecycle_manager.module_count; i++) {
        if (g_lifecycle_manager.modules[i] && 
            g_lifecycle_manager.modules[i]->status == MODULE_STATUS_INITIALIZED) {
            int result = lifecycle_start_module(g_lifecycle_manager.modules[i]->module_name);
            if (result == AUTONOMY_SUCCESS) {
                success_count++;
            } else {
                error_count++;
            }
        }
    }
    
    LOGX_INFO_MSG("Module start complete: %d success, %d errors", 
                  success_count, error_count);
    
    return AUTONOMY_SUCCESS;
}

// Get module status
module_status_t lifecycle_get_module_status(const char* module_name) {
    if (!g_lifecycle_manager.initialized || !module_name) {
        return MODULE_STATUS_ERROR;
    }
    
    pthread_mutex_lock(&g_lifecycle_manager.mutex);
    
    module_lifecycle_t* module = find_module(module_name);
    module_status_t status = module ? module->status : MODULE_STATUS_ERROR;
    
    pthread_mutex_unlock(&g_lifecycle_manager.mutex);
    
    return status;
}

// Check if module is initialized
bool lifecycle_is_module_initialized(const char* module_name) {
    module_status_t status = lifecycle_get_module_status(module_name);
    return (status == MODULE_STATUS_INITIALIZED || status == MODULE_STATUS_RUNNING);
}

// Check if module is running
bool lifecycle_is_module_running(const char* module_name) {
    return lifecycle_get_module_status(module_name) == MODULE_STATUS_RUNNING;
}

// Get statistics
void lifecycle_get_stats(lifecycle_manager_stats_t* stats) {
    if (!stats || !g_lifecycle_manager.initialized) {
        return;
    }
    
    pthread_mutex_lock(&g_lifecycle_manager.mutex);
    memcpy(stats, &g_lifecycle_manager.stats, sizeof(lifecycle_manager_stats_t));
    pthread_mutex_unlock(&g_lifecycle_manager.mutex);
}

// Convert status to string
const char* lifecycle_status_to_string(module_status_t status) {
    switch (status) {
        case MODULE_STATUS_UNINITIALIZED: return "uninitialized";
        case MODULE_STATUS_INITIALIZED: return "initialized";
        case MODULE_STATUS_STARTING: return "starting";
        case MODULE_STATUS_RUNNING: return "running";
        case MODULE_STATUS_STOPPING: return "stopping";
        case MODULE_STATUS_STOPPED: return "stopped";
        case MODULE_STATUS_ERROR: return "error";
        case MODULE_STATUS_CLEANUP: return "cleanup";
        default: return "unknown";
    }
}