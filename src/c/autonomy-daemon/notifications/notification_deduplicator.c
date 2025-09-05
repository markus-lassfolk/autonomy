#include "notification_deduplicator.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

// Initialize deduplicator
static int notification_deduplicator_init(notification_deduplicator_t* dedup, const deduplicator_config_t* config) {
    if (!dedup || !config) {
        return -1;
    }
    
    dedup->config = *config;
    
    // Initialize fingerprint storage
    dedup->max_fingerprints = config->max_fingerprints;
    dedup->fingerprints = malloc(config->max_fingerprints * sizeof(fingerprint_entry_t));
    if (!dedup->fingerprints) {
        return -1;
    }
    
    // Initialize mutex
    dedup->mutex = malloc(sizeof(pthread_mutex_t));
    if (!dedup->mutex) {
        free(dedup->fingerprints);
        return -1;
    }
    
    pthread_mutex_init(dedup->mutex, NULL);
    
    // Initialize statistics
    dedup->total_notifications = 0;
    dedup->duplicate_count = 0;
    dedup->fingerprint_count = 0;
    
    return 0;
}

// Clean up deduplicator
static void notification_deduplicator_cleanup(notification_deduplicator_t* dedup) {
    if (!dedup) return;
    
    if (dedup->mutex) {
        pthread_mutex_destroy(dedup->mutex);
        free(dedup->mutex);
    }
    
    if (dedup->fingerprints) {
        free(dedup->fingerprints);
    }
    
    dedup->fingerprints = NULL;
    dedup->mutex = NULL;
    dedup->fingerprint_count = 0;
    dedup->max_fingerprints = 0;
}

// Generate fingerprint for notification
static char* generate_fingerprint(const notification_event_t* event) {
    if (!event) return NULL;
    
    // Create a simple hash-based fingerprint
    char* fingerprint = malloc(65); // 64 chars + null terminator
    if (!fingerprint) return NULL;
    
    // Combine key fields for fingerprinting
    unsigned long hash = 5381;
    int c;
    
    // Hash title
    const char* str = event->title;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    
    // Hash message
    str = event->message;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    
    // Hash type
    hash = ((hash << 5) + hash) + (int)event->type;
    
    // Hash priority
    hash = ((hash << 5) + hash) + (int)event->priority;
    
    // Hash member name if present
    if (strlen(event->member_name) > 0) {
        str = event->member_name;
        while ((c = *str++)) {
            hash = ((hash << 5) + hash) + c;
        }
    }
    
    // Convert hash to hex string
    snprintf(fingerprint, 65, "%016lx", hash);
    
    return fingerprint;
}

// Calculate similarity between two notifications
static double calculate_similarity(const notification_event_t* event1, const notification_event_t* event2) {
    if (!event1 || !event2) return 0.0;
    
    int matches = 0;
    int total_fields = 0;
    
    // Compare title similarity
    if (strlen(event1->title) > 0 && strlen(event2->title) > 0) {
        total_fields++;
        if (strcmp(event1->title, event2->title) == 0) {
            matches++;
        } else {
            // Check for partial matches
            if (strstr(event1->title, event2->title) || strstr(event2->title, event1->title)) {
                matches += 0.5;
            }
        }
    }
    
    // Compare message similarity
    if (strlen(event1->message) > 0 && strlen(event2->message) > 0) {
        total_fields++;
        if (strcmp(event1->message, event2->message) == 0) {
            matches++;
        } else {
            // Check for partial matches
            if (strstr(event1->message, event2->message) || strstr(event2->message, event1->message)) {
                matches += 0.5;
            }
        }
    }
    
    // Compare type
    total_fields++;
    if (event1->type == event2->type) {
        matches++;
    }
    
    // Compare priority
    total_fields++;
    if (event1->priority == event2->priority) {
        matches++;
    }
    
    // Compare member name
    if (strlen(event1->member_name) > 0 && strlen(event2->member_name) > 0) {
        total_fields++;
        if (strcmp(event1->member_name, event2->member_name) == 0) {
            matches++;
        }
    }
    
    if (total_fields == 0) return 0.0;
    
    return (double)matches / total_fields;
}

