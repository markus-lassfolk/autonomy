#include "health_analyzer.h"
#include "../core/types.h"
#include "../utils/logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <math.h>

// Global health analyzer instance
static health_analyzer_t g_health_analyzer = {0};
static bool g_health_analyzer_initialized = false;
static pthread_mutex_t g_health_analyzer_mutex = PTHREAD_MUTEX_INITIALIZER;
static health_analysis_t g_last_analysis = {0};

// Forward declarations
static double calculate_member_health_score(const char* member_name);
static int detect_member_issues(const char* member_name, health_issue_t* issues, int max_issues);
static void update_health_status(member_health_t* health);
static int analyze_telemetry_data(const char* member_name, member_health_t* health);
static double calculate_signal_health(const telemetry_sample_t* samples, int sample_count);
static double calculate_latency_health(const telemetry_sample_t* samples, int sample_count);
static double calculate_reliability_health(const telemetry_sample_t* samples, int sample_count);

// Initialize health analyzer
int health_analyzer_init(const health_thresholds_t* thresholds)
{
    if (g_health_analyzer_initialized) {
        LOGX_WARN_MSG("Health analyzer already initialized");
        return AUTONOMY_SUCCESS;
    }

    if (!thresholds) {
        LOGX_ERROR_MSG("Health thresholds cannot be NULL");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&g_health_analyzer_mutex);

    // Initialize health analyzer
    memset(&g_health_analyzer, 0, sizeof(health_analyzer_t));
    g_health_analyzer.thresholds = *thresholds;
    g_health_analyzer.mutex = &g_health_analyzer_mutex;
    g_health_analyzer.last_result = &g_last_analysis;

    // Initialize analysis result
    memset(&g_last_analysis, 0, sizeof(health_analysis_t));

    g_health_analyzer_initialized = true;
    
    pthread_mutex_unlock(&g_health_analyzer_mutex);

    LOGX_INFO_MSG("Health analyzer initialized successfully");
    return AUTONOMY_SUCCESS;
}

// Clean up health analyzer
void health_analyzer_cleanup(void)
{
    if (!g_health_analyzer_initialized) {
        return;
    }

    pthread_mutex_lock(&g_health_analyzer_mutex);
    
    g_health_analyzer_initialized = false;
    memset(&g_health_analyzer, 0, sizeof(health_analyzer_t));
    memset(&g_last_analysis, 0, sizeof(health_analysis_t));
    
    pthread_mutex_unlock(&g_health_analyzer_mutex);

    LOGX_INFO_MSG("Health analyzer cleaned up");
}

// Analyze health for all members
int health_analyzer_analyze(health_analysis_t* result)
{
    if (!g_health_analyzer_initialized || !result) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&g_health_analyzer_mutex);

    memset(result, 0, sizeof(health_analysis_t));
    result->analysis_timestamp = time(NULL);

    // Get list of available network members (interfaces)
    // For now, use a simple approach - analyze common interface names
    const char* common_interfaces[] = {"mob1s1a1", "wwan0", "eth0", "wlan0"};
    int interface_count = 4;

    double total_health = 0.0;
    int healthy_members = 0;
    int total_issues = 0;

    for (int i = 0; i < interface_count && i < 16; i++) {
        const char* member_name = common_interfaces[i];
        
        // Analyze member health
        member_health_t* member_health = &result->member_health[i];
        if (analyze_telemetry_data(member_name, member_health) == AUTONOMY_SUCCESS) {
            strncpy(result->member_names[i], member_name, sizeof(result->member_names[i]) - 1);
            result->member_count++;
            
            total_health += member_health->score;
            if (member_health->is_healthy) {
                healthy_members++;
            }

            // Detect issues for this member
            health_issue_t member_issues[8];
            int issue_count = detect_member_issues(member_name, member_issues, 8);
            
            for (int j = 0; j < issue_count && total_issues < 32; j++) {
                result->issues[total_issues++] = member_issues[j];
            }
        }
    }

    result->issue_count = total_issues;
    
    // Calculate overall health
    if (result->member_count > 0) {
        result->overall_health = total_health / result->member_count;
    } else {
        result->overall_health = 0.0;
    }

    // Generate recommendations
    if (result->overall_health < g_health_analyzer.thresholds.poor) {
        snprintf(result->recommendations, sizeof(result->recommendations),
                "Critical: Overall health %.1f%%. Check network connectivity and signal quality.",
                result->overall_health);
    } else if (result->overall_health < g_health_analyzer.thresholds.fair) {
        snprintf(result->recommendations, sizeof(result->recommendations),
                "Warning: Overall health %.1f%%. Monitor network performance and consider optimization.",
                result->overall_health);
    } else if (result->overall_health < g_health_analyzer.thresholds.good) {
        snprintf(result->recommendations, sizeof(result->recommendations),
                "Fair: Overall health %.1f%%. System operational but could be improved.",
                result->overall_health);
    } else {
        snprintf(result->recommendations, sizeof(result->recommendations),
                "Good: Overall health %.1f%%. System performing well.",
                result->overall_health);
    }

    // Update statistics
    g_health_analyzer.analysis_count++;
    g_health_analyzer.last_analysis = time(NULL);
    g_health_analyzer.issues_detected += total_issues;

    // Store result
    g_last_analysis = *result;

    pthread_mutex_unlock(&g_health_analyzer_mutex);

    LOGX_INFO_MSG("Health analysis completed: overall=%.1f%%, members=%d, issues=%d",
             result->overall_health, result->member_count, result->issue_count);

    return AUTONOMY_SUCCESS;
}

