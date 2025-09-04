#ifndef NETWORK_CONTROLLER_H
#define NETWORK_CONTROLLER_H

#include "types.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// Failover methods
typedef enum {
    FAILOVER_METHOD_MWAN3 = 0,
    FAILOVER_METHOD_NETIFD,
    FAILOVER_METHOD_MANUAL,
    FAILOVER_METHOD_MAX
} failover_method_t;

// Network member structure
typedef struct {
    char name[64];                      // Member name
    char interface[32];                 // Interface name (e.g., "mob1s1a1")
    char class[16];                     // Member class (starlink, cellular, wifi, lan)
    char policy[32];                    // MWAN3 policy name
    int weight;                         // Member weight
    bool eligible;                      // Whether member is eligible for use
    char detect[16];                    // Detection mode (auto, disable, force)
    bool is_primary;                    // Whether this is the primary interface
    time_t last_seen;                   // Last time member was seen
    time_t created_at;                  // When member was created
} network_member_t;

// Switch result
typedef struct {
    bool success;                       // Whether switch was successful
    char from_member[64];               // Source member name
    char to_member[64];                 // Target member name
    char method[16];                    // Method used (mwan3, netifd, manual)
    char reason[256];                   // Reason for switch
    time_t timestamp;                   // Switch timestamp
    double duration_ms;                 // Switch duration in milliseconds
    char error_message[256];            // Error message if failed
} switch_result_t;

// Network controller configuration
typedef struct {
    bool enabled;                       // Enable network controller
    bool use_mwan3;                     // Use MWAN3 for switching
    bool dry_run;                       // Dry run mode (don't make changes)
    char mwan3_path[64];                // Path to mwan3 command
    char ubus_path[64];                 // Path to ubus command
    int switch_timeout_seconds;         // Timeout for switch operations
    int validation_timeout_seconds;     // Timeout for validation
    bool enable_callbacks;              // Enable failover callbacks
} network_controller_config_t;

// Network controller statistics
typedef struct {
    int total_switches;                 // Total switch attempts
    int successful_switches;            // Successful switches
    int failed_switches;                // Failed switches
    time_t last_switch;                 // Last switch timestamp
    double average_switch_time_ms;      // Average switch time
    char current_member[64];            // Current active member
    char last_error[256];               // Last error message
} network_controller_stats_t;

// Failover callback function type
typedef int (*failover_callback_t)(const network_member_t* from, const network_member_t* to);

// Network controller structure
typedef struct {
    network_controller_config_t config;
    network_controller_stats_t stats;
    
    // Current state
    network_member_t* current_member;
    network_member_t members[16];
    int member_count;
    int max_members;
    
    // Callbacks
    failover_callback_t callbacks[8];
    int callback_count;
    
    // Thread safety
    pthread_mutex_t mutex;
} network_controller_t;

// Function prototypes

/**
 * Initialize network controller
 * @param config Controller configuration
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int network_controller_init(const network_controller_config_t* config);

/**
 * Cleanup network controller
 */
void network_controller_cleanup(void);

/**
 * Switch from one member to another
 * @param from Source member (can be NULL)
 * @param to Target member
 * @param result Switch result structure to fill
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int network_controller_switch(const network_member_t* from, const network_member_t* to, switch_result_t* result);

/**
 * Get current active member
 * @param member Structure to fill with current member
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int network_controller_get_current_member(network_member_t* member);

/**
 * Set members list
 * @param members Array of members
 * @param count Number of members
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int network_controller_set_members(const network_member_t* members, int count);

/**
 * Get members list
 * @param members Array to fill with members
 * @param max_count Maximum number of members
 * @param actual_count Actual number of members returned
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int network_controller_get_members(network_member_t* members, int max_count, int* actual_count);

/**
 * Validate member configuration
 * @param member Member to validate
 * @return AUTONOMY_SUCCESS if valid, error code if invalid
 */
int network_controller_validate_member(const network_member_t* member);

/**
 * Add failover callback
 * @param callback Callback function to add
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int network_controller_add_callback(failover_callback_t callback);

/**
 * Remove failover callback
 * @param callback Callback function to remove
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int network_controller_remove_callback(failover_callback_t callback);

/**
 * Test MWAN3 availability
 * @return AUTONOMY_SUCCESS if available, error code if not
 */
int network_controller_test_mwan3(void);

/**
 * Test netifd availability
 * @return AUTONOMY_SUCCESS if available, error code if not
 */
int network_controller_test_netifd(void);

/**
 * Get network controller statistics
 * @param stats Structure to fill with statistics
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int network_controller_get_stats(network_controller_stats_t* stats);

/**
 * Get network controller configuration
 * @param config Structure to fill with configuration
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int network_controller_get_config(network_controller_config_t* config);

/**
 * Set network controller configuration
 * @param config New configuration
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int network_controller_set_config(const network_controller_config_t* config);

/**
 * Enable/disable network controller
 * @param enabled Whether to enable controller
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int network_controller_set_enabled(bool enabled);

/**
 * Set dry run mode
 * @param dry_run Whether to enable dry run mode
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int network_controller_set_dry_run(bool dry_run);

/**
 * Reset network controller statistics
 * @return AUTONOMY_SUCCESS on success, error code on failure
 */
int network_controller_reset_stats(void);

/**
 * Check if network controller is initialized
 * @return true if initialized, false otherwise
 */
bool network_controller_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif // NETWORK_CONTROLLER_H