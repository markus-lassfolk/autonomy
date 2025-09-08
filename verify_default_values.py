#!/usr/bin/env python3
"""
Detailed Default Values Verification
Checks if critical services are disabled by default for safety
"""

import os
import re

def parse_uci_config_detailed(config_file):
    """Parse UCI configuration file with detailed analysis"""
    config = {}
    current_section = None
    
    with open(config_file, 'r') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
                
            if line.startswith('config '):
                # New section
                parts = line.split()
                if len(parts) >= 3:
                    current_section = parts[2].strip("'\"")
                    if current_section not in config:
                        config[current_section] = {}
            elif line.startswith('option ') and current_section:
                # Option in current section
                parts = line.split(' ', 2)
                if len(parts) >= 3:
                    option_name = parts[1]
                    option_value = parts[2].strip("'\"")
                    config[current_section][option_name] = option_value
    
    return config

def check_critical_defaults(config):
    """Check critical services that should be disabled by default"""
    critical_services = {
        'auto_failover': 'Should be disabled by default to prevent automatic network changes',
        'mwan3_integration': 'Should be disabled by default to prevent automatic routing changes',
        'resource_monitoring': 'Should be disabled by default to prevent resource overhead',
        'service_monitoring': 'Should be disabled by default to prevent service interference',
        'notifications': 'Should be disabled by default to prevent spam',
        'snow_detection': 'Should be disabled by default to prevent false positives',
        'gps_manager': 'Should be disabled by default to prevent GPS interference',
        'terrain_analysis': 'Should be disabled by default to prevent resource usage',
        'comprehensive_gps': 'Should be disabled by default to prevent GPS overhead',
        'cellular_collector': 'Should be disabled by default to prevent data collection',
        'stability_monitoring': 'Should be disabled by default to prevent monitoring overhead',
        'network_failover': 'Should be disabled by default to prevent automatic failover',
        'network_discovery': 'Should be disabled by default to prevent network scanning',
        'network_collector': 'Should be disabled by default to prevent data collection',
        'network_controller': 'Should be disabled by default to prevent network control',
        'wifi_management': 'Should be disabled by default to prevent WiFi interference'
    }
    
    issues = []
    
    for section_name, section_config in config.items():
        for option_name, option_value in section_config.items():
            if option_name in critical_services and option_value == '1':
                issues.append({
                    'section': section_name,
                    'option': option_name,
                    'value': option_value,
                    'reason': critical_services[option_name]
                })
    
    return issues

def main():
    print("🔍 Detailed Default Values Verification")
    print("=" * 50)
    
    config_file = "autonomy-daemon/files/autonomy.config"
    if not os.path.exists(config_file):
        print("❌ UCI configuration file not found!")
        return False
    
    # Parse configuration
    config = parse_uci_config_detailed(config_file)
    
    # Check critical defaults
    issues = check_critical_defaults(config)
    
    if issues:
        print(f"⚠️  Found {len(issues)} potentially unsafe default values:")
        print()
        for issue in issues:
            print(f"❌ {issue['section']}.{issue['option']} = {issue['value']}")
            print(f"   Reason: {issue['reason']}")
            print()
        
        print("🔧 RECOMMENDATION: Update these values to '0' (disabled) by default")
        return False
    else:
        print("✅ All critical services are disabled by default")
        return True

if __name__ == "__main__":
    main()
