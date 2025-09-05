#include "gps_location_reference.h"
#include "logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>

// Global GPS location reference manager
static gps_location_reference_manager_t g_location_ref_manager = {0};
static bool g_location_ref_initialized = false;

// Earth radius in meters (for distance calculations)
static const double EARTH_RADIUS_M = 6371000.0;

// SQL schema for location reference database
static const char* LOCATION_REFERENCE_SCHEMA_SQL = 
    "CREATE TABLE IF NOT EXISTS gps_location_references ("
    "    location_id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    latitude_reduced REAL NOT NULL,"
    "    longitude_reduced REAL NOT NULL,"
    "    latitude_original REAL NOT NULL,"
    "    longitude_original REAL NOT NULL,"
    "    accuracy_meters REAL,"
    "    gps_source TEXT,"
    "    first_recorded INTEGER NOT NULL,"
    "    last_used INTEGER NOT NULL,"
    "    usage_count INTEGER DEFAULT 1,"
    "    telemetry_samples INTEGER DEFAULT 0,"
    "    location_name TEXT,"
    "    is_stationary_location INTEGER DEFAULT 0,"
    "    avg_movement_speed_kmh REAL DEFAULT 0.0,"
    "    avg_signal_quality REAL DEFAULT 0.0,"
    "    avg_latency_ms REAL DEFAULT 0.0,"
    "    location_score REAL DEFAULT 0.0"
    ");"
    
    "CREATE UNIQUE INDEX IF NOT EXISTS idx_location_coords ON gps_location_references("
    "    latitude_reduced, longitude_reduced"
    ");"
    
    "CREATE INDEX IF NOT EXISTS idx_location_usage ON gps_location_references(last_used);"
    "CREATE INDEX IF NOT EXISTS idx_location_score ON gps_location_references(location_score);";

// Forward declarations
static void* cleanup_thread_worker(void* arg);
static int create_new_location_reference(double latitude, double longitude, double accuracy, 
                                        const char* gps_source, uint32_t* location_id);
static int find_nearby_location_reference(double latitude, double longitude, uint32_t* location_id);
static int init_location_database(void);
static void close_location_database(void);

