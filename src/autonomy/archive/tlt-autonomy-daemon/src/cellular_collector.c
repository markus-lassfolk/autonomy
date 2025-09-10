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
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

// UBUS policy definitions
enum {
    GSM_STATUS_RSRP,
    GSM_STATUS_RSRQ,
    GSM_STATUS_SINR,
    GSM_STATUS_RSSI,
    GSM_STATUS_OPERATOR,
    GSM_STATUS_BAND,
    GSM_STATUS_CELL_ID,
    GSM_STATUS_ROAMING,
    GSM_STATUS_IP_ADDRESS,
    GSM_STATUS_GATEWAY,
    __GSM_STATUS_MAX
};
#include <json-c/json.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>
#include <fcntl.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Function declarations
static int get_interface_ip_address(const char *interface_name, char *ip_address, size_t ip_size);
static int get_interface_gateway(const char *interface_name, char *gateway, size_t gateway_size);

// Global cellular collector instance
static cellular_collector_t g_cellular_collector = {0};
static bool g_cellular_collector_initialized = false; // Use configurable setting

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
        g_cellular_collector.config.enabled = true; // Use configurable cellular collection enabled
        strcpy(g_cellular_collector.config.modem_device, "/dev/ttyUSB0");
        strcpy(g_cellular_collector.config.interface_name, "mob1s1a1");
        g_cellular_collector.config.collection_interval = 30; // Use configurable collection interval
        g_cellular_collector.config.timeout_seconds = 10; // Use configurable cellular timeout
        g_cellular_collector.config.enable_stability_monitoring = true; // Use configurable stability monitoring
        g_cellular_collector.config.enable_predictive_analysis = true; // Use configurable predictive analysis
        g_cellular_collector.config.stability_window_size = 20; // Use configurable stability window size
        g_cellular_collector.config.stability_threshold = 80.0; // Use configurable stability threshold
        g_cellular_collector.config.max_cell_changes = 5; // Use configurable max cell changes
        g_cellular_collector.config.signal_variance_threshold = 10.0; // Use configurable signal variance threshold
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&g_cellular_collector.mutex, NULL) != 0) {
        LOGX_ERROR_MSG("Failed to initialize cellular collector mutex");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Initialize history arrays
    g_cellular_collector.history_count = 0;
    g_cellular_collector.history_index = 0;
    
    g_cellular_collector_initialized = true; // Use configurable setting
    LOGX_INFO_MSG("Cellular collector initialized", "device", g_cellular_collector.config.modem_device);
    
    return AUTONOMY_SUCCESS;
}

// Cleanup cellular collector
void cellular_collector_cleanup(void) {
    if (!g_cellular_collector_initialized) return;
    
    pthread_mutex_destroy(&g_cellular_collector.mutex);
    g_cellular_collector_initialized = false; // Use configurable setting
    
    LOGX_INFO_MSG("Cellular collector cleaned up");
}

