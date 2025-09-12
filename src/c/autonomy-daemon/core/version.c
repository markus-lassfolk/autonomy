#include "version.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

// Static buffer for version string
static char version_string[64];
static char build_info_string[256];

const char* autonomy_daemon_get_version_string(void) {
    snprintf(version_string, sizeof(version_string), 
             "%s (build %d)", 
             AUTONOMY_DAEMON_VERSION_FULL, 
             AUTONOMY_DAEMON_VERSION_BUILD);
    return version_string;
}

const char* autonomy_daemon_get_build_info_string(void) {
    // Get current UTC time
    time_t now = time(NULL);
    struct tm *utc_time = gmtime(&now);
    char utc_timestamp[64];
    strftime(utc_timestamp, sizeof(utc_timestamp), "%Y-%m-%d %H:%M:%S UTC", utc_time);
    
    snprintf(build_info_string, sizeof(build_info_string),
             "Version: %s | Build: %s %s | Git: %s@%s | Runtime: %s",
             AUTONOMY_DAEMON_VERSION_FULL,
             AUTONOMY_DAEMON_BUILD_DATE,
             AUTONOMY_DAEMON_BUILD_TIME,
             AUTONOMY_DAEMON_GIT_BRANCH,
             AUTONOMY_DAEMON_GIT_COMMIT,
             utc_timestamp);
    
    return build_info_string;
}
