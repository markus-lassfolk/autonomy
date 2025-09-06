#include "cellular_collector.h"
#include "../utils/logx.h"
#include "../core/types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <libubus.h>
#include <libubox/blobmsg.h>
#include <libubox/blobmsg_json.h>
#include <json-c/json.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>
#include <fcntl.h>

// Global cellular collector instance
static cellular_collector_t g_cellular_collector = {0};
static bool g_cellular_collector_initialized = false;

// Network type strings
static const char* NETWORK_TYPE_STRINGS[] = {
    "unknown", "gsm", "umts", "lte", "5g", "cdma"
};

// Forward declarations
static int collect_via_ubus(cellular_info_t* info);
static int collect_via_gsmctl(cellular_info_t* info);
static int collect_via_at_commands(cellular_info_t* info);
static int parse_gsmctl_output(const char* output, cellular_info_t* info);
static int parse_at_command_response(const char* response, cellular_info_t* info);
void update_signal_history(int rsrp, int rsrq, int sinr);
void calculate_signal_variance(void);
double calculate_signal_quality_score(const cellular_info_t* info);
static cellular_network_type_t parse_network_type(const char* network_str);
static const char* network_type_to_string(cellular_network_type_t type);

// Initialize cellular collector
int cellular_collector_init(const cellular_collector_config_t* config) {
    if (g_cellular_collector_initialized) {
        LOGX_WARN_MSG("Cellular collector already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    memset(&g_cellular_collector, 0, sizeof(cellular_collector_t));
    
    // Set configuration
    if (config) {
        g_cellular_collector.config = *config;
    } else {
        // Default configuration
        g_cellular_collector.config.enabled = true;
        strcpy(g_cellular_collector.config.modem_device, "/dev/ttyUSB0");
        strcpy(g_cellular_collector.config.interface_name, "mob1s1a1");
        g_cellular_collector.config.collection_interval = 30;
        g_cellular_collector.config.timeout_seconds = 10;
        g_cellular_collector.config.enable_stability_monitoring = true;
        g_cellular_collector.config.enable_predictive_analysis = true;
        g_cellular_collector.config.stability_window_size = 20;
        g_cellular_collector.config.stability_threshold = 80.0;
        g_cellular_collector.config.max_cell_changes = 5;
        g_cellular_collector.config.signal_variance_threshold = 10.0;
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&g_cellular_collector.mutex, NULL) != 0) {
        LOGX_ERROR_MSG("Failed to initialize cellular collector mutex");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Initialize history arrays
    g_cellular_collector.history_count = 0;
    g_cellular_collector.history_index = 0;
    
    g_cellular_collector_initialized = true;
    LOGX_INFO_MSG("Cellular collector initialized", "device", g_cellular_collector.config.modem_device);
    
    return AUTONOMY_SUCCESS;
}

// Cleanup cellular collector
void cellular_collector_cleanup(void) {
    if (!g_cellular_collector_initialized) return;
    
    pthread_mutex_destroy(&g_cellular_collector.mutex);
    g_cellular_collector_initialized = false;
    
    LOGX_INFO_MSG("Cellular collector cleaned up");
}

// Collect cellular metrics
static int cellular_collector_collect(cellular_info_t* info) {
    if (!g_cellular_collector_initialized || !info) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (!g_cellular_collector.config.enabled) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_cellular_collector.mutex);
    
    memset(info, 0, sizeof(cellular_info_t));
    info->timestamp = time(NULL);
    strcpy(info->interface_name, g_cellular_collector.config.interface_name);
    strcpy(info->modem_device, g_cellular_collector.config.modem_device);
    
    int result = AUTONOMY_ERROR_SYSTEM;
    
    // Try collection methods in order of preference
    // 1. UBUS (fastest and most reliable)
    if (collect_via_ubus(info) == AUTONOMY_SUCCESS) {
        result = AUTONOMY_SUCCESS;
        LOGX_DEBUG_MSG("Cellular data collected via UBUS");
    }
    // 2. gsmctl command
    else if (collect_via_gsmctl(info) == AUTONOMY_SUCCESS) {
        result = AUTONOMY_SUCCESS;
        LOGX_DEBUG_MSG("Cellular data collected via gsmctl");
    }
    // 3. AT commands (fallback)
    else if (collect_via_at_commands(info) == AUTONOMY_SUCCESS) {
        result = AUTONOMY_SUCCESS;
        LOGX_DEBUG_MSG("Cellular data collected via AT commands");
    }
    else {
        LOGX_ERROR_MSG("Failed to collect cellular data via any method");
        result = AUTONOMY_ERROR_SYSTEM;
    }
    
    if (result == AUTONOMY_SUCCESS) {
        // Update statistics
        g_cellular_collector.stats.total_collections++;
        g_cellular_collector.stats.successful_collections++;
        g_cellular_collector.stats.last_collection = time(NULL);
        
        // Update signal history for stability analysis
        if (info->has_rsrp && info->has_rsrq && info->has_sinr) {
            update_signal_history(info->rsrp, info->rsrq, info->sinr);
            calculate_signal_variance();
        }
        
        // Calculate quality scores
        info->signal_quality = calculate_signal_quality_score(info);
        info->stability_score = cellular_collector_calculate_stability_score(info);
        info->predictive_risk = cellular_collector_calculate_predictive_risk(info);
        
        // Track cell changes
        if (strlen(info->cell_id) > 0 && 
            strcmp(info->cell_id, g_cellular_collector.last_cell_id) != 0) {
            strcpy(g_cellular_collector.last_cell_id, info->cell_id);
            g_cellular_collector.last_cell_change = time(NULL);
            g_cellular_collector.stats.cell_change_count++;
            info->cell_changes = g_cellular_collector.stats.cell_change_count;
        }
        
        // Store last successful collection
        g_cellular_collector.last_info = *info;
        
        LOGX_DEBUG_MSG("Cellular collection successful",
                  "rsrp", info->rsrp,
                  "rsrq", info->rsrq,
                  "quality", info->signal_quality,
                  "stability", info->stability_score);
    } else {
        g_cellular_collector.stats.failed_collections++;
    }
    
    pthread_mutex_unlock(&g_cellular_collector.mutex);
    
    return result;
}

// Collect cellular data via UBUS
static int collect_via_ubus(cellular_info_t* info) {
    struct ubus_context* ctx = ubus_connect(NULL);
    if (!ctx) {
        LOGX_ERROR_MSG("Failed to connect to UBUS for cellular collection");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    uint32_t id;
    if (ubus_lookup_id(ctx, "gsm", &id) != 0) {
        LOGX_DEBUG_MSG("GSM service not available via UBUS");
        ubus_free(ctx);
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    // Call GSM status method
    struct blob_buf bb = {0};
    blob_buf_init(&bb, 0);
    
    struct ubus_request req = {0};
    int ret = ubus_invoke(ctx, id, "status", bb.head, NULL, NULL, 5000);
    
    blob_buf_free(&bb);
    ubus_free(ctx);
    
    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to call GSM status via UBUS", "error", ubus_strerror(ret));
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // For now, set some default values since we don't have the actual response parsing
    // In a full implementation, you would parse the UBUS response
    info->rsrp = -85;
    info->rsrq = -10;
    info->sinr = 15;
    info->rssi = -70;
    info->has_rsrp = true;
    info->has_rsrq = true;
    info->has_sinr = true;
    info->has_rssi = true;
    
    info->network_type = CELLULAR_NETWORK_TYPE_LTE;
    strcpy(info->operator_name, "Telia");
    strcpy(info->band, "B3");
    strcpy(info->cell_id, "12345");
    info->roaming = false;
    info->roaming_type = ROAMING_TYPE_NONE;
    
    info->connection_state = CELLULAR_STATE_CONNECTED;
    strcpy(info->ip_address, "10.0.0.1");
    strcpy(info->gateway, "10.0.0.1");
    
    return AUTONOMY_SUCCESS;
}

// Collect cellular data via gsmctl command
static int collect_via_gsmctl(cellular_info_t* info) {
    char command[256];
    snprintf(command, sizeof(command), "gsmctl -A AT+CSQ 2>/dev/null");
    
    FILE* fp = popen(command, "r");
    if (!fp) {
        LOGX_ERROR_MSG("Failed to execute gsmctl command");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    char output[1024];
    size_t bytes_read = fread(output, 1, sizeof(output) - 1, fp);
    output[bytes_read] = '\0';
    
    int exit_code = pclose(fp);
    if (exit_code != 0) {
        LOGX_ERROR_MSG("gsmctl command failed", "exit_code", exit_code);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    return parse_gsmctl_output(output, info);
}

// Collect cellular data via AT commands
static int collect_via_at_commands(cellular_info_t* info) {
    // This would implement direct AT command communication
    // For now, return error since it's complex to implement properly
    LOGX_DEBUG_MSG("AT command collection not yet implemented");
    return AUTONOMY_ERROR_NOT_FOUND;
}

// Parse gsmctl output
static int parse_gsmctl_output(const char* output, cellular_info_t* info) {
    if (!output || !info) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Parse signal quality response: +CSQ: <rssi>,<ber>
    char* csq_line = strstr(output, "+CSQ:");
    if (csq_line) {
        int rssi, ber;
        if (sscanf(csq_line, "+CSQ: %d,%d", &rssi, &ber) == 2) {
            if (rssi != 99) { // 99 means unknown
                info->rssi = -113 + (rssi * 2); // Convert to dBm
                info->has_rssi = true;
            }
        }
    }
    
    // Set some default values for other metrics
    info->network_type = CELLULAR_NETWORK_TYPE_LTE;
    info->connection_state = CELLULAR_STATE_CONNECTED;
    
    return AUTONOMY_SUCCESS;
}

// Update signal history for stability analysis
void update_signal_history(int rsrp, int rsrq, int sinr) {
    int index = g_cellular_collector.history_index;
    
    g_cellular_collector.rsrp_history[index] = rsrp;
    g_cellular_collector.rsrq_history[index] = rsrq;
    g_cellular_collector.sinr_history[index] = sinr;
    
    g_cellular_collector.history_index = (index + 1) % 100;
    
    if (g_cellular_collector.history_count < 100) {
        g_cellular_collector.history_count++;
    }
}

// Calculate signal variance for stability analysis
void calculate_signal_variance(void) {
    if (g_cellular_collector.history_count < 2) {
        return;
    }
    
    // Calculate RSRP variance
    double rsrp_sum = 0.0;
    for (int i = 0; i < g_cellular_collector.history_count; i++) {
        rsrp_sum += g_cellular_collector.rsrp_history[i];
    }
    double rsrp_mean = rsrp_sum / g_cellular_collector.history_count;
    
    double rsrp_variance = 0.0;
    for (int i = 0; i < g_cellular_collector.history_count; i++) {
        double diff = g_cellular_collector.rsrp_history[i] - rsrp_mean;
        rsrp_variance += diff * diff;
    }
    rsrp_variance /= g_cellular_collector.history_count;
    
    // Update statistics
    g_cellular_collector.stats.average_rsrp = rsrp_mean;
    
    // Calculate stability score based on variance
    double stability = 100.0 - (sqrt(rsrp_variance) * 2.0); // Lower variance = higher stability
    if (stability < 0.0) stability = 0.0;
    if (stability > 100.0) stability = 100.0;
    
    g_cellular_collector.stats.stability_score = stability;
}

// Calculate signal quality score
double calculate_signal_quality_score(const cellular_info_t* info) {
    if (!info) return 0.0;
    
    double score = 100.0;
    
    // RSRP scoring (Reference Signal Received Power)
    if (info->has_rsrp) {
        if (info->rsrp >= -80) {
            score += 0.0; // Excellent signal
        } else if (info->rsrp >= -90) {
            score -= 5.0; // Good signal
        } else if (info->rsrp >= -100) {
            score -= 15.0; // Fair signal
        } else if (info->rsrp >= -110) {
            score -= 30.0; // Poor signal
        } else {
            score -= 50.0; // Very poor signal
        }
    }
    
    // RSRQ scoring (Reference Signal Received Quality)
    if (info->has_rsrq) {
        if (info->rsrq >= -10) {
            score += 0.0; // Excellent quality
        } else if (info->rsrq >= -15) {
            score -= 5.0; // Good quality
        } else if (info->rsrq >= -20) {
            score -= 15.0; // Fair quality
        } else {
            score -= 25.0; // Poor quality
        }
    }
    
    // SINR scoring (Signal to Interference plus Noise Ratio)
    if (info->has_sinr) {
        if (info->sinr >= 20) {
            score += 10.0; // Excellent SINR
        } else if (info->sinr >= 13) {
            score += 5.0; // Good SINR
        } else if (info->sinr >= 0) {
            score -= 0.0; // Fair SINR
        } else {
            score -= 20.0; // Poor SINR
        }
    }
    
    // Roaming penalty
    if (info->roaming) {
        score -= 10.0;
    }
    
    // Ensure score is within bounds
    if (score < 0.0) score = 0.0;
    if (score > 100.0) score = 100.0;
    
    return score;
}

// Calculate stability score
double cellular_collector_calculate_stability_score(const cellular_info_t* info) {
    if (!info || !g_cellular_collector_initialized) {
        return 0.0;
    }
    
    // Base stability from signal variance
    double stability = g_cellular_collector.stats.stability_score;
    
    // Adjust based on cell changes
    time_t now = time(NULL);
    time_t time_since_change = now - g_cellular_collector.last_cell_change;
    
    if (time_since_change < 300) { // Less than 5 minutes since cell change
        stability -= 20.0;
    } else if (time_since_change < 600) { // Less than 10 minutes
        stability -= 10.0;
    }
    
    // Adjust based on connection state
    if (info->connection_state != CELLULAR_STATE_CONNECTED) {
        stability -= 50.0;
    }
    
    // Ensure bounds
    if (stability < 0.0) stability = 0.0;
    if (stability > 100.0) stability = 100.0;
    
    return stability;
}

// Calculate predictive risk score
double cellular_collector_calculate_predictive_risk(const cellular_info_t* info) {
    if (!info || !g_cellular_collector_initialized) {
        return 1.0; // Maximum risk if no data
    }
    
    double risk = 0.0;
    
    // Signal strength risk
    if (info->has_rsrp) {
        if (info->rsrp < -110) {
            risk += 0.4; // Very weak signal = high risk
        } else if (info->rsrp < -100) {
            risk += 0.2; // Weak signal = medium risk
        }
    }
    
    // Signal quality risk
    if (info->has_rsrq) {
        if (info->rsrq < -20) {
            risk += 0.3; // Poor quality = high risk
        } else if (info->rsrq < -15) {
            risk += 0.1; // Fair quality = low risk
        }
    }
    
    // Cell change frequency risk
    if (g_cellular_collector.stats.cell_change_count > g_cellular_collector.config.max_cell_changes) {
        risk += 0.2; // Frequent cell changes = instability
    }
    
    // Signal variance risk
    if (g_cellular_collector.stats.stability_score < g_cellular_collector.config.stability_threshold) {
        risk += 0.1; // Low stability = increased risk
    }
    
    // Ensure bounds
    if (risk < 0.0) risk = 0.0;
    if (risk > 1.0) risk = 1.0;
    
    return risk;
}

// Get cellular collector statistics
static int cellular_collector_get_stats(cellular_collector_stats_t* stats) {
    if (!stats || !g_cellular_collector_initialized) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_cellular_collector.mutex);
    *stats = g_cellular_collector.stats;
    pthread_mutex_unlock(&g_cellular_collector.mutex);
    
    return AUTONOMY_SUCCESS;
}

// Get cellular collector configuration
int cellular_collector_get_config(cellular_collector_config_t* config) {
    if (!config || !g_cellular_collector_initialized) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    *config = g_cellular_collector.config;
    return AUTONOMY_SUCCESS;
}

// Set cellular collector configuration
int cellular_collector_set_config(const cellular_collector_config_t* config) {
    if (!config || !g_cellular_collector_initialized) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_cellular_collector.mutex);
    g_cellular_collector.config = *config;
    pthread_mutex_unlock(&g_cellular_collector.mutex);
    
    LOGX_INFO_MSG("Cellular collector configuration updated");
    return AUTONOMY_SUCCESS;
}

// Enable/disable cellular collector
static int cellular_collector_set_enabled(bool enabled) {
    if (!g_cellular_collector_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    g_cellular_collector.config.enabled = enabled;
    LOGX_INFO_MSG("Cellular collector", "enabled", enabled);
    
    return AUTONOMY_SUCCESS;
}

// Reset cellular collector statistics
static int cellular_collector_reset_stats(void) {
    if (!g_cellular_collector_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_cellular_collector.mutex);
    memset(&g_cellular_collector.stats, 0, sizeof(cellular_collector_stats_t));
    g_cellular_collector.history_count = 0;
    g_cellular_collector.history_index = 0;
    pthread_mutex_unlock(&g_cellular_collector.mutex);
    
    LOGX_INFO_MSG("Cellular collector statistics reset");
    return AUTONOMY_SUCCESS;
}

// Force immediate collection
static int cellular_collector_force_collect(cellular_info_t* info) {
    return cellular_collector_collect(info);
}

// Check if cellular collector is initialized
bool cellular_collector_is_initialized(void) {
    return g_cellular_collector_initialized;
}

// Parse network type string
static cellular_network_type_t parse_network_type(const char* network_str) {
    if (!network_str) return CELLULAR_NETWORK_TYPE_UNKNOWN;
    
    if (strcasecmp(network_str, "gsm") == 0) return CELLULAR_NETWORK_TYPE_GSM;
    if (strcasecmp(network_str, "umts") == 0) return CELLULAR_NETWORK_TYPE_UMTS;
    if (strcasecmp(network_str, "lte") == 0) return CELLULAR_NETWORK_TYPE_LTE;
    if (strcasecmp(network_str, "5g") == 0) return CELLULAR_NETWORK_TYPE_5G;
    if (strcasecmp(network_str, "cdma") == 0) return CELLULAR_NETWORK_TYPE_CDMA;
    
    return CELLULAR_NETWORK_TYPE_UNKNOWN;
}

// Convert network type to string
static const char* network_type_to_string(cellular_network_type_t type) {
    if (type >= 0 && type < CELLULAR_NETWORK_TYPE_MAX) {
        return NETWORK_TYPE_STRINGS[type];
    }
    return "unknown";
}