// Check if notification is duplicate
bool notification_deduplicator_is_duplicate(notification_deduplicator_t* dedup, 
                                          const notification_event_t* event) {
    if (!dedup || !event || !dedup->mutex) {
        return false;
    }
    
    pthread_mutex_lock(dedup->mutex);
    
    time_t now = time(NULL);
    bool is_duplicate = false;
    
    // Generate fingerprint for this notification
    char* fingerprint = generate_fingerprint(event);
    if (!fingerprint) {
        pthread_mutex_unlock(dedup->mutex);
        return false;
    }
    
    // Check for exact fingerprint match
    for (int i = 0; i < dedup->fingerprint_count; i++) {
        if (strcmp(dedup->fingerprints[i].fingerprint, fingerprint) == 0) {
            // Check if within deduplication window
            if (now - dedup->fingerprints[i].timestamp < dedup->config.deduplication_window_seconds) {
                is_duplicate = true;
                dedup->duplicate_count++;
                break;
            }
        }
    }
    
    // If no exact match, check for similarity
    if (!is_duplicate && dedup->config.similarity_threshold > 0.0) {
        for (int i = 0; i < dedup->fingerprint_count; i++) {
            // Only check recent notifications
            if (now - dedup->fingerprints[i].timestamp < dedup->config.deduplication_window_seconds) {
                double similarity = calculate_similarity(event, &dedup->fingerprints[i].event);
                if (similarity >= dedup->config.similarity_threshold) {
                    is_duplicate = true;
                    dedup->duplicate_count++;
                    break;
                }
            }
        }
    }
    
    // Add this notification to fingerprint storage
    if (!is_duplicate) {
        // Remove old fingerprints if we're at capacity
        if (dedup->fingerprint_count >= dedup->max_fingerprints) {
            // Find oldest fingerprint
            int oldest_index = 0;
            time_t oldest_time = dedup->fingerprints[0].timestamp;
            
            for (int i = 1; i < dedup->fingerprint_count; i++) {
                if (dedup->fingerprints[i].timestamp < oldest_time) {
                    oldest_time = dedup->fingerprints[i].timestamp;
                    oldest_index = i;
                }
            }
            
            // Remove oldest fingerprint
            if (dedup->fingerprints[oldest_index].fingerprint) {
                free(dedup->fingerprints[oldest_index].fingerprint);
            }
            
            // Shift remaining fingerprints
            for (int i = oldest_index; i < dedup->fingerprint_count - 1; i++) {
                dedup->fingerprints[i] = dedup->fingerprints[i + 1];
            }
            dedup->fingerprint_count--;
        }
        
        // Add new fingerprint
        int index = dedup->fingerprint_count;
        dedup->fingerprints[index].fingerprint = fingerprint;
        dedup->fingerprints[index].timestamp = now;
        
        // Copy event data
        memcpy(&dedup->fingerprints[index].event, event, sizeof(notification_event_t));
        
        // Handle location pointer
        if (event->location) {
            dedup->fingerprints[index].event.location = malloc(sizeof(notification_location_t));
            if (dedup->fingerprints[index].event.location) {
                memcpy(dedup->fingerprints[index].event.location, event->location, sizeof(notification_location_t));
            }
        }
        
        dedup->fingerprint_count++;
    } else {
        // Free fingerprint since we didn't store it
        free(fingerprint);
    }
    
    dedup->total_notifications++;
    
    pthread_mutex_unlock(dedup->mutex);
    return is_duplicate;
}

// Clean up old fingerprints
static int notification_deduplicator_cleanup_old(notification_deduplicator_t* dedup) {
    if (!dedup || !dedup->mutex) {
        return -1;
    }
    
    pthread_mutex_lock(dedup->mutex);
    
    time_t now = time(NULL);
    int removed_count = 0;
    int write_index = 0;
    
    for (int i = 0; i < dedup->fingerprint_count; i++) {
        if (now - dedup->fingerprints[i].timestamp < dedup->config.deduplication_window_seconds) {
            // Keep this fingerprint
            if (write_index != i) {
                dedup->fingerprints[write_index] = dedup->fingerprints[i];
            }
            write_index++;
        } else {
            // Remove this fingerprint
            if (dedup->fingerprints[i].fingerprint) {
                free(dedup->fingerprints[i].fingerprint);
            }
            if (dedup->fingerprints[i].event.location) {
                free(dedup->fingerprints[i].event.location);
            }
            removed_count++;
        }
    }
    
    dedup->fingerprint_count = write_index;
    
    pthread_mutex_unlock(dedup->mutex);
    return removed_count;
}

// Get deduplicator statistics
void notification_deduplicator_get_stats(const notification_deduplicator_t* dedup, 
                                       deduplicator_stats_t* stats) {
    if (!dedup || !stats || !dedup->mutex) return;
    
    pthread_mutex_lock(dedup->mutex);
    
    stats->total_notifications = dedup->total_notifications;
    stats->duplicate_count = dedup->duplicate_count;
    stats->fingerprint_count = dedup->fingerprint_count;
    stats->max_fingerprints = dedup->max_fingerprints;
    stats->deduplication_window = dedup->config.deduplication_window_seconds;
    stats->similarity_threshold = dedup->config.similarity_threshold;
    
    if (dedup->total_notifications > 0) {
        stats->duplicate_rate = (double)dedup->duplicate_count / dedup->total_notifications;
    } else {
        stats->duplicate_rate = 0.0;
    }
    
    pthread_mutex_unlock(dedup->mutex);
}

// Reset deduplicator
static void notification_deduplicator_reset(notification_deduplicator_t* dedup) {
    if (!dedup || !dedup->mutex) return;
    
    pthread_mutex_lock(dedup->mutex);
    
    // Free all fingerprints
    for (int i = 0; i < dedup->fingerprint_count; i++) {
        if (dedup->fingerprints[i].fingerprint) {
            free(dedup->fingerprints[i].fingerprint);
        }
        if (dedup->fingerprints[i].event.location) {
            free(dedup->fingerprints[i].event.location);
        }
    }
    
    dedup->fingerprint_count = 0;
    dedup->total_notifications = 0;
    dedup->duplicate_count = 0;
    
    pthread_mutex_unlock(dedup->mutex);
}

// Check if deduplicator is enabled
static bool notification_deduplicator_is_enabled(const notification_deduplicator_t* dedup) {
    return dedup && dedup->config.enabled;
}

// Get deduplication window
static int notification_deduplicator_get_window(const notification_deduplicator_t* dedup) {
    return dedup ? dedup->config.deduplication_window_seconds : 0;
}

// Set deduplication window
static void notification_deduplicator_set_window(notification_deduplicator_t* dedup, int window_seconds) {
    if (dedup && window_seconds > 0) {
        dedup->config.deduplication_window_seconds = window_seconds;
    }
}
