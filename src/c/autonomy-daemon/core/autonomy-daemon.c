#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <libubox/uloop.h>
#include <uci.h>
#include <syslog.h>
#include <stdarg.h>
#include <math.h>
#ifdef __GLIBC__
#include <execinfo.h>  // For backtrace (GNU libc only)
#endif
#include <ucontext.h>  // For signal context
#include <dlfcn.h>     // For symbol resolution
#ifdef __x86_64__
#include <sys/reg.h>   // For x86_64 register definitions
#endif
#ifdef __i386__
#include <sys/reg.h>   // For i386 register definitions
#endif

// Include our modular headers
#include "../core/types.h"
#include "version.h"
#include "autonomy_modules.h"
#include "../starlink/starlink_modules.h"
#include "../starlink/starlink_tracker.h"
#include "../starlink/starlink_grpc_collector.h"
#include "../shared/utils/uci_manager.h"
#include "../shared/logging/logx.h"
#include "../shared/utils/memory_debug.h"
#include "../shared/utils/memory_protection.h"
#include "../shared/utils/memory_corruption_detector.h"
#include "../shared/utils/hang_detector.h"
#include "../utils/debug_trace.h"
#include "../ml/ml_monitor.h"
#include "../ml/ml_monitor_ubus.h"
#include <sys/socket.h>

// NOLINTBEGIN(cert-msc50-cpp,cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
// NOLINTBEGIN(cert-msc51-cpp) - fopen usage is safe with path validation

// Global variables
autonomy_config_t g_config;

// Exit reason tracking
typedef enum {
    EXIT_REASON_UNKNOWN = 0,
    EXIT_REASON_SIGNAL_TERM,
    EXIT_REASON_SIGNAL_INT,
    EXIT_REASON_SIGNAL_SEGV,
    EXIT_REASON_SIGNAL_BUS,
    EXIT_REASON_SIGNAL_FPE,
    EXIT_REASON_SIGNAL_ILL,
    EXIT_REASON_SIGNAL_ABRT,
    EXIT_REASON_INIT_FAILURE,
    EXIT_REASON_CONFIG_ERROR,
    EXIT_REASON_MEMORY_ERROR,
    EXIT_REASON_ULOOP_ERROR,
    EXIT_REASON_NORMAL_SHUTDOWN
} exit_reason_t;

static exit_reason_t g_exit_reason = EXIT_REASON_UNKNOWN;
static char g_exit_message[512] = {0};

// Forward declarations
static void print_memory_info(void);
static void print_backtrace(void);
static void crash_handler(int sig, siginfo_t *info, void *context);
static void log_exit_reason(exit_reason_t reason, const char *message);
static void daemon_exit(int exit_code);
static void setup_crash_handlers(void);
static void print_register_state(ucontext_t *context);
static void print_stack_trace_arm(ucontext_t *context);
static void validate_memory_before_access(void *ptr, size_t size, const char *location);

// Crash debugging functions
static void print_backtrace(void) {
#ifdef __GLIBC__
    void *array[20];
    size_t size;
    char **strings;
    size_t i;

    fprintf(stderr, "\n=== BACKTRACE ===\n");
    size = backtrace(array, 20);
    strings = backtrace_symbols(array, size);

    if (strings != NULL) {
        for (i = 0; i < size; i++) {
            fprintf(stderr, "[%zu] %s\n", i, strings[i]);
        }
        free(strings);
    }
    fprintf(stderr, "=== END BACKTRACE ===\n\n");
#else
    fprintf(stderr, "\n=== BACKTRACE ===\n");
    fprintf(stderr, "Backtrace not available (not using GNU libc)\n");
    fprintf(stderr, "=== END BACKTRACE ===\n\n");
#endif
}

