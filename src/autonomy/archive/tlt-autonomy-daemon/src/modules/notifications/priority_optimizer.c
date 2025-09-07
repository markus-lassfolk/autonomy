#include "priority_optimizer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <pthread.h>

// Global priority optimizer instance
static priority_optimizer_t g_priority_optimizer;
static bool g_priority_optimizer_initialized = false;

// Forward declarations
static double calculate_context_score(notification_type_t alert_type, const system_state_t* system_state, const char* base_data_json);
static double calculate_learning_score(notification_type_t alert_type, notification_priority_t base_priority);
static double calculate_urgency_score(notification_type_t alert_type, const system_state_t* system_state, const char* base_data_json);
static double calculate_business_impact_score(notification_type_t alert_type, const system_state_t* system_state, const char* base_data_json);
static notification_priority_t combine_priority_scores(notification_priority_t base_priority, double context_score, double learning_score, double urgency_score, double business_score);
static double parse_json_double(const char* json, const char* key, double default_value);
static int parse_json_int(const char* json, const char* key, int default_value);

// Initialize priority optimizer
int priority_optimizer_init(const priority_optimizer_config_t* config) {
    if (g_priority_optimizer_initialized) {
        return 0; // Already initialized
    }
    
    if (!config) {
        return -1;
    }
    
    memset(&g_priority_optimizer, 0, sizeof(priority_optimizer_t));
    
    // Copy configuration
    g_priority_optimizer.config = *config;
    
    // Initialize mutex
    g_priority_optimizer.mutex = malloc(sizeof(pthread_mutex_t));
    if (!g_priority_optimizer.mutex) {
        return -1;
    }
    
    pthread_mutex_init(g_priority_optimizer.mutex, NULL);
    
    // Initialize learning data
    if (config->learning_enabled && config->max_learning_entries > 0) {
        g_priority_optimizer.learning_data = malloc(config->max_learning_entries * sizeof(priority_learning_data_t));
        if (!g_priority_optimizer.learning_data) {
            pthread_mutex_destroy(g_priority_optimizer.mutex);
            free(g_priority_optimizer.mutex);
            return -1;
        }
        
        g_priority_optimizer.max_learning_entries = config->max_learning_entries;
        g_priority_optimizer.learning_entries_count = 0;
    }
    
    // Initialize statistics
    g_priority_optimizer.total_optimizations = 0;
    g_priority_optimizer.priority_adjustments_made = 0;
    
    g_priority_optimizer_initialized = true;
    return 0;
}

// Clean up priority optimizer
void priority_optimizer_cleanup(void) {
    if (!g_priority_optimizer_initialized) return;
    
    if (g_priority_optimizer.mutex) {
        pthread_mutex_destroy(g_priority_optimizer.mutex);
        free(g_priority_optimizer.mutex);
    }
    
    if (g_priority_optimizer.learning_data) {
        free(g_priority_optimizer.learning_data);
    }
    
    g_priority_optimizer.learning_data = NULL;
    g_priority_optimizer.mutex = NULL;
    g_priority_optimizer.learning_entries_count = 0;
    g_priority_optimizer.max_learning_entries = 0;
    g_priority_optimizer.total_optimizations = 0;
    g_priority_optimizer.priority_adjustments_made = 0;
    
    g_priority_optimizer_initialized = false;
}

// Parse JSON double value (simplified JSON parsing)
static double parse_json_double(const char* json, const char* key, double default_value) {
    if (!json || !key) return default_value;
    
    char search_pattern[128];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\":", key);
    
    char* found = strstr(json, search_pattern);
    if (!found) return default_value;
    
    found += strlen(search_pattern);
    while (*found == ' ' || *found == '\t') found++; // Skip whitespace
    
    double value = strtod(found, NULL);
    return value;
}

// Parse JSON int value (simplified JSON parsing)
static int parse_json_int(const char* json, const char* key, int default_value) {
    if (!json || !key) return default_value;
    
    char search_pattern[128];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\":", key);
    
    char* found = strstr(json, search_pattern);
    if (!found) return default_value;
    
    found += strlen(search_pattern);
    while (*found == ' ' || *found == '\t') found++; // Skip whitespace
    
    int value = (int)strtol(found, NULL, 10);
    return value;
}

