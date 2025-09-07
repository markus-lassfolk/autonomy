#ifndef SYSTEM_MANAGEMENT_H
#define SYSTEM_MANAGEMENT_H

#include <stdbool.h>
#include <stdint.h>
#include "../core/types.h"

// System health status structure

// Function declarations
int perform_system_health_check(void);
int get_system_memory_usage(unsigned long *total, unsigned long *available);
unsigned long get_system_uptime(void);
int get_system_load_average(double *load1, double *load5, double *load15);
const system_health_t* get_system_health_status(void);

#endif // SYSTEM_MANAGEMENT_H
