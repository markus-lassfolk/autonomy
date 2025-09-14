#include "hang_detector.h"
#include "../logging/logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>

// Global hang detector instance
hang_detector_t g_hang_detector = {0};

// Watchdog thread
static pthread_t watchdog_thread;
static volatile int watchdog_running = 0;

// Signal handler for emergency exit
static void hang_detector_signal_handler(int sig) {
    LOGX_FATAL_MSG("\n=== HANG DETECTOR EMERGENCY EXIT ===");
    LOGX_FATAL_MSG("Signal: %d", sig);
    LOGX_FATAL_MSG("Reason: Hang detection timeout");
    LOGX_FATAL_MSG("Operation: %s", g_hang_detector.operation_name ? g_hang_detector.operation_name : "unknown");
    LOGX_FATAL_MSG("Timeout: %ld seconds", (long)g_hang_detector.timeout_seconds);
    LOGX_FATAL_MSG("=====================================");
    
    LOGX_ERROR_MSG("HANG DETECTOR: Emergency exit due to hang timeout in operation '%s'", 
                   g_hang_detector.operation_name ? g_hang_detector.operation_name : "unknown");
    
    // Force exit
    _exit(1);
}

int hang_detector_init(void) {
    memset(&g_hang_detector, 0, sizeof(hang_detector_t));
    
    if (pthread_mutex_init(&g_hang_detector.mutex, NULL) != 0) {
        LOGX_ERROR_MSG("HANG_DETECTOR: Failed to initialize mutex");
        return -1;
    }
    
    // Set up signal handler for emergency exit
    struct sigaction sa;
    sa.sa_handler = hang_detector_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    if (sigaction(SIGALRM, &sa, NULL) != 0) {
        LOGX_ERROR_MSG("HANG_DETECTOR: Failed to set up signal handler");
        pthread_mutex_destroy(&g_hang_detector.mutex);
        return -1;
    }
    
    g_hang_detector.watchdog_active = 0;
    g_hang_detector.force_exit = 0;
    
    LOGX_INFO_MSG("HANG_DETECTOR: Initialized successfully");
    return 0;
}

void hang_detector_cleanup(void) {
    hang_detector_stop_watchdog();
    
    if (pthread_mutex_destroy(&g_hang_detector.mutex) != 0) {
        LOGX_ERROR_MSG("HANG_DETECTOR: Failed to destroy mutex");
    }
    
    memset(&g_hang_detector, 0, sizeof(hang_detector_t));
    LOGX_INFO_MSG("HANG_DETECTOR: Cleaned up");
}

int hang_detector_start_operation(const char *operation_name, time_t timeout_seconds) {
    if (!operation_name || timeout_seconds <= 0) {
        return -1;
    }
    
    pthread_mutex_lock(&g_hang_detector.mutex);
    
    time_t now = time(NULL);
    g_hang_detector.start_time = now;
    g_hang_detector.last_activity = now;
    g_hang_detector.timeout_seconds = timeout_seconds;
    g_hang_detector.state = HANG_STATE_NORMAL;
    g_hang_detector.operation_name = operation_name;
    
    pthread_mutex_unlock(&g_hang_detector.mutex);
    
    LOGX_INFO_MSG("HANG_DETECTOR: Started monitoring '%s' (timeout: %ld seconds)", 
            operation_name, (long)timeout_seconds);
    
    return 0;
}

void hang_detector_update_activity(void) {
    pthread_mutex_lock(&g_hang_detector.mutex);
    g_hang_detector.last_activity = time(NULL);
    pthread_mutex_unlock(&g_hang_detector.mutex);
}

void hang_detector_end_operation(void) {
    pthread_mutex_lock(&g_hang_detector.mutex);
    
    if (g_hang_detector.operation_name) {
        time_t duration = time(NULL) - g_hang_detector.start_time;
        LOGX_INFO_MSG("HANG_DETECTOR: Completed '%s' in %ld seconds", 
                g_hang_detector.operation_name, (long)duration);
    }
    
    g_hang_detector.operation_name = NULL;
    g_hang_detector.state = HANG_STATE_NORMAL;
    
    pthread_mutex_unlock(&g_hang_detector.mutex);
}