// Calculate context score based on current system state
static double calculate_context_score(notification_type_t alert_type, const system_state_t* system_state, const char* base_data_json) {
    (void)alert_type; // May be used for alert-specific context in the future
    (void)base_data_json; // May be used for additional context
    
    double score = 0.0;
    
    if (!system_state) return score;
    
    // System health context
    const system_health_state_t* health = &system_state->system_health;
    
    // High CPU usage increases priority
    if (health->cpu_usage > 80.0) {
        score += 0.3;
    } else if (health->cpu_usage > 60.0) {
        score += 0.1;
    }
    
    // High memory usage increases priority
    if (health->memory_usage > 90.0) {
        score += 0.4;
    } else if (health->memory_usage > 70.0) {
        score += 0.2;
    }
    
    // High temperature increases priority
    if (health->temperature > 70.0) {
        score += 0.3;
    } else if (health->temperature > 60.0) {
        score += 0.1;
    }
    
    // Network health context
    const network_health_state_t* network = &system_state->network_health;
    
    // Primary interface down significantly increases priority
    if (!network->primary_interface_up) {
        score += 0.5;
    }
    
    // Few backup interfaces increases priority
    if (network->backup_interfaces_up < 2) {
        score += 0.2;
    }
    
    // High packet loss increases priority
    if (network->average_packet_loss > 10.0) {
        score += 0.3;
    } else if (network->average_packet_loss > 5.0) {
        score += 0.1;
    }
    
    // Multiple state changes indicate instability
    if (system_state->state_change_count > 5) {
        score += 0.2;
    } else if (system_state->state_change_count > 2) {
        score += 0.1;
    }
    
    return score;
}

// Calculate learning score based on historical data
static double calculate_learning_score(notification_type_t alert_type, notification_priority_t base_priority) {
    if (!g_priority_optimizer.config.learning_enabled || !g_priority_optimizer.learning_data) {
        return 0.0;
    }
    
    pthread_mutex_lock(g_priority_optimizer.mutex);
    
    double score = 0.0;
    const char* alert_type_str = notification_type_to_string(alert_type);
    
    // Find learning data for this alert type
    for (int i = 0; i < g_priority_optimizer.learning_entries_count; i++) {
        priority_learning_data_t* entry = &g_priority_optimizer.learning_data[i];
        
        if (strcmp(entry->alert_type, alert_type_str) == 0) {
            // Adjust towards learned optimal priority
            int diff = entry->optimal_priority - (int)base_priority;
            score += (double)diff * 0.2; // 20% weight for learned priorities
            
            // Apply confidence weighting
            if (entry->confidence_score > g_priority_optimizer.config.confidence_threshold) {
                score *= 1.2; // High confidence
            } else {
                score *= 0.5; // Low confidence
            }
            
            // Apply effectiveness weighting
            if (entry->effectiveness_score > 0.8) {
                score += 0.1; // Effective pattern
            } else if (entry->effectiveness_score < 0.3) {
                score -= 0.1; // Ineffective pattern
            }
            
            break;
        }
    }
    
    pthread_mutex_unlock(g_priority_optimizer.mutex);
    return score;
}

