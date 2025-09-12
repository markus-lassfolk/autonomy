#include "../core/types.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <uci.h>

extern struct autonomy_config g_config;
extern struct uci_context *uci_ctx;

// UCI configuration loading
int load_uci_config(void) {
    uci_ctx = uci_alloc_context();
    if (!uci_ctx) {
        return -1;
    }
    
    struct uci_package *pkg = NULL;
    int ret = uci_load(uci_ctx, "autonomy", &pkg);
    if (ret != UCI_OK) {
        // Continue with defaults
        return 0;
    }
    
    // Load configuration values
    struct uci_section *s = uci_lookup_section(uci_ctx, pkg, "main");
    if (s) {
        const char *log_level = uci_lookup_option_string(uci_ctx, s, "log_level");
        if (log_level) {
            strncpy(g_config.log_level, log_level, sizeof(g_config.log_level) - 1);
        }
        
        const char *enable_gps = uci_lookup_option_string(uci_ctx, s, "enable_gps");
        if (enable_gps) {
            g_config.enable_gps = strcmp(enable_gps, "1") == 0 || strcmp(enable_gps, "true") == 0;
        }
        
        const char *enable_notifications = uci_lookup_option_string(uci_ctx, s, "enable_notifications");
        if (enable_notifications) {
            g_config.enable_notifications = strcmp(enable_notifications, "1") == 0 || strcmp(enable_notifications, "true") == 0;
        }
        
        const char *health_interval = uci_lookup_option_string(uci_ctx, s, "health_check_interval");
        if (health_interval) {
            g_config.health_check_interval = atoi(health_interval);
        }
    }
    
    return 0;
}
