#ifndef AUTONOMY_TYPES_H
#define AUTONOMY_TYPES_H

#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

// Log levels
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} log_level_t;

// GPS source structure
struct gps_source {
    char name[32];
    char type[16];
    int enabled;
    int active;
    float lat;
    float lon;
    float accuracy;
    int confidence;
    time_t last_update;
    int health_score;
    char status[16];
    char raw_data[256];
};

// Network interface structure
struct network_interface {
    char name[32];
    char type[16];
    int enabled;
    float latency;
    float loss;
    int signal_strength;
    int bandwidth;
    time_t last_check;
    int health_score;
    char status[16];
};

// Global configuration
struct autonomy_config {
    char log_level[16];
    int enable_gps;
    int enable_notifications;
    int health_check_interval;
    char config_file[128];
};

// Autonomy daemon state
struct autonomy_state {
    int running;
    time_t start_time;
    char version[32];
    char status[32];
    int member_count;
    char current_member[64];
    time_t last_failover;
    float memory_mb;
    int goroutines;
    char device_id[64];
    int health_checks_run;
    int health_issues_found;
    
    // Network management
    struct network_interface interfaces[10];
    int interface_count;
    char active_interface[32];
    int failover_enabled;
    time_t last_network_check;
    float network_health_score;
    
    // GPS & Location management
    struct gps_source gps_sources[8];
    int gps_source_count;
    int gps_enabled;
    char active_gps_source[32];
    float current_lat;
    float current_lon;
    float current_accuracy;
    int current_confidence;
    time_t last_gps_update;
    float gps_health_score;
    char location_status[16];
    int movement_detected;
    time_t last_movement_check;
};

// System health structure
struct system_health {
    int starlink_health;
    int uci_health;
    int overlay_health;
    int services_health;
    int network_health;
    int database_health;
    int time_health;
    int logs_health;
    int overall_score;
    time_t last_check;
    char status[16];
};

// Global system health variable
extern struct system_health g_system_health;

// Function declarations
void log_message(log_level_t level, const char *format, ...);

#endif // AUTONOMY_TYPES_H