// Calculate urgency score based on alert type and context
static double calculate_urgency_score(notification_type_t alert_type, const system_state_t* system_state, const char* base_data_json) {
    double score = 0.0;
    
    // Alert type specific urgency
    switch (alert_type) {
        case NOTIFICATION_TYPE_FAILOVER:
            score += 0.4; // Failovers are inherently urgent
            
            // Multiple recent failovers increase urgency
            if (base_data_json) {
                int recent_failover_count = parse_json_int(base_data_json, "recent_failover_count", 0);
                score += (double)recent_failover_count * 0.1;
            }
            break;
            
        case NOTIFICATION_TYPE_SYSTEM_HEALTH:
            score += 0.3; // System health issues are urgent
            
            // High severity increases urgency
            if (base_data_json && strstr(base_data_json, "\"severity\":\"critical\"")) {
                score += 0.5;
            } else if (base_data_json && strstr(base_data_json, "\"severity\":\"high\"")) {
                score += 0.3;
            } else if (base_data_json && strstr(base_data_json, "\"severity\":\"medium\"")) {
                score += 0.1;
            }
            break;
            
        case NOTIFICATION_TYPE_DATA_LIMIT:
            // Data limit urgency depends on usage
            if (base_data_json) {
                double usage_percent = parse_json_double(base_data_json, "usage_percent", 0.0);
                if (usage_percent > 95.0) {
                    score += 0.3;
                } else if (usage_percent > 85.0) {
                    score += 0.1;
                }
            }
            break;
            
        default:
            // Base urgency for other types
            score += 0.1;
            break;
    }
    
    // High temperature increases urgency
    if (base_data_json) {
        double temperature = parse_json_double(base_data_json, "temperature", 0.0);
        if (temperature > 80.0) {
            score += 0.4;
        } else if (temperature > 70.0) {
            score += 0.2;
        }
    }
    
    // Duration of ongoing issues increases urgency
    if (base_data_json) {
        int duration_minutes = parse_json_int(base_data_json, "duration_minutes", 0);
        if (duration_minutes > 30) {
            score += 0.3;
        } else if (duration_minutes > 10) {
            score += 0.1;
        }
    }
    
    // System state urgency indicators
    if (system_state) {
        // Multiple rapid state changes indicate urgency
        if (system_state->state_change_count > 10) {
            score += 0.2;
        }
    }
    
    return score;
}

// Calculate business impact score
static double calculate_business_impact_score(notification_type_t alert_type, const system_state_t* system_state, const char* base_data_json) {
    (void)system_state; // May be used for business hours context in the future
    
    double score = 0.0;
    
    // Alert type specific business impact
    switch (alert_type) {
        case NOTIFICATION_TYPE_FAILOVER:
        case NOTIFICATION_TYPE_NETWORK_ISSUE:
            score += 0.2; // Network issues have business impact
            break;
        case NOTIFICATION_TYPE_SYSTEM_HEALTH:
            score += 0.1; // System issues have moderate business impact
            break;
        default:
            break;
    }
    
    // Parse business impact indicators from JSON
    if (base_data_json) {
        // Multiple affected systems increase business impact
        int affected_systems = parse_json_int(base_data_json, "affected_systems", 0);
        double impact = (double)affected_systems * 0.05;
        score += fmin(impact, 0.3); // Cap at 0.3
        
        // Service availability impact
        double availability = parse_json_double(base_data_json, "service_availability", 1.0);
        if (availability < 0.9) { // Less than 90% availability
            score += (0.9 - availability) * 2.0; // Scale impact
        }
        
        // User impact
        int affected_users = parse_json_int(base_data_json, "affected_users", 0);
        if (affected_users > 100) {
            score += 0.3;
        } else if (affected_users > 10) {
            score += 0.1;
        }
        
        // Financial impact
        double estimated_cost = parse_json_double(base_data_json, "estimated_cost", 0.0);
        if (estimated_cost > 1000.0) {
            score += 0.3;
        } else if (estimated_cost > 100.0) {
            score += 0.1;
        }
    }
    
    return score;
}

// Combine priority scores to determine final priority
static notification_priority_t combine_priority_scores(notification_priority_t base_priority, 
                                                      double context_score, double learning_score, 
                                                      double urgency_score, double business_score) {
    // Combine scores with weights
    double total_adjustment = context_score * 0.3 + learning_score * 0.2 + urgency_score * 0.4 + business_score * 0.1;
    
    // Apply adaptation rate
    total_adjustment *= g_priority_optimizer.config.adaptation_rate;
    
    // Convert to priority adjustment
    int priority_adjustment = (int)round(total_adjustment * 2.0); // Scale to priority levels
    
    // Calculate final priority
    int final_priority = (int)base_priority + priority_adjustment;
    
    // Clamp to valid priority range
    if (final_priority < (int)NOTIFICATION_PRIORITY_LOWEST) {
        final_priority = (int)NOTIFICATION_PRIORITY_LOWEST;
    } else if (final_priority > (int)NOTIFICATION_PRIORITY_EMERGENCY) {
        final_priority = (int)NOTIFICATION_PRIORITY_EMERGENCY;
    }
    
    return (notification_priority_t)final_priority;
}

