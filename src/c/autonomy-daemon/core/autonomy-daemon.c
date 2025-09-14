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
#include "../shared/utils/advanced_debug.h"
#include "../utils/debug_trace.h"
#include "../ml/ml_monitor.h"
#include "../ml/ml_monitor_ubus.h"
#include <sys/socket.h>

// NOLINTBEGIN(cert-msc50-cpp,cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
// NOLINTBEGIN(cert-msc51-cpp) - fopen usage is safe with path validation

// Global variables - initialize with safe defaults
autonomy_config_t g_config = {
    .daemon_mode = true,
    .debug_mode = false,
    .log_level = 2, // INFO level
    .log_file = "/var/log/autonomy-daemon.log",
    .config_file = "/etc/config/autonomy",
    .pid_file_timeout = 30,
    .network_check_interval = 30,
    .failover_timeout = 10,
    .auto_failover = true,
    .min_interface_health = 50,
    .mwan3_integration = true,
    .gps_update_interval = 60,
    .gps_timeout = 30,
    .gps_fusion = true,
    .gps_cache_timeout = 300,
    .min_gps_accuracy = 10.0,
    .starlink_check_interval = 60,
    .starlink_health_monitoring = true,
    .starlink_host = "192.168.100.1",
    .starlink_port = 9200,
    .starlink_timeout = 10,
    .system_check_interval = 60,
    .resource_monitoring = true,
    .service_monitoring = true,
    .alert_threshold = 80,
    .notifications_enabled = false,
    .email_from = "",
    .email_to = "",
    .email_smtp = "",
    .webhook_url = "",
    .snow_detection_enabled = false,
    .snow_detection_samples = 10,
    .snow_obstruction_threshold = 0.1,
    .snow_snr_degradation_threshold = 3.0,
    .snow_temperature_threshold = -5.0,
    .snow_verification_time = 300,
    .snow_melt_timeout = 1800,
    .snow_weather_api_key = ""
};

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
static pthread_mutex_t g_exit_mutex = PTHREAD_MUTEX_INITIALIZER;

// TEMPORARY FIX: Disable threading to isolate crash
static bool g_threading_disabled = true;

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
    fprintf(stderr, "=== END BACKTRACE ===\n");
#else
    fprintf(stderr, "\n=== BACKTRACE ===\n");
    fprintf(stderr, "Backtrace not available (not using GNU libc)\n");
    fprintf(stderr, "=== END BACKTRACE ===\n");
#endif
}

