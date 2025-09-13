#include "cellular_collector.h"
#include "../logging/logx.h"
#include "../../core/types.h"
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
#include <fcntl.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/time.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// GSM UBUS response parsing policy
enum {
    GSM_STATUS_SIGNAL,
    GSM_STATUS_RSRP,
    GSM_STATUS_RSRQ,
    GSM_STATUS_SINR,
    GSM_STATUS_RSSI,
    GSM_STATUS_NETWORK_TYPE,
    GSM_STATUS_OPERATOR,
    GSM_STATUS_BAND,
    GSM_STATUS_CELL_ID,
    GSM_STATUS_IP,
    GSM_STATUS_GATEWAY,
    GSM_STATUS_CONNECTED,
    GSM_STATUS_ROAMING,
    __GSM_STATUS_MAX
};

static const struct blobmsg_policy gsm_status_policy[__GSM_STATUS_MAX] = {
    [GSM_STATUS_SIGNAL] = { .name = "signal", .type = BLOBMSG_TYPE_TABLE },
    [GSM_STATUS_RSRP] = { .name = "rsrp", .type = BLOBMSG_TYPE_INT32 },
    [GSM_STATUS_RSRQ] = { .name = "rsrq", .type = BLOBMSG_TYPE_INT32 },
    [GSM_STATUS_SINR] = { .name = "sinr", .type = BLOBMSG_TYPE_INT32 },
    [GSM_STATUS_RSSI] = { .name = "rssi", .type = BLOBMSG_TYPE_INT32 },
    [GSM_STATUS_NETWORK_TYPE] = { .name = "network_type", .type = BLOBMSG_TYPE_STRING },
    [GSM_STATUS_OPERATOR] = { .name = "operator", .type = BLOBMSG_TYPE_STRING },
    [GSM_STATUS_BAND] = { .name = "band", .type = BLOBMSG_TYPE_STRING },
    [GSM_STATUS_CELL_ID] = { .name = "cell_id", .type = BLOBMSG_TYPE_STRING },
    [GSM_STATUS_IP] = { .name = "ip", .type = BLOBMSG_TYPE_STRING },
    [GSM_STATUS_GATEWAY] = { .name = "gateway", .type = BLOBMSG_TYPE_STRING },
    [GSM_STATUS_CONNECTED] = { .name = "connected", .type = BLOBMSG_TYPE_BOOL },
    [GSM_STATUS_ROAMING] = { .name = "roaming", .type = BLOBMSG_TYPE_BOOL },
};

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
static void update_signal_history(int rsrp, int rsrq, int sinr);
static void calculate_signal_variance(void);
static double calculate_signal_quality_score(const cellular_info_t* info);
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
        // Default configuration using UCI config
        g_cellular_collector.config.enabled = true; // Use configurable cellular collection enabled
        strncpy(g_cellular_collector.config.modem_device, "/dev/ttyUSB0", sizeof(g_cellular_collector.config.modem_device) - 1);
        g_cellular_collector.config.modem_device[sizeof(g_cellular_collector.config.modem_device) - 1] = '\0';
        strncpy(g_cellular_collector.config.interface_name, "mob1s1a1", sizeof(g_cellular_collector.config.interface_name) - 1);
        g_cellular_collector.config.interface_name[sizeof(g_cellular_collector.config.interface_name) - 1] = '\0';
        g_cellular_collector.config.collection_interval = g_config.network_check_interval;
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
    strncpy(info->interface_name, g_cellular_collector.config.interface_name, sizeof(info->interface_name) - 1);
    info->interface_name[sizeof(info->interface_name) - 1] = '\0';
    strncpy(info->modem_device, g_cellular_collector.config.modem_device, sizeof(info->modem_device) - 1);
    info->modem_device[sizeof(info->modem_device) - 1] = '\0';
    
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
            strncpy(g_cellular_collector.last_cell_id, info->cell_id, sizeof(g_cellular_collector.last_cell_id) - 1);
            g_cellular_collector.last_cell_id[sizeof(g_cellular_collector.last_cell_id) - 1] = '\0';
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

    // Set up callback to receive response
    struct blob_attr *tb[__GSM_STATUS_MAX];
    int callback_called = 0;

    void gsm_status_callback(struct ubus_request *req, int type, struct blob_attr *msg) {
        if (!msg || type != 0) {
            return;
        }

        callback_called = 1;

        // Parse the GSM status response
        blobmsg_parse(gsm_status_policy, __GSM_STATUS_MAX, tb, blob_data(msg), blob_len(msg));

        // Extract signal information
        if (tb[GSM_STATUS_RSRP]) {
            info->rsrp = blobmsg_get_u32(tb[GSM_STATUS_RSRP]);
            info->has_rsrp = true;
        } else {
            info->rsrp = 0;
            info->has_rsrp = false;
        }

        if (tb[GSM_STATUS_RSRQ]) {
            info->rsrq = blobmsg_get_u32(tb[GSM_STATUS_RSRQ]);
            info->has_rsrq = true;
        } else {
            info->rsrq = 0;
            info->has_rsrq = false;
        }

        if (tb[GSM_STATUS_SINR]) {
            info->sinr = blobmsg_get_u32(tb[GSM_STATUS_SINR]);
            info->has_sinr = true;
        } else {
            info->sinr = 0;
            info->has_sinr = false;
        }

        if (tb[GSM_STATUS_RSSI]) {
            info->rssi = blobmsg_get_u32(tb[GSM_STATUS_RSSI]);
            info->has_rssi = true;
        } else {
            info->rssi = 0;
            info->has_rssi = false;
        }

        // Extract network information
        if (tb[GSM_STATUS_NETWORK_TYPE]) {
            const char* network_str = blobmsg_get_string(tb[GSM_STATUS_NETWORK_TYPE]);
            if (strcmp(network_str, "LTE") == 0) {
                info->network_type = CELLULAR_NETWORK_TYPE_LTE;
            } else if (strcmp(network_str, "UMTS") == 0) {
                info->network_type = CELLULAR_NETWORK_TYPE_UMTS;
            } else if (strcmp(network_str, "GSM") == 0) {
                info->network_type = CELLULAR_NETWORK_TYPE_GSM;
            } else {
                info->network_type = CELLULAR_NETWORK_TYPE_UNKNOWN;
            }
        } else {
            info->network_type = CELLULAR_NETWORK_TYPE_UNKNOWN;
        }

        if (tb[GSM_STATUS_OPERATOR]) {
            strncpy(info->operator_name, blobmsg_get_string(tb[GSM_STATUS_OPERATOR]),
                    sizeof(info->operator_name) - 1);
        } else {
            strncpy(info->operator_name, "Unknown", sizeof(info->operator_name) - 1);
            info->operator_name[sizeof(info->operator_name) - 1] = '\0';
        }

        if (tb[GSM_STATUS_BAND]) {
            strncpy(info->band, blobmsg_get_string(tb[GSM_STATUS_BAND]),
                    sizeof(info->band) - 1);
        } else {
            strncpy(info->band, "Unknown", sizeof(info->band) - 1);
            info->band[sizeof(info->band) - 1] = '\0';
        }

        if (tb[GSM_STATUS_CELL_ID]) {
            strncpy(info->cell_id, blobmsg_get_string(tb[GSM_STATUS_CELL_ID]),
                    sizeof(info->cell_id) - 1);
        } else {
            strncpy(info->cell_id, "Unknown", sizeof(info->cell_id) - 1);
            info->cell_id[sizeof(info->cell_id) - 1] = '\0';
        }

        if (tb[GSM_STATUS_IP]) {
            strncpy(info->ip_address, blobmsg_get_string(tb[GSM_STATUS_IP]),
                    sizeof(info->ip_address) - 1);
        } else {
            // Try to get real IP address from network interface
            FILE *ip_fp = popen("ip addr show wwan0 2>/dev/null | grep 'inet ' | awk '{print $2}' | cut -d'/' -f1", "r");
            if (ip_fp) {
                if (fgets(info->ip_address, sizeof(info->ip_address), ip_fp)) {
                    // Remove newline
                    info->ip_address[strcspn(info->ip_address, "\n")] = '\0';
                } else {
                    strncpy(info->ip_address, "0.0.0.0", sizeof(info->ip_address) - 1);
                    info->ip_address[sizeof(info->ip_address) - 1] = '\0';
                }
                pclose(ip_fp);
            } else {
                strncpy(info->ip_address, "0.0.0.0", sizeof(info->ip_address) - 1);
                info->ip_address[sizeof(info->ip_address) - 1] = '\0';
            }
        }

        if (tb[GSM_STATUS_GATEWAY]) {
            strncpy(info->gateway, blobmsg_get_string(tb[GSM_STATUS_GATEWAY]),
                    sizeof(info->gateway) - 1);
        } else {
            // Try to get real gateway from routing table
            FILE *gw_fp = popen("ip route show dev wwan0 2>/dev/null | grep default | awk '{print $3}' | head -1", "r");
            if (gw_fp) {
                if (fgets(info->gateway, sizeof(info->gateway), gw_fp)) {
                    // Remove newline
                    info->gateway[strcspn(info->gateway, "\n")] = '\0';
                } else {
                    strncpy(info->gateway, "0.0.0.0", sizeof(info->gateway) - 1);
                    info->gateway[sizeof(info->gateway) - 1] = '\0';
                }
                pclose(gw_fp);
            } else {
                strncpy(info->gateway, "0.0.0.0", sizeof(info->gateway) - 1);
                info->gateway[sizeof(info->gateway) - 1] = '\0';
            }
        }

        if (tb[GSM_STATUS_CONNECTED]) {
            info->connection_state = blobmsg_get_bool(tb[GSM_STATUS_CONNECTED]) ?
                                    CELLULAR_STATE_CONNECTED : CELLULAR_STATE_DISCONNECTED;
        } else {
            info->connection_state = CELLULAR_STATE_UNKNOWN;
        }

        if (tb[GSM_STATUS_ROAMING]) {
            info->roaming = blobmsg_get_bool(tb[GSM_STATUS_ROAMING]);
            info->roaming_type = info->roaming ? ROAMING_TYPE_DOMESTIC : ROAMING_TYPE_NONE;
        } else {
            info->roaming = false;
            info->roaming_type = ROAMING_TYPE_NONE;
        }
    }

    int ret = ubus_invoke(ctx, id, "status", bb.head, gsm_status_callback, NULL, 5000);

    blob_buf_free(&bb);
    ubus_free(ctx);

    if (ret != 0) {
        LOGX_ERROR_MSG("Failed to call GSM status via UBUS", "error", ubus_strerror(ret));
        return AUTONOMY_ERROR_SYSTEM;
    }

    if (!callback_called) {
        LOGX_WARN_MSG("GSM status UBUS call completed but no response received");
        return AUTONOMY_ERROR_NOT_FOUND;
    }

    LOGX_DEBUG_MSG("Successfully collected cellular data via UBUS",
                   "operator", info->operator_name,
                   "network_type", info->network_type,
                   "rsrp", info->rsrp,
                   "connected", info->connection_state == CELLULAR_STATE_CONNECTED);

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
// Send AT command and read response
static int send_at_command(int fd, const char* command, char* response, int response_size, int timeout_ms) {
    if (!command || !response || fd < 0) {
        return -1;
    }
    
    // Clear any pending data
    tcflush(fd, TCIOFLUSH);
    
    // Send command
    char cmd_with_cr[256];
    snprintf(cmd_with_cr, sizeof(cmd_with_cr), "%s\r\n", command);
    
    if (write(fd, cmd_with_cr, strlen(cmd_with_cr)) < 0) {
        LOGX_ERROR_MSG("Failed to write AT command: %s", strerror(errno));
        return -1;
    }
    
    // Read response with timeout
    fd_set readfds;
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    
    if (select(fd + 1, &readfds, NULL, NULL, &timeout) <= 0) {
        LOGX_DEBUG_MSG("AT command timeout or error");
        return -1;
    }
    
    // Read response
    int total_read = 0;
    int bytes_read;
    while (total_read < response_size - 1) {
        bytes_read = read(fd, response + total_read, response_size - total_read - 1);
        if (bytes_read <= 0) break;
        total_read += bytes_read;
        
        // Check for OK or ERROR response
        if (strstr(response, "OK\r\n") || strstr(response, "ERROR\r\n")) {
            break;
        }
    }
    
    response[total_read] = '\0';
    return total_read > 0 ? 0 : -1;
}

