#ifndef HANG_DETECTOR_H
#define HANG_DETECTOR_H

#include <time.h>
#include <pthread.h>
#include <signal.h>

// Hang detection configuration
#define HANG_DETECTION_ENABLED 1
#define DEFAULT_HANG_TIMEOUT_SECONDS 30
#define CRITICAL_HANG_TIMEOUT_SECONDS 10
#define WATCHDOG_INTERVAL_SECONDS 5

// Hang detection states
typedef enum {
    HANG_STATE_NORMAL = 0,
    HANG_STATE_SLOW = 1,
    HANG_STATE_HANGING = 2,
    HANG_STATE_CRITICAL = 3
} hang_state_t;

// Hang detection context
typedef struct {
    time_t start_time;
    time_t last_activity;
    time_t timeout_seconds;
    hang_state_t state;
    const char *operation_name;
    pthread_mutex_t mutex;
    volatile int watchdog_active;
    volatile int force_exit;
} hang_detector_t;

// Global hang detector instance
extern hang_detector_t g_hang_detector;

// Function declarations
int hang_detector_init(void);
void hang_detector_cleanup(void);
int hang_detector_start_operation(const char *operation_name, time_t timeout_seconds);
void hang_detector_update_activity(void);
void hang_detector_end_operation(void);
hang_state_t hang_detector_check_state(void);
void hang_detector_force_exit(void);

// Watchdog thread
void* hang_detector_watchdog_thread(void *arg);
int hang_detector_start_watchdog(void);
void hang_detector_stop_watchdog(void);

// Convenience macros
#define HANG_DETECTOR_START(op_name, timeout) \
    do { \
        if (hang_detector_start_operation(op_name, timeout) != 0) { \
            fprintf(stderr, "HANG_DETECTOR: Failed to start monitoring '%s'\n", op_name); \
        } \
    } while(0)

#define HANG_DETECTOR_UPDATE() \
    hang_detector_update_activity()

#define HANG_DETECTOR_END() \
    hang_detector_end_operation()

#define HANG_DETECTOR_CHECK() \
    hang_detector_check_state()

// Critical operation monitoring
#define CRITICAL_OPERATION_START(op_name) \
    HANG_DETECTOR_START(op_name, CRITICAL_HANG_TIMEOUT_SECONDS)

#define CRITICAL_OPERATION_UPDATE() \
    HANG_DETECTOR_UPDATE()

#define CRITICAL_OPERATION_END() \
    HANG_DETECTOR_END()

// Progress reporting
void hang_detector_report_progress(const char *message);
void hang_detector_report_progress_percent(int percent);

// Emergency exit handler
void hang_detector_emergency_exit(const char *reason);

#endif // HANG_DETECTOR_H