static void crash_handler(int sig, siginfo_t *info, void *context) {
    fprintf(stderr, "\n=== CRASH DETECTED ===\n");
    fprintf(stderr, "Signal: %d (%s)\n", sig, strsignal(sig));
    
    // Defensive programming - check if info is valid
    if (info) {
        fprintf(stderr, "Signal code: %d\n", info->si_code);
        fprintf(stderr, "Fault address: %p\n", info->si_addr);
    } else {
        fprintf(stderr, "Signal code: <info is NULL>\n");
        fprintf(stderr, "Fault address: <info is NULL>\n");
    }
    
    fprintf(stderr, "PID: %d\n", getpid());
    fprintf(stderr, "UID: %d\n", getuid());
    
    // CRITICAL: Skip memory corruption analysis to prevent recursive crashes
    fprintf(stderr, "\n=== MEMORY CORRUPTION ANALYSIS ===\n");
    fprintf(stderr, "Memory corruption analysis disabled to prevent recursive crashes\n");
    fprintf(stderr, "=== END MEMORY CORRUPTION ANALYSIS ===\n");
    
    // Enhanced signal-specific information
    switch (sig) {
        case SIGSEGV:
            if (info) {
                fprintf(stderr, "SEGFAULT: Invalid memory access at %p\n", info->si_addr);
                if (info->si_code == SEGV_MAPERR) {
                    fprintf(stderr, "Cause: Address not mapped to object\n");
                } else if (info->si_code == SEGV_ACCERR) {
                    fprintf(stderr, "Cause: Invalid permissions for mapped object\n");
                }
            } else {
                fprintf(stderr, "SEGFAULT: Invalid memory access (info is NULL)\n");
            }
            break;
        case SIGBUS:
            if (info) {
                fprintf(stderr, "BUS ERROR: Invalid memory access at %p\n", info->si_addr);
            } else {
                fprintf(stderr, "BUS ERROR: Invalid memory access (info is NULL)\n");
            }
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
    
    // Print backtrace for debugging
    print_backtrace();
    
    // Print memory info
    print_memory_info();
    
    // Simplified crash info to prevent recursive crashes
    fprintf(stderr, "\n=== SIMPLIFIED CRASH INFO ===\n");
    fprintf(stderr, "Current function: crash_handler\n");
    fprintf(stderr, "Signal: %d (%s)\n", sig, strsignal(sig));
    if (info) {
        fprintf(stderr, "Fault address: %p\n", info->si_addr);
    } else {
        fprintf(stderr, "Fault address: <info is NULL>\n");
    }
    fprintf(stderr, "=== END SIMPLIFIED CRASH INFO ===\n");
    
    fprintf(stderr, "=== CRASH END ===\n");
    
    
    // Log the crash reason before exiting
    exit_reason_t reason = EXIT_REASON_UNKNOWN;
    char message[256];
    
    switch (sig) {
        case SIGSEGV:
            reason = EXIT_REASON_SIGNAL_SEGV;
            if (info) {
                snprintf(message, sizeof(message), "Segmentation fault at %p (signal %d)", info->si_addr, sig);
            } else {
                snprintf(message, sizeof(message), "Segmentation fault (info is NULL) (signal %d)", sig);
            }
            break;
        case SIGBUS:
            reason = EXIT_REASON_SIGNAL_BUS;
            if (info) {
                snprintf(message, sizeof(message), "Bus error at %p (signal %d)", info->si_addr, sig);
            } else {
                snprintf(message, sizeof(message), "Bus error (info is NULL) (signal %d)", sig);
            }
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
    
    LOGX_DEBUG_MSG("Crash handlers installed");
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
        fprintf(stderr, "=== END MEMORY STATUS ===\n");
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
    pthread_mutex_lock(&g_exit_mutex);
    g_exit_reason = reason;
    if (message) {
        strncpy(g_exit_message, message, sizeof(g_exit_message) - 1);
        g_exit_message[sizeof(g_exit_message) - 1] = '\0';
    }
    pthread_mutex_unlock(&g_exit_mutex);
    
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
    
    // Log to stderr (always visible) - use fprintf for critical errors
    fprintf(stderr, "\n=== DAEMON EXIT ===\n");
    fprintf(stderr, "Exit Reason: %s\n", reason_str);
    fprintf(stderr, "Exit Message: %s\n", g_exit_message);
    fprintf(stderr, "Timestamp: %lld\n", (long long)time(NULL));
    fprintf(stderr, "PID: %d\n", getpid());
    fprintf(stderr, "==================\n");
    
    // Also log via LOGX if available
    LOGX_ERROR_MSG("DAEMON EXIT: Reason=%s, Message=%s, PID=%d", reason_str, g_exit_message, getpid());
}

// Comprehensive daemon exit function
static void daemon_exit(int exit_code) {
    LOGX_INFO_MSG("Performing daemon cleanup before exit...");
    
    // Cleanup UBUS context
    if (ctx) {
        LOGX_INFO_MSG("Cleaning up UBUS context...");
        ubus_free(ctx);
        ctx = NULL;
    }
    
    // Cleanup UCI context
    if (uci_ctx) {
        LOGX_INFO_MSG("Cleaning up UCI context...");
        uci_free_context(uci_ctx);
        uci_ctx = NULL;
    }
    
    // Cleanup UCI manager
    LOGX_INFO_MSG("Cleaning up UCI manager...");
    uci_manager_cleanup();
    
    // Cleanup hang detector
    LOGX_INFO_MSG("Cleaning up hang detector...");
    hang_detector_cleanup();
    
    // Remove PID file
    LOGX_INFO_MSG("Removing PID file...");
    remove_pid_file();
    
    // Stop uloop
    LOGX_INFO_MSG("Stopping uloop...");
    uloop_done();
    
    // Final exit message
    LOGX_INFO_MSG("Daemon cleanup completed. Exiting with code %d.", exit_code);
    
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
    LOGX_DEBUG_MSG("About to initialize hang detection system");
    if (hang_detector_init() != 0) {
        log_exit_reason(EXIT_REASON_INIT_FAILURE, "Failed to initialize hang detection system");
        daemon_exit(1);
    }
    LOGX_DEBUG_MSG("Hang detection system initialized successfully");
    
    // Start hang detection watchdog
    LOGX_DEBUG_MSG("Starting hang detection watchdog");
    if (hang_detector_start_watchdog() != 0) {
        log_exit_reason(EXIT_REASON_INIT_FAILURE, "Failed to start hang detection watchdog");
        daemon_exit(1);
    }
    LOGX_DEBUG_MSG("Hang detection watchdog started successfully");
    
    // Initialize comprehensive memory protection system
    LOGX_DEBUG_MSG("About to initialize memory protection system");
    CRITICAL_OPERATION_START("memory_protection_init");
    if (memory_protection_init() != MEMORY_PROTECTION_SUCCESS) {
        CRITICAL_OPERATION_END();
        log_exit_reason(EXIT_REASON_MEMORY_ERROR, "Failed to initialize memory protection system");
        daemon_exit(1);
    }
    CRITICAL_OPERATION_END();
    LOGX_DEBUG_MSG("Memory protection system initialized successfully");
    
    DEBUG_TRACE_ENTER();
    DEBUG_TRACE_INFO("Starting Autonomy Network Management Daemon");
    
    // Initialize advanced debugging system
    advanced_debug_init();
    
    // Display version and build information
    LOGX_INFO_MSG("========================================");
    LOGX_INFO_MSG("Autonomy Network Management Daemon");
    LOGX_INFO_MSG("========================================");
    LOGX_INFO_MSG("%s", autonomy_daemon_get_build_info_string());
    LOGX_INFO_MSG("========================================");
    LOGX_INFO_MSG("Starting daemon...");
    
    DEBUG_TRACE_INFO("Build info: %s", autonomy_daemon_get_build_info_string());
    
    LOGX_DEBUG_MSG("main() - about to initialize logging system");
    
    DEBUG_TRACE_STEP(1, "Initializing uloop");
    uloop_init();
    LOGX_DEBUG_MSG("uloop initialized");
    DEBUG_TRACE_INFO("uloop initialized successfully");
    
    // Initialize logging system
    DEBUG_TRACE_STEP(2, "Initializing logging system");
    logx_init(NULL);
    LOGX_DEBUG_MSG("Logging system initialized");
    DEBUG_TRACE_INFO("Logging system initialized successfully");
    
    DEBUG_TRACE_STEP(3, "Setting up signal handlers");
    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, handle_sig);
    signal(SIGINT, handle_sig);
    
    // Set up crash debugging handlers
    setup_crash_handlers();
    
    LOGX_DEBUG_MSG("Signal handlers set");
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
    LOGX_DEBUG_MSG("About to call uci_manager_load_config");
    
    // Memory protection checkpoint
    MEMORY_CANARY_CHECK();
    STACK_CHECK();
    
    // Exception handling for configuration loading
    TRY() {
        if (uci_manager_load_config(&g_config) != AUTONOMY_SUCCESS) {
        DEBUG_TRACE_WARN("Failed to load configuration from UCI, using defaults");
        LOGX_DEBUG_MSG("uci_manager_load_config failed, using defaults");
        // Use default configuration if UCI loading fails
        const autonomy_config_t *default_config = uci_manager_get_default_config();
        if (default_config) {
            g_config = *default_config;
            DEBUG_TRACE_INFO("Using default configuration");
            LOGX_DEBUG_MSG("Default configuration applied");
        } else {
            LOGX_ERROR_MSG("ERROR - No default configuration available");
        }
        } else {
            DEBUG_TRACE_INFO("Configuration loaded from UCI successfully");
            LOGX_DEBUG_MSG("Configuration loaded from UCI successfully");
        }
        LOGX_DEBUG_MSG("Configuration loading completed");
    } CATCH() {
        LOGX_ERROR_MSG("Exception caught during configuration loading");
        // Use default configuration on exception
        const autonomy_config_t *default_config = uci_manager_get_default_config();
        if (default_config) {
            g_config = *default_config;
            LOGX_DEBUG_MSG("Using default configuration after exception");
        }
    }

    // Check if another instance is running
    LOGX_DEBUG_MSG("Skipping PID file check for debugging...");
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

    LOGX_DEBUG_MSG("Attempting to connect to ubus...");
    CRITICAL_OPERATION_START("ubus_connect");
    ctx = ubus_connect(NULL);
    if (!ctx) {
        CRITICAL_OPERATION_END();
        log_exit_reason(EXIT_REASON_INIT_FAILURE, "Failed to connect to ubus");
        daemon_exit(1);
    }
    CRITICAL_OPERATION_END();
    LOGX_DEBUG_MSG("Connected to ubus successfully");
    LOGX_DEBUG_MSG("UBUS context: %p", ctx);
    // Load UCI configuration BEFORE setting up UBUS event handling
    // This ensures g_config is initialized before any UBUS methods can be called
    if (load_uci_config() == -1) {
        LOGX_WARN_MSG("Failed to load UCI configuration, using defaults.");
    }
    
    LOGX_DEBUG_MSG("About to call ubus_add_uloop...");
    ubus_add_uloop(ctx);
    LOGX_DEBUG_MSG("Added uloop to ubus context");

    // Initialize network health monitoring
    if (perform_network_health_check() != 0) {
        LOGX_WARN_MSG("Failed to initialize network health monitoring.");
    }
    
    // Initialize comprehensive network discovery system
    extern int network_discovery_comprehensive_init(void);
    if (network_discovery_comprehensive_init() != AUTONOMY_SUCCESS) {
        LOGX_WARN_MSG("Failed to initialize comprehensive network discovery.");
    } else {
        LOGX_INFO_MSG("Comprehensive network discovery initialized successfully.");
    }
    
    // Initialize network controller
    extern int network_controller_init(const void* config);
    if (network_controller_init(NULL) != AUTONOMY_SUCCESS) {
        LOGX_WARN_MSG("Failed to initialize network controller.");
    } else {
        LOGX_INFO_MSG("Network controller initialized successfully.");
    }

    // Initialize GPS health monitoring
    if (perform_gps_health_check() != 0) {
        LOGX_WARN_MSG("Failed to initialize GPS health monitoring.");
    }

    // Initialize Starlink tracking module
    g_starlink_tracker = starlink_tracker_init_from_uci(uci_ctx);
    if (g_starlink_tracker) {
        LOGX_INFO_MSG("Starlink tracking module initialized successfully");
        
        // Initialize tracking UBUS interface
        LOGX_DEBUG_MSG("About to call starlink_tracker_ubus_init");
        int result = starlink_tracker_ubus_init(ctx, g_starlink_tracker);
        LOGX_DEBUG_MSG("starlink_tracker_ubus_init returned: %d", result);
        if (result == 0) {
            LOGX_INFO_MSG("Starlink tracking UBUS interface registered");
        } else {
            LOGX_WARN_MSG("Failed to register Starlink tracking UBUS interface");
        }
    } else {
        LOGX_WARN_MSG("Starlink tracking module initialization failed (check credentials)");
    }

    // Initialize Starlink gRPC collector
    if (starlink_grpc_collector_init() == AUTONOMY_SUCCESS) {
        LOGX_INFO_MSG("Starlink gRPC collector initialized successfully");
        
        // TEMPORARY FIX: Disable threading to isolate crash
        if (!g_threading_disabled) {
            // Start gRPC collector thread
            if (starlink_grpc_collector_start() == AUTONOMY_SUCCESS) {
                LOGX_INFO_MSG("Starlink gRPC collector thread started");
            } else {
                LOGX_WARN_MSG("Failed to start Starlink gRPC collector thread");
            }
        } else {
            LOGX_WARN_MSG("Starlink gRPC collector thread DISABLED for crash debugging");
        }
    } else {
        LOGX_WARN_MSG("Starlink gRPC collector initialization failed");
    }

    // Initialize ML monitoring module - TEMPORARILY DISABLED FOR DEBUGGING
    ml_monitor_config_t ml_config;
    if (ml_monitor_load_config_from_uci(&ml_config) == ML_MONITOR_SUCCESS) {
        if (false) { // TEMPORARILY DISABLED: ml_config.enabled
            CRITICAL_OPERATION_START("ml_monitor_init");
            ml_monitor_t *ml_monitor = ml_monitor_init(&ml_config);
            CRITICAL_OPERATION_END();
            if (ml_monitor) {
                LOGX_INFO_MSG("ML monitoring module initialized successfully");
                
                // ML monitoring UBUS interface will be initialized after main UBUS object registration
                
                // Initialize Phase 3 enhancements
                LOGX_DEBUG_MSG("About to call ml_monitor_init_phase3_enhancements");
                LOGX_DEBUG_MSG("ml_monitor pointer: %p", (void*)ml_monitor);
                
                int phase3_result = ml_monitor_init_phase3_enhancements(ml_monitor);
                
                LOGX_DEBUG_MSG("ml_monitor_init_phase3_enhancements returned: %d", phase3_result);
                LOGX_DEBUG_MSG("ml_monitor pointer after Phase 3: %p", (void*)ml_monitor);
                if (ml_monitor == NULL) {
                    LOGX_ERROR_MSG("ERROR: ml_monitor pointer is NULL after Phase 3!");
                    return -1;
                }
                if (phase3_result == ML_MONITOR_SUCCESS) {
                    LOGX_INFO_MSG("ML monitoring Phase 3 enhancements initialized");
                    LOGX_DEBUG_MSG("ml_monitor_init_phase3_enhancements completed successfully");
                    
                    LOGX_DEBUG_MSG("About to print Phase 3 success message");
                    LOGX_DEBUG_MSG("Phase 3 success message printed");
                    
                    LOGX_DEBUG_MSG("About to start Phase 4 initialization");
                    // Initialize Phase 4 enhancements
                    LOGX_DEBUG_MSG("About to call ml_monitor_init_phase4_enhancements");
                    if (ml_monitor_init_phase4_enhancements(ml_monitor) == ML_MONITOR_SUCCESS) {
                        LOGX_INFO_MSG("ML monitoring Phase 4 enhancements initialized");
                        LOGX_DEBUG_MSG("ml_monitor_init_phase4_enhancements completed successfully");
                        
                        // Initialize Phase 5 mobile optimization
                        LOGX_DEBUG_MSG("About to call ml_monitor_init_phase5_mobile_system");
                        if (ml_monitor_init_phase5_mobile_system(ml_monitor) == ML_MONITOR_SUCCESS) {
                            LOGX_INFO_MSG("ML monitoring Phase 5 mobile optimization initialized");
                            LOGX_DEBUG_MSG("ml_monitor_init_phase5_mobile_system completed successfully");
                            
                            // Initialize Phase 6 self-optimization
                            LOGX_DEBUG_MSG("About to call ml_monitor_init_phase6_self_optimization");
                            if (ml_monitor_init_phase6_self_optimization(ml_monitor) == ML_MONITOR_SUCCESS) {
                                LOGX_INFO_MSG("ML monitoring Phase 6 self-optimization initialized");
                                LOGX_DEBUG_MSG("ml_monitor_init_phase6_self_optimization completed successfully");
                                
                                // Initialize Phase 7 multi-interface intelligence
                                LOGX_DEBUG_MSG("About to call ml_monitor_init_phase7_multi_interface");
                                if (ml_monitor_init_phase7_multi_interface(ml_monitor) == ML_MONITOR_SUCCESS) {
                                    LOGX_INFO_MSG("ML monitoring Phase 7 multi-interface intelligence initialized");
                                    LOGX_DEBUG_MSG("ml_monitor_init_phase7_multi_interface completed successfully");
                                } else {
                                    LOGX_WARN_MSG("ML monitoring Phase 7 initialization failed, using Phase 6 features");
                                }
                            } else {
                                LOGX_WARN_MSG("ML monitoring Phase 6 initialization failed, using Phase 5 features");
                            }
                        } else {
                            LOGX_WARN_MSG("ML monitoring Phase 5 initialization failed, using Phase 4 features");
                        }
                    } else {
                        LOGX_WARN_MSG("ML monitoring Phase 4 initialization failed, using Phase 3 features");
                    }
                } else {
                    LOGX_WARN_MSG("ML monitoring Phase 3 initialization failed, using Phase 2 features");
                }
                
                // Auto-start ML monitoring if configured
                LOGX_DEBUG_MSG("About to call ml_monitor_start");
                if (ml_monitor_start(ml_monitor) == ML_MONITOR_SUCCESS) {
                    LOGX_INFO_MSG("ML monitoring started automatically with Phase 7 multi-interface intelligence");
                    LOGX_DEBUG_MSG("ml_monitor_start completed successfully");
                } else {
                    LOGX_WARN_MSG("ML monitoring initialized but not started (manual start required)");
                    LOGX_DEBUG_MSG("ml_monitor_start failed");
                }
            } else {
                LOGX_ERROR_MSG("ML monitoring module initialization failed");
            }
        } else {
            LOGX_INFO_MSG("ML monitoring module disabled in configuration");
        }
    } else {
        LOGX_WARN_MSG("Failed to load ML monitoring configuration");
    }

    // AGGRESSIVE DEBUGGING - Force immediate output after ML monitor UCI loading
    fprintf(stderr, "=== AGGRESSIVE DEBUG: ML monitor UCI loading completed ===\n");
    fflush(stderr);

    // Initialize random seed for simulation
    fprintf(stderr, "=== AGGRESSIVE DEBUG: About to call srand(time(NULL)) ===\n");
    fflush(stderr);
    
    srand(time(NULL));
    
    fprintf(stderr, "=== AGGRESSIVE DEBUG: srand(time(NULL)) completed ===\n");
    fflush(stderr);

    // Note: ubus_lookup_object doesn't exist in this UBUS version
    // We'll try to register directly and handle conflicts gracefully
    fprintf(stderr, "=== AGGRESSIVE DEBUG: About to start UBUS registration ===\n");
    fflush(stderr);
    
    LOGX_DEBUG_MSG("Attempting to register autonomy ubus object...");

    fprintf(stderr, "=== AGGRESSIVE DEBUG: About to register autonomy ubus objects step by step ===\n");
    fflush(stderr);
    
    LOGX_DEBUG_MSG("Registering autonomy ubus objects step by step...");
    
    // Test each method individually to find the problematic one
    fprintf(stderr, "=== AGGRESSIVE DEBUG: About to test individual UBUS methods ===\n");
    fflush(stderr);
    
    LOGX_DEBUG_MSG("Testing individual UBUS methods...");
    
    // AGGRESSIVE DEBUGGING - Validate UBUS context before registration
    fprintf(stderr, "=== AGGRESSIVE DEBUG: Validating UBUS context before registration ===\n");
    fflush(stderr);
    
    if (!ctx) {
        fprintf(stderr, "=== AGGRESSIVE DEBUG: CRITICAL ERROR - UBUS context is NULL! ===\n");
        fflush(stderr);
        LOGX_FATAL_MSG("CRITICAL: UBUS context is NULL before registration");
        log_exit_reason(EXIT_REASON_INIT_FAILURE, "UBUS context is NULL");
        daemon_exit(1);
    }
    
    fprintf(stderr, "=== AGGRESSIVE DEBUG: UBUS context is valid: %p ===\n", ctx);
    fflush(stderr);
    
    // Test 1: status method only
    fprintf(stderr, "=== AGGRESSIVE DEBUG: Creating status method array ===\n");
    fflush(stderr);
    
    static const struct ubus_method status_methods[] = {
        UBUS_METHOD_NOARG("status", autonomy_status),
    };
    
    fprintf(stderr, "=== AGGRESSIVE DEBUG: Creating status object type ===\n");
    fflush(stderr);
    
    static struct ubus_object_type status_obj_type = 
        UBUS_OBJECT_TYPE("autonomy_status", status_methods);
    
    fprintf(stderr, "=== AGGRESSIVE DEBUG: Creating status object ===\n");
    fflush(stderr);
    
    static struct ubus_object status_obj = {
        .name = "autonomy_status",
        .type = &status_obj_type,
        .methods = status_methods,
        .n_methods = ARRAY_SIZE(status_methods),
    };
    
    fprintf(stderr, "=== AGGRESSIVE DEBUG: About to call ubus_add_object ===\n");
    fflush(stderr);
    
    LOGX_DEBUG_MSG("Testing status method...");
    int ret = ubus_add_object(ctx, &status_obj);
    
    fprintf(stderr, "=== AGGRESSIVE DEBUG: ubus_add_object returned: %d ===\n", ret);
    fflush(stderr);
    
    if (ret) {
        fprintf(stderr, "=== AGGRESSIVE DEBUG: ubus_add_object failed with error %d ===\n", ret);
        fflush(stderr);
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to add status method: %s (error %d)", ubus_strerror(ret), ret);
        log_exit_reason(EXIT_REASON_INIT_FAILURE, error_msg);
        daemon_exit(1);
    }
    
    fprintf(stderr, "=== AGGRESSIVE DEBUG: Status method registered successfully ===\n");
    fflush(stderr);
    
    LOGX_DEBUG_MSG("Status method registered successfully");
    
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
    
    LOGX_DEBUG_MSG("Testing health method...");
    ret = ubus_add_object(ctx, &health_obj);
    if (ret) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to add health method: %s (error %d)", ubus_strerror(ret), ret);
        log_exit_reason(EXIT_REASON_INIT_FAILURE, error_msg);
        daemon_exit(1);
    }
    LOGX_DEBUG_MSG("Health method registered successfully");
    
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
    
    LOGX_DEBUG_MSG("Testing config method...");
    ret = ubus_add_object(ctx, &config_obj);
    if (ret) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to add config method: %s (error %d)", ubus_strerror(ret), ret);
        log_exit_reason(EXIT_REASON_INIT_FAILURE, error_msg);
        daemon_exit(1);
    }
    LOGX_DEBUG_MSG("Config method registered successfully");
    
    LOGX_DEBUG_MSG("All individual methods registered successfully - testing combined object...");
    
    // Now try to register the full object
    LOGX_DEBUG_MSG("Registering full autonomy ubus object (%d methods)...", autonomy_obj.n_methods);
    ret = ubus_add_object(ctx, &autonomy_obj);
    if (ret) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to add full ubus object: %s (error %d)", ubus_strerror(ret), ret);
        log_exit_reason(EXIT_REASON_INIT_FAILURE, error_msg);
        daemon_exit(1);
    }

    LOGX_INFO_MSG("Autonomy daemon started, registered 'autonomy' ubus object");
    
    // Initialize ML monitoring UBUS interface after main UBUS object is registered - TEMPORARILY DISABLED
    // if (ml_monitor_ubus_init(ctx) == ML_MONITOR_SUCCESS) {
    //     LOGX_INFO_MSG("ML monitoring UBUS interface registered");
    // } else {
    //     LOGX_WARN_MSG("Failed to initialize ML monitoring UBUS interface");
    // }
    LOGX_INFO_MSG("ML monitoring UBUS interface temporarily disabled for debugging");
    
    LOGX_INFO_MSG("Available methods: status, health, config, start, stop, restart, pid_status, log_status, config_status");
    LOGX_INFO_MSG("Network methods: network_status, network_interfaces, network_health_check, network_failover");
    LOGX_INFO_MSG("GPS methods: gps_status, gps_sources, gps_health_check");
    
    // AGGRESSIVE DEBUGGING - Force immediate output after GPS methods
    fprintf(stderr, "=== AGGRESSIVE DEBUG: GPS methods logged successfully ===\n");
    fflush(stderr);
    
    // AGGRESSIVE DEBUGGING - Test LOGX system before System methods
    fprintf(stderr, "=== AGGRESSIVE DEBUG: About to call LOGX_INFO_MSG for System methods ===\n");
    fflush(stderr);
    
    // Test with a simple string first
    fprintf(stderr, "=== AGGRESSIVE DEBUG: Testing simple LOGX call ===\n");
    fflush(stderr);
    LOGX_INFO_MSG("Test message");
    
    fprintf(stderr, "=== AGGRESSIVE DEBUG: Simple LOGX call completed ===\n");
    fflush(stderr);
    
    // Now try the problematic System methods message
    fprintf(stderr, "=== AGGRESSIVE DEBUG: About to call LOGX_INFO_MSG for System methods (long message) ===\n");
    fflush(stderr);
    
    LOGX_INFO_MSG("System methods: system_status, system_health_check, system_health_details, system_maintenance, system_restart_services");
    
    // AGGRESSIVE DEBUGGING - Force immediate output after system methods
    fprintf(stderr, "=== AGGRESSIVE DEBUG: System methods logged successfully ===\n");
    fflush(stderr);
    
    LOGX_INFO_MSG("Starlink methods: starlink_status, starlink_health, starlink_location, starlink_collector_stats, starlink_force_collect");
    
    // AGGRESSIVE DEBUGGING - Force immediate output after Starlink methods
    fprintf(stderr, "=== AGGRESSIVE DEBUG: Starlink methods logged successfully ===\n");
    fflush(stderr);
    LOGX_INFO_MSG("Starlink cluster methods: starlink_cluster_status, starlink_cluster_check_failover");
    
    // AGGRESSIVE DEBUGGING - Force immediate output after Starlink cluster methods
    fprintf(stderr, "=== AGGRESSIVE DEBUG: Starlink cluster methods logged successfully ===\n");
    fflush(stderr);
    
    LOGX_INFO_MSG("Starlink tracking methods: starlink_tracker.status, starlink_tracker.predictions, starlink_tracker.satellites");
    
    // AGGRESSIVE DEBUGGING - Force immediate output after Starlink tracking methods
    fprintf(stderr, "=== AGGRESSIVE DEBUG: Starlink tracking methods logged successfully ===\n");
    fflush(stderr);
    
    LOGX_INFO_MSG("Starlink tracking control: starlink_tracker.start_monitoring, starlink_tracker.stop_monitoring, starlink_tracker.update_data");
    
    // AGGRESSIVE DEBUGGING - Force immediate output after Starlink tracking control
    fprintf(stderr, "=== AGGRESSIVE DEBUG: Starlink tracking control logged successfully ===\n");
    fflush(stderr);
    
    LOGX_INFO_MSG("ML monitoring methods: ml_monitor.status, ml_monitor.start, ml_monitor.stop, ml_monitor.restart");
    
    // AGGRESSIVE DEBUGGING - Force immediate output after ML monitoring methods
    fprintf(stderr, "=== AGGRESSIVE DEBUG: ML monitoring methods logged successfully ===\n");
    fflush(stderr);
    
    LOGX_INFO_MSG("ML monitoring config: ml_monitor.get_config, ml_monitor.set_config");
    LOGX_INFO_MSG("ML monitoring data: ml_monitor.get_predictions, ml_monitor.get_statistics, ml_monitor.reset_learning, ml_monitor.export_data");
    LOGX_INFO_MSG("ML monitoring Phase 4: ml_monitor.get_ensemble_status, ml_monitor.get_validation_metrics, ml_monitor.trigger_optimization");
    LOGX_INFO_MSG("ML monitoring Phase 5: ml_monitor.get_mobile_status, ml_monitor.export_field_data, ml_monitor.enable_field_test");
    LOGX_INFO_MSG("ML monitoring Phase 6: ml_monitor.get_system_status, ml_monitor.run_production_validation, ml_monitor.enable_autonomous_mode");
    LOGX_INFO_MSG("ML monitoring Phase 7: ml_monitor.get_multi_interface_status, ml_monitor.predict_interface_outage, ml_monitor.update_mwan3_weights, ml_monitor.validate_failover_prediction");
    LOGX_INFO_MSG("ML Analytics & Visualization: ml_monitor.get_analytics_summary, ml_monitor.get_interface_score_history, ml_monitor.get_accuracy_trends, ml_monitor.get_impact_summary, ml_monitor.get_current_interface_scores");
    LOGX_INFO_MSG("Network Discovery Enhanced: autonomy.network.interfaces_detailed (includes ML recommendations, MWAN3 ping info, enhanced cellular metrics, performance trends)");
    
    // AGGRESSIVE DEBUGGING - Force immediate output after all ML methods
    fprintf(stderr, "=== AGGRESSIVE DEBUG: All ML methods logged successfully ===\n");
    fflush(stderr);
    
    LOGX_INFO_MSG("Daemon running, press Ctrl+C to stop");
    
    // AGGRESSIVE DEBUGGING - Force immediate output after final message
    fprintf(stderr, "=== AGGRESSIVE DEBUG: Final daemon message logged successfully ===\n");
    fflush(stderr);
    
    // Add debugging before uloop_run
    LOGX_DEBUG_MSG("About to call uloop_run()");
    LOGX_DEBUG_MSG("UBUS context: %p", ctx);
    LOGX_DEBUG_MSG("UCI context: %p", uci_ctx);
    
    // Validate critical pointers before entering uloop
    if (!ctx) {
        LOGX_FATAL_MSG("CRITICAL: UBUS context is NULL before uloop_run()");
        log_exit_reason(EXIT_REASON_INIT_FAILURE, "UBUS context is NULL");
        daemon_exit(1);
    }
    
    // AGGRESSIVE DEBUGGING - Force immediate output
    fprintf(stderr, "=== AGGRESSIVE DEBUG: About to call uloop_run() ===\n");
    fflush(stderr);
    
    LOGX_DEBUG_MSG("Calling uloop_run()...");
    LOGX_DEBUG_MSG("Main event loop starting - all modules initialized");
    LOGX_DEBUG_MSG("UBUS context: %p", ctx);
    LOGX_DEBUG_MSG("UCI context: %p", uci_ctx);
    LOGX_DEBUG_MSG("Global state: %p", &g_state);
    LOGX_DEBUG_MSG("Global config: %p", &g_config);
    
    // Force log flush
    fflush(stdout);
    fflush(stderr);
    
    // Validate critical pointers one more time
    if (!ctx) {
        LOGX_FATAL_MSG("CRITICAL: UBUS context is NULL before uloop_run()");
        log_exit_reason(EXIT_REASON_INIT_FAILURE, "UBUS context is NULL");
        daemon_exit(1);
    }
    
    // AGGRESSIVE DEBUGGING - Force immediate output before uloop_run
    fprintf(stderr, "=== AGGRESSIVE DEBUG: Entering uloop_run() - daemon will now handle events ===\n");
    fflush(stderr);
    
    LOGX_DEBUG_MSG("Entering uloop_run() - daemon will now handle events");
    
    // Force log flush before entering uloop
    fflush(stdout);
    fflush(stderr);
    
    uloop_run();
    
    // AGGRESSIVE DEBUGGING - Force immediate output after uloop_run
    fprintf(stderr, "=== AGGRESSIVE DEBUG: uloop_run() returned - daemon shutting down normally ===\n");
    fflush(stderr);
    
    LOGX_DEBUG_MSG("uloop_run() returned - daemon shutting down normally");

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
    fprintf(stderr, "General registers:\n");
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
    fprintf(stderr, "=== END REGISTER STATE ===\n");
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
    fprintf(stderr, "=== END ARM STACK TRACE ===\n");
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
