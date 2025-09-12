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
#include "../utils/debug_trace.h"
#include "../ml/ml_monitor.h"
#include "../ml/ml_monitor_ubus.h"
#include <sys/socket.h>

// Global variables
autonomy_config_t g_config;

// Forward declarations
static void print_memory_info(void\n"\n"\n"\n"\n"\n"\n"\n");
static void print_backtrace(void\n"\n"\n"\n"\n"\n"\n"\n");
static void crash_handler(int sig, siginfo_t *info, void *context\n"\n"\n"\n"\n"\n"\n"\n");
static void setup_crash_handlers(void\n"\n"\n"\n"\n"\n"\n"\n");

// Crash debugging functions
static void print_backtrace(void) {
#ifdef __GLIBC__
    void *array[20];
    size_t size;
    char **strings;
    size_t i;

    fprintf(stderr, "\n=== BACKTRACE ===\n"\n"\n"\n"\n"\n"\n"\n"\n");
    size = backtrace(array, 20\n"\n"\n"\n"\n"\n"\n"\n");
    strings = backtrace_symbols(array, size\n"\n"\n"\n"\n"\n"\n"\n");

    if (strings != NULL) {
        for (i = 0; i < size; i++) {
            fprintf(stderr, "[%zu] %s\n", i, strings[i]\n"\n"\n"\n"\n"\n"\n"\n");
        }
        free(strings\n"\n"\n"\n"\n"\n"\n"\n");
    }
    fprintf(stderr, "=== END BACKTRACE ===\n\n"\n"\n"\n"\n"\n"\n"\n"\n");
#else
    fprintf(stderr, "\n=== BACKTRACE ===\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "Backtrace not available (not using GNU libc)\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "=== END BACKTRACE ===\n\n"\n"\n"\n"\n"\n"\n"\n"\n");
#endif
}

