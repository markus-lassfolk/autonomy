#ifndef CELLULAR_COLLECTOR_H
#define CELLULAR_COLLECTOR_H

#include <stdint.h>
#include <stdbool.h>

// Cellular information structure
typedef struct {
    double rsrp;                    // Reference Signal Received Power
    double rsrq;                    // Reference Signal Received Quality
    double sinr;                    // Signal-to-Interference-plus-Noise Ratio
    char operator[64];              // Network operator name
    uint32_t cell_id;               // Cell tower ID
    double signal_quality;          // Signal quality percentage (0-100)
    double reliability_score;       // Reliability score (0-1)
    double predictive_risk;         // Predictive risk score (0-1)
} cellular_info_t;

// Function declarations
int cellular_collector_init(void);
void cellular_collector_cleanup(void);
int cellular_collector_get_info(cellular_info_t* info);
bool cellular_collector_is_available(void);

#endif // CELLULAR_COLLECTOR_H