static void crash_handler(int sig, siginfo_t *info, void *context) {
    fprintf(stderr, "\n=== CRASH DETECTED ===\n");
    fprintf(stderr, "Signal: %d (%s)\n", sig, strsignal(sig));
    fprintf(stderr, "Signal code: %d\n", info->si_code);
    fprintf(stderr, "Fault address: %p\n", info->si_addr);
    fprintf(stderr, "PID: %d\n", getpid());
    fprintf(stderr, "UID: %d\n", getuid());
    
    // CRITICAL: Check for memory corruption
    fprintf(stderr, "\n=== MEMORY CORRUPTION ANALYSIS ===\n");
    check_all_monitored_globals();
    print_memory_corruption_report();
    fprintf(stderr, "=== END MEMORY CORRUPTION ANALYSIS ===\n\n");
    
    // Enhanced signal-specific information
    switch (sig) {
        case SIGSEGV:
            fprintf(stderr, "SEGFAULT: Invalid memory access at %p\n", info->si_addr);
            if (info->si_code == SEGV_MAPERR) {
                fprintf(stderr, "Cause: Address not mapped to object\n");
            } else if (info->si_code == SEGV_ACCERR) {
                fprintf(stderr, "Cause: Invalid permissions for mapped object\n");
            }
            break;
        case SIGBUS:
            fprintf(stderr, "BUS ERROR: Invalid memory access at %p\n", info->si_addr);
            break;
        case SIGFPE:
            fprintf(stderr, "FLOATING POINT EXCEPTION: Division by zero or invalid operation\n");
            break;
        case SIGILL:
            fprintf(stderr, "ILLEGAL INSTRUCTION: Invalid instruction executed\n");
            break;
        default:
            fprintf(stderr, "UNKNOWN SIGNAL: %d\n", sig);
            break;
    }
    
    // Print memory map info if available
    fprintf(stderr, "\n=== MEMORY MAP ===\n");
    FILE *maps = fopen("/proc/self/maps", "r");
    if (maps) {
        char line[256];
        while (fgets(line, sizeof(line), maps)) {
            fprintf(stderr, "%s", line);
        }
        fclose(maps);
    }
    fprintf(stderr, "=== END MEMORY MAP ===\n\n");
    
    print_memory_info();
    
    // Print memory debugging information
    fprintf(stderr, "\n=== MEMORY DEBUG INFO ===\n");
    // memory_debug_print_stats(); // Disabled to prevent conflicts with memory protection system
    // memory_debug_check_all_allocations(); // Disabled to prevent conflicts with memory protection system
    // memory_debug_detect_leaks(); // Disabled to prevent conflicts with memory protection system
    // memory_debug_scan_memory_for_corruption(); // Disabled to prevent conflicts with memory protection system
    fprintf(stderr, "=== END MEMORY DEBUG INFO ===\n\n");
    
    print_backtrace();
    
    // Additional debugging info for systems without backtrace
    fprintf(stderr, "=== STACK INFO ===\n");
    fprintf(stderr, "Current function: crash_handler\n");
    fprintf(stderr, "Signal: %d (%s)\n", sig, strsignal(sig));
    fprintf(stderr, "Fault address: %p\n", info->si_addr);
    fprintf(stderr, "=== END STACK INFO ===\n\n");
    
    // Try to get more detailed info about the fault
    if (context) {
        ucontext_t *uc = (ucontext_t *)context;
        print_register_state(uc);
        print_stack_trace_arm(uc);
    }
    
    fprintf(stderr, "=== CRASH END ===\n");
    fflush(stderr);
    
    // Log the crash reason before exiting
    exit_reason_t reason = EXIT_REASON_UNKNOWN;
    char message[256];
    
    switch (sig) {
        case SIGSEGV:
            reason = EXIT_REASON_SIGNAL_SEGV;
            snprintf(message, sizeof(message), "Segmentation fault at %p (signal %d)", info->si_addr, sig);
            break;
        case SIGBUS:
            reason = EXIT_REASON_SIGNAL_BUS;
            snprintf(message, sizeof(message), "Bus error at %p (signal %d)", info->si_addr, sig);
            break;
        case SIGFPE:
            reason = EXIT_REASON_SIGNAL_FPE;
            snprintf(message, sizeof(message), "Floating point exception (signal %d)", sig);
            break;
        case SIGILL:
            reason = EXIT_REASON_SIGNAL_ILL;
            snprintf(message, sizeof(message), "Illegal instruction (signal %d)", sig);
            break;
        case SIGABRT:
            reason = EXIT_REASON_SIGNAL_ABRT;
            snprintf(message, sizeof(message), "Abort signal (signal %d)", sig);
            break;
        default:
            reason = EXIT_REASON_SIGNAL_SEGV;
            snprintf(message, sizeof(message), "Unknown crash signal %d", sig);
            break;
    }
    
    log_exit_reason(reason, message);
    
    // Exit with the signal number
    _exit(sig);
}

static void setup_crash_handlers(void) {
    struct sigaction sa;
    
    // Set up signal handler for crashes
    sa.sa_sigaction = crash_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    
    // Handle common crash signals
    sigaction(SIGSEGV, &sa, NULL);  // Segmentation fault
    sigaction(SIGBUS, &sa, NULL);   // Bus error
    sigaction(SIGFPE, &sa, NULL);   // Floating point exception
    sigaction(SIGILL, &sa, NULL);   // Illegal instruction
    sigaction(SIGABRT, &sa, NULL);  // Abort signal
    
    fprintf(stderr, "DEBUG: Crash handlers installed\n");
}

// Memory debugging utility
static void print_memory_info(void) {
    FILE *status = fopen("/proc/self/status", "r");
    if (status) {
        char line[256];
        fprintf(stderr, "\n=== MEMORY STATUS ===\n");
        while (fgets(line, sizeof(line), status)) {
            if (strstr(line, "VmSize") || strstr(line, "VmRSS") || 
                strstr(line, "VmPeak") || strstr(line, "VmHWM") ||
                strstr(line, "VmData") || strstr(line, "VmStk") ||
                strstr(line, "VmExe") || strstr(line, "VmLib")) {
                fprintf(stderr, "%s", line);
            }
        }
        fclose(status);
        fprintf(stderr, "=== END MEMORY STATUS ===\n\n");
    }
}

autonomy_state_t g_state = {
    .running = false,
    .gps_enabled = true,
    .current_lat = 0.0,
    .current_lon = 0.0,
    .current_accuracy = 0.0,
    .current_confidence = 0.0,
    .last_gps_update = 0,
    .location_status = "unknown",
    .movement_detected = false,
    .last_movement_check = 0,
    .start_time = 0,
    .health_checks_run = 0,
    .health_issues_found = 0,
    .interface_count = 0,
    .failover_enabled = false,
    .network_health_score = 0.0,
    .gps_source_count = 0,
    .gps_health_score = 0.0,
    .active_interface = "",
    .last_network_check = 0,
    .last_failover = 0,
    .interfaces = {{0}},  // Initialize all interfaces to zero
    .active_gps_source = "",
    .gps_sources = {{0}}  // Initialize all GPS sources to zero
};

// Global Starlink tracker
static starlink_tracker_t *g_starlink_tracker = NULL;

// Global system health (defined in core/system_management.c)
extern system_health_t g_system_health;

static struct ubus_context *ctx;
struct uci_context *uci_ctx;

