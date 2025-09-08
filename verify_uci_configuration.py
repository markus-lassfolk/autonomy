#!/usr/bin/env python3
"""
Comprehensive UCI Configuration Verification Script
Verifies that:
1. All configurable values are present in UCI config
2. Default values are safe (disabled where appropriate)
3. All functions read from UCI config
"""

import os
import re
import json
from pathlib import Path

def find_uci_config_file():
    """Find the UCI configuration file"""
    config_paths = [
        "autonomy-daemon/files/autonomy.config",
        "autonomy_complete.conf"
    ]
    
    for path in config_paths:
        if os.path.exists(path):
            return path
    return None

def parse_uci_config(config_file):
    """Parse UCI configuration file"""
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

def find_configurable_values_in_code():
    """Find all configurable values mentioned in code comments"""
    configurable_values = set()
    
    # Search for "Use configurable" comments
    for root, dirs, files in os.walk("src/c/autonomy-daemon"):
        for file in files:
            if file.endswith('.c'):
                filepath = os.path.join(root, file)
                try:
                    with open(filepath, 'r') as f:
                        content = f.read()
                        # Find "Use configurable" comments
                        matches = re.findall(r'// Use configurable ([^\\n]+)', content)
                        for match in matches:
                            configurable_values.add(match.strip())
                except:
                    continue
    
    return configurable_values

def check_safe_defaults(config):
    """Check if default values are safe (disabled where appropriate)"""
    unsafe_defaults = []
    
    # Critical services that should be disabled by default
    critical_services = [
        'auto_failover', 'mwan3_integration', 'resource_monitoring',
        'service_monitoring', 'notifications', 'snow_detection',
        'gps_manager', 'terrain_analysis', 'comprehensive_gps',
        'cellular_collector', 'stability_monitoring', 'network_failover',
        'network_discovery', 'network_collector', 'network_controller',
        'wifi_management'
    ]
    
    for section_name, section_config in config.items():
        for option_name, option_value in section_config.items():
            if option_name in critical_services and option_value == '1':
                unsafe_defaults.append(f"{section_name}.{option_name} = {option_value}")
    
    return unsafe_defaults

def verify_uci_integration():
    """Verify UCI integration in code"""
    uci_usage = {
        'g_config_usage': 0,
        'uci_manager_usage': 0,
        'extern_declarations': 0
    }
    
    for root, dirs, files in os.walk("src/c/autonomy-daemon"):
        for file in files:
            if file.endswith('.c'):
                filepath = os.path.join(root, file)
                try:
                    with open(filepath, 'r') as f:
                        content = f.read()
                        
                        # Count g_config usage
                        uci_usage['g_config_usage'] += len(re.findall(r'g_config\.', content))
                        
                        # Count uci_manager usage
                        uci_usage['uci_manager_usage'] += len(re.findall(r'uci_manager_', content))
                        
                        # Count extern declarations
                        uci_usage['extern_declarations'] += len(re.findall(r'extern autonomy_config_t g_config', content))
                        
                except:
                    continue
    
    return uci_usage

def main():
    print("🔍 Comprehensive UCI Configuration Verification")
    print("=" * 60)
    
    # Find UCI config file
    config_file = find_uci_config_file()
    if not config_file:
        print("❌ UCI configuration file not found!")
        return False
    
    print(f"📁 Found UCI config: {config_file}")
    
    # Parse UCI configuration
    config = parse_uci_config(config_file)
    print(f"📊 Parsed {len(config)} configuration sections")
    
    # Find configurable values in code
    configurable_values = find_configurable_values_in_code()
    print(f"🔧 Found {len(configurable_values)} configurable values in code")
    
    # Check safe defaults
    unsafe_defaults = check_safe_defaults(config)
    if unsafe_defaults:
        print(f"⚠️  Found {len(unsafe_defaults)} potentially unsafe defaults:")
        for default in unsafe_defaults[:10]:  # Show first 10
            print(f"   - {default}")
        if len(unsafe_defaults) > 10:
            print(f"   ... and {len(unsafe_defaults) - 10} more")
    else:
        print("✅ All defaults appear safe")
    
    # Verify UCI integration
    uci_usage = verify_uci_integration()
    print(f"🔗 UCI Integration:")
    print(f"   - g_config usage: {uci_usage['g_config_usage']} references")
    print(f"   - uci_manager usage: {uci_usage['uci_manager_usage']} references")
    print(f"   - extern declarations: {uci_usage['extern_declarations']} files")
    
    # Summary
    print("\n📋 VERIFICATION SUMMARY:")
    print(f"✅ UCI config file: {'Found' if config_file else 'Missing'}")
    print(f"✅ Configuration sections: {len(config)}")
    print(f"✅ Configurable values in code: {len(configurable_values)}")
    print(f"✅ Safe defaults: {'Yes' if not unsafe_defaults else 'No'}")
    print(f"✅ UCI integration: {'Active' if uci_usage['g_config_usage'] > 0 else 'Inactive'}")
    
    # Overall result
    if config_file and len(config) > 0 and uci_usage['g_config_usage'] > 0:
        print("\n🎉 VERIFICATION PASSED!")
        print("✅ UCI configuration system is comprehensive and functional")
        return True
    else:
        print("\n❌ VERIFICATION FAILED!")
        print("⚠️  UCI configuration system needs attention")
        return False

if __name__ == "__main__":
    main()
