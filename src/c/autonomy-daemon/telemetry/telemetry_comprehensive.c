#include "telemetry_comprehensive.h"
#include "../gps/gps_comprehensive.h"
#include "../gps/gps_location_reference.h"
#include "../shared/network/cellular_collector.h"
#include "wifi_enhanced.h"
#include "../starlink/starlink_comprehensive.h"
#include "../analytics/performance_monitor.h"
#include "../shared/logging/logx.h"
#include "../shared/utils/string_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>

// External reference to global configuration
extern autonomy_config_t g_config;
#include <sys/socket.h>

// Database schema SQL
static const char* TELEMETRY_SCHEMA_SQL = 
    "CREATE TABLE IF NOT EXISTS telemetry_samples ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "timestamp INTEGER NOT NULL,"
    "member_name TEXT NOT NULL,"
    "interface_name TEXT,"
    "latitude REAL,"
    "longitude REAL,"
    "accuracy REAL,"
    "satellites INTEGER,"
    "hdop REAL,"
    "gps_source TEXT,"
    "movement_kmh REAL,"
    "latency_ms REAL,"
    "packet_loss_percent REAL,"
    "jitter_ms REAL,"
    "throughput_bps INTEGER,"
    "signal_strength REAL,"
    "status TEXT,"
    "obstruction_percent REAL,"
    "snr_db REAL,"
    "temperature_c REAL,"
    "outage_count INTEGER,"
    "pop_ping_drop_rate REAL,"
    "rsrp_dbm REAL,"
    "rsrq_db REAL,"
    "sinr_db REAL,"
    "carrier TEXT,"
    "cell_id INTEGER,"
    "cell_changes INTEGER,"
    "wifi_rssi_dbm REAL,"
    "wifi_channel INTEGER,"
    "wifi_ssid TEXT,"
    "wifi_noise_floor REAL,"
    "cpu_usage_percent REAL,"
    "memory_usage_percent REAL,"
    "disk_usage_percent REAL,"
    "load_avg_1min REAL,"
    "overall_score REAL,"
    "reliability_score REAL,"
    "predictive_risk REAL,"
    "is_active_interface INTEGER,"
    "collection_method TEXT,"
    "collection_time_ms REAL"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_telemetry_timestamp ON telemetry_samples(timestamp);"
    "CREATE INDEX IF NOT EXISTS idx_telemetry_member ON telemetry_samples(member_name);"
    "CREATE INDEX IF NOT EXISTS idx_telemetry_interface ON telemetry_samples(interface_name);";

// Global telemetry comprehensive system
static telemetry_comprehensive_t g_telemetry_comprehensive = {0};
static bool g_telemetry_comprehensive_initialized = false;

// Note: TELEMETRY_SCHEMA_SQL is defined above (removed duplicate)
/*
// Duplicate schema definition removed - using the one defined above
static const char* TELEMETRY_SCHEMA_SQL_DUPLICATE = 
    "CREATE TABLE IF NOT EXISTS telemetry_samples ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    timestamp INTEGER NOT NULL,"
    "    member_name TEXT NOT NULL,"
    "    interface_name TEXT NOT NULL,"
    "    location_reference_id INTEGER DEFAULT 0,"
    "    movement_kmh REAL,"
    "    gps_accuracy REAL,"
    "    gps_source TEXT,"
    "    latency_ms REAL,"
    "    packet_loss_percent REAL,"
    "    jitter_ms REAL,"
    "    throughput_bps INTEGER,"
    "    signal_quality REAL,"
    "    status TEXT,"
    "    obstruction_percent REAL,"
    "    snr_db REAL,"
    "    temperature_c REAL,"
    "    outage_count INTEGER,"
    "    pop_ping_drop_rate REAL,"
    "    rsrp_dbm REAL,"
    "    rsrq_db REAL,"
    "    sinr_db REAL,"
    "    carrier TEXT,"
    "    cell_id INTEGER,"
    "    cell_changes INTEGER,"
    "    wifi_rssi_dbm REAL,"
    "    wifi_channel INTEGER,"
    "    wifi_ssid TEXT,"
    "    wifi_noise_floor REAL,"
    "    cpu_usage_percent REAL,"
    "    memory_usage_percent REAL,"
    "    disk_usage_percent REAL,"
    "    load_avg_1min REAL,"
    "    overall_score REAL,"
    "    reliability_score REAL,"
    "    predictive_risk REAL,"
    "    is_active_interface INTEGER,"
    "    collection_method TEXT,"
    "    collection_time_ms REAL"
    ");"
    
    "CREATE INDEX IF NOT EXISTS idx_telemetry_timestamp ON telemetry_samples(timestamp);"
    "CREATE INDEX IF NOT EXISTS idx_telemetry_member ON telemetry_samples(member_name);"
    "CREATE INDEX IF NOT EXISTS idx_telemetry_location ON telemetry_samples(location_reference_id);"
    "CREATE INDEX IF NOT EXISTS idx_telemetry_active ON telemetry_samples(is_active_interface);"; // End of duplicate - commented out above
    
    "CREATE TABLE IF NOT EXISTS decision_records ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    timestamp INTEGER NOT NULL,"
    "    decision_id TEXT UNIQUE NOT NULL,"
    "    decision_type TEXT NOT NULL,"
    "    trigger TEXT,"
    "    reasoning TEXT,"
    "    confidence REAL,"
    "    from_interface TEXT,"
    "    to_interface TEXT,"
    "    from_member TEXT,"
    "    to_member TEXT,"
    "    location_reference_id INTEGER DEFAULT 0,"
    "    gps_accuracy REAL,"
    "    gps_source TEXT,"
    "    from_score REAL,"
    "    to_score REAL,"
    "    score_difference REAL,"
    "    from_latency REAL,"
    "    from_loss REAL,"
    "    to_latency REAL,"
    "    to_loss REAL,"
    "    success INTEGER,"
    "    execution_time_ms REAL,"
    "    error_message TEXT,"
    "    root_cause TEXT,"
    "    context_json TEXT,"
    "    recommendations TEXT,"
    "    predictive_decision INTEGER,"
    "    prediction_confidence REAL,"
    "    validation_time INTEGER,"
    "    validation_successful INTEGER,"
    "    validation_notes TEXT"
    ");"
    
    "CREATE INDEX IF NOT EXISTS idx_decisions_timestamp ON decision_records(timestamp);"
    "CREATE INDEX IF NOT EXISTS idx_decisions_type ON decision_records(decision_type);"
    "CREATE INDEX IF NOT EXISTS idx_decisions_location ON decision_records(location_reference_id);"
    "CREATE INDEX IF NOT EXISTS idx_decisions_interfaces ON decision_records(from_interface, to_interface);";
*/

// Forward declarations
static void* collection_thread_worker(void* arg\n"\n"\n"\n"\n"\n"\n"\n");
static void* cleanup_thread_worker(void* arg\n"\n"\n"\n"\n"\n"\n"\n");
static void* export_thread_worker(void* arg\n"\n"\n"\n"\n"\n"\n"\n");
static int collect_current_telemetry(void\n"\n"\n"\n"\n"\n"\n"\n");
static int insert_sample_to_database(const telemetry_sample_t* sample\n"\n"\n"\n"\n"\n"\n"\n");
static int insert_decision_to_database(const decision_record_t* decision\n"\n"\n"\n"\n"\n"\n"\n");
int perform_database_cleanup(void\n"\n"\n"\n"\n"\n"\n"\n");
static int export_ml_dataset(void\n"\n"\n"\n"\n"\n"\n"\n");
double calculate_distance_meters(double lat1, double lon1, double lat2, double lon2\n"\n"\n"\n"\n"\n"\n"\n");