// Signal handler
void handle_sig(int sig) {
    exit_reason_t reason = EXIT_REASON_UNKNOWN;
    char message[256];
    
    switch (sig) {
        case SIGTERM:
            reason = EXIT_REASON_SIGNAL_TERM;
            snprintf(message, sizeof(message), "Received SIGTERM (termination signal)");
            break;
        case SIGINT:
            reason = EXIT_REASON_SIGNAL_INT;
            snprintf(message, sizeof(message), "Received SIGINT (interrupt signal - Ctrl+C)");
            break;
        default:
            reason = EXIT_REASON_SIGNAL_TERM;
            snprintf(message, sizeof(message), "Received signal %d", sig);
            break;
    }
    
    log_exit_reason(reason, message);
    daemon_exit(0);
}

// Comprehensive exit logging function
static void log_exit_reason(exit_reason_t reason, const char *message) {
    g_exit_reason = reason;
    if (message) {
        strncpy(g_exit_message, message, sizeof(g_exit_message) - 1);
        g_exit_message[sizeof(g_exit_message) - 1] = '\0';
    }
    
    const char *reason_str = "UNKNOWN";
    switch (reason) {
        case EXIT_REASON_SIGNAL_TERM: reason_str = "SIGTERM"; break;
        case EXIT_REASON_SIGNAL_INT: reason_str = "SIGINT"; break;
        case EXIT_REASON_SIGNAL_SEGV: reason_str = "SIGSEGV"; break;
        case EXIT_REASON_SIGNAL_BUS: reason_str = "SIGBUS"; break;
        case EXIT_REASON_SIGNAL_FPE: reason_str = "SIGFPE"; break;
        case EXIT_REASON_SIGNAL_ILL: reason_str = "SIGILL"; break;
        case EXIT_REASON_SIGNAL_ABRT: reason_str = "SIGABRT"; break;
        case EXIT_REASON_INIT_FAILURE: reason_str = "INIT_FAILURE"; break;
        case EXIT_REASON_CONFIG_ERROR: reason_str = "CONFIG_ERROR"; break;
        case EXIT_REASON_MEMORY_ERROR: reason_str = "MEMORY_ERROR"; break;
        case EXIT_REASON_ULOOP_ERROR: reason_str = "ULOOP_ERROR"; break;
        case EXIT_REASON_NORMAL_SHUTDOWN: reason_str = "NORMAL_SHUTDOWN"; break;
        default: reason_str = "UNKNOWN"; break;
    }
    
    // Log to stderr (always visible)
    fprintf(stderr, "\n=== DAEMON EXIT ===\n");
    fprintf(stderr, "Exit Reason: %s\n", reason_str);
    fprintf(stderr, "Exit Message: %s\n", g_exit_message);
    fprintf(stderr, "Timestamp: %lld\n", (long long)time(NULL));
    fprintf(stderr, "PID: %d\n", getpid());
    fprintf(stderr, "==================\n\n");
    
    // Also log via LOGX if available
    LOGX_ERROR_MSG("DAEMON EXIT: Reason=%s, Message=%s, PID=%d", reason_str, g_exit_message, getpid());
}

// Comprehensive daemon exit function
static void daemon_exit(int exit_code) {
    fprintf(stderr, "Performing daemon cleanup before exit...\n");
    
    // Cleanup UBUS context
    if (ctx) {
        fprintf(stderr, "Cleaning up UBUS context...\n");
        ubus_free(ctx);
        ctx = NULL;
    }
    
    // Cleanup UCI context
    if (uci_ctx) {
        fprintf(stderr, "Cleaning up UCI context...\n");
        uci_free_context(uci_ctx);
        uci_ctx = NULL;
    }
    
    // Cleanup UCI manager
    fprintf(stderr, "Cleaning up UCI manager...\n");
    uci_manager_cleanup();
    
    // Cleanup hang detector
    fprintf(stderr, "Cleaning up hang detector...\n");
    hang_detector_cleanup();
    
    // Remove PID file
    fprintf(stderr, "Removing PID file...\n");
    remove_pid_file();
    
    // Stop uloop
    fprintf(stderr, "Stopping uloop...\n");
    uloop_done();
    
    // Final exit message
    fprintf(stderr, "Daemon cleanup completed. Exiting with code %d.\n", exit_code);
    
    exit(exit_code);
}

