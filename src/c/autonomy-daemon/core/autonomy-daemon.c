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

// Include our modular headers
#include "../core/types.h"
#include "version.h"
#include "autonomy_modules.h"
#include "../starlink/starlink_modules.h"
#include "../starlink/starlink_tracker.h"
#include "../starlink/starlink_grpc_collector.h"
#include "../utils/uci_manager.h"
#include "../utils/logx.h"
#include "../utils/debug_trace.h"
#include "../ml/ml_monitor.h"
#include "../ml/ml_monitor_ubus.h"
#include <sys/socket.h>

// Global variables
autonomy_config_t g_config;

autonomy_state_t g_state = {
    .running = 0,
    .start_time = 0,
    .health_checks_run = 0,
    .health_issues_found = 0,
    .interface_count = 0,
    .failover_enabled = 0,
    .network_health_score = 0.0,
    .gps_source_count = 0,
    .gps_enabled = 1,
    .gps_health_score = 0.0,
    .current_lat = 0.0,
    .current_lon = 0.0,
    .current_accuracy = 0.0,
    .current_confidence = 0,
    .last_gps_update = 0,
    .location_status = "unknown",
    .movement_detected = 0,
    .last_movement_check = 0
};

// Global Starlink tracker
static starlink_tracker_t *g_starlink_tracker = NULL;

// Global system health (defined in core/system_management.c)
extern system_health_t g_system_health;

static struct ubus_context *ctx;
struct uci_context *uci_ctx;

// Signal handler
void handle_sig(int sig) {
    fprintf(stderr, "Received signal %d, shutting down...\n", sig);
    
    if (ctx) {
        ubus_free(ctx);
    }
    if (uci_ctx) {
        uci_free_context(uci_ctx);
    }
    
    // Cleanup UCI manager
    uci_manager_cleanup();
    
    remove_pid_file();
    uloop_done();
    exit(0);
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
    fprintf(stderr, "Signal handlers set\n");
    DEBUG_TRACE_INFO("Signal handlers set successfully");

    // Initialize UCI manager and load configuration
    DEBUG_TRACE_STEP(4, "Initializing UCI manager");
    if (uci_manager_init() != AUTONOMY_SUCCESS) {
        DEBUG_TRACE_ERROR("Failed to initialize UCI manager");
        fprintf(stderr, "Failed to initialize UCI manager\n");
        return 1;
    }
    DEBUG_TRACE_INFO("UCI manager initialized successfully");
    
    DEBUG_TRACE_STEP(5, "Loading configuration from UCI");
    if (uci_manager_load_config(&g_config) != AUTONOMY_SUCCESS) {
        DEBUG_TRACE_WARN("Failed to load configuration from UCI, using defaults");
        fprintf(stderr, "Failed to load configuration from UCI, using defaults\n");
        // Use default configuration if UCI loading fails
        const autonomy_config_t *default_config = uci_manager_get_default_config();
        if (default_config) {
            g_config = *default_config;
            DEBUG_TRACE_INFO("Using default configuration");
        }
    } else {
        DEBUG_TRACE_INFO("Configuration loaded from UCI successfully");
    }
    fprintf(stderr, "Configuration loaded from UCI\n");

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
    ctx = ubus_connect(NULL);
    if (!ctx) {
        fprintf(stderr, "Failed to connect to ubus\n");
        return 1;
    }
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
            ml_monitor_t *ml_monitor = ml_monitor_init(&ml_config);
            if (ml_monitor) {
                fprintf(stderr, "ML monitoring module initialized successfully\n");
                
                // Initialize ML monitoring UBUS interface
                if (ml_monitor_ubus_init(ctx) == ML_MONITOR_SUCCESS) {
                    fprintf(stderr, "ML monitoring UBUS interface registered\n");
                } else {
                    fprintf(stderr, "Failed to initialize ML monitoring UBUS interface\n");
                }
                
                // Initialize Phase 3 enhancements
                if (ml_monitor_init_phase3_enhancements(ml_monitor) == ML_MONITOR_SUCCESS) {
                    fprintf(stderr, "ML monitoring Phase 3 enhancements initialized\n");
                    
                    // Initialize Phase 4 enhancements
                    if (ml_monitor_init_phase4_enhancements(ml_monitor) == ML_MONITOR_SUCCESS) {
                        fprintf(stderr, "ML monitoring Phase 4 enhancements initialized\n");
                        
                        // Initialize Phase 5 mobile optimization
                        if (ml_monitor_init_phase5_mobile_system(ml_monitor) == ML_MONITOR_SUCCESS) {
                            fprintf(stderr, "ML monitoring Phase 5 mobile optimization initialized\n");
                            
                            // Initialize Phase 6 self-optimization
                            if (ml_monitor_init_phase6_self_optimization(ml_monitor) == ML_MONITOR_SUCCESS) {
                                fprintf(stderr, "ML monitoring Phase 6 self-optimization initialized\n");
                                
                                // Initialize Phase 7 multi-interface intelligence
                                if (ml_monitor_init_phase7_multi_interface(ml_monitor) == ML_MONITOR_SUCCESS) {
                                    fprintf(stderr, "ML monitoring Phase 7 multi-interface intelligence initialized\n");
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
                if (ml_monitor_start(ml_monitor) == ML_MONITOR_SUCCESS) {
                    fprintf(stderr, "ML monitoring started automatically with Phase 7 multi-interface intelligence\n");
                } else {
                    fprintf(stderr, "ML monitoring initialized but not started (manual start required)\n");
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

    int ret = ubus_add_object(ctx, &autonomy_obj);
    if (ret) {
        fprintf(stderr, "Failed to add ubus object: %s\n", ubus_strerror(ret));
        ubus_free(ctx);
        uloop_done();
        return 1;
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

    // Cleanup
    if (g_starlink_tracker) {
        starlink_tracker_ubus_cleanup(ctx);
        starlink_tracker_cleanup(g_starlink_tracker);
        g_starlink_tracker = NULL;
        fprintf(stderr, "Starlink tracking module cleaned up\n");
    }
    
    if (ctx) {
        ubus_free(ctx);
    }
    if (uci_ctx) {
        uci_free_context(uci_ctx);
    }
    remove_pid_file();
    uloop_done();

    fprintf(stderr, "Autonomy daemon stopped\n");
    return 0;
}