// Initialize comprehensive telemetry collection system
int telemetry_comprehensive_init(const telemetry_collection_config_t* config) {
    if (g_telemetry_comprehensive_initialized) {
        printf("WARN: "Comprehensive telemetry already initialized"\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_SUCCESS;
    }
    
    if (!config) {
        printf("ERROR: "Telemetry comprehensive config is NULL"\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    memset(&g_telemetry_comprehensive, 0, sizeof(telemetry_comprehensive_t)\n"\n"\n"\n"\n"\n"\n"\n");
    g_telemetry_comprehensive.config = *config;
    
    // Initialize mutex
    if (pthread_mutex_init(&g_telemetry_comprehensive.mutex, NULL) != 0) {
        printf("ERROR: "Failed to initialize telemetry comprehensive mutex"\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Initialize database if persistent storage is enabled
    if (config->enable_persistent_storage) {
        if (telemetry_db_init() != AUTONOMY_SUCCESS) {
            printf("ERROR: "Failed to initialize telemetry database"\n"\n"\n"\n"\n"\n"\n"\n");
            pthread_mutex_destroy(&g_telemetry_comprehensive.mutex\n"\n"\n"\n"\n"\n"\n"\n");
            return AUTONOMY_ERROR_SYSTEM;
        }
        g_telemetry_comprehensive.db_initialized = true;
        
        // Initialize GPS location reference system for storage optimization
        gps_location_reference_config_t location_config = {
            .enabled = true,
            .precision_reduction_meters = 10.0,    // 10m precision (saves ~50% space)
            .movement_threshold_meters = 50.0,     // 50m movement threshold
            .max_locations = 10000,                // Max 10k unique locations
            .cleanup_interval_hours = 24,          // Use configurable cleanup interval
            .min_usage_for_retention = 5,          // Keep locations used 5+ times
            .retention_days = 30,                  // 30 day retention
            .enable_location_clustering = true,
            .clustering_radius_meters = 20.0,      // 20m clustering
            .max_cluster_size = 5,
            .track_location_performance = true,
            .enable_location_scoring = true
        };
        
        if (gps_location_reference_init(&location_config) != AUTONOMY_SUCCESS) {
            printf("ERROR: "Failed to initialize GPS location reference system"\n"\n"\n"\n"\n"\n"\n"\n");
            telemetry_db_close(\n"\n"\n"\n"\n"\n"\n"\n");
            pthread_mutex_destroy(&g_telemetry_comprehensive.mutex\n"\n"\n"\n"\n"\n"\n"\n");
            return AUTONOMY_ERROR_SYSTEM;
        }
    }
    
    // Allocate memory for ring buffers
    g_telemetry_comprehensive.samples_buffer_size = 1000; // Keep 1000 samples in memory
    g_telemetry_comprehensive.samples_buffer = (telemetry_sample_t*)calloc(g_telemetry_comprehensive.samples_buffer_size,
                                                     sizeof(telemetry_sample_t)\n"\n"\n"\n"\n"\n"\n"\n");
    if (!g_telemetry_comprehensive.samples_buffer) {
        printf("ERROR: "Failed to allocate memory for samples buffer"\n"\n"\n"\n"\n"\n"\n"\n");
        if (g_telemetry_comprehensive.db_initialized) telemetry_db_close(\n"\n"\n"\n"\n"\n"\n"\n");
        pthread_mutex_destroy(&g_telemetry_comprehensive.mutex\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    g_telemetry_comprehensive.decisions_buffer_size = 200; // Keep 200 decisions in memory
    g_telemetry_comprehensive.decisions_buffer = (decision_record_t*)calloc(g_telemetry_comprehensive.decisions_buffer_size,
                                                       sizeof(decision_record_t)\n"\n"\n"\n"\n"\n"\n"\n");
    if (!g_telemetry_comprehensive.decisions_buffer) {
        printf("ERROR: "Failed to allocate memory for decisions buffer"\n"\n"\n"\n"\n"\n"\n"\n");
        free(g_telemetry_comprehensive.samples_buffer\n"\n"\n"\n"\n"\n"\n"\n");
        if (g_telemetry_comprehensive.db_initialized) telemetry_db_close(\n"\n"\n"\n"\n"\n"\n"\n");
        pthread_mutex_destroy(&g_telemetry_comprehensive.mutex\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Initialize statistics
    g_telemetry_comprehensive.stats.collection_start_time = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    g_telemetry_comprehensive.next_sample_id = 1;
    g_telemetry_comprehensive.next_decision_id = 1;
    
    // Start background threads if enabled
    if (config->enabled) {
        g_telemetry_comprehensive.threads_running = true;
        
        // Collection thread
        if (pthread_create(&g_telemetry_comprehensive.collection_thread, NULL, 
                          collection_thread_worker, NULL) != 0) {
            printf("ERROR: "Failed to create telemetry collection thread"\n"\n"\n"\n"\n"\n"\n"\n");
            free(g_telemetry_comprehensive.samples_buffer\n"\n"\n"\n"\n"\n"\n"\n");
            free(g_telemetry_comprehensive.decisions_buffer\n"\n"\n"\n"\n"\n"\n"\n");
            if (g_telemetry_comprehensive.db_initialized) telemetry_db_close(\n"\n"\n"\n"\n"\n"\n"\n");
            pthread_mutex_destroy(&g_telemetry_comprehensive.mutex\n"\n"\n"\n"\n"\n"\n"\n");
            return AUTONOMY_ERROR_SYSTEM;
        }
        
        // Cleanup thread
        if (pthread_create(&g_telemetry_comprehensive.cleanup_thread, NULL, 
                          cleanup_thread_worker, NULL) != 0) {
            printf("ERROR: "Failed to create telemetry cleanup thread"\n"\n"\n"\n"\n"\n"\n"\n");
            g_telemetry_comprehensive.threads_running = false;
            pthread_cancel(g_telemetry_comprehensive.collection_thread\n"\n"\n"\n"\n"\n"\n"\n");
            pthread_join(g_telemetry_comprehensive.collection_thread, NULL\n"\n"\n"\n"\n"\n"\n"\n");
            free(g_telemetry_comprehensive.samples_buffer\n"\n"\n"\n"\n"\n"\n"\n");
            free(g_telemetry_comprehensive.decisions_buffer\n"\n"\n"\n"\n"\n"\n"\n");
            if (g_telemetry_comprehensive.db_initialized) telemetry_db_close(\n"\n"\n"\n"\n"\n"\n"\n");
            pthread_mutex_destroy(&g_telemetry_comprehensive.mutex\n"\n"\n"\n"\n"\n"\n"\n");
            return AUTONOMY_ERROR_SYSTEM;
        }
        
        // ML export thread (if enabled)
        if (config->enable_ml_dataset_export) {
            if (pthread_create(&g_telemetry_comprehensive.export_thread, NULL, 
                              export_thread_worker, NULL) != 0) {
                printf("WARN: "Failed to create ML export thread, continuing without ML export"\n"\n"\n"\n"\n"\n"\n"\n");
            }
        }
    }
    
    g_telemetry_comprehensive_initialized = true;
    
    printf("INFO: "Comprehensive telemetry collection initialized",
              "enabled", config->enabled,
              "persistent_storage", config->enable_persistent_storage,
              "collection_interval_s", config->collection_interval_s,
              "retention_hours", config->retention_hours,
              "ml_export", config->enable_ml_dataset_export\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

// Cleanup comprehensive telemetry collection system
void telemetry_comprehensive_cleanup(void) {
    if (!g_telemetry_comprehensive_initialized) return;
    
    pthread_mutex_lock(&g_telemetry_comprehensive.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Stop background threads
    g_telemetry_comprehensive.threads_running = false;
    
    if (g_telemetry_comprehensive.config.enabled) {
        pthread_cancel(g_telemetry_comprehensive.collection_thread\n"\n"\n"\n"\n"\n"\n"\n");
        pthread_cancel(g_telemetry_comprehensive.cleanup_thread\n"\n"\n"\n"\n"\n"\n"\n");
        if (g_telemetry_comprehensive.config.enable_ml_dataset_export) {
            pthread_cancel(g_telemetry_comprehensive.export_thread\n"\n"\n"\n"\n"\n"\n"\n");
        }
        
        pthread_join(g_telemetry_comprehensive.collection_thread, NULL\n"\n"\n"\n"\n"\n"\n"\n");
        pthread_join(g_telemetry_comprehensive.cleanup_thread, NULL\n"\n"\n"\n"\n"\n"\n"\n");
        if (g_telemetry_comprehensive.config.enable_ml_dataset_export) {
            pthread_join(g_telemetry_comprehensive.export_thread, NULL\n"\n"\n"\n"\n"\n"\n"\n");
        }
    }
    
    // Close database
    if (g_telemetry_comprehensive.db_initialized) {
        telemetry_db_close(\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Free memory
    free(g_telemetry_comprehensive.samples_buffer\n"\n"\n"\n"\n"\n"\n"\n");
    free(g_telemetry_comprehensive.decisions_buffer\n"\n"\n"\n"\n"\n"\n"\n");
    
    pthread_mutex_unlock(&g_telemetry_comprehensive.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    pthread_mutex_destroy(&g_telemetry_comprehensive.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    g_telemetry_comprehensive_initialized = false;
    
    printf("INFO: "Comprehensive telemetry collection cleaned up"\n"\n"\n"\n"\n"\n"\n"\n");
}

// Collect telemetry sample with GPS positioning
int telemetry_comprehensive_collect_sample(const char* member_name,
                                          const char* interface_name,
                                          const telemetry_sample_t* sample) {
    if (!g_telemetry_comprehensive_initialized || !member_name || !interface_name || !sample) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_telemetry_comprehensive.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Create sample copy with metadata
    telemetry_sample_t* buffer_sample = &g_telemetry_comprehensive.samples_buffer[g_telemetry_comprehensive.samples_buffer_head];
    *buffer_sample = *sample;
    
    snprintf(buffer_sample->id, sizeof(buffer_sample->id), "%llu", (unsigned long long)g_telemetry_comprehensive.next_sample_id++\n"\n"\n"\n"\n"\n"\n"\n");
    buffer_sample->timestamp = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    strncpy(buffer_sample->member_name, member_name, sizeof(buffer_sample->member_name) - 1\n"\n"\n"\n"\n"\n"\n"\n");
    buffer_sample->member_name[sizeof(buffer_sample->member_name) - 1] = '\0';
    strncpy(buffer_sample->interface_name, interface_name, sizeof(buffer_sample->interface_name) - 1\n"\n"\n"\n"\n"\n"\n"\n");
    buffer_sample->interface_name[sizeof(buffer_sample->interface_name) - 1] = '\0';
    
    // Get location reference ID if GPS data is available and location reference system is enabled
    if (gps_location_reference_is_initialized() && 
        sample->location_reference_id == 0) { // Only if not already set
        
        // Try to get current GPS data for location reference
        standardized_gps_data_t gps_data;
        if (gps_comprehensive_is_initialized() && 
            gps_comprehensive_collect_best(&gps_data) == AUTONOMY_SUCCESS && gps_data.valid) {
            
            uint32_t location_id;
            if (gps_location_reference_get_or_create(gps_data.latitude, gps_data.longitude,
                                                    gps_data.accuracy, gps_data.source,
                                                    &location_id) == AUTONOMY_SUCCESS) {
                buffer_sample->location_reference_id = location_id;
                buffer_sample->gps_accuracy = gps_data.accuracy;
                strncpy(buffer_sample->gps_source, gps_data.source, sizeof(buffer_sample->gps_source) - 1\n"\n"\n"\n"\n"\n"\n"\n");
                buffer_sample->gps_source[sizeof(buffer_sample->gps_source) - 1] = '\0';
                
                // Update location usage with performance metrics
                gps_location_reference_update_usage(location_id, sample->signal_strength, sample->latency_ms\n"\n"\n"\n"\n"\n"\n"\n");
                
                printf("DEBUG: "Using GPS location reference",
                          "location_id", location_id,
                          "lat", gps_data.latitude,
                          "lon", gps_data.longitude\n"\n"\n"\n"\n"\n"\n"\n");
            }
        }
    }
    
    // Update ring buffer tracking
    g_telemetry_comprehensive.samples_buffer_head = 
        (g_telemetry_comprehensive.samples_buffer_head + 1) % g_telemetry_comprehensive.samples_buffer_size;
    
    if (g_telemetry_comprehensive.samples_buffer_count < g_telemetry_comprehensive.samples_buffer_size) {
        g_telemetry_comprehensive.samples_buffer_count++;
    }
    
    // Insert to database if persistent storage is enabled
    if (g_telemetry_comprehensive.config.enable_persistent_storage && g_telemetry_comprehensive.db_initialized) {
        if (insert_sample_to_database(buffer_sample) == AUTONOMY_SUCCESS) {
            g_telemetry_comprehensive.stats.database_inserts++;
        } else {
            g_telemetry_comprehensive.stats.database_errors++;
        }
    }
    
    // Update statistics
    g_telemetry_comprehensive.stats.total_samples_collected++;
    g_telemetry_comprehensive.stats.last_collection = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (buffer_sample->location_reference_id > 0) {
        g_telemetry_comprehensive.stats.samples_with_gps++;
    } else {
        g_telemetry_comprehensive.stats.samples_without_gps++;
    }
    
    // Update member-specific statistics
    if (strstr(member_name, "starlink")) {
        g_telemetry_comprehensive.stats.starlink_samples++;
    } else if (strstr(member_name, "cellular") || strstr(member_name, "wwan")) {
        g_telemetry_comprehensive.stats.cellular_samples++;
    } else if (strstr(member_name, "wifi") || strstr(member_name, "wlan")) {
        g_telemetry_comprehensive.stats.wifi_samples++;
    }
    
    pthread_mutex_unlock(&g_telemetry_comprehensive.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("DEBUG: "Telemetry sample collected",
              "id", buffer_sample->id,
              "member", member_name,
              "interface", interface_name,
              "gps_valid", (sample->latitude != 0.0 && sample->longitude != 0.0),
              "latency", sample->latency_ms,
              "loss", sample->packet_loss_percent\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

// Log failover/failback decision with full context
int telemetry_comprehensive_log_decision(const decision_record_t* decision) {
    if (!g_telemetry_comprehensive_initialized || !decision) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_telemetry_comprehensive.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Create decision copy with metadata
    decision_record_t* buffer_decision = &g_telemetry_comprehensive.decisions_buffer[g_telemetry_comprehensive.decisions_buffer_head];
    *buffer_decision = *decision;
    
    buffer_decision->id = g_telemetry_comprehensive.next_decision_id++;
    buffer_decision->timestamp = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Update ring buffer tracking
    g_telemetry_comprehensive.decisions_buffer_head = 
        (g_telemetry_comprehensive.decisions_buffer_head + 1) % g_telemetry_comprehensive.decisions_buffer_size;
    
    if (g_telemetry_comprehensive.decisions_buffer_count < g_telemetry_comprehensive.decisions_buffer_size) {
        g_telemetry_comprehensive.decisions_buffer_count++;
    }
    
    // Insert to database if persistent storage is enabled
    if (g_telemetry_comprehensive.config.enable_persistent_storage && g_telemetry_comprehensive.db_initialized) {
        if (insert_decision_to_database(buffer_decision) == AUTONOMY_SUCCESS) {
            g_telemetry_comprehensive.stats.database_inserts++;
        } else {
            g_telemetry_comprehensive.stats.database_errors++;
        }
    }
    
    // Update statistics
    g_telemetry_comprehensive.stats.decision_records_logged++;
    
    pthread_mutex_unlock(&g_telemetry_comprehensive.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("INFO: "Failover decision logged",
             "id", buffer_decision->id,
             "decision_id", decision->decision_id,
             "type", decision->decision_type,
             "from", decision->from_interface,
             "to", decision->to_interface,
             "success", decision->success,
             "confidence", decision->confidence,
             "predictive", decision->predictive_decision\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

// Initialize SQLite database with proper schema
int telemetry_db_init(void) {
    // Ensure database directory exists
    char db_dir[256];
    strncpy(db_dir, g_telemetry_comprehensive.config.database_path, sizeof(db_dir) - 1\n"\n"\n"\n"\n"\n"\n"\n");
    db_dir[sizeof(db_dir) - 1] = '\0';
    
    char* last_slash = strrchr(db_dir, '/'\n"\n"\n"\n"\n"\n"\n"\n");
    if (last_slash) {
        *last_slash = '\0';
        if (mkdir(db_dir, 0755) != 0 && errno != EEXIST) {
            printf("ERROR: "Failed to create database directory", "path", db_dir, "error", strerror(errno)\n"\n"\n"\n"\n"\n"\n"\n");
            return AUTONOMY_ERROR_SYSTEM;
        }
    }
    
    // Open database
    int result = sqlite3_open(g_telemetry_comprehensive.config.database_path, &g_telemetry_comprehensive.db\n"\n"\n"\n"\n"\n"\n"\n");
    if (result != SQLITE_OK) {
        printf("ERROR: "Failed to open telemetry database",
                  "path", g_telemetry_comprehensive.config.database_path,
                  "error", sqlite3_errmsg(g_telemetry_comprehensive.db)\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Execute schema creation
    char* error_msg = NULL;
    result = sqlite3_exec(g_telemetry_comprehensive.db, TELEMETRY_SCHEMA_SQL, NULL, NULL, &error_msg\n"\n"\n"\n"\n"\n"\n"\n");
    if (result != SQLITE_OK) {
        printf("ERROR: "Failed to create telemetry database schema",
                  "error", error_msg\n"\n"\n"\n"\n"\n"\n"\n");
        sqlite3_free(error_msg\n"\n"\n"\n"\n"\n"\n"\n");
        sqlite3_close(g_telemetry_comprehensive.db\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Enable WAL mode for better performance
    result = sqlite3_exec(g_telemetry_comprehensive.db, "PRAGMA journal_mode=WAL;", NULL, NULL, &error_msg\n"\n"\n"\n"\n"\n"\n"\n");
    if (result != SQLITE_OK) {
        printf("WARN: "Failed to enable WAL mode", "error", error_msg\n"\n"\n"\n"\n"\n"\n"\n");
        sqlite3_free(error_msg\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    printf("INFO: "Telemetry database initialized",
             "path", g_telemetry_comprehensive.config.database_path\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

// Background collection thread
static void* collection_thread_worker(void* arg) {
    printf("INFO: "Telemetry collection thread started",
             "interval_s", g_telemetry_comprehensive.config.collection_interval_s\n"\n"\n"\n"\n"\n"\n"\n");
    
    while (g_telemetry_comprehensive_initialized && g_telemetry_comprehensive.threads_running) {
        sleep(g_telemetry_comprehensive.config.collection_interval_s\n"\n"\n"\n"\n"\n"\n"\n");
        
        if (!g_telemetry_comprehensive.threads_running) break;
        
        // Collect current telemetry from all sources
        if (collect_current_telemetry() == AUTONOMY_SUCCESS) {
            printf("DEBUG: "Background telemetry collection successful"\n"\n"\n"\n"\n"\n"\n"\n");
        } else {
            printf("WARN: "Background telemetry collection failed"\n"\n"\n"\n"\n"\n"\n"\n");
        }
    }
    
    printf("INFO: "Telemetry collection thread stopped"\n"\n"\n"\n"\n"\n"\n"\n");
    return NULL;
}

// Collect current telemetry from all sources
static int collect_current_telemetry(void) {
    time_t collection_start = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Get current GPS data
    standardized_gps_data_t gps_data;
    bool gps_valid = false;
    if (gps_comprehensive_is_initialized() && 
        gps_comprehensive_collect_best(&gps_data) == AUTONOMY_SUCCESS) {
        gps_valid = gps_data.valid;
    }
    
    // Skip collection if GPS is required but not available
    if (g_telemetry_comprehensive.config.require_gps_for_collection && !gps_valid) {
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    // Collect Starlink metrics if enabled
    if (g_telemetry_comprehensive.config.collect_starlink_metrics && starlink_comprehensive_is_initialized()) {
        starlink_comprehensive_status_t starlink_status;
        if (starlink_comprehensive_collect_all(&starlink_status) == AUTONOMY_SUCCESS) {
            telemetry_sample_t sample = {0};
            
            // Get GPS location reference if valid
            if (gps_valid && gps_location_reference_is_initialized()) {
                uint32_t location_id;
                if (gps_location_reference_get_or_create(gps_data.latitude, gps_data.longitude,
                                                        gps_data.accuracy, gps_data.source,
                                                        &location_id) == AUTONOMY_SUCCESS) {
                    sample.location_reference_id = location_id;
                    sample.gps_accuracy = gps_data.accuracy;
                    strncpy(sample.gps_source, gps_data.source, sizeof(sample.gps_source) - 1\n"\n"\n"\n"\n"\n"\n"\n");
                    sample.gps_source[sizeof(sample.gps_source) - 1] = '\0';
                    sample.movement_kmh = gps_data.speed * 3.6; // Convert m/s to km/h
                }
            }
            
            // Fill Starlink metrics
            sample.latency_ms = starlink_status.pop_ping_latency_ms;
            sample.packet_loss_percent = 0.0; // Would need to calculate from ping drop rate
            sample.throughput_bps = (int64_t)starlink_status.downlink_throughput_bps;
            sample.obstruction_percent = starlink_status.obstruction_stats.fraction_obstructed * 100.0;
            sample.outage_count = starlink_status.events_analysis.total_outages_24h;
            sample.overall_score = starlink_status.overall_health_score * 100.0;
            sample.reliability_score = starlink_status.stability_score;
            sample.is_active_interface = true; // Would need to check actual active interface
            strcpy(sample.collection_method, "comprehensive"\n"\n"\n"\n"\n"\n"\n"\n");
            sample.collection_time_ms = starlink_status.collection_duration_ms;
            
            // Collect sample
            telemetry_comprehensive_collect_sample("starlink", "starlink0", &sample\n"\n"\n"\n"\n"\n"\n"\n");
        }
    }
    
    // Collect cellular metrics if enabled
    if (g_telemetry_comprehensive.config.collect_cellular_metrics && cellular_collector_is_initialized()) {
        cellular_info_t cellular_info;
        if (cellular_collector_collect(&cellular_info) == AUTONOMY_SUCCESS) {
            telemetry_sample_t sample = {0};
            
            // Fill GPS data
            if (gps_valid) {
                sample.latitude = gps_data.latitude;
                sample.longitude = gps_data.longitude;
                sample.accuracy = gps_data.accuracy;
                sample.satellites = gps_data.satellites_used;
                sample.hdop = gps_data.hdop;
                strncpy(sample.gps_source, gps_data.source, sizeof(sample.gps_source) - 1\n"\n"\n"\n"\n"\n"\n"\n");
                sample.gps_source[sizeof(sample.gps_source) - 1] = '\0';
                sample.movement_kmh = gps_data.speed * 3.6;
            }
            
            // Fill cellular metrics
            sample.rsrp_dbm = cellular_info.rsrp;
            sample.rsrq_db = cellular_info.rsrq;
            sample.sinr_db = cellular_info.sinr;
            strncpy(sample.carrier, cellular_info.operator_name, sizeof(sample.carrier) - 1\n"\n"\n"\n"\n"\n"\n"\n");
            sample.carrier[sizeof(sample.carrier) - 1] = '\0';
            // Convert cell_id string to integer
            int cell_id_int = 0;
            if (string_to_int(cellular_info.cell_id, &cell_id_int)) {
                sample.cell_id = (uint32_t)cell_id_int;
            } else {
                sample.cell_id = 0;
            }
            sample.signal_strength = cellular_info.signal_quality / 100.0;
            sample.overall_score = cellular_info.stability_score;
            sample.predictive_risk = cellular_info.predictive_risk;
            strcpy(sample.collection_method, "cellular_collector"\n"\n"\n"\n"\n"\n"\n"\n");
            
            telemetry_comprehensive_collect_sample("cellular", "wwan0", &sample\n"\n"\n"\n"\n"\n"\n"\n");
        }
    }
    
    // Get system metrics
    performance_metrics_t perf_metrics;
    if (performance_monitor_get_metrics(&perf_metrics) == AUTONOMY_SUCCESS) {
        // Add system metrics to the last collected sample
        pthread_mutex_lock(&g_telemetry_comprehensive.mutex\n"\n"\n"\n"\n"\n"\n"\n");
        if (g_telemetry_comprehensive.samples_buffer_count > 0) {
            int last_index = (g_telemetry_comprehensive.samples_buffer_head - 1 + 
                             g_telemetry_comprehensive.samples_buffer_size) % 
                             g_telemetry_comprehensive.samples_buffer_size;
            
            telemetry_sample_t* last_sample = &g_telemetry_comprehensive.samples_buffer[last_index];
            last_sample->cpu_usage_percent = perf_metrics.cpu_usage_percent;
            last_sample->memory_usage_percent = perf_metrics.memory_usage_percent;
            last_sample->load_avg_1min = perf_metrics.load_average_1min;
        }
        pthread_mutex_unlock(&g_telemetry_comprehensive.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    double collection_time_ms = difftime(time(NULL), collection_start) * 1000.0;
    
    // Update average collection time
    g_telemetry_comprehensive.stats.average_collection_time_ms = 
        (g_telemetry_comprehensive.stats.average_collection_time_ms * 
         (g_telemetry_comprehensive.stats.total_samples_collected - 1) + 
         collection_time_ms) / g_telemetry_comprehensive.stats.total_samples_collected;
    
    return AUTONOMY_SUCCESS;
}

// Insert sample to database
static int insert_sample_to_database(const telemetry_sample_t* sample) {
    if (!sample || !g_telemetry_comprehensive.db) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    const char* sql = 
        "INSERT INTO telemetry_samples ("
        "timestamp, member_name, interface_name, latitude, longitude, accuracy, "
        "satellites, hdop, gps_source, movement_kmh, latency_ms, packet_loss_percent, "
        "jitter_ms, throughput_bps, signal_quality, status, obstruction_percent, "
        "snr_db, temperature_c, outage_count, pop_ping_drop_rate, rsrp_dbm, rsrq_db, "
        "sinr_db, carrier, cell_id, cell_changes, wifi_rssi_dbm, wifi_channel, "
        "wifi_ssid, wifi_noise_floor, cpu_usage_percent, memory_usage_percent, "
        "disk_usage_percent, load_avg_1min, overall_score, reliability_score, "
        "predictive_risk, is_active_interface, collection_method, collection_time_ms"
        ") VALUES ("
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?"
        ");";
    
    sqlite3_stmt* stmt;
    int result = sqlite3_prepare_v2(g_telemetry_comprehensive.db, sql, -1, &stmt, NULL\n"\n"\n"\n"\n"\n"\n"\n");
    if (result != SQLITE_OK) {
        printf("ERROR: "Failed to prepare telemetry insert statement", "error", sqlite3_errmsg(g_telemetry_comprehensive.db)\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Bind parameters
    int param = 1;
    sqlite3_bind_int64(stmt, param++, sample->timestamp\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_text(stmt, param++, sample->member_name, -1, SQLITE_STATIC\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_text(stmt, param++, sample->interface_name, -1, SQLITE_STATIC\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, sample->latitude\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, sample->longitude\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, sample->accuracy\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_int(stmt, param++, sample->satellites\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, sample->hdop\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_text(stmt, param++, sample->gps_source, -1, SQLITE_STATIC\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, sample->movement_kmh\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, sample->latency_ms\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, sample->packet_loss_percent\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, sample->jitter_ms\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_int64(stmt, param++, sample->throughput_bps\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, sample->signal_strength\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_text(stmt, param++, sample->status, -1, SQLITE_STATIC\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, sample->obstruction_percent\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, sample->snr_db\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, sample->temperature_c\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_int(stmt, param++, sample->outage_count\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, sample->pop_ping_drop_rate\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, sample->rsrp_dbm\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, sample->rsrq_db\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, sample->sinr_db\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_text(stmt, param++, sample->carrier, -1, SQLITE_STATIC\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_int(stmt, param++, sample->cell_id\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_int(stmt, param++, sample->cell_changes\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, sample->wifi_rssi_dbm\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_int(stmt, param++, sample->wifi_channel\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_text(stmt, param++, sample->wifi_ssid, -1, SQLITE_STATIC\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, sample->wifi_noise_floor\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, sample->cpu_usage_percent\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, sample->memory_usage_percent\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, sample->disk_usage_percent\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, sample->load_avg_1min\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, sample->overall_score\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, sample->reliability_score\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, sample->predictive_risk\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_int(stmt, param++, sample->is_active_interface ? 1 : 0\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_text(stmt, param++, sample->collection_method, -1, SQLITE_STATIC\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, sample->collection_time_ms\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Execute statement
    result = sqlite3_step(stmt\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_finalize(stmt\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (result != SQLITE_DONE) {
        printf("ERROR: "Failed to insert telemetry sample", "error", sqlite3_errmsg(g_telemetry_comprehensive.db)\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    return AUTONOMY_SUCCESS;
}

bool telemetry_comprehensive_is_initialized(void) {
    return g_telemetry_comprehensive_initialized;
}

// Background cleanup thread
static void* cleanup_thread_worker(void* arg) {
    (void)arg; // Suppress unused parameter warning
    
    printf("INFO: "Telemetry cleanup thread started",
             "cleanup_interval_s", g_telemetry_comprehensive.config.cleanup_interval_s\n"\n"\n"\n"\n"\n"\n"\n");
    
    while (g_telemetry_comprehensive_initialized && g_telemetry_comprehensive.threads_running) {
        sleep(g_telemetry_comprehensive.config.cleanup_interval_s\n"\n"\n"\n"\n"\n"\n"\n");
        
        if (!g_telemetry_comprehensive.threads_running) break;
        
        // Perform database cleanup
        if (perform_database_cleanup() == AUTONOMY_SUCCESS) {
            printf("DEBUG: "Telemetry database cleanup completed"\n"\n"\n"\n"\n"\n"\n"\n");
        } else {
            printf("WARN: "Telemetry database cleanup failed"\n"\n"\n"\n"\n"\n"\n"\n");
        }
    }
    
    printf("INFO: "Telemetry cleanup thread stopped"\n"\n"\n"\n"\n"\n"\n"\n");
    return NULL;
}

// ML export thread worker
static void* export_thread_worker(void* arg) {
    (void)arg; // Suppress unused parameter warning
    
    printf("INFO: "Telemetry ML export thread started",
             "export_interval_hours", g_telemetry_comprehensive.config.ml_export_interval_hours\n"\n"\n"\n"\n"\n"\n"\n");
    
    while (g_telemetry_comprehensive_initialized && g_telemetry_comprehensive.threads_running) {
        sleep(g_telemetry_comprehensive.config.ml_export_interval_hours * 3600\n"\n"\n"\n"\n"\n"\n"\n");
        
        if (!g_telemetry_comprehensive.threads_running) break;
        
        // Export ML dataset
        if (export_ml_dataset() == AUTONOMY_SUCCESS) {
            printf("INFO: "ML dataset export completed"\n"\n"\n"\n"\n"\n"\n"\n");
        } else {
            printf("WARN: "ML dataset export failed"\n"\n"\n"\n"\n"\n"\n"\n");
        }
    }
    
    printf("INFO: "Telemetry ML export thread stopped"\n"\n"\n"\n"\n"\n"\n"\n");
    return NULL;
}

// Insert decision to database
static int insert_decision_to_database(const decision_record_t* decision) {
    if (!decision || !g_telemetry_comprehensive.db) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    const char* sql = 
        "INSERT INTO decision_records ("
        "timestamp, decision_id, decision_type, trigger, reasoning, confidence, "
        "from_interface, to_interface, from_member, to_member, location_reference_id, "
        "gps_latitude, gps_longitude, gps_accuracy, gps_source, "
        "from_score, to_score, score_difference, from_latency, from_loss, "
        "to_latency, to_loss, success, execution_time_ms, error_message, "
        "root_cause, context_json, recommendations, predictive_decision, "
        "prediction_confidence, validation_time, validation_successful, validation_notes"
        ") VALUES ("
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?"
        ");";
    
    sqlite3_stmt* stmt;
    int result = sqlite3_prepare_v2(g_telemetry_comprehensive.db, sql, -1, &stmt, NULL\n"\n"\n"\n"\n"\n"\n"\n");
    if (result != SQLITE_OK) {
        printf("ERROR: "Failed to prepare decision insert statement", "error", sqlite3_errmsg(g_telemetry_comprehensive.db)\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Bind parameters (simplified for key fields)
    int param = 1;
    sqlite3_bind_int64(stmt, param++, decision->timestamp\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_text(stmt, param++, decision->decision_id, -1, SQLITE_STATIC\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_text(stmt, param++, decision->decision_type, -1, SQLITE_STATIC\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_text(stmt, param++, decision->trigger, -1, SQLITE_STATIC\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_text(stmt, param++, decision->reasoning, -1, SQLITE_STATIC\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, decision->confidence\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_text(stmt, param++, decision->from_interface, -1, SQLITE_STATIC\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_text(stmt, param++, decision->to_interface, -1, SQLITE_STATIC\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_text(stmt, param++, decision->from_member, -1, SQLITE_STATIC\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_text(stmt, param++, decision->to_member, -1, SQLITE_STATIC\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_int(stmt, param++, decision->location_reference_id\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, decision->gps_latitude\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, decision->gps_longitude\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, decision->gps_accuracy\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_text(stmt, param++, decision->gps_source, -1, SQLITE_STATIC\n"\n"\n"\n"\n"\n"\n"\n");
    // Continue with remaining fields...
    sqlite3_bind_double(stmt, param++, decision->from_latency\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, 0.0); // network_throughput - not available
    sqlite3_bind_double(stmt, param++, decision->from_loss\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_bind_double(stmt, param++, 0.0); // network_jitter - not available
    sqlite3_bind_double(stmt, param++, 0.0); // signal_strength - not available
    sqlite3_bind_double(stmt, param++, 0.0); // signal_quality - not available
    sqlite3_bind_double(stmt, param++, 0.0); // battery_level - not available
    sqlite3_bind_double(stmt, param++, 0.0); // cpu_usage - not available
    sqlite3_bind_double(stmt, param++, 0.0); // memory_usage - not available
    sqlite3_bind_double(stmt, param++, 0.0); // disk_usage - not available
    sqlite3_bind_double(stmt, param++, 0.0); // temperature - not available
    sqlite3_bind_double(stmt, param++, 0.0); // uptime - not available
    sqlite3_bind_double(stmt, param++, 0.0); // load_average - not available
    sqlite3_bind_double(stmt, param++, 0.0); // network_errors - not available
    sqlite3_bind_double(stmt, param++, 0.0); // connection_attempts - not available
    sqlite3_bind_double(stmt, param++, decision->success ? 1.0 : 0.0); // success_rate
    sqlite3_bind_double(stmt, param++, decision->to_score); // cost_score
    sqlite3_bind_double(stmt, param++, decision->from_score); // reliability_score
    sqlite3_bind_double(stmt, param++, decision->score_difference); // performance_score
    sqlite3_bind_double(stmt, param++, decision->confidence); // overall_score
    sqlite3_bind_text(stmt, param++, decision->to_interface, -1, SQLITE_STATIC); // selected_interface
    sqlite3_bind_text(stmt, param++, decision->reasoning, -1, SQLITE_STATIC); // decision_reason
    sqlite3_bind_text(stmt, param++, decision->from_interface, -1, SQLITE_STATIC); // fallback_interface
    sqlite3_bind_int(stmt, param++, 0); // failover_count - not available
    sqlite3_bind_int(stmt, param++, 0); // retry_count - not available
    sqlite3_bind_int(stmt, param++, 0); // timeout_count - not available
    sqlite3_bind_int(stmt, param++, decision->success ? 0 : 1); // error_count
    sqlite3_bind_int(stmt, param++, 0); // warning_count - not available
    sqlite3_bind_int(stmt, param++, 1); // info_count - not available
    sqlite3_bind_text(stmt, param++, decision->context_json, -1, SQLITE_STATIC); // metadata
    
    // Execute statement
    result = sqlite3_step(stmt\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_finalize(stmt\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (result != SQLITE_DONE) {
        printf("ERROR: "Failed to insert decision record", "error", sqlite3_errmsg(g_telemetry_comprehensive.db)\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    return AUTONOMY_SUCCESS;
}

// Perform database cleanup
int perform_database_cleanup(void) {
    if (!g_telemetry_comprehensive.db) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    // Delete old records based on retention policy
    time_t cutoff_time = time(NULL) - (g_telemetry_comprehensive.config.retention_hours * 3600\n"\n"\n"\n"\n"\n"\n"\n");
    
    const char* cleanup_sql = "DELETE FROM telemetry_samples WHERE timestamp < ?;";
    
    sqlite3_stmt* stmt;
    int result = sqlite3_prepare_v2(g_telemetry_comprehensive.db, cleanup_sql, -1, &stmt, NULL\n"\n"\n"\n"\n"\n"\n"\n");
    if (result != SQLITE_OK) {
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    sqlite3_bind_int64(stmt, 1, cutoff_time\n"\n"\n"\n"\n"\n"\n"\n");
    
    result = sqlite3_step(stmt\n"\n"\n"\n"\n"\n"\n"\n");
    int deleted_rows = sqlite3_changes(g_telemetry_comprehensive.db\n"\n"\n"\n"\n"\n"\n"\n");
    sqlite3_finalize(stmt\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (result == SQLITE_DONE && deleted_rows > 0) {
        printf("INFO: "Database cleanup completed", "deleted_rows", deleted_rows\n"\n"\n"\n"\n"\n"\n"\n");
        g_telemetry_comprehensive.stats.cleanup_operations++;
    }
    
    return AUTONOMY_SUCCESS;
}

// Export ML dataset
static int export_ml_dataset(void) {
    if (!g_telemetry_comprehensive.db) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    // Export telemetry data for ML training
    if (!g_telemetry_comprehensive.db) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    // Create export directory if it doesn't exist
    char export_dir[256];
    snprintf(export_dir, sizeof(export_dir), "/tmp/ml_exports"\n"\n"\n"\n"\n"\n"\n"\n");
    
    struct stat st = {0};
    if (stat(export_dir, &st) == -1) {
        mkdir(export_dir, 0755\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Generate export filename with timestamp
    char export_file[512];
    time_t now = time(NULL\n"\n"\n"\n"\n"\n"\n"\n");
    struct tm *tm_info = localtime(&now\n"\n"\n"\n"\n"\n"\n"\n");
    strftime(export_file, sizeof(export_file), "%Y%m%d_%H%M%S_telemetry_export.csv", tm_info\n"\n"\n"\n"\n"\n"\n"\n");
    
    char full_path[768];
    snprintf(full_path, sizeof(full_path), "%s/%s", export_dir, export_file\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Export data to CSV format
    FILE *export_fp = fopen(full_path, "w"\n"\n"\n"\n"\n"\n"\n"\n");
    if (!export_fp) {
        printf("ERROR: "Failed to create ML export file", "file", full_path\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Write CSV header
    fprintf(export_fp, "timestamp,interface,latency,throughput,packet_loss,signal_strength,"
                      "gps_latitude,gps_longitude,gps_accuracy,decision_score,selected_interface\n"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Query and export recent telemetry data
    const char* export_sql = "SELECT timestamp, interface, network_latency, network_throughput, "
                            "network_packet_loss, signal_strength, gps_latitude, gps_longitude, "
                            "gps_accuracy, overall_score, selected_interface "
                            "FROM telemetry_samples WHERE timestamp > ? ORDER BY timestamp DESC LIMIT 10000";
    
    sqlite3_stmt* stmt;
    int result = sqlite3_prepare_v2(g_telemetry_comprehensive.db, export_sql, -1, &stmt, NULL\n"\n"\n"\n"\n"\n"\n"\n");
    if (result == SQLITE_OK) {
        time_t cutoff_time = now - (7 * 24 * 3600); // Last 7 days
        sqlite3_bind_int64(stmt, 1, cutoff_time\n"\n"\n"\n"\n"\n"\n"\n");
        
        int exported_count = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            fprintf(export_fp, "%lld,%s,%.3f,%.3f,%.3f,%.3f,%.6f,%.6f,%.3f,%.3f,%s\n",
                   sqlite3_column_int64(stmt, 0), // timestamp
                   sqlite3_column_text(stmt, 1),  // interface
                   sqlite3_column_double(stmt, 2), // latency
                   sqlite3_column_double(stmt, 3), // throughput
                   sqlite3_column_double(stmt, 4), // packet_loss
                   sqlite3_column_double(stmt, 5), // signal_strength
                   sqlite3_column_double(stmt, 6), // gps_latitude
                   sqlite3_column_double(stmt, 7), // gps_longitude
                   sqlite3_column_double(stmt, 8), // gps_accuracy
                   sqlite3_column_double(stmt, 9), // overall_score
                   sqlite3_column_text(stmt, 10)); // selected_interface
            exported_count++;
        }
        sqlite3_finalize(stmt\n"\n"\n"\n"\n"\n"\n"\n");
        
        printf("INFO: "ML dataset export completed", 
                      "file", full_path,
                      "records_exported", exported_count\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        printf("ERROR: "Failed to prepare ML export query"\n"\n"\n"\n"\n"\n"\n"\n");
        sqlite3_finalize(stmt\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    fclose(export_fp\n"\n"\n"\n"\n"\n"\n"\n");
    g_telemetry_comprehensive.stats.last_ml_export = now;
    
    return AUTONOMY_SUCCESS;
}// Close telemetry database
void telemetry_db_close(void) {
    if (g_telemetry_comprehensive.db) {
        sqlite3_close(g_telemetry_comprehensive.db\n"\n"\n"\n"\n"\n"\n"\n");
        g_telemetry_comprehensive.db = NULL;
        g_telemetry_comprehensive.db_initialized = false;
        printf("INFO: "Telemetry database closed"\n"\n"\n"\n"\n"\n"\n"\n");
    }
}

// Test ML algorithm performance on historical data
int telemetry_comprehensive_test_ml_algorithm(const char* algorithm_name,
                                             time_t start_time,
                                             time_t end_time,
                                             char* results_json) {
    if (!algorithm_name || !results_json || start_time >= end_time) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    printf("DEBUG: "Testing ML algorithm on historical data", 
                   "algorithm", algorithm_name,
                   "start_time", start_time,
                   "end_time", end_time\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Initialize results JSON
    snprintf(results_json, 2048, 
             "{\"success\": false, \"algorithm_name\": \"%s\", \"error\": \"\"}", 
             algorithm_name\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Check if database is available
    if (!g_telemetry_comprehensive.db_initialized) {
        snprintf(results_json, 2048, 
                 "{\"success\": false, \"algorithm_name\": \"%s\", \"error\": \"Database not initialized\"}", 
                 algorithm_name\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    // Query historical data for the specified time range
    char query[512];
    snprintf(query, sizeof(query),
             "SELECT COUNT(*) as sample_count, "
             "AVG(latency_ms) as avg_latency, "
             "AVG(packet_loss_percent) as avg_packet_loss, "
             "AVG(signal_strength) as avg_signal_strength, "
             "AVG(throughput_bps) as avg_throughput, "
             "COUNT(CASE WHEN status = 'connected' THEN 1 END) as connected_count, "
             "COUNT(CASE WHEN status = 'disconnected' THEN 1 END) as disconnected_count "
             "FROM telemetry_samples "
              "WHERE timestamp BETWEEN %ld AND %ld",
             (long)start_time, (long)end_time\n"\n"\n"\n"\n"\n"\n"\n");
    
    sqlite3_stmt* stmt;
    int ret = sqlite3_prepare_v2(g_telemetry_comprehensive.db, query, -1, &stmt, NULL\n"\n"\n"\n"\n"\n"\n"\n");
    if (ret != SQLITE_OK) {
        snprintf(results_json, 2048, 
                 "{\"success\": false, \"algorithm_name\": \"%s\", \"error\": \"Database query failed\"}", 
                 algorithm_name\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    int sample_count = 0;
    double avg_latency = 0.0;
    double avg_packet_loss = 0.0;
    double avg_signal_strength = 0.0;
    double avg_throughput = 0.0;
    int connected_count = 0;
    int disconnected_count = 0;
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        sample_count = sqlite3_column_int(stmt, 0\n"\n"\n"\n"\n"\n"\n"\n");
        avg_latency = sqlite3_column_double(stmt, 1\n"\n"\n"\n"\n"\n"\n"\n");
        avg_packet_loss = sqlite3_column_double(stmt, 2\n"\n"\n"\n"\n"\n"\n"\n");
        avg_signal_strength = sqlite3_column_double(stmt, 3\n"\n"\n"\n"\n"\n"\n"\n");
        avg_throughput = sqlite3_column_double(stmt, 4\n"\n"\n"\n"\n"\n"\n"\n");
        connected_count = sqlite3_column_int(stmt, 5\n"\n"\n"\n"\n"\n"\n"\n");
        disconnected_count = sqlite3_column_int(stmt, 6\n"\n"\n"\n"\n"\n"\n"\n");
    }
    sqlite3_finalize(stmt\n"\n"\n"\n"\n"\n"\n"\n");
    
    if (sample_count == 0) {
        snprintf(results_json, 2048, 
                 "{\"success\": false, \"algorithm_name\": \"%s\", \"error\": \"No data found for time range\"}", 
                 algorithm_name\n"\n"\n"\n"\n"\n"\n"\n");
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    // Execute real ML algorithm testing
    char test_script[512];
    snprintf(test_script, sizeof(test_script),
             "python3 /usr/lib/autonomy/ml/test_algorithm.py "
                 "--algorithm %s --start-time %ld --end-time %ld "
             "--data-dir /var/lib/autonomy/telemetry "
             "--output /tmp/ml_test_results.json 2>/dev/null",
             algorithm_name, (long)start_time, (long)end_time\n"\n"\n"\n"\n"\n"\n"\n");
    
    int test_result = system(test_script\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Read test results from Python script
    FILE* results_file = fopen("/tmp/ml_test_results.json", "r"\n"\n"\n"\n"\n"\n"\n"\n");
    if (results_file) {
        char test_results[1024] = {0};
        if (fgets(test_results, sizeof(test_results), results_file)) {
            // Parse and enhance results with real data
            snprintf(results_json, 2048,
                     "{\"success\": true, "
                     "\"algorithm_name\": \"%s\", "
                      "\"time_range\": {\"start\": %ld, \"end\": %ld}, "
                     "\"samples_analyzed\": %d, "
                     "\"data_quality\": {"
                     "\"avg_latency\": %.2f, "
                     "\"avg_packet_loss\": %.2f, "
                     "\"avg_signal_strength\": %.2f, "
                     "\"avg_throughput\": %.0f, "
                     "\"connected_samples\": %d, "
                     "\"disconnected_samples\": %d"
                     "}, "
                     "\"test_results\": %s}",
                     algorithm_name, (long)start_time, (long)end_time, sample_count,
                     avg_latency, avg_packet_loss, avg_signal_strength, avg_throughput,
                     connected_count, disconnected_count, test_results\n"\n"\n"\n"\n"\n"\n"\n");
        } else {
            snprintf(results_json, 2048, 
                     "{\"success\": false, \"algorithm_name\": \"%s\", \"error\": \"Failed to read test results\"}", 
                     algorithm_name\n"\n"\n"\n"\n"\n"\n"\n");
        }
        fclose(results_file\n"\n"\n"\n"\n"\n"\n"\n");
        unlink("/tmp/ml_test_results.json"\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        // Fallback: perform basic statistical analysis
        double connection_rate = (double)connected_count / sample_count;
        double avg_quality_score = (avg_signal_strength + (100.0 - avg_packet_loss) + (1000.0 - avg_latency) / 10.0) / 3.0;
        
        snprintf(results_json, 2048,
                 "{\"success\": true, "
                 "\"algorithm_name\": \"%s\", "
                      "\"time_range\": {\"start\": %ld, \"end\": %ld}, "
                 "\"samples_analyzed\": %d, "
                 "\"statistical_analysis\": {"
                 "\"connection_rate\": %.3f, "
                 "\"avg_quality_score\": %.2f, "
                 "\"avg_latency\": %.2f, "
                 "\"avg_packet_loss\": %.2f, "
                 "\"avg_signal_strength\": %.2f, "
                 "\"avg_throughput\": %.0f"
                 "}, "
                 "\"test_method\": \"statistical_fallback\"}",
                 algorithm_name, (long)start_time, (long)end_time, sample_count,
                 connection_rate, avg_quality_score,
                 avg_latency, avg_packet_loss, avg_signal_strength, avg_throughput\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    printf("INFO: "ML algorithm testing completed", 
                   "algorithm", algorithm_name,
                   "samples", sample_count,
                   "success", test_result == 0\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

// Get telemetry comprehensive statistics
int telemetry_comprehensive_get_statistics(telemetry_collection_statistics_t* stats) {
    if (!stats) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (!g_telemetry_comprehensive_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_telemetry_comprehensive.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    *stats = g_telemetry_comprehensive.stats;
    pthread_mutex_unlock(&g_telemetry_comprehensive.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    return AUTONOMY_SUCCESS;
}

// Get historical telemetry samples
int telemetry_comprehensive_get_historical_samples(const char* member_name,
                                                  time_t start_time,
                                                  time_t end_time,
                                                  telemetry_sample_t* samples,
                                                  int max_samples) {
    if (!samples || max_samples <= 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (!g_telemetry_comprehensive_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_telemetry_comprehensive.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    int samples_returned = 0;
    int start_index = g_telemetry_comprehensive.samples_buffer_head;
    
    for (int i = 0; i < max_samples && i < g_telemetry_comprehensive.samples_buffer_count; i++) {
        int index = (start_index - i - 1 + g_telemetry_comprehensive.samples_buffer_size) % g_telemetry_comprehensive.samples_buffer_size;
        
        // Filter by member name if specified
        if (member_name && strcmp(g_telemetry_comprehensive.samples_buffer[index].member_name, member_name) != 0) {
            continue;
        }
        
        // Filter by time range if specified
        if (start_time > 0 && g_telemetry_comprehensive.samples_buffer[index].timestamp < start_time) {
            continue;
        }
        if (end_time > 0 && g_telemetry_comprehensive.samples_buffer[index].timestamp > end_time) {
            continue;
        }
        
        samples[samples_returned] = g_telemetry_comprehensive.samples_buffer[index];
        samples_returned++;
    }
    
    pthread_mutex_unlock(&g_telemetry_comprehensive.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    return samples_returned;
}

// Get decision history
int telemetry_comprehensive_get_decision_history(time_t start_time,
                                                time_t end_time,
                                                decision_record_t* decisions,
                                                int max_decisions) {
    if (!decisions || max_decisions <= 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    if (!g_telemetry_comprehensive_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_telemetry_comprehensive.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    int decisions_returned = 0;
    int start_index = g_telemetry_comprehensive.decisions_buffer_head;
    
    for (int i = 0; i < max_decisions && i < g_telemetry_comprehensive.decisions_buffer_count; i++) {
        int index = (start_index - i - 1 + g_telemetry_comprehensive.decisions_buffer_size) % g_telemetry_comprehensive.decisions_buffer_size;
        
        // Filter by time range if specified
        if (start_time > 0 && g_telemetry_comprehensive.decisions_buffer[index].timestamp < start_time) {
            continue;
        }
        if (end_time > 0 && g_telemetry_comprehensive.decisions_buffer[index].timestamp > end_time) {
            continue;
        }
        
        decisions[decisions_returned] = g_telemetry_comprehensive.decisions_buffer[index];
        decisions_returned++;
    }
    
    pthread_mutex_unlock(&g_telemetry_comprehensive.mutex\n"\n"\n"\n"\n"\n"\n"\n");
    
    return decisions_returned;
}
