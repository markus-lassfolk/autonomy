#ifndef WIFI_ENHANCED_H
#define WIFI_ENHANCED_H

#include <stdint.h>
#include <stdbool.h>

// WiFi enhanced information structure
typedef struct {
    double rssi_dbm;                // Received Signal Strength Indicator
    int channel;                    // WiFi channel
    char ssid[64];                  // Service Set Identifier
    double noise_floor;             // Noise floor in dBm
    double signal_quality;          // Signal quality percentage (0-100)
    double throughput_mbps;         // Current throughput in Mbps
    bool is_connected;              // Connection status
    char security_type[32];         // Security type (WPA2, WPA3, etc.)
} wifi_enhanced_info_t;

// Function declarations
int wifi_enhanced_init(void);
void wifi_enhanced_cleanup(void);
int wifi_enhanced_get_info(wifi_enhanced_info_t* info);
bool wifi_enhanced_is_available(void);

#endif // WIFI_ENHANCED_H