#!/usr/bin/env python3
"""
Audit script to find all hardcoded values that should use UCI configuration.
This script systematically searches for hardcoded timeouts, intervals, and thresholds.
"""

import os
import re
import json
from pathlib import Path

# Configuration values that should come from UCI
UCI_CONFIG_FIELDS = {
    'gps_update_interval': ['gps.*interval', 'update.*interval', 'GPS_UPDATE_INTERVAL'],
    'gps_timeout': ['gps.*timeout', 'GPS_SOURCE_TIMEOUT', 'GPS_TIMEOUT'],
    'min_gps_accuracy': ['min.*accuracy', 'accuracy.*threshold', 'MIN_ACCURACY'],
    'network_check_interval': ['network.*interval', 'check.*interval', 'NETWORK_INTERVAL'],
    'failover_timeout': ['failover.*timeout', 'FAILOVER_TIMEOUT'],
    'auto_failover': ['auto_failover', 'AUTO_FAILOVER'],
    'starlink_check_interval': ['starlink.*interval', 'STARLINK_INTERVAL'],
    'starlink_health_monitoring': ['health.*monitoring', 'HEALTH_MONITORING'],
    'system_check_interval': ['system.*interval', 'SYSTEM_INTERVAL'],
    'snow_detection_enabled': ['snow.*enabled', 'SNOW_ENABLED'],
    'snow_obstruction_threshold': ['obstruction.*threshold', 'OBSTRUCTION_THRESHOLD'],
    'snow_snr_degradation_threshold': ['snr.*threshold', 'SNR_THRESHOLD'],
    'snow_temperature_threshold': ['temperature.*threshold', 'TEMP_THRESHOLD'],
    'notifications_enabled': ['notification.*enabled', 'NOTIFICATION_ENABLED'],
    'log_level': ['log.*level', 'LOG_LEVEL'],
    'debug_mode': ['debug.*mode', 'DEBUG_MODE']
}

# Common hardcoded patterns
HARDCODED_PATTERNS = [
    r'=\s*\d+\s*;',  # = 30;
    r'=\s*\d+\.\d+\s*;',  # = 10.0;
    r'=\s*true\s*;',  # = true;
    r'=\s*false\s*;',  # = false;
    r'#define\s+\w+\s+\d+',  # #define TIMEOUT 30
    r'static\s+const\s+\w+\s+\w+\s*=\s*\d+',  # static const int TIMEOUT = 30
    r'sleep\(\s*\d+\s*\)',  # sleep(30)
    r'usleep\(\s*\d+\s*\)',  # usleep(1000)
    r'timeout.*=\s*\d+',  # timeout = 30
    r'interval.*=\s*\d+',  # interval = 30
]

# Files to exclude from search
EXCLUDE_PATTERNS = [
    '*.md',
    '*.txt',
    '*.log',
    '*.json',
    '*.yml',
    '*.yaml',
    'Makefile*',
    '*.py',
    '*.sh',
    '*.conf',  # Configuration files themselves
    'autonomy_complete.conf',
    'autonomy.config'
]

def should_exclude_file(file_path):
    """Check if file should be excluded from search."""
    for pattern in EXCLUDE_PATTERNS:
        if file_path.match(pattern):
            return True
    return False

def find_hardcoded_values(file_path):
    """Find hardcoded values in a file."""
    results = []
    
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
            lines = content.split('\n')
            
        for line_num, line in enumerate(lines, 1):
            # Skip comments and empty lines
            if line.strip().startswith('//') or line.strip().startswith('/*') or not line.strip():
                continue
                
            # Check for hardcoded patterns
            for pattern in HARDCODED_PATTERNS:
                matches = re.finditer(pattern, line, re.IGNORECASE)
                for match in matches:
                    # Check if this looks like a configuration value
                    if any(keyword in line.lower() for keyword in ['timeout', 'interval', 'threshold', 'enabled', 'disabled', 'timeout', 'delay', 'sleep']):
                        results.append({
                            'line': line_num,
                            'content': line.strip(),
                            'match': match.group(),
                            'pattern': pattern
                        })
                        
    except Exception as e:
        print(f"Error reading {file_path}: {e}")
        
    return results

def audit_directory(directory):
    """Audit all C files in directory for hardcoded values."""
    results = {}
    
    for file_path in Path(directory).rglob('*.c'):
        if should_exclude_file(file_path):
            continue
            
        hardcoded_values = find_hardcoded_values(file_path)
        if hardcoded_values:
            results[str(file_path)] = hardcoded_values
            
    return results

def generate_report(results):
    """Generate a comprehensive audit report."""
    report = {
        'summary': {
            'total_files_audited': len(results),
            'files_with_hardcoded_values': len([f for f in results.values() if f]),
            'total_hardcoded_values': sum(len(values) for values in results.values())
        },
        'files': results,
        'recommendations': []
    }
    
    # Generate recommendations
    for file_path, values in results.items():
        if values:
            report['recommendations'].append({
                'file': file_path,
                'issue': f"Found {len(values)} hardcoded values",
                'action': "Replace hardcoded values with g_config references",
                'values': [v['content'] for v in values[:5]]  # First 5 examples
            })
    
    return report

def main():
    """Main audit function."""
    print("🔍 Auditing autonomy daemon for hardcoded configuration values...")
    
    # Audit the main daemon directory
    daemon_dir = Path('src/c/autonomy-daemon')
    if not daemon_dir.exists():
        print(f"❌ Directory {daemon_dir} not found!")
        return
        
    results = audit_directory(daemon_dir)
    
    # Generate report
    report = generate_report(results)
    
    # Save detailed report
    with open('hardcoded_values_audit.json', 'w') as f:
        json.dump(report, f, indent=2)
    
    # Print summary
    print(f"\n📊 Audit Summary:")
    print(f"   Files audited: {report['summary']['total_files_audited']}")
    print(f"   Files with hardcoded values: {report['summary']['files_with_hardcoded_values']}")
    print(f"   Total hardcoded values found: {report['summary']['total_hardcoded_values']}")
    
    print(f"\n🚨 Files with hardcoded values:")
    for file_path, values in results.items():
        if values:
            print(f"   📁 {file_path}: {len(values)} hardcoded values")
            for value in values[:3]:  # Show first 3 examples
                print(f"      Line {value['line']}: {value['content']}")
            if len(values) > 3:
                print(f"      ... and {len(values) - 3} more")
    
    print(f"\n💾 Detailed report saved to: hardcoded_values_audit.json")
    print(f"\n🎯 Next steps:")
    print(f"   1. Review the detailed report")
    print(f"   2. Update modules to use g_config values")
    print(f"   3. Remove hardcoded constants")
    print(f"   4. Test configuration changes take effect")

if __name__ == '__main__':
    main()