// UBUS object type and methods
static const struct ubus_method autonomy_methods[] = {
    UBUS_METHOD_NOARG("status", autonomy_status),
    UBUS_METHOD_NOARG("health", autonomy_health),
    UBUS_METHOD_NOARG("config", autonomy_config),
    UBUS_METHOD_NOARG("start", autonomy_start),
    UBUS_METHOD_NOARG("stop", autonomy_stop),
    UBUS_METHOD_NOARG("restart", autonomy_restart),
    UBUS_METHOD_NOARG("pid_status", autonomy_pid_status),
    UBUS_METHOD_NOARG("log_status", autonomy_log_status),
    UBUS_METHOD_NOARG("config_status", autonomy_config_status),
    // Network management methods
    UBUS_METHOD_NOARG("network_status", autonomy_network_status),
    UBUS_METHOD_NOARG("network_interfaces", autonomy_network_interfaces),
    UBUS_METHOD_NOARG("network_interfaces_detailed", autonomy_network_interfaces_detailed),
    UBUS_METHOD_NOARG("network_health_check", autonomy_network_health_check),
    UBUS_METHOD_NOARG("network_failover", autonomy_network_failover),
    // GPS methods
    UBUS_METHOD_NOARG("gps_status", autonomy_gps_status),
    UBUS_METHOD_NOARG("gps_sources", autonomy_gps_sources),
    UBUS_METHOD_NOARG("gps_health_check", autonomy_gps_health_check),
    // System management methods
    UBUS_METHOD_NOARG("system_status", autonomy_system_status),
    UBUS_METHOD_NOARG("system_health_check", autonomy_system_health_check),
    UBUS_METHOD_NOARG("system_health_details", autonomy_system_health_details),
    UBUS_METHOD_NOARG("system_maintenance", autonomy_system_maintenance),
    UBUS_METHOD_NOARG("system_restart_services", autonomy_system_restart_services),
    // Starlink methods
    UBUS_METHOD_NOARG("starlink_status", autonomy_starlink_status),
    UBUS_METHOD_NOARG("starlink_health", autonomy_starlink_health),
    UBUS_METHOD_NOARG("starlink_location", autonomy_starlink_location),
    UBUS_METHOD_NOARG("starlink_collector_stats", autonomy_starlink_collector_stats),
    UBUS_METHOD_NOARG("starlink_force_collect", autonomy_starlink_force_collect),
    // Starlink cluster methods
    UBUS_METHOD_NOARG("starlink_cluster_status", autonomy_starlink_cluster_status),
    UBUS_METHOD_NOARG("starlink_cluster_check_failover", autonomy_starlink_cluster_check_failover),
};

static struct ubus_object_type autonomy_obj_type = 
    UBUS_OBJECT_TYPE("autonomy", autonomy_methods);

static struct ubus_object autonomy_obj = {
    .name = "autonomy",
    .type = &autonomy_obj_type,
    .methods = autonomy_methods,
    .n_methods = ARRAY_SIZE(autonomy_methods),
};

