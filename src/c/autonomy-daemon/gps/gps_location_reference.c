#include "gps_location_reference.h"
#include "../utils/logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <sqlite3.h>

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
    strncpy(db_dir, db_path, sizeof(db_dir) - 1);
    
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

// Placeholder implementations for remaining functions
int gps_location_reference_get_by_id(uint32_t location_id, gps_location_reference_t* location) {
    if (!g_location_ref_initialized || !location || location_id == 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Placeholder implementation
    memset(location, 0, sizeof(gps_location_reference_t));
    location->location_id = location_id;
    return AUTONOMY_SUCCESS;
}

int gps_location_reference_update_usage(uint32_t location_id, double signal_quality, double latency_ms) {
    if (!g_location_ref_initialized || location_id == 0) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Placeholder implementation
    (void)signal_quality;
    (void)latency_ms;
    return AUTONOMY_SUCCESS;
}

int gps_location_reference_force_cleanup(void) {
    if (!g_location_ref_initialized) {
        return AUTONOMY_ERROR_NOT_INITIALIZED;
    }
    
    // Placeholder implementation
    return 0; // No locations cleaned up
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
    
    // Placeholder implementation
    *location_id = g_location_ref_manager.next_location_id++;
    return AUTONOMY_SUCCESS;
}

// Find nearby location reference
static int find_nearby_location_reference(double latitude, double longitude, uint32_t* location_id) {
    if (!g_location_ref_manager.db || !location_id) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Placeholder implementation
    (void)latitude;
    (void)longitude;
    return AUTONOMY_ERROR_NOT_FOUND;
}

// Note: Functions implemented above

bool gps_location_reference_is_initialized(void) {
    return g_location_ref_initialized;
}