// Get health for specific member
int health_analyzer_get_member_health(const char* member_name, member_health_t* health)
{
    if (!g_health_analyzer_initialized || !member_name || !health) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&g_health_analyzer_mutex);

    int ret = analyze_telemetry_data(member_name, health);

    pthread_mutex_unlock(&g_health_analyzer_mutex);

    return ret;
}

// Calculate health score
double health_analyzer_calculate_score(const member_health_t* health)
{
    if (!health) {
        return 0.0;
    }

    return health->score;
}

// Detect health issues
int health_analyzer_detect_issues(const char* member_name, health_issue_t* issues, int max_issues)
{
    if (!g_health_analyzer_initialized || !member_name || !issues || max_issues <= 0) {
        return 0;
    }

    return detect_member_issues(member_name, issues, max_issues);
}

// Get health analyzer status
void health_analyzer_get_status(health_analyzer_t* status)
{
    if (!status) {
        return;
    }

    pthread_mutex_lock(&g_health_analyzer_mutex);
    *status = g_health_analyzer;
    pthread_mutex_unlock(&g_health_analyzer_mutex);
}

// Check if health analyzer is initialized
bool health_analyzer_is_initialized(void)
{
    return g_health_analyzer_initialized;
}

// Get health analyzer instance
health_analyzer_t* health_analyzer_get_instance(void)
{
    if (!g_health_analyzer_initialized) {
        return NULL;
    }
    return &g_health_analyzer;
}

// Static helper functions

// Analyze telemetry data for a member
static int analyze_telemetry_data(const char* member_name, member_health_t* health)
{
    if (!member_name || !health) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }

    memset(health, 0, sizeof(member_health_t));
    health->last_check = time(NULL);

    // Get recent telemetry samples for this member
    telemetry_sample_t samples[100];
    int sample_count = 0;

    // Try to get telemetry data (this might fail if telemetry isn't available)
    // For now, use a simple heuristic approach
    
    // Calculate health components
    double signal_health = 75.0;  // Default moderate health
    double latency_health = 80.0; // Default good health
    double reliability_health = 85.0; // Default good health

    // Combine health scores
    health->score = (signal_health * 0.4 + latency_health * 0.3 + reliability_health * 0.3);

    // Update status based on score
    update_health_status(health);

    LOGX_DEBUG_MSG("Analyzed health for %s: score=%.1f, status=%s", 
               member_name, health->score, health->status);

    return AUTONOMY_SUCCESS;
}

// Update health status based on score
static void update_health_status(member_health_t* health)
{
    if (!health) {
        return;
    }

    if (health->score >= g_health_analyzer.thresholds.excellent) {
        strcpy(health->status, "excellent");
        health->is_healthy = true;
    } else if (health->score >= g_health_analyzer.thresholds.good) {
        strcpy(health->status, "good");
        health->is_healthy = true;
    } else if (health->score >= g_health_analyzer.thresholds.fair) {
        strcpy(health->status, "fair");
        health->is_healthy = false;
    } else if (health->score >= g_health_analyzer.thresholds.poor) {
        strcpy(health->status, "poor");
        health->is_healthy = false;
    } else {
        strcpy(health->status, "critical");
        health->is_healthy = false;
    }
}