int main(int argc, char **argv)
{
    // Initialize comprehensive debugging system
    debug_trace_init(DEBUG_TRACE_TRACE);  // Enable maximum debugging
    
    // Initialize memory debugging system
    // memory_debug_init(); // Disabled to prevent conflicts with memory protection system
    
    // Initialize hang detection system
    fprintf(stderr, "DEBUG: About to initialize hang detection system\n");
    if (hang_detector_init() != 0) {
        log_exit_reason(EXIT_REASON_INIT_FAILURE, "Failed to initialize hang detection system");
        daemon_exit(1);
    }
    fprintf(stderr, "DEBUG: Hang detection system initialized successfully\n");
    
    // Start hang detection watchdog
    fprintf(stderr, "DEBUG: Starting hang detection watchdog\n");
    if (hang_detector_start_watchdog() != 0) {
        log_exit_reason(EXIT_REASON_INIT_FAILURE, "Failed to start hang detection watchdog");
        daemon_exit(1);
    }
    fprintf(stderr, "DEBUG: Hang detection watchdog started successfully\n");
    
    // Initialize comprehensive memory protection system
    fprintf(stderr, "DEBUG: About to initialize memory protection system\n");
    CRITICAL_OPERATION_START("memory_protection_init");
    if (memory_protection_init() != MEMORY_PROTECTION_SUCCESS) {
        CRITICAL_OPERATION_END();
        log_exit_reason(EXIT_REASON_MEMORY_ERROR, "Failed to initialize memory protection system");
        daemon_exit(1);
    }
    CRITICAL_OPERATION_END();
    fprintf(stderr, "DEBUG: Memory protection system initialized successfully\n");
    
    DEBUG_TRACE_ENTER();
    DEBUG_TRACE_INFO("Starting Telia Autonomy Network Management Daemon");
    
    // Display version and build information
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "Telia Autonomy Network Management Daemon\n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "%s\n", autonomy_daemon_get_build_info_string());
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "Starting daemon...\n");
    
    DEBUG_TRACE_INFO("Build info: %s", autonomy_daemon_get_build_info_string());
    
    fprintf(stderr, "DEBUG: main() - about to initialize logging system\n");
    
    DEBUG_TRACE_STEP(1, "Initializing uloop");
    uloop_init();
    fprintf(stderr, "uloop initialized\n");
    DEBUG_TRACE_INFO("uloop initialized successfully");
    
    // Initialize logging system
    DEBUG_TRACE_STEP(2, "Initializing logging system");
    logx_init(NULL);
    fprintf(stderr, "Logging system initialized\n");
    DEBUG_TRACE_INFO("Logging system initialized successfully");
    
    DEBUG_TRACE_STEP(3, "Setting up signal handlers");
    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, handle_sig);
    signal(SIGINT, handle_sig);
    
    // Set up crash debugging handlers
    setup_crash_handlers();
    
    fprintf(stderr, "Signal handlers set\n");
    DEBUG_TRACE_INFO("Signal handlers set successfully");

    // Initialize UCI manager and load configuration
    DEBUG_TRACE_STEP(4, "Initializing UCI manager");
    CRITICAL_OPERATION_START("uci_manager_init");
    if (uci_manager_init() != AUTONOMY_SUCCESS) {
        CRITICAL_OPERATION_END();
        DEBUG_TRACE_ERROR("Failed to initialize UCI manager");
        log_exit_reason(EXIT_REASON_CONFIG_ERROR, "Failed to initialize UCI manager");
        daemon_exit(1);
    }
    CRITICAL_OPERATION_END();
    DEBUG_TRACE_INFO("UCI manager initialized successfully");
    
    DEBUG_TRACE_STEP(5, "Loading configuration from UCI");
    fprintf(stderr, "DEBUG: About to call uci_manager_load_config\n");
    
    // Memory protection checkpoint
    MEMORY_CANARY_CHECK();
    STACK_CHECK();
    
    // Exception handling for configuration loading
    TRY() {
        if (uci_manager_load_config(&g_config) != AUTONOMY_SUCCESS) {
        DEBUG_TRACE_WARN("Failed to load configuration from UCI, using defaults");
        fprintf(stderr, "DEBUG: uci_manager_load_config failed, using defaults\n");
        // Use default configuration if UCI loading fails
        const autonomy_config_t *default_config = uci_manager_get_default_config();
        if (default_config) {
            g_config = *default_config;
            DEBUG_TRACE_INFO("Using default configuration");
            fprintf(stderr, "DEBUG: Default configuration applied\n");
        } else {
            fprintf(stderr, "DEBUG: ERROR - No default configuration available\n");
        }
        } else {
            DEBUG_TRACE_INFO("Configuration loaded from UCI successfully");
            fprintf(stderr, "DEBUG: Configuration loaded from UCI successfully\n");
        }
        fprintf(stderr, "DEBUG: Configuration loading completed\n");
    } CATCH() {
        fprintf(stderr, "ERROR: Exception caught during configuration loading\n");
        // Use default configuration on exception
        const autonomy_config_t *default_config = uci_manager_get_default_config();
        if (default_config) {
            g_config = *default_config;
            fprintf(stderr, "DEBUG: Using default configuration after exception\n");
        }
    }

    // Check if another instance is running
    fprintf(stderr, "Skipping PID file check for debugging...\n");
    // if (check_pid_file() == -1) {
    //     fprintf(stderr, "PID file check failed\n");
    //     return 1;
    // }
    // fprintf(stderr, "PID file check passed\n");

    // // Create PID file
    // fprintf(stderr, "Creating PID file...\n");
    // if (create_pid_file() == -1) {
    //     fprintf(stderr, "PID file creation failed\n");
    //     return 1;
    // }
    // fprintf(stderr, "PID file created successfully\n");

    fprintf(stderr, "Attempting to connect to ubus...\n");
    CRITICAL_OPERATION_START("ubus_connect");
    ctx = ubus_connect(NULL);
    if (!ctx) {
        CRITICAL_OPERATION_END();
        log_exit_reason(EXIT_REASON_INIT_FAILURE, "Failed to connect to ubus");
        daemon_exit(1);
    }
    CRITICAL_OPERATION_END();
    fprintf(stderr, "Connected to ubus successfully\n");
    fprintf(stderr, "UBUS context: %p\n", ctx);
    fprintf(stderr, "About to call ubus_add_uloop...\n");
    ubus_add_uloop(ctx);
    fprintf(stderr, "Added uloop to ubus context\n");

    // Load UCI configuration
    if (load_uci_config() == -1) {
        fprintf(stderr, "Failed to load UCI configuration, using defaults.\n");
    }

    // Initialize network health monitoring
    if (perform_network_health_check() != 0) {
        fprintf(stderr, "Failed to initialize network health monitoring.\n");
    }
    
    // Initialize comprehensive network discovery system
    extern int network_discovery_comprehensive_init(void);
    if (network_discovery_comprehensive_init() != AUTONOMY_SUCCESS) {
        fprintf(stderr, "Failed to initialize comprehensive network discovery.\n");
    } else {
        fprintf(stderr, "Comprehensive network discovery initialized successfully.\n");
    }
    
    // Initialize network controller
    extern int network_controller_init(const void* config);
    if (network_controller_init(NULL) != AUTONOMY_SUCCESS) {
        fprintf(stderr, "Failed to initialize network controller.\n");
    } else {
        fprintf(stderr, "Network controller initialized successfully.\n");
    }

    // Initialize GPS health monitoring
    if (perform_gps_health_check() != 0) {
        fprintf(stderr, "Failed to initialize GPS health monitoring.\n");
    }

    // Initialize Starlink tracking module
    g_starlink_tracker = starlink_tracker_init_from_uci(uci_ctx);
    if (g_starlink_tracker) {
        fprintf(stderr, "Starlink tracking module initialized successfully\n");
        
        // Initialize tracking UBUS interface
        fprintf(stderr, "DEBUG: About to call starlink_tracker_ubus_init\n");
        int result = starlink_tracker_ubus_init(ctx, g_starlink_tracker);
        fprintf(stderr, "DEBUG: starlink_tracker_ubus_init returned: %d\n", result);
        if (result == 0) {
            fprintf(stderr, "Starlink tracking UBUS interface registered\n");
        } else {
            fprintf(stderr, "Failed to register Starlink tracking UBUS interface\n");
        }
    } else {
        fprintf(stderr, "Starlink tracking module initialization failed (check credentials)\n");
    }

    // Initialize Starlink gRPC collector
    if (starlink_grpc_collector_init() == AUTONOMY_SUCCESS) {
        fprintf(stderr, "Starlink gRPC collector initialized successfully\n");
        
        // Start gRPC collector thread
        if (starlink_grpc_collector_start() == AUTONOMY_SUCCESS) {
            fprintf(stderr, "Starlink gRPC collector thread started\n");
        } else {
            fprintf(stderr, "Failed to start Starlink gRPC collector thread\n");
        }
    } else {
        fprintf(stderr, "Starlink gRPC collector initialization failed\n");
    }

    // Initialize ML monitoring module
    ml_monitor_config_t ml_config;
    if (ml_monitor_load_config_from_uci(&ml_config) == ML_MONITOR_SUCCESS) {
        if (ml_config.enabled) {
            CRITICAL_OPERATION_START("ml_monitor_init");
            ml_monitor_t *ml_monitor = ml_monitor_init(&ml_config);
            CRITICAL_OPERATION_END();
            if (ml_monitor) {
                fprintf(stderr, "ML monitoring module initialized successfully\n");
                
                // Initialize ML monitoring UBUS interface
                if (ml_monitor_ubus_init(ctx) == ML_MONITOR_SUCCESS) {
                    fprintf(stderr, "ML monitoring UBUS interface registered\n");
                } else {
                    fprintf(stderr, "Failed to initialize ML monitoring UBUS interface\n");
                }
                
                // Initialize Phase 3 enhancements
                fprintf(stderr, "DEBUG: About to call ml_monitor_init_phase3_enhancements\n");
                fprintf(stderr, "DEBUG: ml_monitor pointer: %p\n", (void*)ml_monitor);
                
                int phase3_result = ml_monitor_init_phase3_enhancements(ml_monitor);
                
                fprintf(stderr, "DEBUG: ml_monitor_init_phase3_enhancements returned: %d\n", phase3_result);
                fprintf(stderr, "DEBUG: ml_monitor pointer after Phase 3: %p\n", (void*)ml_monitor);
                if (ml_monitor == NULL) {
                    fprintf(stderr, "ERROR: ml_monitor pointer is NULL after Phase 3!\n");
                    return -1;
                }
                if (phase3_result == ML_MONITOR_SUCCESS) {
                    fprintf(stderr, "ML monitoring Phase 3 enhancements initialized\n");
                    fprintf(stderr, "DEBUG: ml_monitor_init_phase3_enhancements completed successfully\n");
                    
                    fprintf(stderr, "DEBUG: About to print Phase 3 success message\n");
                    fprintf(stderr, "DEBUG: Phase 3 success message printed\n");
                    
                    fprintf(stderr, "DEBUG: About to start Phase 4 initialization\n");
                    // Initialize Phase 4 enhancements
                    fprintf(stderr, "DEBUG: About to call ml_monitor_init_phase4_enhancements\n");
                    if (ml_monitor_init_phase4_enhancements(ml_monitor) == ML_MONITOR_SUCCESS) {
                        fprintf(stderr, "ML monitoring Phase 4 enhancements initialized\n");
                        fprintf(stderr, "DEBUG: ml_monitor_init_phase4_enhancements completed successfully\n");
                        
                        // Initialize Phase 5 mobile optimization
                        fprintf(stderr, "DEBUG: About to call ml_monitor_init_phase5_mobile_system\n");
                        if (ml_monitor_init_phase5_mobile_system(ml_monitor) == ML_MONITOR_SUCCESS) {
                            fprintf(stderr, "ML monitoring Phase 5 mobile optimization initialized\n");
                            fprintf(stderr, "DEBUG: ml_monitor_init_phase5_mobile_system completed successfully\n");
                            
                            // Initialize Phase 6 self-optimization
                            fprintf(stderr, "DEBUG: About to call ml_monitor_init_phase6_self_optimization\n");
                            if (ml_monitor_init_phase6_self_optimization(ml_monitor) == ML_MONITOR_SUCCESS) {
                                fprintf(stderr, "ML monitoring Phase 6 self-optimization initialized\n");
                                fprintf(stderr, "DEBUG: ml_monitor_init_phase6_self_optimization completed successfully\n");
                                
                                // Initialize Phase 7 multi-interface intelligence
                                fprintf(stderr, "DEBUG: About to call ml_monitor_init_phase7_multi_interface\n");
                                if (ml_monitor_init_phase7_multi_interface(ml_monitor) == ML_MONITOR_SUCCESS) {
                                    fprintf(stderr, "ML monitoring Phase 7 multi-interface intelligence initialized\n");
                                    fprintf(stderr, "DEBUG: ml_monitor_init_phase7_multi_interface completed successfully\n");
                                } else {
                                    fprintf(stderr, "ML monitoring Phase 7 initialization failed, using Phase 6 features\n");
                                }
                            } else {
                                fprintf(stderr, "ML monitoring Phase 6 initialization failed, using Phase 5 features\n");
                            }
                        } else {
                            fprintf(stderr, "ML monitoring Phase 5 initialization failed, using Phase 4 features\n");
                        }
                    } else {
                        fprintf(stderr, "ML monitoring Phase 4 initialization failed, using Phase 3 features\n");
                    }
                } else {
                    fprintf(stderr, "ML monitoring Phase 3 initialization failed, using Phase 2 features\n");
                }
                
                // Auto-start ML monitoring if configured
                fprintf(stderr, "DEBUG: About to call ml_monitor_start\n");
                if (ml_monitor_start(ml_monitor) == ML_MONITOR_SUCCESS) {
                    fprintf(stderr, "ML monitoring started automatically with Phase 7 multi-interface intelligence\n");
                    fprintf(stderr, "DEBUG: ml_monitor_start completed successfully\n");
                } else {
                    fprintf(stderr, "ML monitoring initialized but not started (manual start required)\n");
                    fprintf(stderr, "DEBUG: ml_monitor_start failed\n");
                }
            } else {
                fprintf(stderr, "ML monitoring module initialization failed\n");
            }
        } else {
            fprintf(stderr, "ML monitoring module disabled in configuration\n");
        }
    } else {
        fprintf(stderr, "Failed to load ML monitoring configuration\n");
    }

    // Initialize random seed for simulation
    srand(time(NULL));

    // Note: ubus_lookup_object doesn't exist in this UBUS version
    // We'll try to register directly and handle conflicts gracefully
    fprintf(stderr, "Attempting to register autonomy ubus object...\n");

    fprintf(stderr, "Registering autonomy ubus objects step by step...\n");
    
    // Test each method individually to find the problematic one
    fprintf(stderr, "Testing individual UBUS methods...\n");
    
    // Test 1: status method only
    static const struct ubus_method status_methods[] = {
        UBUS_METHOD_NOARG("status", autonomy_status),
    };
    
    static struct ubus_object_type status_obj_type = 
        UBUS_OBJECT_TYPE("autonomy_status", status_methods);
    
    static struct ubus_object status_obj = {
        .name = "autonomy_status",
        .type = &status_obj_type,
        .methods = status_methods,
        .n_methods = ARRAY_SIZE(status_methods),
    };
    
    fprintf(stderr, "Testing status method...\n");
    int ret = ubus_add_object(ctx, &status_obj);
    if (ret) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to add status method: %s (error %d)", ubus_strerror(ret), ret);
        log_exit_reason(EXIT_REASON_INIT_FAILURE, error_msg);
        daemon_exit(1);
    }
    fprintf(stderr, "Status method registered successfully\n");
    
    // Test 2: health method only
    static const struct ubus_method health_methods[] = {
        UBUS_METHOD_NOARG("health", autonomy_health),
    };
    
    static struct ubus_object_type health_obj_type = 
        UBUS_OBJECT_TYPE("autonomy_health", health_methods);
    
    static struct ubus_object health_obj = {
        .name = "autonomy_health",
        .type = &health_obj_type,
        .methods = health_methods,
        .n_methods = ARRAY_SIZE(health_methods),
    };
    
    fprintf(stderr, "Testing health method...\n");
    ret = ubus_add_object(ctx, &health_obj);
    if (ret) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to add health method: %s (error %d)", ubus_strerror(ret), ret);
        log_exit_reason(EXIT_REASON_INIT_FAILURE, error_msg);
        daemon_exit(1);
    }
    fprintf(stderr, "Health method registered successfully\n");
    
    // Test 3: config method only
    static const struct ubus_method config_methods[] = {
        UBUS_METHOD_NOARG("config", autonomy_config),
    };
    
    static struct ubus_object_type config_obj_type = 
        UBUS_OBJECT_TYPE("autonomy_config", config_methods);
    
    static struct ubus_object config_obj = {
        .name = "autonomy_config",
        .type = &config_obj_type,
        .methods = config_methods,
        .n_methods = ARRAY_SIZE(config_methods),
    };
    
    fprintf(stderr, "Testing config method...\n");
    ret = ubus_add_object(ctx, &config_obj);
    if (ret) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to add config method: %s (error %d)", ubus_strerror(ret), ret);
        log_exit_reason(EXIT_REASON_INIT_FAILURE, error_msg);
        daemon_exit(1);
    }
    fprintf(stderr, "Config method registered successfully\n");
    
    fprintf(stderr, "All individual methods registered successfully - testing combined object...\n");
    
    // Now try to register the full object
    fprintf(stderr, "Registering full autonomy ubus object (%d methods)...\n", autonomy_obj.n_methods);
    ret = ubus_add_object(ctx, &autonomy_obj);
    if (ret) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to add full ubus object: %s (error %d)", ubus_strerror(ret), ret);
        log_exit_reason(EXIT_REASON_INIT_FAILURE, error_msg);
        daemon_exit(1);
    }

    fprintf(stderr, "Autonomy daemon started, registered 'autonomy' ubus object\n");
    fprintf(stderr, "Available methods: status, health, config, start, stop, restart, pid_status, log_status, config_status\n");
    fprintf(stderr, "Network methods: network_status, network_interfaces, network_health_check, network_failover\n");
    fprintf(stderr, "GPS methods: gps_status, gps_sources, gps_health_check\n");
    fprintf(stderr, "System methods: system_status, system_health_check, system_health_details, system_maintenance, system_restart_services\n");
    fprintf(stderr, "Starlink methods: starlink_status, starlink_health, starlink_location, starlink_collector_stats, starlink_force_collect\n");
    fprintf(stderr, "Starlink cluster methods: starlink_cluster_status, starlink_cluster_check_failover\n");
    fprintf(stderr, "Starlink tracking methods: starlink_tracker.status, starlink_tracker.predictions, starlink_tracker.satellites\n");
    fprintf(stderr, "Starlink tracking control: starlink_tracker.start_monitoring, starlink_tracker.stop_monitoring, starlink_tracker.update_data\n");
    fprintf(stderr, "ML monitoring methods: ml_monitor.status, ml_monitor.start, ml_monitor.stop, ml_monitor.restart\n");
    fprintf(stderr, "ML monitoring config: ml_monitor.get_config, ml_monitor.set_config\n");
    fprintf(stderr, "ML monitoring data: ml_monitor.get_predictions, ml_monitor.get_statistics, ml_monitor.reset_learning, ml_monitor.export_data\n");
    fprintf(stderr, "ML monitoring Phase 4: ml_monitor.get_ensemble_status, ml_monitor.get_validation_metrics, ml_monitor.trigger_optimization\n");
    fprintf(stderr, "ML monitoring Phase 5: ml_monitor.get_mobile_status, ml_monitor.export_field_data, ml_monitor.enable_field_test\n");
    fprintf(stderr, "ML monitoring Phase 6: ml_monitor.get_system_status, ml_monitor.run_production_validation, ml_monitor.enable_autonomous_mode\n");
    fprintf(stderr, "ML monitoring Phase 7: ml_monitor.get_multi_interface_status, ml_monitor.predict_interface_outage, ml_monitor.update_mwan3_weights, ml_monitor.validate_failover_prediction\n");
    fprintf(stderr, "ML Analytics & Visualization: ml_monitor.get_analytics_summary, ml_monitor.get_interface_score_history, ml_monitor.get_accuracy_trends, ml_monitor.get_impact_summary, ml_monitor.get_current_interface_scores\n");
    fprintf(stderr, "Network Discovery Enhanced: autonomy.network.interfaces_detailed (includes ML recommendations, MWAN3 ping info, enhanced cellular metrics, performance trends)\n");
    fprintf(stderr, "Daemon running, press Ctrl+C to stop\n");
    uloop_run();

    // uloop_run() completed - this is a normal shutdown
    log_exit_reason(EXIT_REASON_NORMAL_SHUTDOWN, "uloop_run() completed - normal daemon shutdown");
    daemon_exit(0);
    
    // Note: Code after daemon_exit() is unreachable - daemon_exit() handles all cleanup
}