hang_state_t hang_detector_check_state(void) {
    pthread_mutex_lock(&g_hang_detector.mutex);
    
    if (!g_hang_detector.operation_name) {
        pthread_mutex_unlock(&g_hang_detector.mutex);
        return HANG_STATE_NORMAL;
    }
    
    time_t now = time(NULL);
    time_t elapsed = now - g_hang_detector.last_activity;
    time_t total_elapsed = now - g_hang_detector.start_time;
    
    hang_state_t state = HANG_STATE_NORMAL;
    
    if (elapsed >= g_hang_detector.timeout_seconds) {
        state = HANG_STATE_HANGING;
        LOGX_WARN_MSG("HANG_DETECTOR: WARNING - Operation '%s' appears to be hanging (no activity for %ld seconds)",
                g_hang_detector.operation_name, (long)elapsed);
    } else if (elapsed >= (g_hang_detector.timeout_seconds / 2)) {
        state = HANG_STATE_SLOW;
        LOGX_WARN_MSG("HANG_DETECTOR: WARNING - Operation '%s' is slow (no activity for %ld seconds)",
                g_hang_detector.operation_name, (long)elapsed);
    }
    
    if (total_elapsed >= (g_hang_detector.timeout_seconds * 2)) {
        state = HANG_STATE_CRITICAL;
        LOGX_FATAL_MSG("HANG_DETECTOR: CRITICAL - Operation '%s' has been running for %ld seconds (timeout: %ld)",
                g_hang_detector.operation_name, (long)total_elapsed, (long)g_hang_detector.timeout_seconds);
    }
    
    g_hang_detector.state = state;
    pthread_mutex_unlock(&g_hang_detector.mutex);
    
    return state;
}

void hang_detector_force_exit(void) {
    pthread_mutex_lock(&g_hang_detector.mutex);
    g_hang_detector.force_exit = 1;
    pthread_mutex_unlock(&g_hang_detector.mutex);
    
    LOGX_FATAL_MSG("HANG_DETECTOR: Force exit requested");
    hang_detector_emergency_exit("Force exit requested");
}

void* hang_detector_watchdog_thread(void *arg) {
    (void)arg; // Unused parameter
    
    LOGX_INFO_MSG("HANG_DETECTOR: Watchdog thread started");
    
    while (watchdog_running) {
        sleep(WATCHDOG_INTERVAL_SECONDS);
        
        if (!watchdog_running) break;
        
        hang_state_t state = hang_detector_check_state();
        
        if (state == HANG_STATE_CRITICAL) {
            LOGX_FATAL_MSG("HANG_DETECTOR: CRITICAL hang detected - forcing exit");
            hang_detector_force_exit();
            break;
        }
    }
    
    LOGX_INFO_MSG("HANG_DETECTOR: Watchdog thread stopped");
    return NULL;
}

int hang_detector_start_watchdog(void) {
    if (watchdog_running) {
        return 0; // Already running
    }
    
    watchdog_running = 1;
    
    if (pthread_create(&watchdog_thread, NULL, hang_detector_watchdog_thread, NULL) != 0) {
        LOGX_ERROR_MSG("HANG_DETECTOR: Failed to create watchdog thread");
        watchdog_running = 0;
        return -1;
    }
    
    LOGX_INFO_MSG("HANG_DETECTOR: Watchdog started");
    return 0;
}

void hang_detector_stop_watchdog(void) {
    if (!watchdog_running) {
        return;
    }
    
    watchdog_running = 0;
    
    if (pthread_join(watchdog_thread, NULL) != 0) {
        LOGX_ERROR_MSG("HANG_DETECTOR: Failed to join watchdog thread");
    }
    
    LOGX_INFO_MSG("HANG_DETECTOR: Watchdog stopped");
}

void hang_detector_report_progress(const char *message) {
    if (!message) return;
    
    hang_detector_update_activity();
    
    pthread_mutex_lock(&g_hang_detector.mutex);
    if (g_hang_detector.operation_name) {
        time_t elapsed = time(NULL) - g_hang_detector.start_time;
        LOGX_INFO_MSG("HANG_DETECTOR: [%lds] %s: %s", 
                (long)elapsed, g_hang_detector.operation_name, message);
    } else {
        LOGX_INFO_MSG("HANG_DETECTOR: %s", message);
    }
    pthread_mutex_unlock(&g_hang_detector.mutex);
}

void hang_detector_report_progress_percent(int percent) {
    if (percent < 0 || percent > 100) return;
    
    hang_detector_update_activity();
    
    pthread_mutex_lock(&g_hang_detector.mutex);
    if (g_hang_detector.operation_name) {
        time_t elapsed = time(NULL) - g_hang_detector.start_time;
        LOGX_INFO_MSG("HANG_DETECTOR: [%lds] %s: %d%% complete", 
                (long)elapsed, g_hang_detector.operation_name, percent);
    }
    pthread_mutex_unlock(&g_hang_detector.mutex);
}

void hang_detector_emergency_exit(const char *reason) {
    LOGX_FATAL_MSG("\n=== HANG DETECTOR EMERGENCY EXIT ===");
    LOGX_FATAL_MSG("Reason: %s", reason ? reason : "Unknown");
    LOGX_FATAL_MSG("Operation: %s", g_hang_detector.operation_name ? g_hang_detector.operation_name : "unknown");
    LOGX_FATAL_MSG("Timeout: %ld seconds", (long)g_hang_detector.timeout_seconds);
    LOGX_FATAL_MSG("=====================================");
    
    LOGX_ERROR_MSG("HANG DETECTOR: Emergency exit - %s", reason ? reason : "Unknown");
    
    // Force exit
    _exit(1);
}