static int collect_via_at_commands(cellular_info_t* info) {
    if (!info) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Common modem device paths
    const char* modem_devices[] = {
        "/dev/ttyUSB0",
        "/dev/ttyUSB1",
        "/dev/ttyUSB2",
        "/dev/ttyUSB3",
        "/dev/ttyACM0",
        "/dev/ttyACM1",
        "/dev/cdc-wdm0",
        "/dev/ttyS0"
    };
    
    int fd = -1;
    char response[1024];
    
    // Try to open modem device
    for (int i = 0; i < sizeof(modem_devices)/sizeof(modem_devices[0]); i++) {
        fd = open(modem_devices[i], O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd >= 0) {
            LOGX_DEBUG_MSG("Opened modem device: %s", modem_devices[i]);
            
            // Configure serial port
            struct termios tty;
            if (tcgetattr(fd, &tty) == 0) {
                cfsetospeed(&tty, B115200);
                cfsetispeed(&tty, B115200);
                
                tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;  // 8-bit chars
                tty.c_iflag &= ~IGNBRK;                       // disable break processing
                tty.c_lflag = 0;                              // no signaling chars, no echo
                tty.c_oflag = 0;                              // no remapping, no delays
                tty.c_cc[VMIN]  = 0;                          // read doesn't block
                tty.c_cc[VTIME] = 5;                          // 0.5 seconds read timeout
                
                tty.c_iflag &= ~(IXON | IXOFF | IXANY);       // shut off xon/xoff ctrl
                tty.c_cflag |= (CLOCAL | CREAD);              // ignore modem controls
                tty.c_cflag &= ~(PARENB | PARODD);            // shut off parity
                tty.c_cflag &= ~CSTOPB;
                tty.c_cflag &= ~CRTSCTS;
                
                tcsetattr(fd, TCSANOW, &tty);
            }
            
            // Test with AT command
            if (send_at_command(fd, "AT", response, sizeof(response), 1000) == 0) {
                break; // Found working modem
            }
            
            close(fd);
            fd = -1;
        }
    }
    
    if (fd < 0) {
        LOGX_DEBUG_MSG("No cellular modem found on common device paths");
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    // Get manufacturer info
    if (send_at_command(fd, "AT+CGMI", response, sizeof(response), 2000) == 0) {
        char* start = strstr(response, "\r\n");
        if (start) {
            start += 2;
            char* end = strstr(start, "\r\n");
            if (end) {
                *end = '\0';
                strncpy(info->manufacturer, start, sizeof(info->manufacturer) - 1);
            }
        }
    }
    
    // Get model info
    if (send_at_command(fd, "AT+CGMM", response, sizeof(response), 2000) == 0) {
        char* start = strstr(response, "\r\n");
        if (start) {
            start += 2;
            char* end = strstr(start, "\r\n");
            if (end) {
                *end = '\0';
                strncpy(info->model, start, sizeof(info->model) - 1);
            }
        }
    }
    
    // Get IMEI
    if (send_at_command(fd, "AT+CGSN", response, sizeof(response), 2000) == 0) {
        char* start = strstr(response, "\r\n");
        if (start) {
            start += 2;
            char* end = strstr(start, "\r\n");
            if (end) {
                *end = '\0';
                strncpy(info->imei, start, sizeof(info->imei) - 1);
            }
        }
    }
    
    // Get signal quality
    if (send_at_command(fd, "AT+CSQ", response, sizeof(response), 2000) == 0) {
        int rssi, ber;
        if (sscanf(response, "\r\n+CSQ: %d,%d", &rssi, &ber) == 2) {
            if (rssi != 99) { // 99 means unknown
                info->rssi = -113 + (rssi * 2); // Convert to dBm
                info->has_rssi = true;
            }
        }
    }
    
    // Get extended signal quality (for LTE)
    if (send_at_command(fd, "AT+CESQ", response, sizeof(response), 2000) == 0) {
        int rxlev, ber, rscp, ecno, rsrq, rsrp;
        if (sscanf(response, "\r\n+CESQ: %d,%d,%d,%d,%d,%d", 
                   &rxlev, &ber, &rscp, &ecno, &rsrq, &rsrp) >= 5) {
            if (rsrp != 255) { // 255 means unknown
                info->rsrp = -140 + rsrp; // Convert to dBm
                info->has_rsrp = true;
            }
            if (rsrq != 255) {
                info->rsrq = -20 + (rsrq * 0.5); // Convert to dB
                info->has_rsrq = true;
            }
        }
    }
    
    // Get network registration status
    if (send_at_command(fd, "AT+CREG?", response, sizeof(response), 2000) == 0) {
        int n, stat;
        if (sscanf(response, "\r\n+CREG: %d,%d", &n, &stat) >= 2) {
            switch(stat) {
                case 1:
                case 5:
                    info->connection_state = CELLULAR_STATE_CONNECTED;
                    break;
                case 2:
                    info->connection_state = CELLULAR_STATE_SEARCHING;
                    break;
                default:
                    info->connection_state = CELLULAR_STATE_DISCONNECTED;
            }
        }
    }
    
    // Get operator name
    if (send_at_command(fd, "AT+COPS?", response, sizeof(response), 2000) == 0) {
        int mode, format, oper_code;
        char oper_name[64];
        if (sscanf(response, "\r\n+COPS: %d,%d,\"%63[^\"]\",%d", 
                   &mode, &format, oper_name, &oper_code) >= 3) {
            strncpy(info->operator_name, oper_name, sizeof(info->operator_name) - 1);
            
            // Determine network type based on access technology
            switch(oper_code) {
                case 0: // GSM
                case 1: // GSM Compact
                    info->network_type = CELLULAR_NETWORK_TYPE_2G;
                    break;
                case 2: // UTRAN
                case 3: // GSM w/EGPRS
                case 4: // UTRAN w/HSDPA
                case 5: // UTRAN w/HSUPA
                case 6: // UTRAN w/HSDPA and HSUPA
                    info->network_type = CELLULAR_NETWORK_TYPE_3G;
                    break;
                case 7: // E-UTRAN (LTE)
                    info->network_type = CELLULAR_NETWORK_TYPE_LTE;
                    break;
                default:
                    info->network_type = CELLULAR_NETWORK_TYPE_UNKNOWN;
            }
        }
    }
    
    // Get SIM card status
    if (send_at_command(fd, "AT+CPIN?", response, sizeof(response), 2000) == 0) {
        if (strstr(response, "READY")) {
            strncpy(info->sim_status, "READY", sizeof(info->sim_status) - 1);
            
            // Get ICCID if SIM is ready
            if (send_at_command(fd, "AT+CCID", response, sizeof(response), 2000) == 0) {
                char* start = strstr(response, "\r\n");
                if (start) {
                    start += 2;
                    char* end = strstr(start, "\r\n");
                    if (end) {
                        *end = '\0';
                        strncpy(info->iccid, start, sizeof(info->iccid) - 1);
                    }
                }
            }
        } else if (strstr(response, "SIM PIN")) {
            strncpy(info->sim_status, "PIN_REQUIRED", sizeof(info->sim_status) - 1);
        } else {
            strncpy(info->sim_status, "ERROR", sizeof(info->sim_status) - 1);
        }
    }
    
    // Get cell info (serving cell)
    if (send_at_command(fd, "AT+QENG=\"servingcell\"", response, sizeof(response), 2000) == 0) {
        // Parse Quectel-specific serving cell info
        char* line = strstr(response, "+QENG:");
        if (line) {
            int mcc, mnc, cellid, pci, tac, band;
            if (sscanf(line, "+QENG: \"servingcell\",\"%*[^\"]\",\"%*[^\"]\",\"%*[^\"]\",\"%d\",\"%d\",%*d,%*d,%d,%*x,%d,%*d,%d,%d",
                      &mcc, &mnc, &cellid, &pci, &tac, &band) >= 4) {
                snprintf(info->mcc, sizeof(info->mcc), "%d", mcc);
                snprintf(info->mnc, sizeof(info->mnc), "%d", mnc);
                snprintf(info->cell_id, sizeof(info->cell_id), "%d", cellid);
                info->pci = pci;
                snprintf(info->tac, sizeof(info->tac), "%d", tac);
                snprintf(info->band, sizeof(info->band), "%d", band);
                info->has_cell_info = true;
            }
        }
    } else {
        // Try generic cell info command
        if (send_at_command(fd, "AT+CGREG=2", response, sizeof(response), 1000) == 0) {
            if (send_at_command(fd, "AT+CGREG?", response, sizeof(response), 2000) == 0) {
                int n, stat, lac, ci;
                if (sscanf(response, "\r\n+CGREG: %d,%d,\"%x\",\"%x\"", &n, &stat, &lac, &ci) >= 4) {
                    info->lac = lac;
                    snprintf(info->cell_id, sizeof(info->cell_id), "%d", ci);
                    info->has_cell_info = true;
                }
            }
        }
    }
    
    close(fd);
    
    // Update timestamp
    info->timestamp = time(NULL);
    
    LOGX_DEBUG_MSG("Successfully collected cellular info via AT commands");
    return AUTONOMY_SUCCESS;
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
    char* net_type_line = strstr(output, "+COPS:");
    if (net_type_line) {
        // Parse network type from COPS response
        char* tech_start = strstr(net_type_line, "(");
        if (tech_start) {
            char* tech_end = strstr(tech_start, ")");
            if (tech_end) {
                char tech[16] = {0};
                strncpy(tech, tech_start + 1, tech_end - tech_start - 1);
                
                if (strstr(tech, "LTE") || strstr(tech, "4G")) {
                    info->network_type = CELLULAR_NETWORK_TYPE_LTE;
                } else if (strstr(tech, "UMTS") || strstr(tech, "3G")) {
                    info->network_type = CELLULAR_NETWORK_TYPE_UMTS;
                } else if (strstr(tech, "GSM") || strstr(tech, "2G")) {
                    info->network_type = CELLULAR_NETWORK_TYPE_GSM;
                } else if (strstr(tech, "5G") || strstr(tech, "NR")) {
                    info->network_type = CELLULAR_NETWORK_TYPE_5G;
                } else {
                    info->network_type = CELLULAR_NETWORK_TYPE_LTE; // Default fallback
                }
            }
        }
    } else {
        // Try to get network type from system
        FILE *net_fp = popen("cat /sys/class/net/wwan0/operstate 2>/dev/null", "r");
        if (net_fp) {
            char state[16];
            if (fgets(state, sizeof(state), net_fp)) {
                if (strstr(state, "up")) {
                    info->connection_state = CELLULAR_STATE_CONNECTED;
                } else {
                    info->connection_state = CELLULAR_STATE_DISCONNECTED;
                }
            }
            pclose(net_fp);
        } else {
            info->connection_state = CELLULAR_STATE_UNKNOWN;
        }
        
        // Try to get network type from modem info
        FILE *modem_fp = popen("mmcli -m 0 --command='AT+QNWINFO' 2>/dev/null | grep -E '(LTE|UMTS|GSM|5G)'", "r");
        if (modem_fp) {
            char net_info[64];
            if (fgets(net_info, sizeof(net_info), modem_fp)) {
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
            }
            pclose(modem_fp);
        } else {
            info->network_type = CELLULAR_NETWORK_TYPE_LTE; // Default fallback
        }
    }
    
    // Get real connection state
    char* reg_line = strstr(output, "+CREG:");
    if (reg_line) {
        int reg_status;
        if (sscanf(reg_line, "+CREG: %*d,%d", &reg_status) == 1) {
            switch (reg_status) {
                case 1:
                case 5:
                    info->connection_state = CELLULAR_STATE_CONNECTED;
                    break;
                case 2:
                    info->connection_state = CELLULAR_STATE_SEARCHING;
                    break;
                case 3:
                case 4:
                    info->connection_state = CELLULAR_STATE_DENIED;
                    break;
                default:
                    info->connection_state = CELLULAR_STATE_UNKNOWN;
                    break;
            }
        }
    } else {
        // Fallback: check if we have an IP address
        if (strcmp(info->ip_address, "0.0.0.0") != 0) {
            info->connection_state = CELLULAR_STATE_CONNECTED;
        } else {
            info->connection_state = CELLULAR_STATE_DISCONNECTED;
        }
    }
    
    return AUTONOMY_SUCCESS;
}

// Update signal history for stability analysis
static void update_signal_history(int rsrp, int rsrq, int sinr) {
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
static void calculate_signal_variance(void) {
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
static double calculate_signal_quality_score(const cellular_info_t* info) {
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