// Optimize notification priority
notification_priority_t priority_optimizer_optimize_priority(notification_type_t alert_type,
                                                           notification_priority_t base_priority,
                                                           const system_state_t* system_state,
                                                           const char* base_data_json) {
    if (!g_priority_optimizer_initialized || !g_priority_optimizer.config.priority_optimization_enabled) {
        return base_priority; // No optimization
    }
    
    pthread_mutex_lock(g_priority_optimizer.mutex);
    g_priority_optimizer.total_optimizations++;
    pthread_mutex_unlock(g_priority_optimizer.mutex);
    
    // Calculate different priority scores
    double context_score = calculate_context_score(alert_type, system_state, base_data_json);
    double learning_score = calculate_learning_score(alert_type, base_priority);
    double urgency_score = calculate_urgency_score(alert_type, system_state, base_data_json);
    double business_score = calculate_business_impact_score(alert_type, system_state, base_data_json);
    
    // Combine scores to get optimized priority
    notification_priority_t optimized_priority = combine_priority_scores(
        base_priority, context_score, learning_score, urgency_score, business_score);
    
    // Track if priority was adjusted
    if (optimized_priority != base_priority) {
        pthread_mutex_lock(g_priority_optimizer.mutex);
        g_priority_optimizer.priority_adjustments_made++;
        pthread_mutex_unlock(g_priority_optimizer.mutex);
    }
    
    return optimized_priority;
}

// Update learning data
int priority_optimizer_update_learning(notification_type_t alert_type,
                                      notification_priority_t used_priority,
                                      double effectiveness_score) {
    if (!g_priority_optimizer_initialized || !g_priority_optimizer.config.learning_enabled || !g_priority_optimizer.learning_data) {
        return -1;
    }
    
    pthread_mutex_lock(g_priority_optimizer.mutex);
    
    const char* alert_type_str = notification_type_to_string(alert_type);
    time_t now = time(NULL);
    
    // Find existing entry or create new one
    priority_learning_data_t* entry = NULL;
    for (int i = 0; i < g_priority_optimizer.learning_entries_count; i++) {
        if (strcmp(g_priority_optimizer.learning_data[i].alert_type, alert_type_str) == 0) {
            entry = &g_priority_optimizer.learning_data[i];
            break;
        }
    }
    
    if (!entry) {
        // Create new entry
        if (g_priority_optimizer.learning_entries_count < g_priority_optimizer.max_learning_entries) {
            entry = &g_priority_optimizer.learning_data[g_priority_optimizer.learning_entries_count];
            g_priority_optimizer.learning_entries_count++;
        } else {
            // Replace oldest entry
            int oldest_index = 0;
            time_t oldest_time = g_priority_optimizer.learning_data[0].last_updated;
            for (int i = 1; i < g_priority_optimizer.max_learning_entries; i++) {
                if (g_priority_optimizer.learning_data[i].last_updated < oldest_time) {
                    oldest_time = g_priority_optimizer.learning_data[i].last_updated;
                    oldest_index = i;
                }
            }
            entry = &g_priority_optimizer.learning_data[oldest_index];
        }
        
        strncpy(entry->alert_type, alert_type_str, sizeof(entry->alert_type) - 1);
        entry->optimal_priority = (int)used_priority;
        entry->effectiveness_score = effectiveness_score;
        entry->confidence_score = 0.5; // Start with medium confidence
    } else {
        // Update existing entry with exponential smoothing
        double alpha = 0.3; // Learning rate
        entry->optimal_priority = (int)round((1.0 - alpha) * entry->optimal_priority + alpha * (int)used_priority);
        entry->effectiveness_score = (1.0 - alpha) * entry->effectiveness_score + alpha * effectiveness_score;
        
        // Increase confidence with more data points
        entry->confidence_score = fmin(entry->confidence_score + 0.1, 1.0);
    }
    
    entry->last_updated = now;
    
    pthread_mutex_unlock(g_priority_optimizer.mutex);
    return 0;
}

