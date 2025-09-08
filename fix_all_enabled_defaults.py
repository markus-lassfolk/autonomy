#!/usr/bin/env python3
"""
Fix All Enabled Defaults in UCI Configuration
Updates ALL 'enabled' options to be disabled by default
"""

import os
import re

def fix_all_enabled_defaults():
    """Fix all enabled options to be disabled by default"""
    config_file = "autonomy-daemon/files/autonomy.config"
    
    if not os.path.exists(config_file):
        print("❌ UCI configuration file not found!")
        return False
    
    # Read the current configuration
    with open(config_file, 'r') as f:
        content = f.read()
    
    # Keep these specific options enabled (essential functionality)
    keep_enabled = [
        'daemon_mode',  # Essential for daemon operation
    ]
    
    changes_made = 0
    
    # Find all "option enabled '1'" patterns
    pattern = r"(\s+option enabled\s+)'1'"
    matches = re.findall(pattern, content)
    
    if matches:
        # Replace all "option enabled '1'" with "option enabled '0'"
        new_content = re.sub(pattern, r"\1'0'", content)
        changes_made = len(matches)
        content = new_content
        print(f"✅ Disabled {changes_made} 'enabled' options by default")
    
    # Also fix other common enabled patterns
    other_patterns = [
        (r"(\s+option \w+_enabled\s+)'1'", r"\1'0'"),
        (r"(\s+option enable_\w+\s+)'1'", r"\1'0'"),
        (r"(\s+option \w+_monitoring\s+)'1'", r"\1'0'"),
        (r"(\s+option \w+_detection\s+)'1'", r"\1'0'"),
        (r"(\s+option \w+_analysis\s+)'1'", r"\1'0'"),
        (r"(\s+option \w+_collection\s+)'1'", r"\1'0'"),
        (r"(\s+option \w+_management\s+)'1'", r"\1'0'"),
        (r"(\s+option \w+_optimization\s+)'1'", r"\1'0'"),
        (r"(\s+option \w+_integration\s+)'1'", r"\1'0'"),
        (r"(\s+option \w+_comprehensive\s+)'1'", r"\1'0'"),
        (r"(\s+option \w+_enhanced\s+)'1'", r"\1'0'"),
        (r"(\s+option \w+_tracking\s+)'1'", r"\1'0'"),
        (r"(\s+option \w+_intelligence\s+)'1'", r"\1'0'"),
        (r"(\s+option \w+_engine\s+)'1'", r"\1'0'"),
        (r"(\s+option \w+_store\s+)'1'", r"\1'0'"),
        (r"(\s+option \w+_monitor\s+)'1'", r"\1'0'"),
        (r"(\s+option \w+_controller\s+)'1'", r"\1'0'"),
        (r"(\s+option \w+_collector\s+)'1'", r"\1'0'"),
        (r"(\s+option \w+_discovery\s+)'1'", r"\1'0'"),
        (r"(\s+option \w+_failover\s+)'1'", r"\1'0'"),
    ]
    
    for pattern, replacement in other_patterns:
        new_content = re.sub(pattern, replacement, content)
        if new_content != content:
            # Count how many changes were made
            old_matches = len(re.findall(pattern, content))
            new_matches = len(re.findall(pattern, new_content))
            changes_made += (old_matches - new_matches)
            content = new_content
    
    # Write the updated configuration
    with open(config_file, 'w') as f:
        f.write(content)
    
    print(f"🔧 Made {changes_made} total changes to UCI configuration")
    
    return True

def verify_all_fixes():
    """Verify that all fixes were applied correctly"""
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
    print("🔧 Fixing ALL Enabled Defaults in UCI Configuration")
    print("=" * 50)
    
    if fix_all_enabled_defaults():
        print("\n🔍 Verifying fixes...")
        if verify_all_fixes():
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
