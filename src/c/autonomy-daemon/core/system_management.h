#ifndef SYSTEM_MANAGEMENT_H
#define SYSTEM_MANAGEMENT_H

#include <stdbool.h>
#include <stdint.h>

// System health status structure
typedef struct {
    bool starlink_healthy;
    bool uci_healthy;
    bool overlay_healthy;
    bool services_healthy;
    bool network_healthy;
    bool database_healthy;
    bool time_healthy;
    bool logs_healthy;
    int overall_score;
    char status_message[256];
} system_health_t;

// Function declarations
int perform_system_health_check(void);
int get_system_memory_usage(unsigned long *total, unsigned long *available);
unsigned long get_system_uptime(void);
int get_system_load_average(double *load1, double *load5, double *load15);
const system_health_t* get_system_health_status(void);

#endif // SYSTEM_MANAGEMENT_H
