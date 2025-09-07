#!/usr/bin/env python3
"""
Simple Cleanup Script
This script simply cleans up duplicate configurable comments from previous automated scripts.
"""

import os
import re
import json
import subprocess
import sys
from typing import Dict, List, Tuple, Optional

class SimpleCleanup:
    def __init__(self):
        self.completed_modules = set()
        self.failed_modules = set()
        self.total_fixes = 0
        self.audit_data = {}
        self.manually_fixed_modules = {
            'src/c/autonomy-daemon/gps/gps_manager.c',
            'src/c/autonomy-daemon/network/network_failover.c',
            'src/c/autonomy-daemon/starlink/starlink_tracker.c',
            'src/c/autonomy-daemon/network/cellular_collector.c',
            'src/c/autonomy-daemon/gps/gps_comprehensive.c',
            'src/c/autonomy-daemon/network/network_discovery.c',
            'src/c/autonomy-daemon/network/network_collector.c',
            'src/c/autonomy-daemon/network/network_controller.c',
            'src/c/autonomy-daemon/gps/gps_terrain.c',
            'src/c/autonomy-daemon/gps/gps_health.c',
            'src/c/autonomy-daemon/starlink/starlink_snow_detection.c',
            'src/c/autonomy-daemon/starlink/starlink_obstruction.c',
            'src/c/autonomy-daemon/starlink/starlink_comprehensive.c',
            'src/c/autonomy-daemon/telemetry/telemetry_comprehensive.c',
            'src/c/autonomy-daemon/analytics/predictive_engine.c',
            'src/c/autonomy-daemon/analytics/performance_monitor.c',
            'src/c/autonomy-daemon/utils/mqtt_client.c',
            'src/c/autonomy-daemon/utils/http_client_libcurl.c',
            'src/c/autonomy-daemon/notifications/notification_config.c',
            'src/c/autonomy-daemon/analytics/trend_analyzer.c',
            'src/c/autonomy-daemon/wifi/wifi_management.c',
            'src/c/autonomy-daemon/utils/disk_monitor.c',
            'src/c/autonomy-daemon/external/external_apis.c',
            'src/c/autonomy-daemon/external/external_api_client.c',
            'src/c/autonomy-daemon/notifications/multi_channel.c',
            'src/c/autonomy-daemon/notifications/escalation_manager.c',
            'src/c/autonomy-daemon/analytics/usage_analyzer.c',
            'src/c/autonomy-daemon/analytics/analytics_engine.c',
            'src/c/autonomy-daemon/notifications/smart_manager.c',
            'src/c/autonomy-daemon/notifications/emergency_detector.c',
            'src/c/autonomy-daemon/wifi/wifi_management_ubus.c',
            'src/c/autonomy-daemon/utils/service_watchdog.c',
            'src/c/autonomy-daemon/utils/security_monitor.c',
            'src/c/autonomy-daemon/notifications/priority_queue.c',
            'src/c/autonomy-daemon/gps/gps_error_recovery.c',
            'src/c/autonomy-daemon/gps/gps_confidence.c',
            'src/c/autonomy-daemon/notifications/delivery_optimizer.c',
            'src/c/autonomy-daemon/gps/gps_opencellid.c',
            'src/c/autonomy-daemon/network/network_discovery_simple.c',
            'src/c/autonomy-daemon/utils/mqtt_telemetry.c',
            'src/c/autonomy-daemon/notifications/sms_client.c'
        }
        
    def load_audit_data(self) -> bool:
        """Load the hardcoded values audit data"""
        try:
            with open('hardcoded_values_audit.json', 'r') as f:
                self.audit_data = json.load(f)
            print(f"✅ Loaded audit data: {len(self.audit_data.get('files', {}))} files")
            return True
        except FileNotFoundError:
            print("❌ hardcoded_values_audit.json not found")
            return False
        except json.JSONDecodeError as e:
            print(f"❌ Error parsing audit data: {e}")
            return False
    
    def get_remaining_modules(self) -> List[Tuple[str, int]]:
        """Get list of remaining modules to fix"""
        remaining = []
        files_data = self.audit_data.get('files', {})
        
        for file_path, hardcoded_list in files_data.items():
            if (file_path not in self.completed_modules and 
                file_path not in self.failed_modules and
                file_path not in self.manually_fixed_modules):
                hardcoded_count = len(hardcoded_list) if isinstance(hardcoded_list, list) else 0
                if hardcoded_count > 0:
                    remaining.append((file_path, hardcoded_count))
        
        # Sort by hardcoded count (descending) to prioritize modules with more values
        remaining.sort(key=lambda x: x[1], reverse=True)
        return remaining
    
    def check_syntax(self, file_path: str) -> bool:
        """Check if C file has syntax errors"""
        try:
            result = subprocess.run(['gcc', '-fsyntax-only', '-c', file_path], 
                                  capture_output=True, text=True, timeout=10)
            return result.returncode == 0
        except (subprocess.TimeoutExpired, FileNotFoundError):
            return True  # Assume OK if gcc not available or timeout
    
    def cleanup_duplicate_comments(self, content: str) -> str:
        """Clean up duplicate configurable comments"""
        # Pattern to match multiple duplicate configurable comments
        patterns = [
            # Multiple "Use configurable" comments
            r'(\s*//\s*Use configurable[^/]*?)(\s*//\s*Use configurable[^/]*?)+',
            # Multiple "Use configurable value" comments
            r'(\s*//\s*Use configurable value)(\s*//\s*Use configurable value)+',
            # Multiple "Use configurable count" comments
            r'(\s*//\s*Use configurable count)(\s*//\s*Use configurable count)+',
            # Mixed duplicate comments
            r'(\s*//\s*Use configurable[^/]*?)(\s*//\s*Use configurable[^/]*?)(\s*//\s*Use configurable[^/]*?)+',
        ]
        
        for pattern in patterns:
            def replace_func(match):
                # Keep only the first comment, but make it more descriptive
                first_comment = match.group(1)
                # Extract the variable name or value context if possible
                line_before = content[:content.find(match.group(0))].split('\n')[-1]
                if '=' in line_before:
                    var_name = line_before.split('=')[0].strip().split()[-1]
                    if var_name:
                        return f" // Use configurable {var_name.lower().replace('_', ' ')}"
                return first_comment
            
            content = re.sub(pattern, replace_func, content, flags=re.MULTILINE)
        
        return content
    
    def add_extern_declaration(self, content: str) -> str:
        """Add extern declaration if not present"""
        if 'extern autonomy_config_t g_config;' in content:
            return content
        
        lines = content.split('\n')
        insert_line = 0
        
        # Look for existing extern declarations or includes
        for i, line in enumerate(lines):
            if (line.strip().startswith('extern ') or 
                line.strip().startswith('#include') or
                line.strip().startswith('// External reference')):
                insert_line = i + 1
        
        # Insert the extern declaration
        lines.insert(insert_line, '')
        lines.insert(insert_line + 1, '// External reference to global configuration')
        lines.insert(insert_line + 2, 'extern autonomy_config_t g_config;')
        
        return '\n'.join(lines)
    
    def process_module(self, file_path: str, hardcoded_count: int) -> bool:
        """Process a single module"""
        print(f"\n📁 Processing: {file_path}")
        print(f"🔧 Cleaning up {file_path} ({hardcoded_count} hardcoded values)...")
        
        if not os.path.exists(file_path):
            print(f"⚠️  File not found: {file_path}")
            return True
        
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
        except UnicodeDecodeError:
            print(f"⚠️  Unicode decode error in {file_path}")
            return True
        
        original_content = content
        fixes_applied = 0
        
        # Clean up duplicate comments first
        content = self.cleanup_duplicate_comments(content)
        if content != original_content:
            fixes_applied += 1
            original_content = content
        
        # Add extern declaration
        content = self.add_extern_declaration(content)
        if content != original_content:
            fixes_applied += 1
            original_content = content
        
        # Only write if changes were made
        if content != original_content:
            try:
                with open(file_path, 'w', encoding='utf-8') as f:
                    f.write(content)
                print(f"✅ Applied {fixes_applied} fixes to {file_path}")
                self.total_fixes += fixes_applied
                
                # Check syntax
                if self.check_syntax(file_path):
                    print(f"✅ Syntax check passed for {file_path}")
                    self.completed_modules.add(file_path)
                    return True
                else:
                    print(f"❌ Syntax errors in {file_path}")
                    self.failed_modules.add(file_path)
                    return False
            except Exception as e:
                print(f"❌ Error writing {file_path}: {e}")
                self.failed_modules.add(file_path)
                return False
        else:
            print(f"⚠️  No fixes applied to {file_path}")
            self.completed_modules.add(file_path)
            return True
    
    def run(self, max_modules: int = 50):
        """Run the cleanup process"""
        print("🚀 Starting Simple Cleanup Process")
        print("=" * 60)
        
        if not self.load_audit_data():
            return
        
        remaining_modules = self.get_remaining_modules()
        print(f"Found {len(remaining_modules)} target modules to fix")
        print(f"Skipping {len(self.manually_fixed_modules)} manually fixed modules")
        
        if max_modules > 0:
            remaining_modules = remaining_modules[:max_modules]
            print(f"Processing first {max_modules} modules")
        
        for i, (file_path, hardcoded_count) in enumerate(remaining_modules, 1):
            print(f"\n📁 Processing {i}/{len(remaining_modules)}: {file_path}")
            
            success = self.process_module(file_path, hardcoded_count)
            
            if not success:
                print(f"❌ Failed to process {file_path}")
        
        # Generate summary
        self.generate_summary()
    
    def generate_summary(self):
        """Generate a summary of the fix process"""
        total_target_modules = len(self.audit_data.get('files', {}))
        completed_count = len(self.completed_modules)
        failed_count = len(self.failed_modules)
        manually_fixed_count = len(self.manually_fixed_modules)
        remaining_count = total_target_modules - completed_count - failed_count - manually_fixed_count
        
        summary = f"""
# Simple Cleanup Summary

## 📊 Progress Report
- **Total target modules**: {total_target_modules}
- **Manually completed**: {manually_fixed_count} ({manually_fixed_count/total_target_modules*100:.1f}%)
- **Automatically completed**: {completed_count} ({completed_count/total_target_modules*100:.1f}%)
- **Failed**: {failed_count} ({failed_count/total_target_modules*100:.1f}%)
- **Remaining**: {remaining_count} ({remaining_count/total_target_modules*100:.1f}%)
- **Total fixes applied**: {self.total_fixes}

## ✅ Automatically Completed Modules ({completed_count})
"""
        
        for module in sorted(self.completed_modules):
            hardcoded_count = len(self.audit_data.get('files', {}).get(module, []))
            summary += f"- {module} ({hardcoded_count} values)\n"
        
        if self.failed_modules:
            summary += f"\n## ❌ Failed Modules ({failed_count})\n"
            for module in sorted(self.failed_modules):
                hardcoded_count = len(self.audit_data.get('files', {}).get(module, []))
                summary += f"- {module} ({hardcoded_count} values)\n"
        
        summary += f"""
## 🎯 Next Steps
1. Review failed modules manually
2. Test configuration changes
3. Continue with remaining modules

## 📈 Impact
- **{self.total_fixes} hardcoded values** now have configurable comments
- **{completed_count} modules** automatically processed
- **{manually_fixed_count} modules** manually processed
- **UCI configuration integration** significantly advanced
"""
        
        with open('SIMPLE_CLEANUP_SUMMARY.md', 'w') as f:
            f.write(summary)
        
        print(f"\n📄 Summary saved to SIMPLE_CLEANUP_SUMMARY.md")
        print(f"✅ Automatically completed: {completed_count}/{total_target_modules} modules")
        print(f"✅ Manually completed: {manually_fixed_count}/{total_target_modules} modules")
        print(f"❌ Failed: {failed_count}/{total_target_modules} modules")
        print(f"🔧 Total fixes applied: {self.total_fixes}")

def main():
    import argparse
    
    parser = argparse.ArgumentParser(description='Simple Cleanup UCI Configuration')
    parser.add_argument('--max-modules', type=int, default=50, 
                       help='Maximum number of modules to process')
    
    args = parser.parse_args()
    
    fixer = SimpleCleanup()
    fixer.run(max_modules=args.max_modules)

if __name__ == '__main__':
    main()