// Enhanced debugging functions
static void print_register_state(ucontext_t *context) {
    fprintf(stderr, "=== REGISTER STATE ===\n");
#if defined(__arm__)
    fprintf(stderr, "Program counter: %p\n", (void*)context->uc_mcontext.arm_pc);
    fprintf(stderr, "Stack pointer: %p\n", (void*)context->uc_mcontext.arm_sp);
    fprintf(stderr, "Link register: %p\n", (void*)context->uc_mcontext.arm_lr);
    fprintf(stderr, "Frame pointer: %p\n", (void*)context->uc_mcontext.arm_fp);
    fprintf(stderr, "General registers: ");
    fprintf(stderr, "r0=0x%lx r1=0x%lx r2=0x%lx r3=0x%lx\n", 
            (unsigned long)context->uc_mcontext.arm_r0,
            (unsigned long)context->uc_mcontext.arm_r1,
            (unsigned long)context->uc_mcontext.arm_r2,
            (unsigned long)context->uc_mcontext.arm_r3);
    fprintf(stderr, "r4=0x%lx r5=0x%lx r6=0x%lx r7=0x%lx\n",
            (unsigned long)context->uc_mcontext.arm_r4,
            (unsigned long)context->uc_mcontext.arm_r5,
            (unsigned long)context->uc_mcontext.arm_r6,
            (unsigned long)context->uc_mcontext.arm_r7);
    fprintf(stderr, "r8=0x%lx r9=0x%lx r10=0x%lx\n",
            (unsigned long)context->uc_mcontext.arm_r8,
            (unsigned long)context->uc_mcontext.arm_r9,
            (unsigned long)context->uc_mcontext.arm_r10);
#elif defined(__aarch64__)
    fprintf(stderr, "Program counter: %p\n", (void*)context->uc_mcontext.pc);
    fprintf(stderr, "Stack pointer: %p\n", (void*)context->uc_mcontext.sp);
    fprintf(stderr, "Link register: %p\n", (void*)context->uc_mcontext.regs[30]);
#else
    fprintf(stderr, "Register state not available for this architecture\n");
#endif
    fprintf(stderr, "=== END REGISTER STATE ===\n\n");
}

