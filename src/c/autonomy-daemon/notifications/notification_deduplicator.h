#ifndef NOTIFICATION_DEDUPLICATOR_H
#define NOTIFICATION_DEDUPLICATOR_H

#include "notification_types.h"
#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include <stdint.h>

// Deduplicator configuration
typedef struct {
    bool enabled;                        // Whether deduplication is enabled
    int max_fingerprints;                // Maximum fingerprints to store
    int deduplication_window_seconds;    // Time window for deduplication
    double similarity_threshold;          // Similarity threshold (0.0-1.0)
} deduplicator_config_t;

// Fingerprint entry
typedef struct {
    char* fingerprint;                   // Unique fingerprint string
    time_t timestamp;                    // When this fingerprint was created
    notification_event_t event;          // Associated notification event
} fingerprint_entry_t;

// Deduplicator structure
typedef struct {
    deduplicator_config_t config;
    
    // Fingerprint storage
    fingerprint_entry_t* fingerprints;
    int max_fingerprints;
    int fingerprint_count;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
    
    // Statistics
    uint64_t total_notifications;
    uint64_t duplicate_count;
} notification_deduplicator_t;

// Deduplicator statistics
typedef struct {
    uint64_t total_notifications;
    uint64_t duplicate_count;
    int fingerprint_count;
    int max_fingerprints;
    int deduplication_window;
    double similarity_threshold;
    double duplicate_rate;
} deduplicator_stats_t;

// Initialize deduplicator
int notification_deduplicator_init(notification_deduplicator_t* dedup, const deduplicator_config_t* config);

// Clean up deduplicator
void notification_deduplicator_cleanup(notification_deduplicator_t* dedup);

// Check if notification is duplicate
bool notification_deduplicator_is_duplicate(notification_deduplicator_t* dedup, 
                                          const notification_event_t* event);

// Clean up old fingerprints
int notification_deduplicator_cleanup_old(notification_deduplicator_t* dedup);

// Get deduplicator statistics
void notification_deduplicator_get_stats(const notification_deduplicator_t* dedup, 
                                       deduplicator_stats_t* stats);

// Reset deduplicator
void notification_deduplicator_reset(notification_deduplicator_t* dedup);

// Check if deduplicator is enabled
bool notification_deduplicator_is_enabled(const notification_deduplicator_t* dedup);

// Get deduplication window
int notification_deduplicator_get_window(const notification_deduplicator_t* dedup);

// Set deduplication window
void notification_deduplicator_set_window(notification_deduplicator_t* dedup, int window_seconds);

#endif // NOTIFICATION_DEDUPLICATOR_H
