#include "network_collector.h"
#include "../shared/logging/logx.h"
#include "../core/types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Archive-specific functionality for network collector
// Note: Core network collector functions are implemented in network_collector.c

// Archive network metrics to persistent storage
int network_collector_archive_metrics(const network_metrics_t *metrics, const char *archive_path) {
    if (!metrics || !archive_path) {
        LOGX_ERROR_MSG("Invalid parameters for network metrics archiving");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    FILE *archive_file = fopen(archive_path, "a");
    if (!archive_file) {
        LOGX_ERROR_MSG("Failed to open archive file: %s", archive_path);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Write metrics in CSV format with timestamp
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    fprintf(archive_file, "%04d-%02d-%02d %02d:%02d:%02d,%.2f,%.2f,%.2f,%.2f,%llu,%llu,%llu,%llu\n",
            tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
            tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
            metrics->ping_latency_ms, metrics->ping_packet_loss, metrics->throughput_mbps,
            metrics->ping_jitter_ms, metrics->packets_transmitted, metrics->packets_received,
            metrics->bytes_transmitted, metrics->bytes_received);
    
    fclose(archive_file);
    
    LOGX_DEBUG_MSG("Network metrics archived to: %s", archive_path);
    return AUTONOMY_SUCCESS;
}

// Load archived network metrics from storage
int network_collector_load_archived_metrics(const char *archive_path, network_metrics_t *metrics, int max_count) {
    if (!archive_path || !metrics || max_count <= 0) {
        LOGX_ERROR_MSG("Invalid parameters for loading archived metrics");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    FILE *archive_file = fopen(archive_path, "r");
    if (!archive_file) {
        LOGX_WARN_MSG("Archive file not found: %s", archive_path);
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    int loaded_count = 0;
    char line[512];
    
    // Read CSV lines and parse metrics
    while (fgets(line, sizeof(line), archive_file) && loaded_count < max_count) {
        // Parse CSV format: timestamp,ping_latency_ms,ping_packet_loss,throughput_mbps,ping_jitter_ms,packets_transmitted,packets_received,bytes_transmitted,bytes_received
        int year, month, day, hour, min, sec;
        if (sscanf(line, "%d-%d-%d %d:%d:%d,%lf,%lf,%lf,%lf,%llu,%llu,%llu,%llu",
                   &year, &month, &day, &hour, &min, &sec,
                   &metrics[loaded_count].ping_latency_ms,
                   &metrics[loaded_count].ping_packet_loss,
                   &metrics[loaded_count].throughput_mbps,
                   &metrics[loaded_count].ping_jitter_ms,
                   &metrics[loaded_count].packets_transmitted,
                   &metrics[loaded_count].packets_received,
                   &metrics[loaded_count].bytes_transmitted,
                   &metrics[loaded_count].bytes_received) == 14) {
            loaded_count++;
        }
    }
    
    fclose(archive_file);
    
    LOGX_INFO_MSG("Loaded %d archived network metrics from: %s", loaded_count, archive_path);
    return loaded_count;
}

// Clean up old archived metrics (keep only recent entries)
int network_collector_cleanup_archived_metrics(const char *archive_path, int keep_days) {
    if (!archive_path || keep_days <= 0) {
        LOGX_ERROR_MSG("Invalid parameters for cleaning up archived metrics");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Create temporary file for filtered data
    char temp_path[512];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", archive_path);
    
    FILE *archive_file = fopen(archive_path, "r");
    if (!archive_file) {
        LOGX_WARN_MSG("Archive file not found: %s", archive_path);
        return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    FILE *temp_file = fopen(temp_path, "w");
    if (!temp_file) {
        fclose(archive_file);
        LOGX_ERROR_MSG("Failed to create temporary file: %s", temp_path);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    time_t cutoff_time = time(NULL) - (keep_days * 24 * 3600);
    char line[512];
    int kept_count = 0;
    int total_count = 0;
    
    while (fgets(line, sizeof(line), archive_file)) {
        total_count++;
        
        // Parse timestamp from line
        int year, month, day, hour, min, sec;
        if (sscanf(line, "%d-%d-%d %d:%d:%d,", &year, &month, &day, &hour, &min, &sec) == 6) {
            struct tm tm_info = {0};
            tm_info.tm_year = year - 1900;
            tm_info.tm_mon = month - 1;
            tm_info.tm_mday = day;
            tm_info.tm_hour = hour;
            tm_info.tm_min = min;
            tm_info.tm_sec = sec;
            
            time_t entry_time = mktime(&tm_info);
            if (entry_time >= cutoff_time) {
                fputs(line, temp_file);
                kept_count++;
            }
        }
    }
    
    fclose(archive_file);
    fclose(temp_file);
    
    // Replace original file with filtered data
    if (rename(temp_path, archive_path) != 0) {
        LOGX_ERROR_MSG("Failed to replace archive file");
        unlink(temp_path);
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    LOGX_INFO_MSG("Cleaned up archived metrics: kept %d/%d entries (last %d days)", 
                  kept_count, total_count, keep_days);
    return AUTONOMY_SUCCESS;
}

// Get archive statistics
int network_collector_get_archive_stats(const char *archive_path, network_archive_stats_t *stats) {
    if (!archive_path || !stats) {
        LOGX_ERROR_MSG("Invalid parameters for archive statistics");
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    memset(stats, 0, sizeof(network_archive_stats_t));
    
    FILE *archive_file = fopen(archive_path, "r");
    if (!archive_file) {
        LOGX_WARN_MSG("Archive file not found: %s", archive_path);
    return AUTONOMY_ERROR_NOT_FOUND;
    }
    
    char line[512];
    float total_latency = 0.0f;
    float total_packet_loss = 0.0f;
    float total_bandwidth = 0.0f;
    float total_jitter = 0.0f;
    time_t oldest_time = 0;
    time_t newest_time = 0;
    
    while (fgets(line, sizeof(line), archive_file)) {
        int year, month, day, hour, min, sec;
        float latency, packet_loss, bandwidth, jitter;
        
        if (sscanf(line, "%d-%d-%d %d:%d:%d,%f,%f,%f,%f,", 
                   &year, &month, &day, &hour, &min, &sec,
                   &latency, &packet_loss, &bandwidth, &jitter) == 9) {
            
            stats->total_entries++;
            total_latency += latency;
            total_packet_loss += packet_loss;
            total_bandwidth += bandwidth;
            total_jitter += jitter;
            
            struct tm tm_info = {0};
            tm_info.tm_year = year - 1900;
            tm_info.tm_mon = month - 1;
            tm_info.tm_mday = day;
            tm_info.tm_hour = hour;
            tm_info.tm_min = min;
            tm_info.tm_sec = sec;
            
            time_t entry_time = mktime(&tm_info);
            if (oldest_time == 0 || entry_time < oldest_time) {
                oldest_time = entry_time;
            }
            if (newest_time == 0 || entry_time > newest_time) {
                newest_time = entry_time;
            }
        }
    }
    
    fclose(archive_file);
    
    if (stats->total_entries > 0) {
        stats->avg_latency_ms = total_latency / stats->total_entries;
        stats->avg_packet_loss_pct = total_packet_loss / stats->total_entries;
        stats->avg_bandwidth_mbps = total_bandwidth / stats->total_entries;
        stats->avg_jitter_ms = total_jitter / stats->total_entries;
        stats->oldest_entry = oldest_time;
        stats->newest_entry = newest_time;
    }
    
    LOGX_DEBUG_MSG("Archive statistics: %d entries, avg latency: %.2f ms", 
                   stats->total_entries, stats->avg_latency_ms);
    return AUTONOMY_SUCCESS;
}
