#!/usr/bin/env python3
"""
Fix UCI Configuration Defaults
Updates all critical services to be disabled by default for safety
"""

import os
import re

def fix_uci_defaults():
    """Fix UCI configuration defaults to be safe"""
    config_file = "autonomy-daemon/files/autonomy.config"
    
    if not os.path.exists(config_file):
        print("❌ UCI configuration file not found!")
        return False
    
    # Read the current configuration
    with open(config_file, 'r') as f:
        content = f.read()
    
    # Services that should be disabled by default for safety
    services_to_disable = [
        'auto_failover',
        'mwan3_integration', 
        'resource_monitoring',
        'service_monitoring',
        'notifications',
        'snow_detection',
        'gps_manager',
        'terrain_analysis',
        'comprehensive_gps',
        'cellular_collector',
        'stability_monitoring',
        'network_failover',
        'network_discovery',
        'network_collector',
        'network_controller',
        'wifi_management',
        'wifi_enhanced',
        'starlink_comprehensive',
        'obstruction_analysis',
        'snow_detection_integration',
        'api_version_monitor',
        'telemetry_comprehensive',
        'telemetry_store',
        'predictive_engine',
        'notification_config',
        'health_monitoring',
        'fusion',
        'use_mwan3',
        'compression_enabled'
    ]
    
    # Keep these enabled by default (essential functionality)
    services_to_keep_enabled = [
        'daemon_mode',  # Essential for daemon operation
        'debug_mode'    # Should be disabled by default (already is)
    ]
    
    changes_made = 0
    
    # Fix each service
    for service in services_to_disable:
        # Pattern to match: option service_name '1'
        pattern = rf"(\s+option {re.escape(service)}\s+)'1'"
        replacement = rf"\1'0'"
        
        new_content = re.sub(pattern, replacement, content)
        if new_content != content:
            changes_made += 1
            content = new_content
            print(f"✅ Disabled {service} by default")
    
    # Write the updated configuration
    with open(config_file, 'w') as f:
        f.write(content)
    
    print(f"\n🔧 Made {changes_made} changes to UCI configuration")
    print("✅ All critical services are now disabled by default")
    
    return True

def verify_fixes():
    """Verify that the fixes were applied correctly"""
    config_file = "autonomy-daemon/files/autonomy.config"
    
    with open(config_file, 'r') as f:
        content = f.read()
    
    # Check for remaining enabled services
    enabled_services = re.findall(r"option (\w+) '1'", content)
    
    # Filter out services that should remain enabled
    critical_enabled = [s for s in enabled_services if s not in ['daemon_mode']]
    
    if critical_enabled:
        print(f"⚠️  Still enabled: {critical_enabled}")
        return False
    else:
        print("✅ All critical services are now disabled by default")
        return True

def main():
    print("🔧 Fixing UCI Configuration Defaults")
    print("=" * 40)
    
    if fix_uci_defaults():
        print("\n🔍 Verifying fixes...")
        if verify_fixes():
            print("\n🎉 SUCCESS!")
            print("✅ UCI configuration now has safe defaults")
            print("✅ All critical services are disabled by default")
            print("✅ Users must explicitly enable services they want")
        else:
            print("\n⚠️  Some services may still be enabled")
    else:
        print("\n❌ Failed to fix UCI configuration")

if __name__ == "__main__":
    main()