static void crash_handler(int sig, siginfo_t *info, void *context) {
    fprintf(stderr, "\n=== CRASH DETECTED ===\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "Signal: %d (%s)\n", sig, strsignal(sig)\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "Signal code: %d\n", info->si_code\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "Fault address: %p\n", info->si_addr\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "PID: %d\n", getpid()\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "UID: %d\n", getuid()\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Print memory map info if available
    fprintf(stderr, "\n=== MEMORY MAP ===\n"\n"\n"\n"\n"\n"\n"\n"\n");
    FILE *maps = fopen("/proc/self/maps", "r"\n"\n"\n"\n"\n"\n"\n"\n");
    if (maps) {
        char line[256];
        while (fgets(line, sizeof(line), maps)) {
            fprintf(stderr, "%s", line\n"\n"\n"\n"\n"\n"\n"\n");
        }
        fclose(maps\n"\n"\n"\n"\n"\n"\n"\n");
    }
    fprintf(stderr, "=== END MEMORY MAP ===\n\n"\n"\n"\n"\n"\n"\n"\n"\n");
    
    print_memory_info(\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Print memory debugging information
    fprintf(stderr, "\n=== MEMORY DEBUG INFO ===\n"\n"\n"\n"\n"\n"\n"\n"\n");
    memory_debug_print_stats(\n"\n"\n"\n"\n"\n"\n"\n");
    memory_debug_check_all_allocations(\n"\n"\n"\n"\n"\n"\n"\n");
    memory_debug_detect_leaks(\n"\n"\n"\n"\n"\n"\n"\n");
    memory_debug_scan_memory_for_corruption(\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "=== END MEMORY DEBUG INFO ===\n\n"\n"\n"\n"\n"\n"\n"\n"\n");
    
    print_backtrace(\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Additional debugging info for systems without backtrace
    fprintf(stderr, "=== STACK INFO ===\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "Current function: crash_handler\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "Signal: %d (%s)\n", sig, strsignal(sig)\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "Fault address: %p\n", info->si_addr\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "=== END STACK INFO ===\n\n"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Try to get more detailed info about the fault
    if (context) {
        ucontext_t *uc = (ucontext_t *)context;
        fprintf(stderr, "=== REGISTER STATE ===\n"\n"\n"\n"\n"\n"\n"\n"\n");
#if defined(__arm__)
        fprintf(stderr, "Program counter: %p\n", (void*)uc->uc_mcontext.arm_pc\n"\n"\n"\n"\n"\n"\n"\n");
        fprintf(stderr, "Stack pointer: %p\n", (void*)uc->uc_mcontext.arm_sp\n"\n"\n"\n"\n"\n"\n"\n");
        fprintf(stderr, "Link register: %p\n", (void*)uc->uc_mcontext.arm_lr\n"\n"\n"\n"\n"\n"\n"\n");
#elif defined(__aarch64__)
        fprintf(stderr, "Program counter: %p\n", (void*)uc->uc_mcontext.pc\n"\n"\n"\n"\n"\n"\n"\n");
        fprintf(stderr, "Stack pointer: %p\n", (void*)uc->uc_mcontext.sp\n"\n"\n"\n"\n"\n"\n"\n");
        fprintf(stderr, "Link register: %p\n", (void*)uc->uc_mcontext.regs[30]\n"\n"\n"\n"\n"\n"\n"\n");
#elif defined(__x86_64__)
        fprintf(stderr, "Program counter: %p\n", (void*)uc->uc_mcontext.gregs[REG_RIP]\n"\n"\n"\n"\n"\n"\n"\n");
        fprintf(stderr, "Stack pointer: %p\n", (void*)uc->uc_mcontext.gregs[REG_RSP]\n"\n"\n"\n"\n"\n"\n"\n");
        fprintf(stderr, "Base pointer: %p\n", (void*)uc->uc_mcontext.gregs[REG_RBP]\n"\n"\n"\n"\n"\n"\n"\n");
#elif defined(__i386__)
        fprintf(stderr, "Program counter: %p\n", (void*)uc->uc_mcontext.gregs[REG_EIP]\n"\n"\n"\n"\n"\n"\n"\n");
        fprintf(stderr, "Stack pointer: %p\n", (void*)uc->uc_mcontext.gregs[REG_ESP]\n"\n"\n"\n"\n"\n"\n"\n");
        fprintf(stderr, "Base pointer: %p\n", (void*)uc->uc_mcontext.gregs[REG_EBP]\n"\n"\n"\n"\n"\n"\n"\n");
#else
        fprintf(stderr, "Register state reporting not available for this architecture.\n"\n"\n"\n"\n"\n"\n"\n"\n");
#endif
        fprintf(stderr, "=== END REGISTER STATE ===\n\n"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    fprintf(stderr, "=== CRASH END ===\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fflush(stderr\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Exit with the signal number
    _exit(sig\n"\n"\n"\n"\n"\n"\n"\n");
}

static void setup_crash_handlers(void) {
    struct sigaction sa;
    
    // Set up signal handler for crashes
    sa.sa_sigaction = crash_handler;
    sigemptyset(&sa.sa_mask\n"\n"\n"\n"\n"\n"\n"\n");
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    
    // Handle common crash signals
    sigaction(SIGSEGV, &sa, NULL);  // Segmentation fault
    sigaction(SIGBUS, &sa, NULL);   // Bus error
    sigaction(SIGFPE, &sa, NULL);   // Floating point exception
    sigaction(SIGILL, &sa, NULL);   // Illegal instruction
    sigaction(SIGABRT, &sa, NULL);  // Abort signal
    
    fprintf(stderr, "DEBUG: Crash handlers installed\n"\n"\n"\n"\n"\n"\n"\n"\n");
}

// Memory debugging utility
static void print_memory_info(void) {
    FILE *status = fopen("/proc/self/status", "r"\n"\n"\n"\n"\n"\n"\n"\n");
    if (status) {
        char line[256];
        fprintf(stderr, "\n=== MEMORY STATUS ===\n"\n"\n"\n"\n"\n"\n"\n"\n");
        while (fgets(line, sizeof(line), status)) {
            if (strstr(line, "VmSize") || strstr(line, "VmRSS") || 
                strstr(line, "VmPeak") || strstr(line, "VmHWM") ||
                strstr(line, "VmData") || strstr(line, "VmStk") ||
                strstr(line, "VmExe") || strstr(line, "VmLib")) {
                fprintf(stderr, "%s", line\n"\n"\n"\n"\n"\n"\n"\n");
            }
        }
        fclose(status\n"\n"\n"\n"\n"\n"\n"\n");
        fprintf(stderr, "=== END MEMORY STATUS ===\n\n"\n"\n"\n"\n"\n"\n"\n"\n");
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
    fprintf(stderr, "Received signal %d, shutting down...\n", sig\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (ctx) {
        ubus_free(ctx\n"\n"\n"\n"\n"\n"\n"\n");
    }
    if (uci_ctx) {
        uci_free_context(uci_ctx\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Cleanup UCI manager
    uci_manager_cleanup(\n"\n"\n"\n"\n"\n"\n"\n");
    
    remove_pid_file(\n"\n"\n"\n"\n"\n"\n"\n");
    uloop_done(\n"\n"\n"\n"\n"\n"\n"\n");
    exit(0\n"\n"\n"\n"\n"\n"\n"\n");
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
    UBUS_OBJECT_TYPE("autonomy", autonomy_methods\n"\n"\n"\n"\n"\n"\n"\n");

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
    memory_debug_init(\n"\n"\n"\n"\n"\n"\n"\n");
    
    DEBUG_TRACE_ENTER(\n"\n"\n"\n"\n"\n"\n"\n");
    DEBUG_TRACE_INFO("Starting Telia Autonomy Network Management Daemon"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Display version and build information
    fprintf(stderr, "========================================\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "Telia Autonomy Network Management Daemon\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "========================================\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "%s\n", autonomy_daemon_get_build_info_string()\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "========================================\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "Starting daemon...\n"\n"\n"\n"\n"\n"\n"\n"\n");
    
    DEBUG_TRACE_INFO("Build info: %s", autonomy_daemon_get_build_info_string()\n"\n"\n"\n"\n"\n"\n"\n");
    
    DEBUG_TRACE_STEP(1, "Initializing uloop"\n"\n"\n"\n"\n"\n"\n"\n");
    uloop_init(\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "uloop initialized\n"\n"\n"\n"\n"\n"\n"\n"\n");
    DEBUG_TRACE_INFO("uloop initialized successfully"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Initialize logging system
    DEBUG_TRACE_STEP(2, "Initializing logging system"\n"\n"\n"\n"\n"\n"\n"\n");
    logx_init(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "Logging system initialized\n"\n"\n"\n"\n"\n"\n"\n"\n");
    DEBUG_TRACE_INFO("Logging system initialized successfully"\n"\n"\n"\n"\n"\n"\n"\n");
    
    DEBUG_TRACE_STEP(3, "Setting up signal handlers"\n"\n"\n"\n"\n"\n"\n"\n");
    signal(SIGPIPE, SIG_IGN\n"\n"\n"\n"\n"\n"\n"\n");
    signal(SIGTERM, handle_sig\n"\n"\n"\n"\n"\n"\n"\n");
    signal(SIGINT, handle_sig\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Set up crash debugging handlers
    setup_crash_handlers(\n"\n"\n"\n"\n"\n"\n"\n");
    
    fprintf(stderr, "Signal handlers set\n"\n"\n"\n"\n"\n"\n"\n"\n");
    DEBUG_TRACE_INFO("Signal handlers set successfully"\n"\n"\n"\n"\n"\n"\n"\n");

    // Initialize UCI manager and load configuration
    DEBUG_TRACE_STEP(4, "Initializing UCI manager"\n"\n"\n"\n"\n"\n"\n"\n");
    if (uci_manager_init() != AUTONOMY_SUCCESS) {
        DEBUG_TRACE_ERROR("Failed to initialize UCI manager"\n"\n"\n"\n"\n"\n"\n"\n");
        fprintf(stderr, "Failed to initialize UCI manager\n"\n"\n"\n"\n"\n"\n"\n"\n");
        return 1;
    }
    DEBUG_TRACE_INFO("UCI manager initialized successfully"\n"\n"\n"\n"\n"\n"\n"\n");
    
    DEBUG_TRACE_STEP(5, "Loading configuration from UCI"\n"\n"\n"\n"\n"\n"\n"\n");
    if (uci_manager_load_config(&g_config) != AUTONOMY_SUCCESS) {
        DEBUG_TRACE_WARN("Failed to load configuration from UCI, using defaults"\n"\n"\n"\n"\n"\n"\n"\n");
        fprintf(stderr, "Failed to load configuration from UCI, using defaults\n"\n"\n"\n"\n"\n"\n"\n"\n");
        // Use default configuration if UCI loading fails
        const autonomy_config_t *default_config = uci_manager_get_default_config(\n"\n"\n"\n"\n"\n"\n"\n");
        if (default_config) {
            g_config = *default_config;
            DEBUG_TRACE_INFO("Using default configuration"\n"\n"\n"\n"\n"\n"\n"\n");
        }
    } else {
        DEBUG_TRACE_INFO("Configuration loaded from UCI successfully"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    fprintf(stderr, "Configuration loaded from UCI\n"\n"\n"\n"\n"\n"\n"\n"\n");

    // Check if another instance is running
    fprintf(stderr, "Skipping PID file check for debugging...\n"\n"\n"\n"\n"\n"\n"\n"\n");
    // if (check_pid_file() == -1) {
    //     fprintf(stderr, "PID file check failed\n"\n"\n"\n"\n"\n"\n"\n"\n");
    //     return 1;
    // }
    // fprintf(stderr, "PID file check passed\n"\n"\n"\n"\n"\n"\n"\n"\n");

    // // Create PID file
    // fprintf(stderr, "Creating PID file...\n"\n"\n"\n"\n"\n"\n"\n"\n");
    // if (create_pid_file() == -1) {
    //     fprintf(stderr, "PID file creation failed\n"\n"\n"\n"\n"\n"\n"\n"\n");
    //     return 1;
    // }
    // fprintf(stderr, "PID file created successfully\n"\n"\n"\n"\n"\n"\n"\n"\n");

    fprintf(stderr, "Attempting to connect to ubus...\n"\n"\n"\n"\n"\n"\n"\n"\n");
    ctx = ubus_connect(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    if (!ctx) {
        fprintf(stderr, "Failed to connect to ubus\n"\n"\n"\n"\n"\n"\n"\n"\n");
        return 1;
    }
    fprintf(stderr, "Connected to ubus successfully\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "UBUS context: %p\n", ctx\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "About to call ubus_add_uloop...\n"\n"\n"\n"\n"\n"\n"\n"\n");
    ubus_add_uloop(ctx\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "Added uloop to ubus context\n"\n"\n"\n"\n"\n"\n"\n"\n");

    // Load UCI configuration
    if (load_uci_config() == -1) {
        fprintf(stderr, "Failed to load UCI configuration, using defaults.\n"\n"\n"\n"\n"\n"\n"\n"\n");
    }

    // Initialize network health monitoring
    if (perform_network_health_check() != 0) {
        fprintf(stderr, "Failed to initialize network health monitoring.\n"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Initialize comprehensive network discovery system
    extern int network_discovery_comprehensive_init(void\n"\n"\n"\n"\n"\n"\n"\n");
    if (network_discovery_comprehensive_init() != AUTONOMY_SUCCESS) {
        fprintf(stderr, "Failed to initialize comprehensive network discovery.\n"\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        fprintf(stderr, "Comprehensive network discovery initialized successfully.\n"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Initialize network controller
    extern int network_controller_init(const void* config\n"\n"\n"\n"\n"\n"\n"\n");
    if (network_controller_init(NULL) != AUTONOMY_SUCCESS) {
        fprintf(stderr, "Failed to initialize network controller.\n"\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        fprintf(stderr, "Network controller initialized successfully.\n"\n"\n"\n"\n"\n"\n"\n"\n");
    }

    // Initialize GPS health monitoring
    if (perform_gps_health_check() != 0) {
        fprintf(stderr, "Failed to initialize GPS health monitoring.\n"\n"\n"\n"\n"\n"\n"\n"\n");
    }

    // Initialize Starlink tracking module
    g_starlink_tracker = starlink_tracker_init_from_uci(uci_ctx\n"\n"\n"\n"\n"\n"\n"\n");
    if (g_starlink_tracker) {
        fprintf(stderr, "Starlink tracking module initialized successfully\n"\n"\n"\n"\n"\n"\n"\n"\n");
        
        // Initialize tracking UBUS interface
        fprintf(stderr, "DEBUG: About to call starlink_tracker_ubus_init\n"\n"\n"\n"\n"\n"\n"\n"\n");
        int result = starlink_tracker_ubus_init(ctx, g_starlink_tracker\n"\n"\n"\n"\n"\n"\n"\n");
        fprintf(stderr, "DEBUG: starlink_tracker_ubus_init returned: %d\n", result\n"\n"\n"\n"\n"\n"\n"\n");
        if (result == 0) {
            fprintf(stderr, "Starlink tracking UBUS interface registered\n"\n"\n"\n"\n"\n"\n"\n"\n");
        } else {
            fprintf(stderr, "Failed to register Starlink tracking UBUS interface\n"\n"\n"\n"\n"\n"\n"\n"\n");
        }
    } else {
        fprintf(stderr, "Starlink tracking module initialization failed (check credentials)\n"\n"\n"\n"\n"\n"\n"\n"\n");
    }

    // Initialize Starlink gRPC collector
    if (starlink_grpc_collector_init() == AUTONOMY_SUCCESS) {
        fprintf(stderr, "Starlink gRPC collector initialized successfully\n"\n"\n"\n"\n"\n"\n"\n"\n");
        
        // Start gRPC collector thread
        if (starlink_grpc_collector_start() == AUTONOMY_SUCCESS) {
            fprintf(stderr, "Starlink gRPC collector thread started\n"\n"\n"\n"\n"\n"\n"\n"\n");
        } else {
            fprintf(stderr, "Failed to start Starlink gRPC collector thread\n"\n"\n"\n"\n"\n"\n"\n"\n");
        }
    } else {
        fprintf(stderr, "Starlink gRPC collector initialization failed\n"\n"\n"\n"\n"\n"\n"\n"\n");
    }

    // Initialize ML monitoring module
    ml_monitor_config_t ml_config;
    if (ml_monitor_load_config_from_uci(&ml_config) == ML_MONITOR_SUCCESS) {
        if (ml_config.enabled) {
            ml_monitor_t *ml_monitor = ml_monitor_init(&ml_config\n"\n"\n"\n"\n"\n"\n"\n");
            if (ml_monitor) {
                fprintf(stderr, "ML monitoring module initialized successfully\n"\n"\n"\n"\n"\n"\n"\n"\n");
                
                // Initialize ML monitoring UBUS interface
                if (ml_monitor_ubus_init(ctx) == ML_MONITOR_SUCCESS) {
                    fprintf(stderr, "ML monitoring UBUS interface registered\n"\n"\n"\n"\n"\n"\n"\n"\n");
                } else {
                    fprintf(stderr, "Failed to initialize ML monitoring UBUS interface\n"\n"\n"\n"\n"\n"\n"\n"\n");
                }
                
                // Initialize Phase 3 enhancements
                fprintf(stderr, "DEBUG: About to call ml_monitor_init_phase3_enhancements\n"\n"\n"\n"\n"\n"\n"\n"\n");
                fprintf(stderr, "DEBUG: ml_monitor pointer: %p\n", (void*)ml_monitor\n"\n"\n"\n"\n"\n"\n"\n");
                
                int phase3_result = ml_monitor_init_phase3_enhancements(ml_monitor\n"\n"\n"\n"\n"\n"\n"\n");
                
                fprintf(stderr, "DEBUG: ml_monitor_init_phase3_enhancements returned: %d\n", phase3_result\n"\n"\n"\n"\n"\n"\n"\n");
                fprintf(stderr, "DEBUG: ml_monitor pointer after Phase 3: %p\n", (void*)ml_monitor\n"\n"\n"\n"\n"\n"\n"\n");
                if (ml_monitor == NULL) {
                    fprintf(stderr, "ERROR: ml_monitor pointer is NULL after Phase 3!\n"\n"\n"\n"\n"\n"\n"\n"\n");
                    return -1;
                }
                if (phase3_result == ML_MONITOR_SUCCESS) {
                    fprintf(stderr, "ML monitoring Phase 3 enhancements initialized\n"\n"\n"\n"\n"\n"\n"\n"\n");
                    fprintf(stderr, "DEBUG: ml_monitor_init_phase3_enhancements completed successfully\n"\n"\n"\n"\n"\n"\n"\n"\n");
                    
                    fprintf(stderr, "DEBUG: About to print Phase 3 success message\n"\n"\n"\n"\n"\n"\n"\n"\n");
                    fprintf(stderr, "DEBUG: Phase 3 success message printed\n"\n"\n"\n"\n"\n"\n"\n"\n");
                    
                    fprintf(stderr, "DEBUG: About to start Phase 4 initialization\n"\n"\n"\n"\n"\n"\n"\n"\n");
                    // Initialize Phase 4 enhancements
                    fprintf(stderr, "DEBUG: About to call ml_monitor_init_phase4_enhancements\n"\n"\n"\n"\n"\n"\n"\n"\n");
                    if (ml_monitor_init_phase4_enhancements(ml_monitor) == ML_MONITOR_SUCCESS) {
                        fprintf(stderr, "ML monitoring Phase 4 enhancements initialized\n"\n"\n"\n"\n"\n"\n"\n"\n");
                        fprintf(stderr, "DEBUG: ml_monitor_init_phase4_enhancements completed successfully\n"\n"\n"\n"\n"\n"\n"\n"\n");
                        
                        // Initialize Phase 5 mobile optimization
                        fprintf(stderr, "DEBUG: About to call ml_monitor_init_phase5_mobile_system\n"\n"\n"\n"\n"\n"\n"\n"\n");
                        if (ml_monitor_init_phase5_mobile_system(ml_monitor) == ML_MONITOR_SUCCESS) {
                            fprintf(stderr, "ML monitoring Phase 5 mobile optimization initialized\n"\n"\n"\n"\n"\n"\n"\n"\n");
                            fprintf(stderr, "DEBUG: ml_monitor_init_phase5_mobile_system completed successfully\n"\n"\n"\n"\n"\n"\n"\n"\n");
                            
                            // Initialize Phase 6 self-optimization
                            fprintf(stderr, "DEBUG: About to call ml_monitor_init_phase6_self_optimization\n"\n"\n"\n"\n"\n"\n"\n"\n");
                            if (ml_monitor_init_phase6_self_optimization(ml_monitor) == ML_MONITOR_SUCCESS) {
                                fprintf(stderr, "ML monitoring Phase 6 self-optimization initialized\n"\n"\n"\n"\n"\n"\n"\n"\n");
                                fprintf(stderr, "DEBUG: ml_monitor_init_phase6_self_optimization completed successfully\n"\n"\n"\n"\n"\n"\n"\n"\n");
                                
                                // Initialize Phase 7 multi-interface intelligence
                                fprintf(stderr, "DEBUG: About to call ml_monitor_init_phase7_multi_interface\n"\n"\n"\n"\n"\n"\n"\n"\n");
                                if (ml_monitor_init_phase7_multi_interface(ml_monitor) == ML_MONITOR_SUCCESS) {
                                    fprintf(stderr, "ML monitoring Phase 7 multi-interface intelligence initialized\n"\n"\n"\n"\n"\n"\n"\n"\n");
                                    fprintf(stderr, "DEBUG: ml_monitor_init_phase7_multi_interface completed successfully\n"\n"\n"\n"\n"\n"\n"\n"\n");
                                } else {
                                    fprintf(stderr, "ML monitoring Phase 7 initialization failed, using Phase 6 features\n"\n"\n"\n"\n"\n"\n"\n"\n");
                                }
                            } else {
                                fprintf(stderr, "ML monitoring Phase 6 initialization failed, using Phase 5 features\n"\n"\n"\n"\n"\n"\n"\n"\n");
                            }
                        } else {
                            fprintf(stderr, "ML monitoring Phase 5 initialization failed, using Phase 4 features\n"\n"\n"\n"\n"\n"\n"\n"\n");
                        }
                    } else {
                        fprintf(stderr, "ML monitoring Phase 4 initialization failed, using Phase 3 features\n"\n"\n"\n"\n"\n"\n"\n"\n");
                    }
                } else {
                    fprintf(stderr, "ML monitoring Phase 3 initialization failed, using Phase 2 features\n"\n"\n"\n"\n"\n"\n"\n"\n");
                }
                
                // Auto-start ML monitoring if configured
                fprintf(stderr, "DEBUG: About to call ml_monitor_start\n"\n"\n"\n"\n"\n"\n"\n"\n");
                if (ml_monitor_start(ml_monitor) == ML_MONITOR_SUCCESS) {
                    fprintf(stderr, "ML monitoring started automatically with Phase 7 multi-interface intelligence\n"\n"\n"\n"\n"\n"\n"\n"\n");
                    fprintf(stderr, "DEBUG: ml_monitor_start completed successfully\n"\n"\n"\n"\n"\n"\n"\n"\n");
                } else {
                    fprintf(stderr, "ML monitoring initialized but not started (manual start required)\n"\n"\n"\n"\n"\n"\n"\n"\n");
                    fprintf(stderr, "DEBUG: ml_monitor_start failed\n"\n"\n"\n"\n"\n"\n"\n"\n");
                }
            } else {
                fprintf(stderr, "ML monitoring module initialization failed\n"\n"\n"\n"\n"\n"\n"\n"\n");
            }
        } else {
            fprintf(stderr, "ML monitoring module disabled in configuration\n"\n"\n"\n"\n"\n"\n"\n"\n");
        }
    } else {
        fprintf(stderr, "Failed to load ML monitoring configuration\n"\n"\n"\n"\n"\n"\n"\n"\n");
    }

    // Initialize random seed for simulation
    srand(time(NULL)\n"\n"\n"\n"\n"\n"\n"\n");

    int ret = ubus_add_object(ctx, &autonomy_obj\n"\n"\n"\n"\n"\n"\n"\n");
    if (ret) {
        fprintf(stderr, "Failed to add ubus object: %s\n", ubus_strerror(ret)\n"\n"\n"\n"\n"\n"\n"\n");
        ubus_free(ctx\n"\n"\n"\n"\n"\n"\n"\n");
        uloop_done(\n"\n"\n"\n"\n"\n"\n"\n");
        return 1;
    }

    fprintf(stderr, "Autonomy daemon started, registered 'autonomy' ubus object\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "Available methods: status, health, config, start, stop, restart, pid_status, log_status, config_status\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "Network methods: network_status, network_interfaces, network_health_check, network_failover\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "GPS methods: gps_status, gps_sources, gps_health_check\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "System methods: system_status, system_health_check, system_health_details, system_maintenance, system_restart_services\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "Starlink methods: starlink_status, starlink_health, starlink_location, starlink_collector_stats, starlink_force_collect\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "Starlink cluster methods: starlink_cluster_status, starlink_cluster_check_failover\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "Starlink tracking methods: starlink_tracker.status, starlink_tracker.predictions, starlink_tracker.satellites\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "Starlink tracking control: starlink_tracker.start_monitoring, starlink_tracker.stop_monitoring, starlink_tracker.update_data\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "ML monitoring methods: ml_monitor.status, ml_monitor.start, ml_monitor.stop, ml_monitor.restart\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "ML monitoring config: ml_monitor.get_config, ml_monitor.set_config\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "ML monitoring data: ml_monitor.get_predictions, ml_monitor.get_statistics, ml_monitor.reset_learning, ml_monitor.export_data\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "ML monitoring Phase 4: ml_monitor.get_ensemble_status, ml_monitor.get_validation_metrics, ml_monitor.trigger_optimization\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "ML monitoring Phase 5: ml_monitor.get_mobile_status, ml_monitor.export_field_data, ml_monitor.enable_field_test\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "ML monitoring Phase 6: ml_monitor.get_system_status, ml_monitor.run_production_validation, ml_monitor.enable_autonomous_mode\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "ML monitoring Phase 7: ml_monitor.get_multi_interface_status, ml_monitor.predict_interface_outage, ml_monitor.update_mwan3_weights, ml_monitor.validate_failover_prediction\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "ML Analytics & Visualization: ml_monitor.get_analytics_summary, ml_monitor.get_interface_score_history, ml_monitor.get_accuracy_trends, ml_monitor.get_impact_summary, ml_monitor.get_current_interface_scores\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "Network Discovery Enhanced: autonomy.network.interfaces_detailed (includes ML recommendations, MWAN3 ping info, enhanced cellular metrics, performance trends)\n"\n"\n"\n"\n"\n"\n"\n"\n");
    fprintf(stderr, "Daemon running, press Ctrl+C to stop\n"\n"\n"\n"\n"\n"\n"\n"\n");
    uloop_run(\n"\n"\n"\n"\n"\n"\n"\n");

    // Cleanup
    if (g_starlink_tracker) {
        starlink_tracker_ubus_cleanup(ctx\n"\n"\n"\n"\n"\n"\n"\n");
        starlink_tracker_cleanup(g_starlink_tracker\n"\n"\n"\n"\n"\n"\n"\n");
        g_starlink_tracker = NULL;
        fprintf(stderr, "Starlink tracking module cleaned up\n"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    if (ctx) {
        ubus_free(ctx\n"\n"\n"\n"\n"\n"\n"\n");
    }
    if (uci_ctx) {
        uci_free_context(uci_ctx\n"\n"\n"\n"\n"\n"\n"\n");
    }
    remove_pid_file(\n"\n"\n"\n"\n"\n"\n"\n");
    uloop_done(\n"\n"\n"\n"\n"\n"\n"\n");

    // Cleanup memory debugging system
    memory_debug_cleanup(\n"\n"\n"\n"\n"\n"\n"\n");

    fprintf(stderr, "Autonomy daemon stopped\n"\n"\n"\n"\n"\n"\n"\n"\n");
    return 0;
}