// Get priority scores breakdown
void priority_optimizer_get_priority_scores(notification_type_t alert_type,
                                           notification_priority_t base_priority,
                                           const system_state_t* system_state,
                                           const char* base_data_json,
                                           priority_scores_t* scores) {
    if (!scores) return;
    
    memset(scores, 0, sizeof(priority_scores_t));
    
    if (!g_priority_optimizer_initialized) return;
    
    scores->context_score = calculate_context_score(alert_type, system_state, base_data_json);
    scores->learning_score = calculate_learning_score(alert_type, base_priority);
    scores->urgency_score = calculate_urgency_score(alert_type, system_state, base_data_json);
    scores->business_score = calculate_business_impact_score(alert_type, system_state, base_data_json);
    
    scores->total_adjustment = scores->context_score * 0.3 + scores->learning_score * 0.2 + 
                              scores->urgency_score * 0.4 + scores->business_score * 0.1;
    scores->total_adjustment *= g_priority_optimizer.config.adaptation_rate;
}

// Get priority optimizer status
void priority_optimizer_get_status(priority_optimizer_status_t* status) {
    if (!status || !g_priority_optimizer_initialized) return;
    
    pthread_mutex_lock(g_priority_optimizer.mutex);
    
    status->enabled = g_priority_optimizer.config.priority_optimization_enabled;
    status->learning_enabled = g_priority_optimizer.config.learning_enabled;
    status->learning_entries_count = g_priority_optimizer.learning_entries_count;
    status->max_learning_entries = g_priority_optimizer.max_learning_entries;
    status->adaptation_rate = g_priority_optimizer.config.adaptation_rate;
    status->confidence_threshold = g_priority_optimizer.config.confidence_threshold;
    status->total_optimizations = g_priority_optimizer.total_optimizations;
    status->priority_adjustments_made = g_priority_optimizer.priority_adjustments_made;
    
    pthread_mutex_unlock(g_priority_optimizer.mutex);
}

// Get optimization statistics
void priority_optimizer_get_stats(char* stats_json, size_t max_size) {
    if (!stats_json || max_size == 0 || !g_priority_optimizer_initialized) return;
    
    pthread_mutex_lock(g_priority_optimizer.mutex);
    
    snprintf(stats_json, max_size,
             "{"
             "\"optimization_enabled\":%s,"
             "\"learning_enabled\":%s,"
             "\"adaptation_rate\":%.2f,"
             "\"confidence_threshold\":%.2f,"
             "\"total_optimizations\":%d,"
             "\"priority_adjustments_made\":%d,"
             "\"learning_entries\":%d,"
             "\"adjustment_rate\":%.2f"
             "}",
             g_priority_optimizer.config.priority_optimization_enabled ? "true" : "false",
             g_priority_optimizer.config.learning_enabled ? "true" : "false",
             g_priority_optimizer.config.adaptation_rate,
             g_priority_optimizer.config.confidence_threshold,
             g_priority_optimizer.total_optimizations,
             g_priority_optimizer.priority_adjustments_made,
             g_priority_optimizer.learning_entries_count,
             g_priority_optimizer.total_optimizations > 0 ? 
                (double)g_priority_optimizer.priority_adjustments_made / g_priority_optimizer.total_optimizations : 0.0);
    
    pthread_mutex_unlock(g_priority_optimizer.mutex);
}

// Check if priority optimizer is initialized
bool priority_optimizer_is_initialized(void) {
    return g_priority_optimizer_initialized;
}

// Get priority optimizer instance
priority_optimizer_t* priority_optimizer_get_instance(void) {
    return g_priority_optimizer_initialized ? &g_priority_optimizer : NULL;
}
