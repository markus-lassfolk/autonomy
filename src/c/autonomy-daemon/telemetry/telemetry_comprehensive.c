#include "telemetry_comprehensive.h"
#include "gps_comprehensive.h"
#include "gps_location_reference.h"
#include "cellular_collector.h"
#include "wifi_enhanced.h"
#include "starlink_comprehensive.h"
#include "performance/performance_monitor.h"
#include "logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>

// Global telemetry comprehensive system
static telemetry_comprehensive_t g_telemetry_comprehensive = {0};
static bool g_telemetry_comprehensive_initialized = false;

// SQL schema for telemetry database
static const char* TELEMETRY_SCHEMA_SQL = 
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
    "CREATE INDEX IF NOT EXISTS idx_telemetry_active ON telemetry_samples(is_active_interface);"
    
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

// Forward declarations
static void* collection_thread_worker(void* arg);
static void* cleanup_thread_worker(void* arg);
static void* export_thread_worker(void* arg);
static int collect_current_telemetry(void);
static int insert_sample_to_database(const telemetry_sample_t* sample);
static int insert_decision_to_database(const decision_record_t* decision);
static int perform_database_cleanup(void);
static int export_ml_dataset(void);
static double calculate_distance_meters(double lat1, double lon1, double lat2, double lon2);

