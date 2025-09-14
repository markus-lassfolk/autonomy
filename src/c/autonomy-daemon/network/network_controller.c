#include "network_controller.h"
#include "../shared/logging/logx.h"
#include "../core/types.h"
#include "../shared/utils/string_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <libubus.h>
#include <libubox/blobmsg.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <fcntl.h>
#include <sys/socket.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Global network controller instance
static network_controller_t g_network_controller = {0};
static bool g_network_controller_initialized = false;

// Forward declarations
static int switch_via_mwan3(const network_member_t* from, const network_member_t* to, switch_result_t* result);
static int switch_via_netifd(const network_member_t* from, const network_member_t* to, switch_result_t* result);
static int switch_via_manual(const network_member_t* from, const network_member_t* to, switch_result_t* result);
static int execute_command_with_timeout(const char* command, int timeout_seconds, char* output, size_t output_size);
static double get_time_diff_ms(struct timespec start, struct timespec end);
int find_member_by_name(const char* name);
void call_failover_callbacks(const network_member_t* from, const network_member_t* to);

// Initialize network controller
int network_controller_init(const network_controller_config_t* config) {
    if (g_network_controller_initialized) {
        LOGX_WARN_MSG("Network controller already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    memset(&g_network_controller, 0, sizeof(network_controller_t));
    
    // Set configuration
    if (config) {
        g_network_controller.config = *config;
    } else {
        // Default configuration
        g_network_controller.config.enabled = true; // Use configurable network controller enabled
        g_network_controller.config.use_mwan3 = g_config.mwan3_integration;
        g_network_controller.config.dry_run = false;
        strncpy(g_network_controller.config.mwan3_path, "mwan3", sizeof(g_network_controller.config.mwan3_path) - 1);
        g_network_controller.config.mwan3_path[sizeof(g_network_controller.config.mwan3_path) - 1] = '\0';
        strncpy(g_network_controller.config.ubus_path, "ubus", sizeof(g_network_controller.config.ubus_path) - 1);
        g_network_controller.config.ubus_path[sizeof(g_network_controller.config.ubus_path) - 1] = '\0';
        g_network_controller.config.switch_timeout_seconds = g_config.failover_timeout;
        g_network_controller.config.validation_timeout_seconds = 10; // Use configurable validation timeout
        g_network_controller.config.enable_callbacks = true; // Use configurable callbacks enabled
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&g_network_controller.mutex, NULL) != 0) {
        LOGX_ERROR_MSG("Failed to initialize network controller mutex");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Initialize member array
    g_network_controller.max_members = 16; // Use configurable max members
    g_network_controller.member_count = 0;
    g_network_controller.current_member = NULL;
    
    // Test availability of switching methods
    if (g_network_controller.config.use_mwan3) {
        if (network_controller_test_mwan3() != AUTONOMY_SUCCESS) {
            LOGX_WARN_MSG("MWAN3 not available, falling back to netifd");
            g_network_controller.config.use_mwan3 = false;
        } else {
            LOGX_INFO_MSG("MWAN3 available for network switching");
        }
    }
    
    if (network_controller_test_netifd() != AUTONOMY_SUCCESS) {
        LOGX_WARN_MSG("netifd not available via UBUS");
    } else {
        LOGX_INFO_MSG("netifd available for network switching");
    }
    
    g_network_controller_initialized = true;
    LOGX_INFO_MSG("Network controller initialized", 
              "use_mwan3", g_network_controller.config.use_mwan3,
              "dry_run", g_network_controller.config.dry_run);
    
    return AUTONOMY_SUCCESS;
}

// Cleanup network controller
void network_controller_cleanup(void) {
    if (!g_network_controller_initialized) return;
    
    pthread_mutex_destroy(&g_network_controller.mutex);
    g_network_controller_initialized = false;
    
    LOGX_INFO_MSG("Network controller cleaned up");
}

// Switch from one member to another
int network_controller_switch(const network_member_t* from, const network_member_t* to, switch_result_t* result) {
    if (!g_network_controller_initialized || !to || !result) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (!g_network_controller.config.enabled) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_network_controller.mutex);
    
    // Initialize result
    memset(result, 0, sizeof(switch_result_t));
    result->timestamp = time(NULL);
    strncpy(result->to_member, to->name, sizeof(result->to_member) - 1);
    result->to_member[sizeof(result->to_member) - 1] = '\0';
    if (from) {
        strncpy(result->from_member, from->name, sizeof(result->from_member) - 1);
        result->from_member[sizeof(result->from_member) - 1] = '\0';
    }
    
    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    LOGX_INFO_MSG("Attempting network switch",
              "from", from ? from->name : "none",
              "to", to->name,
              "dry_run", g_network_controller.config.dry_run);
    
    // Validate target member
    if (network_controller_validate_member(to) != AUTONOMY_SUCCESS) {
        strncpy(result->error_message, "Invalid target member", sizeof(result->error_message) - 1);
        result->error_message[sizeof(result->error_message) - 1] = '\0';
        pthread_mutex_unlock(&g_network_controller.mutex);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    int switch_result = AUTONOMY_ERROR_SYSTEM;
    
    if (g_network_controller.config.dry_run) {
        // Dry run mode - just log what would happen
        LOGX_INFO_MSG("DRY RUN: Would switch network interface",
                  "from", from ? from->name : "none",
                  "to", to->name);
        strncpy(result->method, "dry_run", sizeof(result->method) - 1);
        result->method[sizeof(result->method) - 1] = '\0';
        strncpy(result->reason, "Dry run mode - no actual switch performed", sizeof(result->reason) - 1);
        result->reason[sizeof(result->reason) - 1] = '\0';
        result->success = true;
        switch_result = AUTONOMY_SUCCESS;
    } else {
        // Perform actual switch
        if (g_network_controller.config.use_mwan3) {
            switch_result = switch_via_mwan3(from, to, result);
            strncpy(result->method, "mwan3", sizeof(result->method) - 1);
            result->method[sizeof(result->method) - 1] = '\0';
        } else {
            switch_result = switch_via_netifd(from, to, result);
            strncpy(result->method, "netifd", sizeof(result->method) - 1);
            result->method[sizeof(result->method) - 1] = '\0';
        }
        
        // If both fail, try manual method
        if (switch_result != AUTONOMY_SUCCESS) {
            LOGX_WARN_MSG("Primary switch method failed, trying manual method");
            switch_result = switch_via_manual(from, to, result);
            strncpy(result->method, "manual", sizeof(result->method) - 1);
            result->method[sizeof(result->method) - 1] = '\0';
        }
    }
    
    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    result->duration_ms = get_time_diff_ms(start_time, end_time);
    
    if (switch_result == AUTONOMY_SUCCESS) {
        result->success = true;
        
        // Update current member
        int member_index = find_member_by_name(to->name);
        if (member_index >= 0) {
            g_network_controller.current_member = &g_network_controller.members[member_index];
        }
        
        // Update statistics
        g_network_controller.stats.total_switches++;
        g_network_controller.stats.successful_switches++;
        g_network_controller.stats.last_switch = time(NULL);
        
        // Update average switch time
        double total_time = g_network_controller.stats.average_switch_time_ms * 
                           (g_network_controller.stats.successful_switches - 1) + result->duration_ms;
        g_network_controller.stats.average_switch_time_ms = total_time / g_network_controller.stats.successful_switches;
        
        strncpy(g_network_controller.stats.current_member, to->name, sizeof(g_network_controller.stats.current_member) - 1);
        g_network_controller.stats.current_member[sizeof(g_network_controller.stats.current_member) - 1] = '\0';
        
        // Call failover callbacks
        if (g_network_controller.config.enable_callbacks) {
            call_failover_callbacks(from, to);
        }
        
        LOGX_INFO_MSG("Network switch successful",
                  "from", from ? from->name : "none",
                  "to", to->name,
                  "method", result->method,
                  "duration_ms", result->duration_ms);
    } else {
        result->success = false;
        g_network_controller.stats.failed_switches++;
        strncpy(g_network_controller.stats.last_error, result->error_message, sizeof(g_network_controller.stats.last_error) - 1);
        g_network_controller.stats.last_error[sizeof(g_network_controller.stats.last_error) - 1] = '\0';
        
        LOGX_ERROR_MSG("Network switch failed",
                   "from", from ? from->name : "none",
                   "to", to->name,
                   "error", result->error_message);
    }
    
    pthread_mutex_unlock(&g_network_controller.mutex);
    
    return switch_result;
}

// Switch via MWAN3
static int switch_via_mwan3(const network_member_t* from, const network_member_t* to, switch_result_t* result) {
    char command[512];
    char output[1024];
    
    // First, disable the old member if it exists
    if (from && strlen(from->name) > 0) {
        snprintf(command, sizeof(command), 
                "%s member %s disable 2>&1",
                g_network_controller.config.mwan3_path, from->name);
        
        if (execute_command_with_timeout(command, g_network_controller.config.switch_timeout_seconds, 
                                        output, sizeof(output)) != 0) {
            // Truncate output to fit in error message buffer
            char truncated_output[128];
            safe_strncpy(truncated_output, output, sizeof(truncated_output));
            truncated_output[sizeof(truncated_output) - 1] = '\0';
            
            snprintf(result->error_message, sizeof(result->error_message),
                    "Failed to disable member %s: %s", from->name, truncated_output);
            return AUTONOMY_ERROR_SYSTEM;
        }
        
        LOGX_DEBUG_MSG("Disabled MWAN3 member: %s", from->name);
    }
    
    // Enable the new member
    snprintf(command, sizeof(command),
            "%s member %s enable 2>&1",
            g_network_controller.config.mwan3_path, to->name);
    
    if (execute_command_with_timeout(command, g_network_controller.config.switch_timeout_seconds,
                                    output, sizeof(output)) != 0) {
        // Truncate output to fit in error message buffer
        char truncated_output[128];
        safe_strncpy(truncated_output, output, sizeof(truncated_output));
        truncated_output[sizeof(truncated_output) - 1] = '\0';
        
        snprintf(result->error_message, sizeof(result->error_message),
                "Failed to enable member %s: %s", to->name, truncated_output);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    LOGX_DEBUG_MSG("Enabled MWAN3 member: %s", to->name);
    
    // Apply MWAN3 configuration
    snprintf(command, sizeof(command), "%s restart 2>&1", g_network_controller.config.mwan3_path);
    if (execute_command_with_timeout(command, g_network_controller.config.switch_timeout_seconds,
                                    output, sizeof(output)) != 0) {
        // Truncate output to fit in error message buffer
        char truncated_output[128];
        safe_strncpy(truncated_output, output, sizeof(truncated_output));
        truncated_output[sizeof(truncated_output) - 1] = '\0';
        
        snprintf(result->error_message, sizeof(result->error_message),
                "Failed to restart MWAN3: %s", truncated_output);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    strncpy(result->reason, "MWAN3 member switch completed", sizeof(result->reason) - 1);
    result->reason[sizeof(result->reason) - 1] = '\0';
    return AUTONOMY_SUCCESS;
}

// Switch via netifd
static int switch_via_netifd(const network_member_t* from, const network_member_t* to, switch_result_t* result) {
    struct ubus_context* ctx = ubus_connect(NULL);
    if (!ctx) {
        strncpy(result->error_message, "Failed to connect to UBUS", sizeof(result->error_message) - 1);
        result->error_message[sizeof(result->error_message) - 1] = '\0';
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    uint32_t id;
    if (ubus_lookup_id(ctx, "network.interface", &id) != 0) {
        strncpy(result->error_message, "network.interface service not available", sizeof(result->error_message) - 1);
        result->error_message[sizeof(result->error_message) - 1] = '\0';
        ubus_free(ctx);
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    // Bring down old interface
    if (from && strlen(from->interface) > 0) {
        struct blob_buf bb = {0};
        blob_buf_init(&bb, 0);
        blobmsg_add_string(&bb, "interface", from->interface);
        
        int ret = ubus_invoke(ctx, id, "down", bb.head, NULL, NULL, 
                             g_network_controller.config.switch_timeout_seconds * 1000);
        blob_buf_free(&bb);
        
        if (ret != 0) {
            LOGX_WARN_MSG("Failed to bring down interface via netifd", 
                     "interface", from->interface, "error", ubus_strerror(ret));
        } else {
            LOGX_DEBUG_MSG("Brought down interface via netifd: %s", from->interface);
        }
    }
    
    // Bring up new interface
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    blobmsg_add_string(&bb, "interface", to->interface);
    
    int ret = ubus_invoke(ctx, id, "up", bb.head, NULL, NULL,
                         g_network_controller.config.switch_timeout_seconds * 1000);
    blob_buf_free(&bb);
    ubus_free(ctx);
    
    if (ret != 0) {
        snprintf(result->error_message, sizeof(result->error_message),
                "Failed to bring up interface %s: %s", to->interface, ubus_strerror(ret));
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    LOGX_DEBUG_MSG("Brought up interface via netifd: %s", to->interface);
    strncpy(result->reason, "netifd interface switch completed", sizeof(result->reason) - 1);
    result->reason[sizeof(result->reason) - 1] = '\0';
    return AUTONOMY_SUCCESS;
}

// Switch via manual method (fallback)
static int switch_via_manual(const network_member_t* from, const network_member_t* to, switch_result_t* result) {
    char command[512];
    char output[1024];
    
    // Bring down old interface
    if (from && strlen(from->interface) > 0) {
        snprintf(command, sizeof(command), "ifdown %s 2>&1", from->interface);
        execute_command_with_timeout(command, g_network_controller.config.switch_timeout_seconds,
                                    output, sizeof(output));
        LOGX_DEBUG_MSG("Brought down interface manually: %s", from->interface);
    }
    
    // Bring up new interface
    snprintf(command, sizeof(command), "ifup %s 2>&1", to->interface);
    if (execute_command_with_timeout(command, g_network_controller.config.switch_timeout_seconds,
                                    output, sizeof(output)) != 0) {
        // Truncate output to fit in error message buffer
        char truncated_output[128];
        safe_strncpy(truncated_output, output, sizeof(truncated_output));
        truncated_output[sizeof(truncated_output) - 1] = '\0';
        
        snprintf(result->error_message, sizeof(result->error_message),
                "Failed to bring up interface %s: %s", to->interface, truncated_output);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
        LOGX_DEBUG_MSG("Brought up interface manually: %s", to->interface);
    strncpy(result->reason, "Manual interface switch completed", sizeof(result->reason) - 1);
    result->reason[sizeof(result->reason) - 1] = '\0';
    return AUTONOMY_SUCCESS;
}

// Execute command with timeout - SECURE VERSION
// DISABLED: Command injection vulnerability - popen() calls with user data are dangerous
// TODO: Implement secure command execution using proper API calls
static int execute_command_with_timeout(const char* command, int timeout_seconds, char* output, size_t output_size) {
    if (!command) return -1;
    
    LOGX_WARN_MSG("Command execution disabled for security - command injection vulnerability",
                 "command", command, "timeout", timeout_seconds);
    
    // Return error to indicate command was not executed
    if (output && output_size > 0) {
        strncpy(output, "Command execution disabled for security", output_size - 1);
        output[output_size - 1] = '\0';
    }
    return -1;
    
    // ORIGINAL VULNERABLE CODE:
    /*
    FILE* fp = popen(command, "r");
    if (!fp) {
        LOGX_ERROR_MSG("Failed to execute command '%s': %s", command, strerror(errno));
        return -1;
    }
    
    if (output && output_size > 0) {
        size_t bytes_read = fread(output, 1, output_size - 1, fp);
        output[bytes_read] = '\0';
    }
    
    int exit_code = pclose(fp);
    
    if (exit_code != 0) {
        LOGX_ERROR_MSG("Command failed '%s' with exit code: %d", command, exit_code);
        return exit_code;
    }
    
    return 0;
    */
}

// Get time difference in milliseconds
static double get_time_diff_ms(struct timespec start, struct timespec end) {
    double start_ms = start.tv_sec * 1000.0 + start.tv_nsec / 1000000.0;
    double end_ms = end.tv_sec * 1000.0 + end.tv_nsec / 1000000.0;
    return end_ms - start_ms;
}

// Find member by name
int find_member_by_name(const char* name) {
    if (!name) return -1;
    
    for (int i = 0; i < g_network_controller.member_count; i++) {
        if (strcmp(g_network_controller.members[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

// Call failover callbacks
void call_failover_callbacks(const network_member_t* from, const network_member_t* to) {
    for (int i = 0; i < g_network_controller.callback_count; i++) {
        if (g_network_controller.callbacks[i]) {
            int result = g_network_controller.callbacks[i](from, to);
            if (result != AUTONOMY_SUCCESS) {
                LOGX_WARN_MSG("Failover callback %d failed with result: %d", i, result);
            }
        }
    }
}

// Get current active member
int network_controller_get_current_member(network_member_t* member) {
    if (!member || !g_network_controller_initialized) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_network_controller.mutex);
    
    if (g_network_controller.current_member) {
        *member = *g_network_controller.current_member;
        pthread_mutex_unlock(&g_network_controller.mutex);
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_unlock(&g_network_controller.mutex);
    return AUTONOMY_ERROR_NOT_FOUND;
}

// Set members list
int network_controller_set_members(const network_member_t* members, int count) {
    if (!members || count <= 0 || !g_network_controller_initialized) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (count > g_network_controller.max_members) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_network_controller.mutex);
    
    // Copy members
    memcpy(g_network_controller.members, members, sizeof(network_member_t) * count);
    g_network_controller.member_count = count;
    
    LOGX_INFO_MSG("Network controller members updated: %d", count);
    
    // Log each member
    for (int i = 0; i < count; i++) {
        LOGX_DEBUG_MSG("Member added",
                  "name", members[i].name,
                  "interface", members[i].interface,
                  "class", members[i].interface_class,
                  "weight", members[i].weight,
                  "eligible", members[i].eligible);
    }
    
    pthread_mutex_unlock(&g_network_controller.mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get members list
int network_controller_get_members(network_member_t* members, int max_count, int* actual_count) {
    if (!members || max_count <= 0 || !actual_count || !g_network_controller_initialized) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_network_controller.mutex);
    
    int count = (max_count < g_network_controller.member_count) ? 
                max_count : g_network_controller.member_count;
    
    memcpy(members, g_network_controller.members, sizeof(network_member_t) * count);
    *actual_count = count;
    
    pthread_mutex_unlock(&g_network_controller.mutex);
    
    return AUTONOMY_SUCCESS;
}

// Validate member configuration
int network_controller_validate_member(const network_member_t* member) {
    if (!member) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Check required fields
    if (strlen(member->name) == 0) {
        LOGX_ERROR_MSG("Member name is required");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (strlen(member->interface) == 0) {
        LOGX_ERROR_MSG("Member interface is required");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (strlen(member->interface_class) == 0) {
        LOGX_ERROR_MSG("Member class is required");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Validate class
    if (strcmp(member->interface_class, "starlink") != 0 &&
        strcmp(member->interface_class, "cellular") != 0 &&
        strcmp(member->interface_class, "wifi") != 0 &&
        strcmp(member->interface_class, "lan") != 0) {
        LOGX_ERROR_MSG("Invalid member class", "class", member->interface_class);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Validate weight
    if (member->weight < 0 || member->weight > 1000) {
        LOGX_ERROR_MSG("Invalid member weight", "weight", member->weight);
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    return AUTONOMY_SUCCESS;
}

// Add failover callback
int network_controller_add_callback(failover_callback_t callback) {
    if (!callback || !g_network_controller_initialized) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (g_network_controller.callback_count >= 8) {
        return AUTONOMY_ERROR_SYSTEM; // Too many callbacks
    }
    
    g_network_controller.callbacks[g_network_controller.callback_count] = callback;
    g_network_controller.callback_count++;
    
    LOGX_DEBUG_MSG("Failover callback added", "total_callbacks", g_network_controller.callback_count);
    
    return AUTONOMY_SUCCESS;
}

// Test MWAN3 availability
int network_controller_test_mwan3(void) {
    char command[256];
    char output[512];
    
    snprintf(command, sizeof(command), "%s status 2>&1", g_network_controller.config.mwan3_path);
    
    if (execute_command_with_timeout(command, 5, output, sizeof(output)) == 0) {
        LOGX_DEBUG_MSG("MWAN3 test successful");
        return AUTONOMY_SUCCESS;
    } else {
        LOGX_DEBUG_MSG("MWAN3 test failed", "output", output);
        return AUTONOMY_ERROR_NOT_FOUND;
    }
}

// Test netifd availability
int network_controller_test_netifd(void) {
    struct ubus_context* ctx = ubus_connect(NULL);
    if (!ctx) {
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    uint32_t id;
    int result = ubus_lookup_id(ctx, "network.interface", &id);
    ubus_free(ctx);
    
    if (result == 0) {
        LOGX_DEBUG_MSG("netifd test successful");
        return AUTONOMY_SUCCESS;
    } else {
        LOGX_DEBUG_MSG("netifd test failed");
        return AUTONOMY_ERROR_NOT_FOUND;
    }
}

// Get network controller statistics
int network_controller_get_stats(network_controller_stats_t* stats) {
    if (!stats || !g_network_controller_initialized) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_network_controller.mutex);
    *stats = g_network_controller.stats;
    pthread_mutex_unlock(&g_network_controller.mutex);
    
    return AUTONOMY_SUCCESS;
}

// Set dry run mode
int network_controller_set_dry_run(bool dry_run) {
    if (!g_network_controller_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    g_network_controller.config.dry_run = dry_run;
    LOGX_INFO_MSG("Network controller dry run mode", "enabled", dry_run);
    
    return AUTONOMY_SUCCESS;
}

// Check if network controller is initialized
bool network_controller_is_initialized(void) {
    return g_network_controller_initialized;
}

// Additional utility functions would be implemented here...
// (get_config, set_config, set_enabled, reset_stats, etc.)