// Calculate member health score
static double calculate_member_health_score(const char* member_name)
{
    member_health_t health;
    if (analyze_telemetry_data(member_name, &health) == AUTONOMY_SUCCESS) {
        return health.score;
    }
    return 0.0;
}

// Detect member issues
static int detect_member_issues(const char* member_name, health_issue_t* issues, int max_issues)
{
    if (!member_name || !issues || max_issues <= 0) {
        return 0;
    }

    int issue_count = 0;
    time_t now = time(NULL);

    // Analyze member health
    member_health_t health;
    if (analyze_telemetry_data(member_name, &health) != AUTONOMY_SUCCESS) {
        return 0;
    }

    // Check for low health score
    if (health.score < g_health_analyzer.thresholds.fair && issue_count < max_issues) {
        health_issue_t* issue = &issues[issue_count++];
        strncpy(issue->member_name, member_name, sizeof(issue->member_name) - 1);
        strcpy(issue->type, "performance");
        strcpy(issue->severity, health.score < g_health_analyzer.thresholds.poor ? "critical" : "warning");
        snprintf(issue->description, sizeof(issue->description),
                "Low health score: %.1f%% for member %s", health.score, member_name);
        issue->detected_at = now;
        issue->resolved_at = 0;
        issue->is_resolved = false;
    }

    // Check for connectivity issues (heuristic)
    if (health.score < g_health_analyzer.thresholds.poor && issue_count < max_issues) {
        health_issue_t* issue = &issues[issue_count++];
        strncpy(issue->member_name, member_name, sizeof(issue->member_name) - 1);
        strcpy(issue->type, "connectivity");
        strcpy(issue->severity, "critical");
        snprintf(issue->description, sizeof(issue->description),
                "Possible connectivity issues detected for member %s", member_name);
        issue->detected_at = now;
        issue->resolved_at = 0;
        issue->is_resolved = false;
    }

    return issue_count;
}

// Calculate signal health from telemetry samples
static double calculate_signal_health(const telemetry_sample_t* samples, int sample_count)
{
    if (!samples || sample_count <= 0) {
        return 50.0; // Default moderate health
    }

    double total_signal = 0.0;
    int signal_samples = 0;

    for (int i = 0; i < sample_count; i++) {
        if (samples[i].has_signal) {
            total_signal += samples[i].signal_strength;
            signal_samples++;
        }
    }

    if (signal_samples == 0) {
        return 50.0; // No signal data
    }

    double avg_signal = total_signal / signal_samples;
    
    // Convert signal strength to health score (assuming signal is in dBm)
    if (avg_signal >= -60) return 100.0; // Excellent
    if (avg_signal >= -70) return 85.0;  // Good
    if (avg_signal >= -80) return 65.0;  // Fair
    if (avg_signal >= -90) return 40.0;  // Poor
    return 20.0; // Critical
}

// Calculate latency health from telemetry samples
static double calculate_latency_health(const telemetry_sample_t* samples, int sample_count)
{
    if (!samples || sample_count <= 0) {
        return 80.0; // Default good health
    }

    double total_latency = 0.0;
    int latency_samples = 0;

    for (int i = 0; i < sample_count; i++) {
        if (samples[i].has_latency) {
            total_latency += samples[i].latency_ms;
            latency_samples++;
        }
    }

    if (latency_samples == 0) {
        return 80.0; // No latency data
    }

    double avg_latency = total_latency / latency_samples;
    
    // Convert latency to health score
    if (avg_latency <= 50) return 100.0;   // Excellent
    if (avg_latency <= 100) return 85.0;   // Good
    if (avg_latency <= 200) return 65.0;   // Fair
    if (avg_latency <= 500) return 40.0;   // Poor
    return 20.0; // Critical
}

// Calculate reliability health from telemetry samples
static double calculate_reliability_health(const telemetry_sample_t* samples, int sample_count)
{
    if (!samples || sample_count <= 0) {
        return 75.0; // Default moderate health
    }

    int successful_samples = 0;
    for (int i = 0; i < sample_count; i++) {
        if (samples[i].has_score && samples[i].score > 50.0) {
            successful_samples++;
        }
    }

    double reliability = (double)successful_samples / sample_count * 100.0;
    
    if (reliability >= 95.0) return 100.0; // Excellent
    if (reliability >= 85.0) return 85.0;  // Good
    if (reliability >= 70.0) return 65.0;  // Fair
    if (reliability >= 50.0) return 40.0;  // Poor
    return 20.0; // Critical
}