// Initialize comprehensive telemetry collection system
static int telemetry_comprehensive_init(const telemetry_collection_config_t* config) {
    if (g_telemetry_comprehensive_initialized) {
        LOGX_WARN("Comprehensive telemetry already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    if (!config) {
        LOGX_ERROR("Telemetry comprehensive config is NULL");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    memset(&g_telemetry_comprehensive, 0, sizeof(telemetry_comprehensive_t));
    g_telemetry_comprehensive.config = *config;
    
    // Initialize mutex
    if (pthread_mutex_init(&g_telemetry_comprehensive.mutex, NULL) != 0) {
        LOGX_ERROR("Failed to initialize telemetry comprehensive mutex");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Initialize database if persistent storage is enabled
    if (config->enable_persistent_storage) {
        if (telemetry_db_init() != AUTONOMY_SUCCESS) {
            LOGX_ERROR("Failed to initialize telemetry database");
            pthread_mutex_destroy(&g_telemetry_comprehensive.mutex);
            return AUTONOMY_ERROR_SYSTEM;
        }
        g_telemetry_comprehensive.db_initialized = true;
        
        // Initialize GPS location reference system for storage optimization
        gps_location_reference_config_t location_config = {
            .enabled = true,
            .precision_reduction_meters = 10.0,    // 10m precision (saves ~50% space)
            .movement_threshold_meters = 50.0,     // 50m movement threshold
            .max_locations = 10000,                // Max 10k unique locations
            .cleanup_interval_hours = 24,          // Daily cleanup
            .min_usage_for_retention = 5,          // Keep locations used 5+ times
            .retention_days = 30,                  // 30 day retention
            .enable_location_clustering = true,
            .clustering_radius_meters = 20.0,      // 20m clustering
            .max_cluster_size = 5,
            .track_location_performance = true,
            .enable_location_scoring = true
        };
        
        if (gps_location_reference_init(&location_config) != AUTONOMY_SUCCESS) {
            LOGX_ERROR("Failed to initialize GPS location reference system");
            telemetry_db_close();
            pthread_mutex_destroy(&g_telemetry_comprehensive.mutex);
            return AUTONOMY_ERROR_SYSTEM;
        }
    }
    
    // Allocate memory for ring buffers
    g_telemetry_comprehensive.samples_buffer_size = 1000; // Keep 1000 samples in memory
    g_telemetry_comprehensive.samples_buffer = calloc(g_telemetry_comprehensive.samples_buffer_size,
                                                     sizeof(telemetry_sample_t));
    if (!g_telemetry_comprehensive.samples_buffer) {
        LOGX_ERROR("Failed to allocate memory for samples buffer");
        if (g_telemetry_comprehensive.db_initialized) telemetry_db_close();
        pthread_mutex_destroy(&g_telemetry_comprehensive.mutex);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    g_telemetry_comprehensive.decisions_buffer_size = 200; // Keep 200 decisions in memory
    g_telemetry_comprehensive.decisions_buffer = calloc(g_telemetry_comprehensive.decisions_buffer_size,
                                                       sizeof(decision_record_t));
    if (!g_telemetry_comprehensive.decisions_buffer) {
        LOGX_ERROR("Failed to allocate memory for decisions buffer");
        free(g_telemetry_comprehensive.samples_buffer);
        if (g_telemetry_comprehensive.db_initialized) telemetry_db_close();
        pthread_mutex_destroy(&g_telemetry_comprehensive.mutex);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Initialize statistics
    g_telemetry_comprehensive.stats.collection_start_time = time(NULL);
    g_telemetry_comprehensive.next_sample_id = 1;
    g_telemetry_comprehensive.next_decision_id = 1;
    
    // Start background threads if enabled
    if (config->enabled) {
        g_telemetry_comprehensive.threads_running = true;
        
        // Collection thread
        if (pthread_create(&g_telemetry_comprehensive.collection_thread, NULL, 
                          collection_thread_worker, NULL) != 0) {
            LOGX_ERROR("Failed to create telemetry collection thread");
            free(g_telemetry_comprehensive.samples_buffer);
            free(g_telemetry_comprehensive.decisions_buffer);
            if (g_telemetry_comprehensive.db_initialized) telemetry_db_close();
            pthread_mutex_destroy(&g_telemetry_comprehensive.mutex);
            return AUTONOMY_ERROR_SYSTEM;
        }
        
        // Cleanup thread
        if (pthread_create(&g_telemetry_comprehensive.cleanup_thread, NULL, 
                          cleanup_thread_worker, NULL) != 0) {
            LOGX_ERROR("Failed to create telemetry cleanup thread");
            g_telemetry_comprehensive.threads_running = false;
            pthread_cancel(g_telemetry_comprehensive.collection_thread);
            pthread_join(g_telemetry_comprehensive.collection_thread, NULL);
            free(g_telemetry_comprehensive.samples_buffer);
            free(g_telemetry_comprehensive.decisions_buffer);
            if (g_telemetry_comprehensive.db_initialized) telemetry_db_close();
            pthread_mutex_destroy(&g_telemetry_comprehensive.mutex);
            return AUTONOMY_ERROR_SYSTEM;
        }
        
        // ML export thread (if enabled)
        if (config->enable_ml_dataset_export) {
            if (pthread_create(&g_telemetry_comprehensive.export_thread, NULL, 
                              export_thread_worker, NULL) != 0) {
                LOGX_WARN("Failed to create ML export thread, continuing without ML export");
            }
        }
    }
    
    g_telemetry_comprehensive_initialized = true;
    
    LOGX_INFO("Comprehensive telemetry collection initialized",
              "enabled", config->enabled,
              "persistent_storage", config->enable_persistent_storage,
              "collection_interval_s", config->collection_interval_s,
              "retention_hours", config->retention_hours,
              "ml_export", config->enable_ml_dataset_export);
    
    return AUTONOMY_SUCCESS;
}

// Cleanup comprehensive telemetry collection system
static void telemetry_comprehensive_cleanup(void) {
    if (!g_telemetry_comprehensive_initialized) return;
    
    pthread_mutex_lock(&g_telemetry_comprehensive.mutex);
    
    // Stop background threads
    g_telemetry_comprehensive.threads_running = false;
    
    if (g_telemetry_comprehensive.config.enabled) {
        pthread_cancel(g_telemetry_comprehensive.collection_thread);
        pthread_cancel(g_telemetry_comprehensive.cleanup_thread);
        if (g_telemetry_comprehensive.config.enable_ml_dataset_export) {
            pthread_cancel(g_telemetry_comprehensive.export_thread);
        }
        
        pthread_join(g_telemetry_comprehensive.collection_thread, NULL);
        pthread_join(g_telemetry_comprehensive.cleanup_thread, NULL);
        if (g_telemetry_comprehensive.config.enable_ml_dataset_export) {
            pthread_join(g_telemetry_comprehensive.export_thread, NULL);
        }
    }
    
    // Close database
    if (g_telemetry_comprehensive.db_initialized) {
        telemetry_db_close();
    }
    
    // Free memory
    free(g_telemetry_comprehensive.samples_buffer);
    free(g_telemetry_comprehensive.decisions_buffer);
    
    pthread_mutex_unlock(&g_telemetry_comprehensive.mutex);
    pthread_mutex_destroy(&g_telemetry_comprehensive.mutex);
    
    g_telemetry_comprehensive_initialized = false;
    
    LOGX_INFO("Comprehensive telemetry collection cleaned up");
}

// Collect telemetry sample with GPS positioning
int telemetry_comprehensive_collect_sample(const char* member_name,
                                          const char* interface_name,
                                          const telemetry_sample_t* sample) {
    if (!g_telemetry_comprehensive_initialized || !member_name || !interface_name || !sample) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_telemetry_comprehensive.mutex);
    
    // Create sample copy with metadata
    telemetry_sample_t* buffer_sample = &g_telemetry_comprehensive.samples_buffer[g_telemetry_comprehensive.samples_buffer_head];
    *buffer_sample = *sample;
    
    buffer_sample->id = g_telemetry_comprehensive.next_sample_id++;
    buffer_sample->timestamp = time(NULL);
    strncpy(buffer_sample->member_name, member_name, sizeof(buffer_sample->member_name) - 1);
    buffer_sample->member_name[sizeof(buffer_sample->member_name) - 1] = '\0';
    strncpy(buffer_sample->interface_name, interface_name, sizeof(buffer_sample->interface_name) - 1);
    buffer_sample->interface_name[sizeof(buffer_sample->interface_name) - 1] = '\0';
    
    // Get location reference ID if GPS data is available and location reference system is enabled
    if (gps_location_reference_is_initialized() && 
        sample->location_reference_id == 0) { // Only if not already set
        
        // Try to get current GPS data for location reference
        standardized_gps_data_t gps_data;
        if (gps_comprehensive_is_initialized() && 
            gps_comprehensive_collect_best_gps(&gps_data) == AUTONOMY_SUCCESS && gps_data.valid) {
            
            uint32_t location_id;
            if (gps_location_reference_get_or_create(gps_data.latitude, gps_data.longitude,
                                                    gps_data.accuracy, gps_data.source,
                                                    &location_id) == AUTONOMY_SUCCESS) {
                buffer_sample->location_reference_id = location_id;
                buffer_sample->gps_accuracy = gps_data.accuracy;
                strncpy(buffer_sample->gps_source, gps_data.source, sizeof(buffer_sample->gps_source) - 1);
                buffer_sample->gps_source[sizeof(buffer_sample->gps_source) - 1] = '\0';
                
                // Update location usage with performance metrics
                gps_location_reference_update_usage(location_id, sample->signal_quality, sample->latency_ms);
                
                LOGX_DEBUG("Using GPS location reference",
                          "location_id", location_id,
                          "lat", gps_data.latitude,
                          "lon", gps_data.longitude);
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
    g_telemetry_comprehensive.stats.last_collection = time(NULL);
    
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
    
    pthread_mutex_unlock(&g_telemetry_comprehensive.mutex);
    
    LOGX_DEBUG("Telemetry sample collected",
              "id", buffer_sample->id,
              "member", member_name,
              "interface", interface_name,
              "gps_valid", (sample->latitude != 0.0 && sample->longitude != 0.0),
              "latency", sample->latency_ms,
              "loss", sample->packet_loss_percent);
    
    return AUTONOMY_SUCCESS;
}

// Log failover/failback decision with full context
static int telemetry_comprehensive_log_decision(const decision_record_t* decision) {
    if (!g_telemetry_comprehensive_initialized || !decision) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_telemetry_comprehensive.mutex);
    
    // Create decision copy with metadata
    decision_record_t* buffer_decision = &g_telemetry_comprehensive.decisions_buffer[g_telemetry_comprehensive.decisions_buffer_head];
    *buffer_decision = *decision;
    
    buffer_decision->id = g_telemetry_comprehensive.next_decision_id++;
    buffer_decision->timestamp = time(NULL);
    
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
    
    pthread_mutex_unlock(&g_telemetry_comprehensive.mutex);
    
    LOGX_INFO("Failover decision logged",
             "id", buffer_decision->id,
             "decision_id", decision->decision_id,
             "type", decision->decision_type,
             "from", decision->from_interface,
             "to", decision->to_interface,
             "success", decision->success,
             "confidence", decision->confidence,
             "predictive", decision->predictive_decision);
    
    return AUTONOMY_SUCCESS;
}

// Initialize SQLite database with proper schema
static int telemetry_db_init(void) {
    // Ensure database directory exists
    char db_dir[256];
    strncpy(db_dir, g_telemetry_comprehensive.config.database_path, sizeof(db_dir) - 1);
    db_dir[sizeof(db_dir) - 1] = '\0';
    
    char* last_slash = strrchr(db_dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        if (mkdir(db_dir, 0755) != 0 && errno != EEXIST) {
            LOGX_ERROR("Failed to create database directory", "path", db_dir, "error", strerror(errno));
            return AUTONOMY_ERROR_SYSTEM;
        }
    }
    
    // Open database
    int result = sqlite3_open(g_telemetry_comprehensive.config.database_path, &g_telemetry_comprehensive.db);
    if (result != SQLITE_OK) {
        LOGX_ERROR("Failed to open telemetry database",
                  "path", g_telemetry_comprehensive.config.database_path,
                  "error", sqlite3_errmsg(g_telemetry_comprehensive.db));
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Execute schema creation
    char* error_msg = NULL;
    result = sqlite3_exec(g_telemetry_comprehensive.db, TELEMETRY_SCHEMA_SQL, NULL, NULL, &error_msg);
    if (result != SQLITE_OK) {
        LOGX_ERROR("Failed to create telemetry database schema",
                  "error", error_msg);
        sqlite3_free(error_msg);
        sqlite3_close(g_telemetry_comprehensive.db);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Enable WAL mode for better performance
    result = sqlite3_exec(g_telemetry_comprehensive.db, "PRAGMA journal_mode=WAL;", NULL, NULL, &error_msg);
    if (result != SQLITE_OK) {
        LOGX_WARN("Failed to enable WAL mode", "error", error_msg);
        sqlite3_free(error_msg);
    }
    
    LOGX_INFO("Telemetry database initialized",
             "path", g_telemetry_comprehensive.config.database_path);
    
    return AUTONOMY_SUCCESS;
}

// Background collection thread
static void* collection_thread_worker(void* arg) {
    LOGX_INFO("Telemetry collection thread started",
             "interval_s", g_telemetry_comprehensive.config.collection_interval_s);
    
    while (g_telemetry_comprehensive_initialized && g_telemetry_comprehensive.threads_running) {
        sleep(g_telemetry_comprehensive.config.collection_interval_s);
        
        if (!g_telemetry_comprehensive.threads_running) break;
        
        // Collect current telemetry from all sources
        if (collect_current_telemetry() == AUTONOMY_SUCCESS) {
            LOGX_DEBUG("Background telemetry collection successful");
        } else {
            LOGX_WARN("Background telemetry collection failed");
        }
    }
    
    LOGX_INFO("Telemetry collection thread stopped");
    return NULL;
}

// Collect current telemetry from all sources
static int collect_current_telemetry(void) {
    time_t collection_start = time(NULL);
    
    // Get current GPS data
    standardized_gps_data_t gps_data;
    bool gps_valid = false;
    if (gps_comprehensive_is_initialized() && 
        gps_comprehensive_collect_best_gps(&gps_data) == AUTONOMY_SUCCESS) {
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
                    strncpy(sample.gps_source, gps_data.source, sizeof(sample.gps_source) - 1);
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
            strcpy(sample.collection_method, "comprehensive");
            sample.collection_time_ms = starlink_status.collection_duration_ms;
            
            // Collect sample
            telemetry_comprehensive_collect_sample("starlink", "starlink0", &sample);
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
                sample.satellites = gps_data.satellites;
                sample.hdop = gps_data.hdop;
                strncpy(sample.gps_source, gps_data.source, sizeof(sample.gps_source) - 1);
                sample.gps_source[sizeof(sample.gps_source) - 1] = '\0';
                sample.movement_kmh = gps_data.speed * 3.6;
            }
            
            // Fill cellular metrics
            sample.rsrp_dbm = cellular_info.rsrp;
            sample.rsrq_db = cellular_info.rsrq;
            sample.sinr_db = cellular_info.sinr;
            strncpy(sample.carrier, cellular_info.operator, sizeof(sample.carrier) - 1);
            sample.carrier[sizeof(sample.carrier) - 1] = '\0';
            sample.cell_id = cellular_info.cell_id;
            sample.signal_quality = cellular_info.signal_quality / 100.0;
            sample.overall_score = cellular_info.reliability_score * 100.0;
            sample.predictive_risk = cellular_info.predictive_risk;
            strcpy(sample.collection_method, "cellular_collector");
            
            telemetry_comprehensive_collect_sample("cellular", "wwan0", &sample);
        }
    }
    
    // Get system metrics
    performance_metrics_t perf_metrics;
    if (performance_monitor_get_metrics(&perf_metrics) == AUTONOMY_SUCCESS) {
        // Add system metrics to the last collected sample
        pthread_mutex_lock(&g_telemetry_comprehensive.mutex);
        if (g_telemetry_comprehensive.samples_buffer_count > 0) {
            int last_index = (g_telemetry_comprehensive.samples_buffer_head - 1 + 
                             g_telemetry_comprehensive.samples_buffer_size) % 
                             g_telemetry_comprehensive.samples_buffer_size;
            
            telemetry_sample_t* last_sample = &g_telemetry_comprehensive.samples_buffer[last_index];
            last_sample->cpu_usage_percent = perf_metrics.cpu_usage_percent;
            last_sample->memory_usage_percent = perf_metrics.memory_usage_percent;
            last_sample->load_avg_1min = perf_metrics.load_average_1min;
        }
        pthread_mutex_unlock(&g_telemetry_comprehensive.mutex);
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
    int result = sqlite3_prepare_v2(g_telemetry_comprehensive.db, sql, -1, &stmt, NULL);
    if (result != SQLITE_OK) {
        LOGX_ERROR("Failed to prepare telemetry insert statement", "error", sqlite3_errmsg(g_telemetry_comprehensive.db));
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Bind parameters
    int param = 1;
    sqlite3_bind_int64(stmt, param++, sample->timestamp);
    sqlite3_bind_text(stmt, param++, sample->member_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, param++, sample->interface_name, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, param++, sample->latitude);
    sqlite3_bind_double(stmt, param++, sample->longitude);
    sqlite3_bind_double(stmt, param++, sample->accuracy);
    sqlite3_bind_int(stmt, param++, sample->satellites);
    sqlite3_bind_double(stmt, param++, sample->hdop);
    sqlite3_bind_text(stmt, param++, sample->gps_source, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, param++, sample->movement_kmh);
    sqlite3_bind_double(stmt, param++, sample->latency_ms);
    sqlite3_bind_double(stmt, param++, sample->packet_loss_percent);
    sqlite3_bind_double(stmt, param++, sample->jitter_ms);
    sqlite3_bind_int64(stmt, param++, sample->throughput_bps);
    sqlite3_bind_double(stmt, param++, sample->signal_quality);
    sqlite3_bind_text(stmt, param++, sample->status, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, param++, sample->obstruction_percent);
    sqlite3_bind_double(stmt, param++, sample->snr_db);
    sqlite3_bind_double(stmt, param++, sample->temperature_c);
    sqlite3_bind_int(stmt, param++, sample->outage_count);
    sqlite3_bind_double(stmt, param++, sample->pop_ping_drop_rate);
    sqlite3_bind_double(stmt, param++, sample->rsrp_dbm);
    sqlite3_bind_double(stmt, param++, sample->rsrq_db);
    sqlite3_bind_double(stmt, param++, sample->sinr_db);
    sqlite3_bind_text(stmt, param++, sample->carrier, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, param++, sample->cell_id);
    sqlite3_bind_int(stmt, param++, sample->cell_changes);
    sqlite3_bind_double(stmt, param++, sample->wifi_rssi_dbm);
    sqlite3_bind_int(stmt, param++, sample->wifi_channel);
    sqlite3_bind_text(stmt, param++, sample->wifi_ssid, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, param++, sample->wifi_noise_floor);
    sqlite3_bind_double(stmt, param++, sample->cpu_usage_percent);
    sqlite3_bind_double(stmt, param++, sample->memory_usage_percent);
    sqlite3_bind_double(stmt, param++, sample->disk_usage_percent);
    sqlite3_bind_double(stmt, param++, sample->load_avg_1min);
    sqlite3_bind_double(stmt, param++, sample->overall_score);
    sqlite3_bind_double(stmt, param++, sample->reliability_score);
    sqlite3_bind_double(stmt, param++, sample->predictive_risk);
    sqlite3_bind_int(stmt, param++, sample->is_active_interface ? 1 : 0);
    sqlite3_bind_text(stmt, param++, sample->collection_method, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, param++, sample->collection_time_ms);
    
    // Execute statement
    result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (result != SQLITE_DONE) {
        LOGX_ERROR("Failed to insert telemetry sample", "error", sqlite3_errmsg(g_telemetry_comprehensive.db));
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    return AUTONOMY_SUCCESS;
}

static bool telemetry_comprehensive_is_initialized(void) {
    return g_telemetry_comprehensive_initialized;
}

// Additional functions would be implemented here...
// (get_historical_samples, get_decision_history, export_ml_dataset, etc.)