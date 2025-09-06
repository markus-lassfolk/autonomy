#ifndef HEALTH_ANALYZER_H
#define HEALTH_ANALYZER_H

#include "../telemetry/telemetry_store.h"
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

// Health thresholds
typedef struct {
    double excellent; // > 80
    double good;      // 60-80
    double fair;      // 40-60
    double poor;      // 20-40
    double critical;  // < 20
} health_thresholds_t;

// Health issue
typedef struct {
    char member_name[128];
    char type[64];
    char severity[32];
    char description[512];
    time_t detected_at;
    time_t resolved_at;
    bool is_resolved;
} health_issue_t;

// Member health
typedef struct {
    double score;
    char status[32];
    char issues[512];
    time_t last_check;
    bool is_healthy;
    int issue_count;
} member_health_t;

// Health analysis result
typedef struct {
    member_health_t member_health[16];
    char member_names[16][128];
    int member_count;
    double overall_health;
    health_issue_t issues[32];
    int issue_count;
    char recommendations[1024];
    time_t analysis_timestamp;
} health_analysis_t;

// Health analyzer structure
typedef struct {
    health_thresholds_t thresholds;
    
    // Analysis results
    health_analysis_t* last_result;
    
    // Statistics
    time_t last_analysis;
    int analysis_count;
    int issues_detected;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
} health_analyzer_t;

// Initialize health analyzer
int health_analyzer_init(const health_thresholds_t* thresholds);

// Clean up health analyzer
void health_analyzer_cleanup(void);

// Analyze health for all members
int health_analyzer_analyze(health_analysis_t* result);

// Get health for specific member
int health_analyzer_get_member_health(const char* member_name, member_health_t* health);

// Calculate health score
double health_analyzer_calculate_score(const member_health_t* health);

// Detect health issues
int health_analyzer_detect_issues(const char* member_name, health_issue_t* issues, int max_issues);

// Get health analyzer status
void health_analyzer_get_status(health_analyzer_t* status);

// Check if health analyzer is initialized
bool health_analyzer_is_initialized(void);

// Get health analyzer instance
health_analyzer_t* health_analyzer_get_instance(void);

#endif // HEALTH_ANALYZER_H