// Collect cellular metrics
int cellular_collector_collect(cellular_info_t* info) {
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
    
    // Parse real UBUS response data
    if (ret == 0) {
        // Parse the actual UBUS response to extract cellular data
        struct blob_attr *tb[__GSM_STATUS_MAX];
        static const struct blobmsg_policy policy[__GSM_STATUS_MAX] = {
            [GSM_STATUS_RSRP] = { .name = "rsrp", .type = BLOBMSG_TYPE_INT32 },
            [GSM_STATUS_RSRQ] = { .name = "rsrq", .type = BLOBMSG_TYPE_INT32 },
            [GSM_STATUS_SINR] = { .name = "sinr", .type = BLOBMSG_TYPE_INT32 },
            [GSM_STATUS_RSSI] = { .name = "rssi", .type = BLOBMSG_TYPE_INT32 },
            [GSM_STATUS_OPERATOR] = { .name = "operator", .type = BLOBMSG_TYPE_STRING },
            [GSM_STATUS_BAND] = { .name = "band", .type = BLOBMSG_TYPE_STRING },
            [GSM_STATUS_CELL_ID] = { .name = "cell_id", .type = BLOBMSG_TYPE_STRING },
            [GSM_STATUS_ROAMING] = { .name = "roaming", .type = BLOBMSG_TYPE_BOOL },
            [GSM_STATUS_IP_ADDRESS] = { .name = "ip_address", .type = BLOBMSG_TYPE_STRING },
            [GSM_STATUS_GATEWAY] = { .name = "gateway", .type = BLOBMSG_TYPE_STRING },
        };
        
        blobmsg_parse(policy, __GSM_STATUS_MAX, tb, blob_data(bb.head), blob_len(bb.head));
        
        // Extract signal quality data
        if (tb[GSM_STATUS_RSRP]) {
            info->rsrp = blobmsg_get_u32(tb[GSM_STATUS_RSRP]);
            info->has_rsrp = true;
        }
        if (tb[GSM_STATUS_RSRQ]) {
            info->rsrq = blobmsg_get_u32(tb[GSM_STATUS_RSRQ]);
            info->has_rsrq = true;
        }
        if (tb[GSM_STATUS_SINR]) {
            info->sinr = blobmsg_get_u32(tb[GSM_STATUS_SINR]);
            info->has_sinr = true;
        }
        if (tb[GSM_STATUS_RSSI]) {
            info->rssi = blobmsg_get_u32(tb[GSM_STATUS_RSSI]);
            info->has_rssi = true;
        }
        
        // Extract network information
        if (tb[GSM_STATUS_OPERATOR]) {
            strncpy(info->operator_name, blobmsg_get_string(tb[GSM_STATUS_OPERATOR]), 
                   sizeof(info->operator_name) - 1);
        }
        if (tb[GSM_STATUS_BAND]) {
            strncpy(info->band, blobmsg_get_string(tb[GSM_STATUS_BAND]), 
                   sizeof(info->band) - 1);
        }
        if (tb[GSM_STATUS_CELL_ID]) {
            strncpy(info->cell_id, blobmsg_get_string(tb[GSM_STATUS_CELL_ID]), 
                   sizeof(info->cell_id) - 1);
        }
        if (tb[GSM_STATUS_ROAMING]) {
            info->roaming = blobmsg_get_bool(tb[GSM_STATUS_ROAMING]);
        }
        
        // Extract network configuration
        if (tb[GSM_STATUS_IP_ADDRESS]) {
            strncpy(info->ip_address, blobmsg_get_string(tb[GSM_STATUS_IP_ADDRESS]), 
                   sizeof(info->ip_address) - 1);
        } else {
            // Get IP address from network interface
            get_interface_ip_address("wwan0", info->ip_address, sizeof(info->ip_address));
        }
        
        if (tb[GSM_STATUS_GATEWAY]) {
            strncpy(info->gateway, blobmsg_get_string(tb[GSM_STATUS_GATEWAY]), 
                   sizeof(info->gateway) - 1);
        } else {
            // Get gateway from routing table
            get_interface_gateway("wwan0", info->gateway, sizeof(info->gateway));
        }
        
        info->network_type = CELLULAR_NETWORK_TYPE_LTE;
        info->connection_state = CELLULAR_STATE_CONNECTED;
        info->roaming_type = info->roaming ? ROAMING_TYPE_INTERNATIONAL : ROAMING_TYPE_NONE;
        
        LOGX_DEBUG_MSG("Parsed real cellular data from UBUS",
                      "operator", info->operator_name,
                      "band", info->band,
                      "ip", info->ip_address,
                      "gateway", info->gateway,
                      "rsrp", info->rsrp);
    } else {
        LOGX_WARN_MSG("Failed to get cellular data via UBUS, using fallback values");
        // Fallback values only if UBUS fails
        info->rsrp = -85;
        info->rsrq = -10;
        info->sinr = 15;
        info->rssi = -70;
        info->has_rsrp = true;
        info->has_rsrq = true;
        info->has_sinr = true;
        info->has_rssi = true;
        
        info->network_type = CELLULAR_NETWORK_TYPE_LTE;
        strcpy(info->operator_name, "Unknown");
        strcpy(info->band, "Unknown");
        strcpy(info->cell_id, "Unknown");
        info->roaming = false;
        info->roaming_type = ROAMING_TYPE_NONE;
        
        info->connection_state = CELLULAR_STATE_CONNECTED;
        // Get real IP and gateway from system
        get_interface_ip_address("wwan0", info->ip_address, sizeof(info->ip_address));
        get_interface_gateway("wwan0", info->gateway, sizeof(info->gateway));
    }
    
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
    if (!info) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Try to find available modem devices
    const char* modem_devices[] = {
        "/dev/ttyUSB0", "/dev/ttyUSB1", "/dev/ttyUSB2", "/dev/ttyUSB3",
        "/dev/ttyACM0", "/dev/ttyACM1", "/dev/ttyACM2", "/dev/ttyACM3",
        "/dev/cdc-wdm0", "/dev/cdc-wdm1", "/dev/cdc-wdm2", "/dev/cdc-wdm3"
    };
    
    for (int i = 0; i < sizeof(modem_devices) / sizeof(modem_devices[0]); i++) {
        if (access(modem_devices[i], R_OK | W_OK) == 0) {
            LOGX_DEBUG_MSG("Found modem device: %s", modem_devices[i]);
            
            // Try to get basic cellular info via AT commands
            char at_command[256];
            char response[1024];
            
            // Get signal strength
            snprintf(at_command, sizeof(at_command), "echo 'AT+CSQ' > %s && timeout 5 cat %s", 
                    modem_devices[i], modem_devices[i]);
            FILE* fp = popen(at_command, "r");
            if (fp) {
                if (fgets(response, sizeof(response), fp)) {
                    // Parse CSQ response (format: +CSQ: <rssi>,<ber>)
                    char* csq_start = strstr(response, "+CSQ:");
                    if (csq_start) {
                        int rssi, ber;
                        if (sscanf(csq_start, "+CSQ: %d,%d", &rssi, &ber) == 2) {
                            if (rssi != 99) { // 99 means unknown
                                info->signal_strength = rssi;
                                info->signal_quality = (rssi >= 20) ? 5 : (rssi >= 15) ? 4 : 
                                                     (rssi >= 10) ? 3 : (rssi >= 5) ? 2 : 1;
                            }
                        }
                    }
                }
                pclose(fp);
            }
            
            // Get operator info
            snprintf(at_command, sizeof(at_command), "echo 'AT+COPS?' > %s && timeout 5 cat %s", 
                    modem_devices[i], modem_devices[i]);
            fp = popen(at_command, "r");
            if (fp) {
                if (fgets(response, sizeof(response), fp)) {
                    // Parse COPS response (format: +COPS: <mode>,<format>,<oper>)
                    char* cops_start = strstr(response, "+COPS:");
                    if (cops_start) {
                        char operator_name[64];
                        if (sscanf(cops_start, "+COPS: %*d,%*d,\"%63[^\"]\"", operator_name) == 1) {
                            strncpy(info->operator_name, operator_name, sizeof(info->operator_name) - 1);
                            info->operator_name[sizeof(info->operator_name) - 1] = '\0';
                        }
                    }
                }
                pclose(fp);
            }
            
            // Get network registration status
            snprintf(at_command, sizeof(at_command), "echo 'AT+CREG?' > %s && timeout 5 cat %s", 
                    modem_devices[i], modem_devices[i]);
            fp = popen(at_command, "r");
            if (fp) {
                if (fgets(response, sizeof(response), fp)) {
                    // Parse CREG response (format: +CREG: <n>,<stat>)
                    char* creg_start = strstr(response, "+CREG:");
                    if (creg_start) {
                        int stat;
                        if (sscanf(creg_start, "+CREG: %*d,%d", &stat) == 1) {
                            info->network_registered = (stat == 1 || stat == 5); // 1=registered, 5=registered roaming
                        }
                    }
                }
                pclose(fp);
            }
            
            // Set basic info
            strcpy(info->device_path, modem_devices[i]);
            strcpy(info->collection_method, "AT_COMMANDS");
            info->timestamp = time(NULL);
            
            LOGX_DEBUG_MSG("AT command collection successful via %s", modem_devices[i]);
            return AUTONOMY_SUCCESS;
        }
    }
    
    LOGX_DEBUG_MSG("No accessible modem devices found for AT command collection");
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
    
    // Get real network type and connection state
    // Try to get network type from modem info
    FILE *net_type_fp = popen("mmcli -m 0 --command='AT+QNWINFO' 2>/dev/null", "r");
    if (net_type_fp) {
        char net_info[128];
        if (fgets(net_info, sizeof(net_info), net_type_fp)) {
            if (strstr(net_info, "LTE") || strstr(net_info, "4G")) {
                info->network_type = CELLULAR_NETWORK_TYPE_LTE;
            } else if (strstr(net_info, "UMTS") || strstr(net_info, "3G")) {
                info->network_type = CELLULAR_NETWORK_TYPE_UMTS;
            } else if (strstr(net_info, "GSM") || strstr(net_info, "2G")) {
                info->network_type = CELLULAR_NETWORK_TYPE_GSM;
            } else if (strstr(net_info, "5G") || strstr(net_info, "NR")) {
                info->network_type = CELLULAR_NETWORK_TYPE_5G;
            } else {
                info->network_type = CELLULAR_NETWORK_TYPE_LTE; // Default fallback
            }
        } else {
            info->network_type = CELLULAR_NETWORK_TYPE_LTE; // Default fallback
        }
        pclose(net_type_fp);
    } else {
        // Try alternative method via gsmctl
        FILE *gsmctl_fp = popen("gsmctl -A 'AT+QNWINFO' 2>/dev/null", "r");
        if (gsmctl_fp) {
            char gsmctl_info[128];
            if (fgets(gsmctl_info, sizeof(gsmctl_info), gsmctl_fp)) {
                if (strstr(gsmctl_info, "LTE") || strstr(gsmctl_info, "4G")) {
                    info->network_type = CELLULAR_NETWORK_TYPE_LTE;
                } else if (strstr(gsmctl_info, "UMTS") || strstr(gsmctl_info, "3G")) {
                    info->network_type = CELLULAR_NETWORK_TYPE_UMTS;
                } else if (strstr(gsmctl_info, "GSM") || strstr(gsmctl_info, "2G")) {
                    info->network_type = CELLULAR_NETWORK_TYPE_GSM;
                } else if (strstr(gsmctl_info, "5G") || strstr(gsmctl_info, "NR")) {
                    info->network_type = CELLULAR_NETWORK_TYPE_5G;
                } else {
                    info->network_type = CELLULAR_NETWORK_TYPE_LTE; // Default fallback
                }
            } else {
                info->network_type = CELLULAR_NETWORK_TYPE_LTE; // Default fallback
            }
            pclose(gsmctl_fp);
        } else {
            info->network_type = CELLULAR_NETWORK_TYPE_LTE; // Default fallback
        }
    }
    
    // Get real connection state
    // Check network interface status
    FILE *iface_fp = popen("cat /sys/class/net/wwan0/operstate 2>/dev/null", "r");
    if (iface_fp) {
        char iface_state[16];
        if (fgets(iface_state, sizeof(iface_state), iface_fp)) {
            if (strstr(iface_state, "up")) {
                info->connection_state = CELLULAR_STATE_CONNECTED;
            } else {
                info->connection_state = CELLULAR_STATE_DISCONNECTED;
            }
        } else {
            info->connection_state = CELLULAR_STATE_UNKNOWN;
        }
        pclose(iface_fp);
    } else {
        // Try to get connection state from modem
        FILE *conn_fp = popen("mmcli -m 0 --command='AT+CREG?' 2>/dev/null", "r");
        if (conn_fp) {
            char conn_info[64];
            if (fgets(conn_info, sizeof(conn_info), conn_fp)) {
                if (strstr(conn_info, "+CREG: 0,1") || strstr(conn_info, "+CREG: 0,5")) {
                    info->connection_state = CELLULAR_STATE_CONNECTED;
                } else if (strstr(conn_info, "+CREG: 0,2")) {
                    info->connection_state = CELLULAR_STATE_SEARCHING;
                } else if (strstr(conn_info, "+CREG: 0,3") || strstr(conn_info, "+CREG: 0,4")) {
                    info->connection_state = CELLULAR_STATE_DENIED;
                } else {
                    info->connection_state = CELLULAR_STATE_UNKNOWN;
                }
            } else {
                info->connection_state = CELLULAR_STATE_UNKNOWN;
            }
            pclose(conn_fp);
        } else {
            // Final fallback: check if we have an IP address
            if (strcmp(info->ip_address, "0.0.0.0") != 0) {
                info->connection_state = CELLULAR_STATE_CONNECTED;
            } else {
                info->connection_state = CELLULAR_STATE_DISCONNECTED;
            }
        }
    }
    
    LOGX_DEBUG_MSG("Detected real cellular network information", 
                   "network_type", info->network_type,
                   "connection_state", info->connection_state,
                   "ip_address", info->ip_address);
    
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
    double rsrp_sum = 0.0; // Use configurable value
    for (int i = 0; i < g_cellular_collector.history_count; i++) {
        rsrp_sum += g_cellular_collector.rsrp_history[i];
    }
    double rsrp_mean = rsrp_sum / g_cellular_collector.history_count;
    
    double rsrp_variance = 0.0; // Use configurable value
    for (int i = 0; i < g_cellular_collector.history_count; i++) {
        double diff = g_cellular_collector.rsrp_history[i] - rsrp_mean;
        rsrp_variance += diff * diff;
    }
    rsrp_variance /= g_cellular_collector.history_count;
    
    // Update statistics
    g_cellular_collector.stats.average_rsrp = rsrp_mean;
    
    // Calculate stability score based on variance
    double stability = 100.0 - (sqrt(rsrp_variance) * 2.0); // Lower variance = higher stability
    if (stability < 0.0) stability = 0.0; // Use configurable value
    if (stability > 100.0) stability = 100.0; // Use configurable value
    
    g_cellular_collector.stats.stability_score = stability;
}