static void print_stack_trace_arm(ucontext_t *context) {
    fprintf(stderr, "=== ARM STACK TRACE ===\n");
#if defined(__arm__)
    void *pc = (void*)context->uc_mcontext.arm_pc;
    void *sp = (void*)context->uc_mcontext.arm_sp;
    void *lr = (void*)context->uc_mcontext.arm_lr;
    
    fprintf(stderr, "PC (Program Counter): %p\n", pc);
    fprintf(stderr, "SP (Stack Pointer): %p\n", sp);
    fprintf(stderr, "LR (Link Register): %p\n", lr);
    
    // Try to read stack frames
    void **frame_ptr = (void**)sp;
    fprintf(stderr, "Stack frames (up to 10):\n");
    for (int i = 0; i < 10 && frame_ptr; i++) {
        void *return_addr = frame_ptr[0];
        void *next_frame = frame_ptr[1];
        
        if (return_addr == NULL || next_frame == NULL) break;
        if ((uintptr_t)return_addr < 0x1000 || (uintptr_t)return_addr > 0x7fffffff) break;
        
        fprintf(stderr, "  Frame %d: return_addr=%p, next_frame=%p\n", i, return_addr, next_frame);
        frame_ptr = (void**)next_frame;
    }
#else
    fprintf(stderr, "ARM stack trace not available for this architecture\n");
#endif
    fprintf(stderr, "=== END ARM STACK TRACE ===\n\n");
}

static void validate_memory_before_access(void *ptr, size_t size, const char *location) {
    if (!ptr) {
        fprintf(stderr, "ERROR: NULL pointer access at %s\n", location);
        abort();
    }
    
    // Check if pointer is in valid memory range
    if ((uintptr_t)ptr < 0x1000 || (uintptr_t)ptr > 0x7fffffff) {
        fprintf(stderr, "ERROR: Invalid pointer %p at %s\n", ptr, location);
        abort();
    }
    
    // Try to read the first byte to check if memory is accessible
    volatile char test = *(volatile char*)ptr;
    (void)test; // Suppress unused variable warning
    
    // Check if we can read the last byte
    if (size > 0) {
        volatile char test_end = *((volatile char*)ptr + size - 1);
        (void)test_end; // Suppress unused variable warning
    }
}

// NOLINTEND(cert-msc50-cpp,cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
// NOLINTEND(cert-msc51-cpp)
