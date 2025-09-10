#ifndef UCI_MANAGER_H
#define UCI_MANAGER_H

#include "../core/types.h"
#include "../utils/logx.h"
#include "../starlink/starlink_snow_detection.h"
#include <stdbool.h>
#include <uci.h>

// Initialize UCI system
int uci_manager_init(void);

// Load configuration from UCI
int uci_manager_load_config(autonomy_config_t *config);

// Save configuration to UCI
int uci_manager_save_config(const autonomy_config_t *config);

// Validate configuration
int uci_manager_validate_config(const autonomy_config_t *config);

// Get default configuration
const autonomy_config_t* uci_manager_get_default_config(void);

// Check if UCI is available
bool uci_manager_is_available(void);

// Cleanup UCI system
void uci_manager_cleanup(void);

// Convert autonomy config to snow detection config
void uci_manager_convert_to_snow_config(const autonomy_config_t *autonomy_config, 
                                       starlink_snow_detection_config_t *snow_config);

// Convert snow detection config to autonomy config
void uci_manager_convert_from_snow_config(const starlink_snow_detection_config_t *snow_config,
                                         autonomy_config_t *autonomy_config);

#endif // UCI_MANAGER_H
