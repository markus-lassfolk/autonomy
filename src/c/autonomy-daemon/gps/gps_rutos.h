#ifndef GPS_RUTOS_H
#define GPS_RUTOS_H

#include "../core/types.h"
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
static void* rutos_monitor_thread(void *arg);
static int read_rutos_gps_data(gps_data_t *data);
static bool validate_gps_data(const gps_data_t *data);
static float calculate_reliability_score(void);

// RUTOS GPS configuration
typedef struct {
    bool enabled;                // GPS enabled
    int update_interval;         // Update interval in seconds
    int timeout;                 // Timeout in seconds
    float min_accuracy;          // Minimum accuracy in meters
} gps_rutos_config_t;

// RUTOS GPS status
typedef struct {
    bool enabled;                // GPS enabled
    int update_interval;         // Current update interval
    int timeout;                 // Current timeout
    float min_accuracy;          // Current minimum accuracy
    time_t last_update;          // Last update timestamp
    uint64_t total_updates;      // Total updates count
    uint32_t consecutive_failures; // Consecutive failures
    uint32_t consecutive_successes; // Consecutive successes
    float reliability_score;     // Reliability score (0-100)
    bool data_valid;             // Current data valid
} gps_rutos_status_t;

// RUTOS GPS state
typedef struct {
    bool enabled;                // GPS enabled
    int update_interval;         // Update interval in seconds
    int timeout;                 // Timeout in seconds
    float min_accuracy;          // Minimum accuracy in meters
    time_t last_update;          // Last update timestamp
    uint64_t total_updates;      // Total updates count
    uint32_t consecutive_failures; // Consecutive failures
    uint32_t consecutive_successes; // Consecutive successes
    float reliability_score;     // Reliability score (0-100)
    gps_data_t gps_data;        // Current GPS data
} gps_rutos_t;

// Initialize RUTOS GPS system
int gps_rutos_init(void);

// Start RUTOS GPS monitoring thread
int gps_rutos_start_monitoring(void);

// Stop RUTOS GPS monitoring
void gps_rutos_stop_monitoring(void);

// Read GPS data from RUTOS system
int gps_rutos_read_data(void);

// Get current GPS data
int gps_rutos_get_data(gps_data_t *data);

// Check if RUTOS GPS is initialized
bool gps_rutos_is_initialized(void);

// Get RUTOS GPS status
int gps_rutos_get_status(gps_rutos_status_t *status);

// Set RUTOS GPS configuration
int gps_rutos_set_config(const gps_rutos_config_t *config);

// Enable/disable RUTOS GPS
int gps_rutos_set_enabled(bool enabled);

// Check if RUTOS GPS data is recent
bool gps_rutos_is_data_recent(int max_age_seconds);

// Check if RUTOS GPS data meets accuracy requirements
bool gps_rutos_meets_accuracy(float required_accuracy);

// Cleanup RUTOS GPS system
void gps_rutos_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // GPS_RUTOS_H