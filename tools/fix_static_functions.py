#!/usr/bin/env python3
"""
Fix Static Functions Script
===========================

This script automatically adds 'static' keyword to helper functions that are
defined but not declared, based on the verification results.
"""

import os
import re
import sys
from pathlib import Path

def fix_static_functions_in_file(file_path):
    """Fix static functions in a single file"""
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
        
        lines = content.split('\n')
        modified = False
        
        # Common patterns for function definitions that should be static
        function_patterns = [
            # Standard function definitions
            r'^int\s+(\w+)\s*\([^)]*\)\s*\{',
            r'^void\s+(\w+)\s*\([^)]*\)\s*\{',
            r'^char\s*\*\s*(\w+)\s*\([^)]*\)\s*\{',
            r'^double\s+(\w+)\s*\([^)]*\)\s*\{',
            r'^float\s+(\w+)\s*\([^)]*\)\s*\{',
            r'^bool\s+(\w+)\s*\([^)]*\)\s*\{',
            r'^size_t\s+(\w+)\s*\([^)]*\)\s*\{',
            r'^time_t\s+(\w+)\s*\([^)]*\)\s*\{',
            r'^uint32_t\s+(\w+)\s*\([^)]*\)\s*\{',
            r'^uint64_t\s+(\w+)\s*\([^)]*\)\s*\{',
            r'^int32_t\s+(\w+)\s*\([^)]*\)\s*\{',
            r'^int64_t\s+(\w+)\s*\([^)]*\)\s*\{',
            r'^long\s+(\w+)\s*\([^)]*\)\s*\{',
            r'^short\s+(\w+)\s*\([^)]*\)\s*\{',
            r'^unsigned\s+(\w+)\s*\([^)]*\)\s*\{',
            r'^signed\s+(\w+)\s*\([^)]*\)\s*\{',
            # Complex return types
            r'^[a-zA-Z_][a-zA-Z0-9_]*\s*\*\s*(\w+)\s*\([^)]*\)\s*\{',
            r'^[a-zA-Z_][a-zA-Z0-9_]*\s+(\w+)\s*\([^)]*\)\s*\{',
            # More complex patterns
            r'^[a-zA-Z_][a-zA-Z0-9_]*\s+[a-zA-Z_][a-zA-Z0-9_]*\s+(\w+)\s*\([^)]*\)\s*\{',
            r'^[a-zA-Z_][a-zA-Z0-9_]*\s*\*\s*[a-zA-Z_][a-zA-Z0-9_]*\s+(\w+)\s*\([^)]*\)\s*\{',
        ]
        
        # Functions that should NOT be made static (public API functions)
        public_functions = {
            'main', 'autonomy_status', 'autonomy_health', 'autonomy_config',
            'autonomy_start', 'autonomy_stop', 'autonomy_restart',
            'autonomy_pid_status', 'autonomy_log_status', 'autonomy_config_status',
            'autonomy_network_status', 'autonomy_network_interfaces', 'autonomy_network_health_check',
            'autonomy_network_failover', 'autonomy_gps_status', 'autonomy_gps_sources',
            'autonomy_gps_health_check', 'autonomy_system_status', 'autonomy_system_health_check',
            'autonomy_system_health_details', 'autonomy_system_maintenance', 'autonomy_system_restart_services',
            'autonomy_starlink_status', 'autonomy_starlink_health', 'autonomy_starlink_location',
            'autonomy_starlink_collector_stats', 'autonomy_starlink_force_collect',
            'autonomy_starlink_cluster_status', 'autonomy_starlink_cluster_check_failover',
            'discover_network_interfaces', 'perform_network_health_check',
            'discover_gps_sources', 'perform_gps_health_check', 'load_uci_config',
            'create_pid_file', 'remove_pid_file', 'check_pid_file'
        }
        
        for i, line in enumerate(lines):
            # Skip if already static
            if line.strip().startswith('static '):
                continue
            
            # Check each pattern
            for pattern in function_patterns:
                match = re.match(pattern, line.strip())
                if match:
                    func_name = match.group(1)
                    
                    # Skip public functions
                    if func_name in public_functions:
                        continue
                    
                    # Skip if it's already declared in a header (has extern or is in a header file)
                    if file_path.endswith('.h'):
                        continue
                    
                    # Add static keyword
                    if not line.strip().startswith('static '):
                        # Find the start of the line (preserve indentation)
                        indent = len(line) - len(line.lstrip())
                        new_line = ' ' * indent + 'static ' + line.strip()
                        lines[i] = new_line
                        modified = True
                        print(f"  Made {func_name} static in {file_path}")
                    break
        
        if modified:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write('\n'.join(lines))
            return True
        
        return False
        
    except Exception as e:
        print(f"Error processing {file_path}: {e}")
        return False

def main():
    """Main function"""
    if len(sys.argv) != 2:
        print("Usage: python3 fix_static_functions.py <directory>")
        sys.exit(1)
    
    directory = sys.argv[1]
    if not os.path.exists(directory):
        print(f"Directory {directory} does not exist")
        sys.exit(1)
    
    print(f"Fixing static functions in {directory}")
    
    # Find all C files
    c_files = []
    for root, dirs, files in os.walk(directory):
        for file in files:
            if file.endswith(('.c', '.h')):
                c_files.append(os.path.join(root, file))
    
    print(f"Found {len(c_files)} C files")
    
    modified_count = 0
    for file_path in c_files:
        if fix_static_functions_in_file(file_path):
            modified_count += 1
    
    print(f"Modified {modified_count} files")

if __name__ == "__main__":
    main()
