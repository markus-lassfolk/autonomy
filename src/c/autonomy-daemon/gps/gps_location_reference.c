#include "gps_location_reference.h"
#include "../shared/logging/logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <sqlite3.h>
#include <unistd.h>

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
int gps_location_reference_init(const gps_location_reference_config_t* config) {
    if (g_location_ref_initialized) {
        LOGX_WARN_MSG("GPS location reference already initialized");
        return AUTONOMY_SUCCESS;
    }
    
    if (!config) {
        LOGX_ERROR_MSG("GPS location reference config is NULL");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    memset(&g_location_ref_manager, 0, sizeof(gps_location_reference_manager_t));
    g_location_ref_manager.config = *config;
    
    // Initialize mutex
    if (pthread_mutex_init(&g_location_ref_manager.mutex, NULL) != 0) {
        LOGX_ERROR_MSG("Failed to initialize GPS location reference mutex");
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Initialize database
    if (init_location_database() != AUTONOMY_SUCCESS) {
        LOGX_ERROR_MSG("Failed to initialize location reference database");
        pthread_mutex_destroy(&g_location_ref_manager.mutex);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Allocate memory for location cache
    g_location_ref_manager.cache_size = 100; // Cache last 100 locations
    g_location_ref_manager.location_cache = calloc(g_location_ref_manager.cache_size,
                                                  sizeof(gps_location_reference_t));
    if (!g_location_ref_manager.location_cache) {
        LOGX_ERROR_MSG("Failed to allocate memory for location cache");
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
            LOGX_ERROR_MSG("Failed to create location reference cleanup thread");
            free(g_location_ref_manager.location_cache);
            close_location_database();
            pthread_mutex_destroy(&g_location_ref_manager.mutex);
            return AUTONOMY_ERROR_SYSTEM;
        }
    }
    
    g_location_ref_initialized = true;
    
    LOGX_INFO_MSG("GPS location reference system initialized",
              "enabled", config->enabled ? "true" : "false",
              "precision_reduction_m", config->precision_reduction_meters,
              "movement_threshold_m", config->movement_threshold_meters,
              "max_locations", config->max_locations);
    
    return AUTONOMY_SUCCESS;
}

// Close location database
static void close_location_database(void) {
    if (g_location_ref_manager.db) {
        sqlite3_close(g_location_ref_manager.db);
        g_location_ref_manager.db = NULL;
        g_location_ref_manager.db_initialized = false;
    }
}

// Cleanup GPS location reference system
void gps_location_reference_cleanup(void) {
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
    
    LOGX_INFO_MSG("GPS location reference system cleaned up");
}

// Cleanup thread worker
static void* cleanup_thread_worker(void* arg) {
    (void)arg; // Suppress unused parameter warning
    
    LOGX_INFO_MSG("GPS location reference cleanup thread started",
             "cleanup_interval_hours", g_location_ref_manager.config.cleanup_interval_hours);
    
    while (g_location_ref_initialized && g_location_ref_manager.thread_running) {
        sleep(g_location_ref_manager.config.cleanup_interval_hours * 3600);
        
        if (!g_location_ref_manager.thread_running) break;
        
        // Perform cleanup
        int cleaned_up = gps_location_reference_force_cleanup();
        if (cleaned_up > 0) {
            LOGX_INFO_MSG("GPS location reference cleanup completed",
                     "locations_cleaned", cleaned_up);
        }
    }
    
    LOGX_INFO_MSG("GPS location reference cleanup thread stopped");
    return NULL;
}

// Initialize location database
static int init_location_database(void) {
    // Use same database as telemetry but separate table
    const char* db_path = "/etc/autonomy/telemetry.db";
    
    // Ensure database directory exists
    char db_dir[256];
    safe_strncpy(db_dir, db_path, sizeof(db_dir));
    
    char* last_slash = strrchr(db_dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        if (mkdir(db_dir, 0755) != 0 && errno != EEXIST) {
            LOGX_ERROR_MSG("Failed to create database directory", "path", db_dir);
            return AUTONOMY_ERROR_SYSTEM;
        }
    }
    
    // Open database
    int result = sqlite3_open(db_path, &g_location_ref_manager.db);
    if (result != SQLITE_OK) {
        LOGX_ERROR_MSG("Failed to open location reference database",
                  "path", db_path,
                  "error", sqlite3_errmsg(g_location_ref_manager.db));
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Execute schema creation
    char* error_msg = NULL;
    result = sqlite3_exec(g_location_ref_manager.db, LOCATION_REFERENCE_SCHEMA_SQL, NULL, NULL, &error_msg);
    if (result != SQLITE_OK) {
        LOGX_ERROR_MSG("Failed to create location reference schema", "error", error_msg);
        sqlite3_free(error_msg);
        sqlite3_close(g_location_ref_manager.db);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    g_location_ref_manager.db_initialized = true;
    
    LOGX_INFO_MSG("GPS location reference database initialized", "path", db_path);
    
    return AUTONOMY_SUCCESS;
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
            
            LOGX_DEBUG_MSG("Using existing location reference",
                      "location_id", *location_id,
                      "distance_m", distance,
                      "threshold_m", g_location_ref_manager.config.movement_threshold_meters);
        }
    }
    
    if (create_new_location) {
        // Check for nearby existing location
        if (find_nearby_location_reference(latitude, longitude, location_id) == AUTONOMY_SUCCESS) {
            LOGX_DEBUG_MSG("Found nearby location reference",
                      "location_id", *location_id);
            create_new_location = false;
        }
    }
    
    if (create_new_location) {
        // Create new location reference
        if (create_new_location_reference(latitude, longitude, accuracy, gps_source, location_id) == AUTONOMY_SUCCESS) {
            g_location_ref_manager.stats.total_locations++;
            
            LOGX_INFO_MSG("Created new location reference",
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

// Location reference management functions
int gps_location_reference_get_by_id(uint32_t location_id, gps_location_reference_t* location) {
    if (!g_location_ref_initialized || !location || location_id == 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_location_ref_manager.mutex);
    
    // First check cache
    for (int i = 0; i < g_location_ref_manager.cache_count; i++) {
        if (g_location_ref_manager.location_cache[i].location_id == location_id) {
            *location = g_location_ref_manager.location_cache[i];
            g_location_ref_manager.stats.cache_hits++;
            pthread_mutex_unlock(&g_location_ref_manager.mutex);
            return AUTONOMY_SUCCESS;
        }
    }
    
    // Not in cache, check database
    if (g_location_ref_manager.db) {
        sqlite3_stmt* stmt;
        const char* sql = "SELECT location_id, latitude_reduced, longitude_reduced, "
                         "latitude_original, longitude_original, accuracy_meters, gps_source, "
                         "first_recorded, last_used, usage_count, telemetry_samples, "
                         "location_name, is_stationary_location, avg_movement_speed_kmh, "
                         "avg_signal_quality, avg_latency_ms, location_score "
                         "FROM location_references WHERE location_id = ?";
        
        if (sqlite3_prepare_v2(g_location_ref_manager.db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int64(stmt, 1, location_id);
            
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                location->location_id = sqlite3_column_int64(stmt, 0);
                location->latitude_reduced = sqlite3_column_double(stmt, 1);
                location->longitude_reduced = sqlite3_column_double(stmt, 2);
                location->latitude_original = sqlite3_column_double(stmt, 3);
                location->longitude_original = sqlite3_column_double(stmt, 4);
                location->accuracy_meters = sqlite3_column_double(stmt, 5);
                
                const char* gps_source = (const char*)sqlite3_column_text(stmt, 6);
                if (gps_source) {
                    safe_strncpy(location->gps_source, gps_source, sizeof(location->gps_source));
                    location->gps_source[sizeof(location->gps_source) - 1] = '\0';
                }
                
                location->first_recorded = sqlite3_column_int64(stmt, 7);
                location->last_used = sqlite3_column_int64(stmt, 8);
                location->usage_count = sqlite3_column_int64(stmt, 9);
                location->telemetry_samples = sqlite3_column_int64(stmt, 10);
                
                const char* location_name = (const char*)sqlite3_column_text(stmt, 11);
                if (location_name) {
                    safe_strncpy(location->location_name, location_name, sizeof(location->location_name));
                    location->location_name[sizeof(location->location_name) - 1] = '\0';
                }
                
                location->is_stationary_location = sqlite3_column_int(stmt, 12) != 0;
                location->avg_movement_speed_kmh = sqlite3_column_double(stmt, 13);
                location->avg_signal_quality = sqlite3_column_double(stmt, 14);
                location->avg_latency_ms = sqlite3_column_double(stmt, 15);
                location->location_score = sqlite3_column_double(stmt, 16);
                
                // Add to cache if there's space
                if (g_location_ref_manager.cache_count < g_location_ref_manager.cache_size) {
                    g_location_ref_manager.location_cache[g_location_ref_manager.cache_count] = *location;
                    g_location_ref_manager.cache_count++;
                }
                
                sqlite3_finalize(stmt);
                g_location_ref_manager.stats.cache_misses++;
                pthread_mutex_unlock(&g_location_ref_manager.mutex);
                return AUTONOMY_SUCCESS;
            }
            sqlite3_finalize(stmt);
        }
    }
    
    pthread_mutex_unlock(&g_location_ref_manager.mutex);
    return AUTONOMY_ERROR_NOT_FOUND;
}

int gps_location_reference_update_usage(uint32_t location_id, double signal_quality, double latency_ms) {
    if (!g_location_ref_initialized || location_id == 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_location_ref_manager.mutex);
    
    // Update database
    if (g_location_ref_manager.db) {
        sqlite3_stmt* stmt;
        const char* sql = "UPDATE location_references SET "
                         "last_used = ?, usage_count = usage_count + 1, "
                         "avg_signal_quality = (avg_signal_quality * usage_count + ?) / (usage_count + 1), "
                         "avg_latency_ms = (avg_latency_ms * usage_count + ?) / (usage_count + 1), "
                         "location_score = (avg_signal_quality * 0.6 + (100.0 - avg_latency_ms) * 0.4) / 100.0 "
                         "WHERE location_id = ?";
        
        if (sqlite3_prepare_v2(g_location_ref_manager.db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            time_t now = time(NULL);
            sqlite3_bind_int64(stmt, 1, now);
            sqlite3_bind_double(stmt, 2, signal_quality);
            sqlite3_bind_double(stmt, 3, latency_ms);
            sqlite3_bind_int64(stmt, 4, location_id);
            
            if (sqlite3_step(stmt) == SQLITE_DONE) {
                sqlite3_finalize(stmt);
                
                // Update cache if location is cached
                for (int i = 0; i < g_location_ref_manager.cache_count; i++) {
                    if (g_location_ref_manager.location_cache[i].location_id == location_id) {
                        g_location_ref_manager.location_cache[i].last_used = now;
                        g_location_ref_manager.location_cache[i].usage_count++;
                        
                        // Update running averages
                        uint32_t old_count = g_location_ref_manager.location_cache[i].usage_count - 1;
                        g_location_ref_manager.location_cache[i].avg_signal_quality = 
                            (g_location_ref_manager.location_cache[i].avg_signal_quality * old_count + signal_quality) / 
                            g_location_ref_manager.location_cache[i].usage_count;
                        g_location_ref_manager.location_cache[i].avg_latency_ms = 
                            (g_location_ref_manager.location_cache[i].avg_latency_ms * old_count + latency_ms) / 
                            g_location_ref_manager.location_cache[i].usage_count;
                        
                        // Update location score
                        g_location_ref_manager.location_cache[i].location_score = 
                            (g_location_ref_manager.location_cache[i].avg_signal_quality * 0.6 + 
                             (100.0 - g_location_ref_manager.location_cache[i].avg_latency_ms) * 0.4) / 100.0;
                        break;
                    }
                }
                
                g_location_ref_manager.stats.total_references++;
                pthread_mutex_unlock(&g_location_ref_manager.mutex);
                return AUTONOMY_SUCCESS;
            }
            sqlite3_finalize(stmt);
        }
    }
    
    pthread_mutex_unlock(&g_location_ref_manager.mutex);
    return AUTONOMY_ERROR_NOT_FOUND;
}

int gps_location_reference_force_cleanup(void) {
    if (!g_location_ref_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_location_ref_manager.mutex);
    
    int locations_cleaned = 0;
    time_t now = time(NULL);
    time_t cutoff_time = now - (g_location_ref_manager.config.retention_days * 24 * 3600);
    
    if (g_location_ref_manager.db) {
        // Clean up old unused locations
        sqlite3_stmt* stmt;
        const char* sql = "DELETE FROM location_references WHERE "
                         "(last_used < ? AND usage_count < ?) OR "
                         "(first_recorded < ? AND usage_count = 0)";
        
        if (sqlite3_prepare_v2(g_location_ref_manager.db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int64(stmt, 1, cutoff_time);
            sqlite3_bind_int(stmt, 2, g_location_ref_manager.config.min_usage_for_retention);
            sqlite3_bind_int64(stmt, 3, cutoff_time);
            
            if (sqlite3_step(stmt) == SQLITE_DONE) {
                locations_cleaned = sqlite3_changes(g_location_ref_manager.db);
            }
            sqlite3_finalize(stmt);
        }
        
        // Clean up cache entries for deleted locations
        for (int i = g_location_ref_manager.cache_count - 1; i >= 0; i--) {
            uint32_t location_id = g_location_ref_manager.location_cache[i].location_id;
            
            // Check if location still exists in database
            sqlite3_stmt* check_stmt;
            const char* check_sql = "SELECT 1 FROM location_references WHERE location_id = ?";
            
            if (sqlite3_prepare_v2(g_location_ref_manager.db, check_sql, -1, &check_stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int64(check_stmt, 1, location_id);
                
                if (sqlite3_step(check_stmt) != SQLITE_ROW) {
                    // Location no longer exists, remove from cache
                    for (int j = i; j < g_location_ref_manager.cache_count - 1; j++) {
                        g_location_ref_manager.location_cache[j] = g_location_ref_manager.location_cache[j + 1];
                    }
                    g_location_ref_manager.cache_count--;
                }
                sqlite3_finalize(check_stmt);
            }
        }
        
        // Update statistics
        g_location_ref_manager.stats.last_cleanup = now;
        
        // Get updated location count
        sqlite3_stmt* count_stmt;
        const char* count_sql = "SELECT COUNT(*) FROM location_references";
        
        if (sqlite3_prepare_v2(g_location_ref_manager.db, count_sql, -1, &count_stmt, NULL) == SQLITE_OK) {
            if (sqlite3_step(count_stmt) == SQLITE_ROW) {
                g_location_ref_manager.stats.total_locations = sqlite3_column_int64(count_stmt, 0);
            }
            sqlite3_finalize(count_stmt);
        }
    }
    
    pthread_mutex_unlock(&g_location_ref_manager.mutex);
    
    LOGX_INFO_MSG("Location reference cleanup completed", 
                  "locations_cleaned", locations_cleaned,
                  "total_locations", g_location_ref_manager.stats.total_locations);
    
    return locations_cleaned;
}

double gps_calculate_distance_meters(double lat1, double lon1, double lat2, double lon2) {
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

// Create new location reference
static int create_new_location_reference(double latitude, double longitude, double accuracy, 
                                        const char* gps_source, uint32_t* location_id) {
    if (!g_location_ref_manager.db || !location_id) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    time_t now = time(NULL);
    double reduced_lat = gps_reduce_coordinate_precision(latitude, g_location_ref_manager.config.precision_reduction_meters);
    double reduced_lon = gps_reduce_coordinate_precision(longitude, g_location_ref_manager.config.precision_reduction_meters);
    
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO location_references ("
                     "location_id, latitude_reduced, longitude_reduced, latitude_original, longitude_original, "
                     "accuracy_meters, gps_source, first_recorded, last_used, usage_count, telemetry_samples, "
                     "location_name, is_stationary_location, avg_movement_speed_kmh, "
                     "avg_signal_quality, avg_latency_ms, location_score) "
                     "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 1, 0, '', 0, 0.0, 0.0, 0.0, 0.0)";
    
    if (sqlite3_prepare_v2(g_location_ref_manager.db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        *location_id = g_location_ref_manager.next_location_id++;
        
        sqlite3_bind_int64(stmt, 1, *location_id);
        sqlite3_bind_double(stmt, 2, reduced_lat);
        sqlite3_bind_double(stmt, 3, reduced_lon);
        sqlite3_bind_double(stmt, 4, latitude);
        sqlite3_bind_double(stmt, 5, longitude);
        sqlite3_bind_double(stmt, 6, accuracy);
        sqlite3_bind_text(stmt, 7, gps_source, -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 8, now);
        sqlite3_bind_int64(stmt, 9, now);
        
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            sqlite3_finalize(stmt);
            
            // Add to cache if there's space
            if (g_location_ref_manager.cache_count < g_location_ref_manager.cache_size) {
                gps_location_reference_t new_location;
                memset(&new_location, 0, sizeof(new_location));
                
                new_location.location_id = *location_id;
                new_location.latitude_reduced = reduced_lat;
                new_location.longitude_reduced = reduced_lon;
                new_location.latitude_original = latitude;
                new_location.longitude_original = longitude;
                new_location.accuracy_meters = accuracy;
                safe_strncpy(new_location.gps_source, gps_source, sizeof(new_location.gps_source));
                new_location.gps_source[sizeof(new_location.gps_source) - 1] = '\0';
                new_location.first_recorded = now;
                new_location.last_used = now;
                new_location.usage_count = 1;
                new_location.telemetry_samples = 0;
                new_location.is_stationary_location = false;
                new_location.avg_movement_speed_kmh = 0.0;
                new_location.avg_signal_quality = 0.0;
                new_location.avg_latency_ms = 0.0;
                new_location.location_score = 0.0;
                
                g_location_ref_manager.location_cache[g_location_ref_manager.cache_count] = new_location;
                g_location_ref_manager.cache_count++;
            }
            
            g_location_ref_manager.stats.total_locations++;
            g_location_ref_manager.stats.total_references++;
            
            LOGX_DEBUG_MSG("Created new location reference", 
                          "location_id", *location_id,
                          "latitude", latitude,
                          "longitude", longitude,
                          "accuracy", accuracy,
                          "gps_source", gps_source);
            
            return AUTONOMY_SUCCESS;
        }
        sqlite3_finalize(stmt);
    }
    
    return AUTONOMY_ERROR_SYSTEM;
}

// Find nearby location reference
static int find_nearby_location_reference(double latitude, double longitude, uint32_t* location_id) {
    if (!g_location_ref_manager.db || !location_id) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    double search_radius = g_location_ref_manager.config.movement_threshold_meters;
    
    // First check cache for nearby locations
    for (int i = 0; i < g_location_ref_manager.cache_count; i++) {
        double distance = gps_calculate_distance_meters(
            latitude, longitude,
            g_location_ref_manager.location_cache[i].latitude_original,
            g_location_ref_manager.location_cache[i].longitude_original
        );
        
        if (distance <= search_radius) {
            *location_id = g_location_ref_manager.location_cache[i].location_id;
            g_location_ref_manager.stats.cache_hits++;
            return AUTONOMY_SUCCESS;
        }
    }
    
    // Not in cache, search database
    sqlite3_stmt* stmt;
    const char* sql = "SELECT location_id, latitude_original, longitude_original FROM location_references";
    
    if (sqlite3_prepare_v2(g_location_ref_manager.db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            uint32_t id = sqlite3_column_int64(stmt, 0);
            double lat = sqlite3_column_double(stmt, 1);
            double lon = sqlite3_column_double(stmt, 2);
            
            double distance = gps_calculate_distance_meters(latitude, longitude, lat, lon);
            
            if (distance <= search_radius) {
                *location_id = id;
                sqlite3_finalize(stmt);
                g_location_ref_manager.stats.cache_misses++;
                return AUTONOMY_SUCCESS;
            }
        }
        sqlite3_finalize(stmt);
    }
    
    return AUTONOMY_ERROR_NOT_FOUND;
}

// Note: Functions implemented above

bool gps_location_reference_is_initialized(void) {
    return g_location_ref_initialized;
}

// Get location reference system status
int gps_location_reference_get_status(gps_location_reference_stats_t *status) {
    if (!g_location_ref_initialized || !status) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_location_ref_manager.mutex);
    
    *status = g_location_ref_manager.stats;
    
    pthread_mutex_unlock(&g_location_ref_manager.mutex);
    
    return AUTONOMY_SUCCESS;
}

// Reduce coordinate precision to specified meters
double gps_reduce_coordinate_precision(double coordinate, double precision_meters) {
    if (precision_meters <= 0.0) {
        return coordinate;
    }
    
    // Convert precision from meters to degrees (approximate)
    // 1 degree latitude  111,000 meters
    // 1 degree longitude  111,000 * cos(latitude) meters
    double precision_degrees = precision_meters / 111000.0;
    
    // Round to the nearest precision step
    return round(coordinate / precision_degrees) * precision_degrees;
}