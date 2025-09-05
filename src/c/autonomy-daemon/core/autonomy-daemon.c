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
#include "autonomy_types.h"
#include "autonomy_modules.h"
#include "../starlink/starlink_modules.h"
#include "../starlink/starlink_tracker.h"

// Global variables
struct autonomy_config g_config = {
    .log_level = "info",
    .enable_gps = 1,
    .enable_notifications = 1,
    .health_check_interval = 30,
    .config_file = "/etc/config/autonomy"
};

struct system_health g_system_health = {
    .status = "unknown",
    .starlink_health = 0,
    .network_health = 0,
    .gps_health = 0,
    .overall_health = 0,
    .last_check = 0
};

struct autonomy_state g_state = {
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

// Global system health
struct system_health g_system_health = {
    .starlink_health = 0,
    .uci_health = 0,
    .overlay_health = 0,
    .services_health = 0,
    .network_health = 0,
    .database_health = 0,
    .time_health = 0,
    .logs_health = 0,
    .overall_score = 0,
    .last_check = 0,
    .status = "unknown"
};

static struct ubus_context *ctx;
struct uci_context *uci_ctx;

// Signal handler
static void handle_sig(int sig) {
    fprintf(stderr, "Received signal %d, shutting down...\n", sig);
    
    if (ctx) {
        ubus_free(ctx);
    }
    if (uci_ctx) {
        uci_free_context(uci_ctx);
    }
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
    fprintf(stderr, "Autonomy daemon starting...\n");
    uloop_init();
    fprintf(stderr, "uloop initialized\n");
    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, handle_sig);
    signal(SIGINT, handle_sig);
    fprintf(stderr, "Signal handlers set\n");

    // Check if another instance is running
    if (check_pid_file() == -1) {
        return 1;
    }

    // Create PID file
    if (create_pid_file() == -1) {
        return 1;
    }

    fprintf(stderr, "Attempting to connect to ubus...\n");
    ctx = ubus_connect(NULL);
    if (!ctx) {
        fprintf(stderr, "Failed to connect to ubus\n");
        return 1;
    }
    fprintf(stderr, "Connected to ubus successfully\n");
    ubus_add_uloop(ctx);
    fprintf(stderr, "Added uloop to ubus context\n");

    // Load UCI configuration
    if (load_uci_config() == -1) {
        fprintf(stderr, "Failed to load UCI configuration, using defaults.\n");
    }

    // Initialize network interfaces
    if (discover_network_interfaces() == -1) {
        fprintf(stderr, "Failed to initialize network interfaces.\n");
    }

    // Initialize GPS sources
    if (discover_gps_sources() == -1) {
        fprintf(stderr, "Failed to initialize GPS sources.\n");
    }

    // Initialize Starlink tracking module
    g_starlink_tracker = starlink_tracker_init_from_uci(uci_ctx);
    if (g_starlink_tracker) {
        fprintf(stderr, "Starlink tracking module initialized successfully\n");
        
        // Initialize tracking UBUS interface
        if (starlink_tracker_ubus_init(ctx, g_starlink_tracker) == 0) {
            fprintf(stderr, "Starlink tracking UBUS interface registered\n");
        } else {
            fprintf(stderr, "Failed to register Starlink tracking UBUS interface\n");
        }
    } else {
        fprintf(stderr, "Starlink tracking module initialization failed (check credentials)\n");
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
