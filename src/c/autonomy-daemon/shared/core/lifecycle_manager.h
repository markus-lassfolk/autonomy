#ifndef SHARED_LIFECYCLE_MANAGER_H
#define SHARED_LIFECYCLE_MANAGER_H

#include <stdbool.h>
#include <time.h>
#include "common_types.h"

// Standardized module lifecycle management
// This consolidates the common init/cleanup/start/stop patterns

// Module status
typedef enum {
    MODULE_STATUS_UNINITIALIZED = 0,
    MODULE_STATUS_INITIALIZED,
    MODULE_STATUS_STARTING,
    MODULE_STATUS_RUNNING,
    MODULE_STATUS_STOPPING,
    MODULE_STATUS_STOPPED,
    MODULE_STATUS_ERROR,
    MODULE_STATUS_CLEANUP
} module_status_t;

// Module lifecycle function signatures
typedef int (*module_init_func_t)(void);
typedef void (*module_cleanup_func_t)(void);
typedef int (*module_start_func_t)(void);
typedef int (*module_stop_func_t)(void);
typedef bool (*module_is_initialized_func_t)(void);
typedef void (*module_get_status_func_t)(void*);

// Module lifecycle definition
typedef struct {
    const char* module_name;                    // Module name for logging
    const char* module_description;             // Module description
    
    // Lifecycle functions
    module_init_func_t init_func;               // Initialize module
    module_cleanup_func_t cleanup_func;         // Cleanup module
    module_start_func_t start_func;             // Start module (optional)
    module_stop_func_t stop_func;               // Stop module (optional)
    module_is_initialized_func_t is_initialized_func; // Check if initialized
    module_get_status_func_t get_status_func;   // Get module status (optional)
    
    // Module state
    module_status_t status;                     // Current status
    time_t init_time;                           // When module was initialized
    time_t start_time;                          // When module was started
    time_t stop_time;                           // When module was stopped
    time_t last_status_check;                   // Last status check
    int error_count;                            // Number of errors encountered
    char last_error[256];                       // Last error message
    
    // Dependencies
    const char** dependencies;                  // Array of dependency module names
    int dependency_count;                       // Number of dependencies
    
    // Configuration
    bool auto_start;                            // Start automatically after init
    bool critical;                              // Critical module (failure stops daemon)
    int init_timeout_ms;                        // Initialization timeout
    int start_timeout_ms;                       // Start timeout
    int stop_timeout_ms;                        // Stop timeout
    
    // Statistics
    uint64_t init_count;                        // Number of initializations
    uint64_t start_count;                       // Number of starts
    uint64_t stop_count;                        // Number of stops
    uint64_t error_count_total;                 // Total errors
    double avg_init_time_ms;                    // Average init time
    double avg_start_time_ms;                   // Average start time
    double avg_stop_time_ms;                    // Average stop time
} module_lifecycle_t;

// Lifecycle manager configuration
typedef struct {
    bool enable_dependency_checking;            // Enable dependency validation
    bool enable_auto_restart;                   // Enable automatic restart on failure
    int max_restart_attempts;                   // Maximum restart attempts
    int restart_delay_ms;                       // Delay between restart attempts
    bool enable_status_monitoring;              // Enable periodic status monitoring
    int status_check_interval_ms;               // Status check interval
    bool enable_performance_monitoring;         // Enable performance monitoring
    char log_level[16];                         // Log level for lifecycle events
} lifecycle_manager_config_t;

// Initialize lifecycle manager
int lifecycle_manager_init(const lifecycle_manager_config_t* config);

// Cleanup lifecycle manager
void lifecycle_manager_cleanup(void);

// Module registration
int lifecycle_register_module(const module_lifecycle_t* module);
int lifecycle_unregister_module(const char* module_name);

// Module management
int lifecycle_init_module(const char* module_name);
int lifecycle_cleanup_module(const char* module_name);
int lifecycle_start_module(const char* module_name);
int lifecycle_stop_module(const char* module_name);

// Batch operations
int lifecycle_init_all_modules(void);
int lifecycle_cleanup_all_modules(void);
int lifecycle_start_all_modules(void);
int lifecycle_stop_all_modules(void);

// Dependency management
int lifecycle_init_with_dependencies(const char* module_name);
int lifecycle_stop_with_dependents(const char* module_name);
int lifecycle_validate_dependencies(void);

// Status and monitoring
module_status_t lifecycle_get_module_status(const char* module_name);
bool lifecycle_is_module_initialized(const char* module_name);
bool lifecycle_is_module_running(const char* module_name);
int lifecycle_get_module_count(void);
char** lifecycle_get_module_names(int* count);

// Error handling
const char* lifecycle_get_module_error(const char* module_name);
int lifecycle_get_module_error_count(const char* module_name);
void lifecycle_clear_module_errors(const char* module_name);

// Statistics
typedef struct {
    int total_modules;                          // Total registered modules
    int initialized_modules;                    // Initialized modules
    int running_modules;                        // Running modules
    int error_modules;                          // Modules in error state
    time_t manager_start_time;                  // When manager was started
    uint64_t total_init_operations;             // Total init operations
    uint64_t total_start_operations;            // Total start operations
    uint64_t total_stop_operations;             // Total stop operations
    uint64_t total_errors;                      // Total errors across all modules
    double avg_init_time_ms;                    // Average initialization time
    double avg_start_time_ms;                   // Average start time
    double avg_stop_time_ms;                    // Average stop time
} lifecycle_manager_stats_t;

void lifecycle_get_stats(lifecycle_manager_stats_t* stats);
void lifecycle_reset_stats(void);

// Utility functions
const char* lifecycle_status_to_string(module_status_t status);
bool lifecycle_is_valid_module_name(const char* module_name);

// Common lifecycle patterns
#define LIFECYCLE_DEFINE_MODULE(name, desc, init_fn, cleanup_fn, start_fn, stop_fn, is_init_fn, get_status_fn) \
    static module_lifecycle_t name##_lifecycle = { \
        .module_name = #name, \
        .module_description = desc, \
        .init_func = init_fn, \
        .cleanup_func = cleanup_fn, \
        .start_func = start_fn, \
        .stop_func = stop_fn, \
        .is_initialized_func = is_init_fn, \
        .get_status_func = get_status_fn, \
        .status = MODULE_STATUS_UNINITIALIZED, \
        .auto_start = false, \
        .critical = false, \
        .init_timeout_ms = 5000, \
        .start_timeout_ms = 5000, \
        .stop_timeout_ms = 5000 \
    }

#define LIFECYCLE_REGISTER_MODULE(name) \
    lifecycle_register_module(&name##_lifecycle)

#define LIFECYCLE_INIT_MODULE(name) \
    lifecycle_init_module(#name)

#define LIFECYCLE_START_MODULE(name) \
    lifecycle_start_module(#name)

#define LIFECYCLE_STOP_MODULE(name) \
    lifecycle_stop_module(#name)

#define LIFECYCLE_CLEANUP_MODULE(name) \
    lifecycle_cleanup_module(#name)

#endif // SHARED_LIFECYCLE_MANAGER_H