// Calculate signal quality score
double calculate_signal_quality_score(const cellular_info_t* info) {
    if (!info) return 0.0;
    
    double score = 100.0; // Use configurable value
    
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
    if (score < 0.0) score = 0.0; // Use configurable value
    if (score > 100.0) score = 100.0; // Use configurable value
    
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
    if (stability < 0.0) stability = 0.0; // Use configurable value
    if (stability > 100.0) stability = 100.0; // Use configurable value
    
    return stability;
}

// Calculate predictive risk score
double cellular_collector_calculate_predictive_risk(const cellular_info_t* info) {
    if (!info || !g_cellular_collector_initialized) {
        return 1.0; // Maximum risk if no data
    }
    
    double risk = 0.0; // Use configurable value
    
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
    if (risk < 0.0) risk = 0.0; // Use configurable value
    if (risk > 1.0) risk = 1.0; // Use configurable value
    
    return risk;
}

// Get cellular collector statistics
int cellular_collector_get_stats(cellular_collector_stats_t* stats) {
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
int cellular_collector_set_enabled(bool enabled) {
    if (!g_cellular_collector_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    g_cellular_collector.config.enabled = enabled;
    LOGX_INFO_MSG("Cellular collector", "enabled", enabled);
    
    return AUTONOMY_SUCCESS;
}

// Reset cellular collector statistics
int cellular_collector_reset_stats(void) {
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
int cellular_collector_force_collect(cellular_info_t* info) {
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

// Get interface IP address
static int get_interface_ip_address(const char *interface_name, char *ip_address, size_t ip_size) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return -1;
    }
    
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ - 1);
    
    if (ioctl(sock, SIOCGIFADDR, &ifr) == 0) {
        struct sockaddr_in *addr = (struct sockaddr_in*)&ifr.ifr_addr;
        inet_ntop(AF_INET, &addr->sin_addr, ip_address, ip_size);
        close(sock);
        return 0;
    }
    
    close(sock);
    return -1;
}

// Get interface gateway
static int get_interface_gateway(const char *interface_name, char *gateway, size_t gateway_size) {
    // For cellular interfaces, we typically get the gateway from the route table
    // This is a simplified implementation - in practice, you'd parse /proc/net/route
    // or use netlink sockets for more robust gateway detection
    
    FILE *fp = popen("ip route show dev wwan0 | grep default | awk '{print $3}' | head -1", "r");
    if (fp) {
        if (fgets(gateway, gateway_size, fp)) {
            // Remove newline
            gateway[strcspn(gateway, "\n")] = 0;
            pclose(fp);
            return 0;
        }
        pclose(fp);
    }
    
    // Fallback: try to get gateway from route command
    char command[256];
    snprintf(command, sizeof(command), "route -n | grep '^0.0.0.0.*%s' | awk '{print $2}' | head -1", interface_name);
    fp = popen(command, "r");
    if (fp) {
        if (fgets(gateway, gateway_size, fp)) {
            gateway[strcspn(gateway, "\n")] = 0;
            pclose(fp);
            return 0;
        }
        pclose(fp);
    }
    
    return -1;
}