// Initialize GPS location reference system
static int gps_location_reference_init(const gps_location_reference_config_t* config) {
    if (g_location_ref_initialized) {
        LOGX_WARN("GPS location reference already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    if (!config) {
        LOGX_ERROR("GPS location reference config is NULL");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    memset(&g_location_ref_manager, 0, sizeof(gps_location_reference_manager_t));
    g_location_ref_manager.config = *config;
    
    // Initialize mutex
    if (pthread_mutex_init(&g_location_ref_manager.mutex, NULL) != 0) {
        LOGX_ERROR("Failed to initialize GPS location reference mutex");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Initialize database
    if (init_location_database() != AUTONOMY_SUCCESS) {
        LOGX_ERROR("Failed to initialize location reference database");
        pthread_mutex_destroy(&g_location_ref_manager.mutex);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Allocate memory for location cache
    g_location_ref_manager.cache_size = 100; // Cache last 100 locations
    g_location_ref_manager.location_cache = calloc(g_location_ref_manager.cache_size,
                                                  sizeof(gps_location_reference_t));
    if (!g_location_ref_manager.location_cache) {
        LOGX_ERROR("Failed to allocate memory for location cache");
        close_location_database();
        pthread_mutex_destroy(&g_location_ref_manager.mutex);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Initialize statistics
    g_location_ref_manager.stats.stats_start_time = time(NULL);
    g_location_ref_manager.next_location_id = 1;
    
    // Start cleanup thread if enabled
    if (config->enabled) {
        g_location_ref_manager.thread_running = true;
        
        if (pthread_create(&g_location_ref_manager.cleanup_thread, NULL, 
                          cleanup_thread_worker, NULL) != 0) {
            LOGX_ERROR("Failed to create location reference cleanup thread");
            free(g_location_ref_manager.location_cache);
            close_location_database();
            pthread_mutex_destroy(&g_location_ref_manager.mutex);
            return AUTONOMY_ERROR_SYSTEM;
        }
    }
    
    g_location_ref_initialized = true;
    
    LOGX_INFO("GPS location reference system initialized",
              "enabled", config->enabled,
              "precision_reduction_m", config->precision_reduction_meters,
              "movement_threshold_m", config->movement_threshold_meters,
              "max_locations", config->max_locations);
    
    return AUTONOMY_SUCCESS;
}

// Cleanup GPS location reference system
static void gps_location_reference_cleanup(void) {
    if (!g_location_ref_initialized) return;
    
    pthread_mutex_lock(&g_location_ref_manager.mutex);
    
    // Stop cleanup thread
    g_location_ref_manager.thread_running = false;
    
    if (g_location_ref_manager.config.enabled) {
        pthread_cancel(g_location_ref_manager.cleanup_thread);
        pthread_join(g_location_ref_manager.cleanup_thread, NULL);
    }
    
    // Close database
    close_location_database();
    
    // Free memory
    free(g_location_ref_manager.location_cache);
    
    pthread_mutex_unlock(&g_location_ref_manager.mutex);
    pthread_mutex_destroy(&g_location_ref_manager.mutex);
    
    g_location_ref_initialized = false;
    
    LOGX_INFO("GPS location reference system cleaned up");
}

// Get or create location reference for GPS coordinates
int gps_location_reference_get_or_create(double latitude, double longitude, 
                                        double accuracy, const char* gps_source,
                                        uint32_t* location_id) {
    if (!g_location_ref_initialized || !location_id) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Invalid coordinates
    if (latitude == 0.0 && longitude == 0.0) {
        *location_id = 0; // Special ID for no GPS
        return AUTONOMY_SUCCESS;
    }
    
    pthread_mutex_lock(&g_location_ref_manager.mutex);
    
    // Check if we should create a new location based on movement threshold
    bool create_new_location = true;
    
    if (g_location_ref_manager.last_location_valid) {
        double distance = gps_calculate_distance_meters(
            latitude, longitude,
            g_location_ref_manager.last_location.latitude_original,
            g_location_ref_manager.last_location.longitude_original
        );
        
        if (distance < g_location_ref_manager.config.movement_threshold_meters) {
            // Movement threshold not exceeded, use existing location
            *location_id = g_location_ref_manager.last_location.location_id;
            create_new_location = false;
            
            // Update usage statistics
            gps_location_reference_update_usage(*location_id, 0.0, 0.0); // Basic usage update
            
            LOGX_DEBUG("Using existing location reference",
                      "location_id", *location_id,
                      "distance_m", distance,
                      "threshold_m", g_location_ref_manager.config.movement_threshold_meters);
        }
    }
    
    if (create_new_location) {
        // Check for nearby existing location
        if (find_nearby_location_reference(latitude, longitude, location_id) == AUTONOMY_SUCCESS) {
            LOGX_DEBUG("Found nearby location reference",
                      "location_id", *location_id);
            create_new_location = false;
        }
    }
    
    if (create_new_location) {
        // Create new location reference
        if (create_new_location_reference(latitude, longitude, accuracy, gps_source, location_id) == AUTONOMY_SUCCESS) {
            g_location_ref_manager.stats.total_locations++;
            
            LOGX_INFO("Created new location reference",
                     "location_id", *location_id,
                     "lat", latitude,
                     "lon", longitude,
                     "accuracy", accuracy);
        } else {
            pthread_mutex_unlock(&g_location_ref_manager.mutex);
            return AUTONOMY_ERROR_SYSTEM;
        }
    }
    
    // Update last known location
    if (*location_id > 0) {
        gps_location_reference_get_by_id(*location_id, &g_location_ref_manager.last_location);
        g_location_ref_manager.last_location_valid = true;
    }
    
    // Update statistics
    g_location_ref_manager.stats.total_references++;
    
    pthread_mutex_unlock(&g_location_ref_manager.mutex);
    
    return AUTONOMY_SUCCESS;
}

// Reduce GPS coordinate precision for storage optimization
static double gps_reduce_coordinate_precision(double coordinate, double precision_meters) {
    // Calculate precision reduction factor
    // At equator: 1 degree ≈ 111,000 meters
    // For 10m precision: ~0.00009 degrees
    double precision_degrees = precision_meters / 111000.0;
    
    // Round coordinate to nearest precision unit
    return round(coordinate / precision_degrees) * precision_degrees;
}

// Calculate distance between two GPS coordinates using Haversine formula
static double gps_calculate_distance_meters(double lat1, double lon1, double lat2, double lon2) {
    // Convert degrees to radians
    double lat1_rad = lat1 * M_PI / 180.0;
    double lon1_rad = lon1 * M_PI / 180.0;
    double lat2_rad = lat2 * M_PI / 180.0;
    double lon2_rad = lon2 * M_PI / 180.0;
    
    // Haversine formula
    double dlat = lat2_rad - lat1_rad;
    double dlon = lon2_rad - lon1_rad;
    
    double a = sin(dlat/2) * sin(dlat/2) + 
               cos(lat1_rad) * cos(lat2_rad) * 
               sin(dlon/2) * sin(dlon/2);
    
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    
    return EARTH_RADIUS_M * c;
}

// Check if movement threshold is exceeded
bool gps_movement_threshold_exceeded(double current_lat, double current_lon,
                                    double reference_lat, double reference_lon,
                                    double threshold_meters) {
    if (current_lat == 0.0 && current_lon == 0.0) return false;
    if (reference_lat == 0.0 && reference_lon == 0.0) return true;
    
    double distance = gps_calculate_distance_meters(current_lat, current_lon, 
                                                   reference_lat, reference_lon);
    return distance >= threshold_meters;
}

// Create new location reference
static int create_new_location_reference(double latitude, double longitude, double accuracy, 
                                        const char* gps_source, uint32_t* location_id) {
    if (!g_location_ref_manager.db || !location_id) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Reduce coordinate precision for storage optimization
    double lat_reduced = gps_reduce_coordinate_precision(latitude, 
                                                        g_location_ref_manager.config.precision_reduction_meters);
    double lon_reduced = gps_reduce_coordinate_precision(longitude, 
                                                        g_location_ref_manager.config.precision_reduction_meters);
    
    const char* sql = 
        "INSERT INTO gps_location_references ("
        "    latitude_reduced, longitude_reduced, latitude_original, longitude_original, "
        "    accuracy_meters, gps_source, first_recorded, last_used"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    
    sqlite3_stmt* stmt;
    int result = sqlite3_prepare_v2(g_location_ref_manager.db, sql, -1, &stmt, NULL);
    if (result != SQLITE_OK) {
        LOGX_ERROR("Failed to prepare location insert statement", 
                  "error", sqlite3_errmsg(g_location_ref_manager.db));
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    time_t now = time(NULL);
    
    sqlite3_bind_double(stmt, 1, lat_reduced);
    sqlite3_bind_double(stmt, 2, lon_reduced);
    sqlite3_bind_double(stmt, 3, latitude);
    sqlite3_bind_double(stmt, 4, longitude);
    sqlite3_bind_double(stmt, 5, accuracy);
    sqlite3_bind_text(stmt, 6, gps_source ? gps_source : "unknown", -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 7, now);
    sqlite3_bind_int64(stmt, 8, now);
    
    result = sqlite3_step(stmt);
    
    if (result == SQLITE_DONE) {
        *location_id = (uint32_t)sqlite3_last_insert_rowid(g_location_ref_manager.db);
        
        // Add to cache
        if (g_location_ref_manager.cache_count < g_location_ref_manager.cache_size) {
            gps_location_reference_t* cache_entry = 
                &g_location_ref_manager.location_cache[g_location_ref_manager.cache_count];
            
            cache_entry->location_id = *location_id;
            cache_entry->latitude_reduced = lat_reduced;
            cache_entry->longitude_reduced = lon_reduced;
            cache_entry->latitude_original = latitude;
            cache_entry->longitude_original = longitude;
            cache_entry->accuracy_meters = accuracy;
            strncpy(cache_entry->gps_source, gps_source ? gps_source : "unknown", 
                   sizeof(cache_entry->gps_source) - 1);
            cache_entry->first_recorded = now;
            cache_entry->last_used = now;
            cache_entry->usage_count = 1;
            
            g_location_ref_manager.cache_count++;
        }
        
        // Calculate space saved
        uint64_t space_saved = sizeof(double) * 2; // Two coordinates not stored per sample
        g_location_ref_manager.stats.space_saved_bytes += space_saved;
        
        LOGX_DEBUG("Created new location reference",
                  "location_id", *location_id,
                  "lat_original", latitude,
                  "lon_original", longitude,
                  "lat_reduced", lat_reduced,
                  "lon_reduced", lon_reduced,
                  "precision_reduction_m", g_location_ref_manager.config.precision_reduction_meters);
    } else {
        LOGX_ERROR("Failed to insert location reference", 
                  "error", sqlite3_errmsg(g_location_ref_manager.db));
        sqlite3_finalize(stmt);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    sqlite3_finalize(stmt);
    return AUTONOMY_SUCCESS;
}

// Find nearby location reference
static int find_nearby_location_reference(double latitude, double longitude, uint32_t* location_id) {
    if (!g_location_ref_manager.db || !location_id) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // First check cache for performance
    for (int i = 0; i < g_location_ref_manager.cache_count; i++) {
        gps_location_reference_t* cached = &g_location_ref_manager.location_cache[i];
        
        double distance = gps_calculate_distance_meters(latitude, longitude,
                                                       cached->latitude_original,
                                                       cached->longitude_original);
        
        if (distance < g_location_ref_manager.config.movement_threshold_meters) {
            *location_id = cached->location_id;
            g_location_ref_manager.stats.cache_hits++;
            return AUTONOMY_SUCCESS;
        }
    }
    
    g_location_ref_manager.stats.cache_misses++;
    
    // Search database for nearby locations
    const char* sql = 
        "SELECT location_id, latitude_original, longitude_original "
        "FROM gps_location_references "
        "WHERE latitude_reduced BETWEEN ? AND ? "
        "AND longitude_reduced BETWEEN ? AND ? "
        "ORDER BY last_used DESC "
        "LIMIT 10;";
    
    sqlite3_stmt* stmt;
    int result = sqlite3_prepare_v2(g_location_ref_manager.db, sql, -1, &stmt, NULL);
    if (result != SQLITE_OK) {
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Calculate search bounds (approximate)
    double search_degrees = g_location_ref_manager.config.movement_threshold_meters / 111000.0;
    
    sqlite3_bind_double(stmt, 1, latitude - search_degrees);
    sqlite3_bind_double(stmt, 2, latitude + search_degrees);
    sqlite3_bind_double(stmt, 3, longitude - search_degrees);
    sqlite3_bind_double(stmt, 4, longitude + search_degrees);
    
    bool found = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        uint32_t candidate_id = sqlite3_column_int(stmt, 0);
        double candidate_lat = sqlite3_column_double(stmt, 1);
        double candidate_lon = sqlite3_column_double(stmt, 2);
        
        double distance = gps_calculate_distance_meters(latitude, longitude,
                                                       candidate_lat, candidate_lon);
        
        if (distance < g_location_ref_manager.config.movement_threshold_meters) {
            *location_id = candidate_id;
            found = true;
            break;
        }
    }
    
    sqlite3_finalize(stmt);
    
    return found ? AUTONOMY_SUCCESS : AUTONOMY_ERROR_NOT_FOUND;
}

// Update location usage statistics
static int gps_location_reference_update_usage(uint32_t location_id, double signal_quality, double latency_ms) {
    if (!g_location_ref_initialized || location_id == 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_location_ref_manager.mutex);
    
    const char* sql = 
        "UPDATE gps_location_references SET "
        "    last_used = ?, "
        "    usage_count = usage_count + 1, "
        "    telemetry_samples = telemetry_samples + 1, "
        "    avg_signal_quality = CASE "
        "        WHEN avg_signal_quality = 0.0 THEN ? "
        "        ELSE (avg_signal_quality * 0.9) + (? * 0.1) "
        "    END, "
        "    avg_latency_ms = CASE "
        "        WHEN avg_latency_ms = 0.0 THEN ? "
        "        ELSE (avg_latency_ms * 0.9) + (? * 0.1) "
        "    END "
        "WHERE location_id = ?;";
    
    sqlite3_stmt* stmt;
    int result = sqlite3_prepare_v2(g_location_ref_manager.db, sql, -1, &stmt, NULL);
    if (result != SQLITE_OK) {
        pthread_mutex_unlock(&g_location_ref_manager.mutex);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    sqlite3_bind_int64(stmt, 1, time(NULL));
    sqlite3_bind_double(stmt, 2, signal_quality);
    sqlite3_bind_double(stmt, 3, signal_quality);
    sqlite3_bind_double(stmt, 4, latency_ms);
    sqlite3_bind_double(stmt, 5, latency_ms);
    sqlite3_bind_int(stmt, 6, location_id);
    
    result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    pthread_mutex_unlock(&g_location_ref_manager.mutex);
    
    if (result == SQLITE_DONE) {
        LOGX_DEBUG("Updated location usage statistics",
                  "location_id", location_id,
                  "signal_quality", signal_quality,
                  "latency_ms", latency_ms);
        return AUTONOMY_SUCCESS;
    }
    
    return AUTONOMY_ERROR_SYSTEM;
}

// Estimate storage space saved by using location references
static uint64_t gps_location_reference_estimate_space_saved(uint64_t total_samples, uint32_t unique_locations) {
    // Each telemetry sample saves:
    // - 2 x sizeof(double) = 16 bytes (lat/lon coordinates)
    // - Various GPS metadata strings ≈ 32 bytes
    // Total saved per sample ≈ 48 bytes
    
    // Cost of location reference table:
    // - Each location entry ≈ 200 bytes
    
    uint64_t space_saved_per_sample = 48;
    uint64_t space_cost_per_location = 200;
    
    uint64_t total_saved = total_samples * space_saved_per_sample;
    uint64_t total_cost = unique_locations * space_cost_per_location;
    
    return (total_saved > total_cost) ? (total_saved - total_cost) : 0;
}

static bool gps_location_reference_is_initialized(void) {
    return g_location_ref_initialized;
}

// Initialize location database
static int init_location_database(void) {
    // Use same database as telemetry but separate table
    const char* db_path = "/etc/autonomy/telemetry.db";
    
    // Ensure database directory exists
    char db_dir[256];
    strncpy(db_dir, db_path, sizeof(db_dir) - 1);
    db_dir[sizeof(db_dir) - 1] = '\0';
    
    char* last_slash = strrchr(db_dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        if (mkdir(db_dir, 0755) != 0 && errno != EEXIST) {
            LOGX_ERROR("Failed to create database directory", "path", db_dir);
            return AUTONOMY_ERROR_SYSTEM;
        }
    }
    
    // Open database
    int result = sqlite3_open(db_path, &g_location_ref_manager.db);
    if (result != SQLITE_OK) {
        LOGX_ERROR("Failed to open location reference database",
                  "path", db_path,
                  "error", sqlite3_errmsg(g_location_ref_manager.db));
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Execute schema creation
    char* error_msg = NULL;
    result = sqlite3_exec(g_location_ref_manager.db, LOCATION_REFERENCE_SCHEMA_SQL, NULL, NULL, &error_msg);
    if (result != SQLITE_OK) {
        LOGX_ERROR("Failed to create location reference schema", "error", error_msg);
        sqlite3_free(error_msg);
        sqlite3_close(g_location_ref_manager.db);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    g_location_ref_manager.db_initialized = true;
    
    LOGX_INFO("GPS location reference database initialized", "path", db_path);
    
    return AUTONOMY_SUCCESS;
}

// Cleanup thread worker
static void* cleanup_thread_worker(void* arg) {
    LOGX_INFO("GPS location reference cleanup thread started",
             "cleanup_interval_hours", g_location_ref_manager.config.cleanup_interval_hours);
    
    while (g_location_ref_initialized && g_location_ref_manager.thread_running) {
        sleep(g_location_ref_manager.config.cleanup_interval_hours * 3600);
        
        if (!g_location_ref_manager.thread_running) break;
        
        // Perform cleanup
        int cleaned_up = gps_location_reference_force_cleanup();
        if (cleaned_up > 0) {
            LOGX_INFO("GPS location reference cleanup completed",
                     "locations_cleaned", cleaned_up);
        }
    }
    
    LOGX_INFO("GPS location reference cleanup thread stopped");
    return NULL;
}