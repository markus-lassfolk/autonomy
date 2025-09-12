#ifndef CELLULAR_COLLECTOR_H
#define CELLULAR_COLLECTOR_H

#include <stdint.h>
#include <stdbool.h>

// Cellular connection states
typedef enum {
    CELLULAR_STATE_DISCONNECTED = 0,
    CELLULAR_STATE_CONNECTING,
    CELLULAR_STATE_CONNECTED,
    CELLULAR_STATE_RECONNECTING,
    CELLULAR_STATE_ERROR,
    CELLULAR_STATE_UNKNOWN,
    CELLULAR_STATE_SEARCHING,
    CELLULAR_STATE_DENIED,
    CELLULAR_STATE_MAX
} cellular_connection_state_t;

// Cellular network types
typedef enum {
    CELLULAR_NETWORK_TYPE_UNKNOWN = 0,
    CELLULAR_NETWORK_TYPE_GSM,
    CELLULAR_NETWORK_TYPE_2G,
    CELLULAR_NETWORK_TYPE_3G,
    CELLULAR_NETWORK_TYPE_UMTS,
    CELLULAR_NETWORK_TYPE_LTE,
    CELLULAR_NETWORK_TYPE_5G,
    CELLULAR_NETWORK_TYPE_CDMA,
    CELLULAR_NETWORK_TYPE_MAX
} cellular_network_type_t;

// Cellular roaming types
typedef enum {
    ROAMING_TYPE_NONE = 0,
    ROAMING_TYPE_NATIONAL,
    ROAMING_TYPE_INTERNATIONAL,
    ROAMING_TYPE_DOMESTIC,
    ROAMING_TYPE_MAX
} cellular_roaming_type_t;

// Cellular SIM status
typedef enum {
    CELLULAR_SIM_STATUS_UNKNOWN = 0,
    CELLULAR_SIM_STATUS_READY,
    CELLULAR_SIM_STATUS_PIN_REQUIRED,
    CELLULAR_SIM_STATUS_PUK_REQUIRED,
    CELLULAR_SIM_STATUS_ERROR,
    CELLULAR_SIM_STATUS_MAX
} cellular_sim_status_t;

// Cellular information structure
typedef struct {
    double rsrp;                    // Reference Signal Received Power
    double rsrq;                    // Reference Signal Received Quality
    double sinr;                    // Signal-to-Interference-plus-Noise Ratio
    char operator_name[64];         // Network operator name
    uint32_t cell_id;               // Cell tower ID
    double signal_quality;          // Signal quality percentage (0-100)
    double reliability_score;       // Reliability score (0-1)
    double predictive_risk;         // Predictive risk score (0-1)
    bool roaming;                   // Whether device is roaming
    
    // Additional fields for compatibility
    cellular_connection_state_t connection_state;
    cellular_network_type_t network_type;
    cellular_roaming_type_t roaming_type;
    char manufacturer[64];
    char model[64];
    char imei[32];
    char iccid[32];
    char mcc[8];
    char mnc[8];
    char tac[16];
    int lac;
    int pci;
    bool has_cell_info;
} cellular_info_t;

// Function declarations
int cellular_collector_init(void);
void cellular_collector_cleanup(void);
int cellular_collector_get_info(cellular_info_t* info);
bool cellular_collector_is_available(void);

#endif // CELLULAR_COLLECTOR_H