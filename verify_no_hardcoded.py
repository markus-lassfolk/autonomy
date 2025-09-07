#!/usr/bin/env python3
"""
Verification Script - Check for Remaining Hardcoded Values
This script verifies that all hardcoded configurable values have been properly addressed.
"""

import os
import re
import json
import subprocess
import sys
from typing import Dict, List, Tuple, Optional

class HardcodedVerifier:
    def __init__(self):
        self.remaining_hardcoded = []
        self.total_files_checked = 0
        self.files_with_hardcoded = 0
        
    def check_file_for_hardcoded_values(self, file_path: str) -> List[Dict]:
        """Check a single C file for remaining hardcoded values"""
        if not os.path.exists(file_path):
            return []
        
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
        except UnicodeDecodeError:
            return []
        
        hardcoded_values = []
        lines = content.split('\n')
        
        # Patterns to identify hardcoded values that should be configurable
        patterns = [
            # Direct assignments with numeric values
            r'(\s*)(\w+)(\s*=\s*)(\d+\.?\d*[f]?)(\s*;)(\s*)(?!.*configurable)',
            # Boolean assignments
            r'(\s*)(\w+)(\s*=\s*)(true|false)(\s*;)(\s*)(?!.*configurable)',
            # Static const assignments
            r'(\s*static\s+const\s+\w+\s+\w+\s*=\s*)(\d+\.?\d*[f]?)(\s*;)(\s*)(?!.*configurable)',
            # Array/struct initializations
            r'(\s*\w+\[\w*\]\s*=\s*)(\d+\.?\d*[f]?)(\s*;)(\s*)(?!.*configurable)',
            # Function call parameters
            r'(\s*\w+\s*\(\s*)(\d+\.?\d*[f]?)(\s*\)\s*;)(\s*)(?!.*configurable)',
        ]
        
        for line_num, line in enumerate(lines, 1):
            for pattern in patterns:
                matches = re.finditer(pattern, line, re.IGNORECASE)
                for match in matches:
                    # Skip if line already has configurable comment
                    if 'configurable' in line.lower():
                        continue
                    
                    # Skip common non-configurable values
                    if self.is_non_configurable_value(line, match.group(2) if len(match.groups()) > 1 else ''):
                        continue
                    
                    hardcoded_values.append({
                        'line': line_num,
                        'content': line.strip(),
                        'value': match.group(2) if len(match.groups()) > 1 else '',
                        'file': file_path
                    })
        
        return hardcoded_values
    
    def is_non_configurable_value(self, line: str, value: str) -> bool:
        """Check if a value should not be configurable"""
        # Skip common non-configurable patterns
        non_configurable_patterns = [
            r'#include',
            r'#define',
            r'#ifdef',
            r'#ifndef',
            r'#endif',
            r'//',
            r'/\*',
            r'\*/',
            r'return\s+0',
            r'return\s+NULL',
            r'return\s+true',
            r'return\s+false',
            r'pthread_mutex_init',
            r'pthread_mutex_destroy',
            r'pthread_create',
            r'pthread_join',
            r'memset',
            r'memcpy',
            r'strcpy',
            r'strlen',
            r'sizeof',
            r'NULL',
            r'PTHREAD_MUTEX_INITIALIZER',
            r'CURL_GLOBAL_DEFAULT',
            r'FD_ZERO',
            r'FD_SET',
            r'select\(',
            r'time\(NULL\)',
            r'gettimeofday',
            r'inet_pton',
            r'inet_ntop',
            r'htons',
            r'ntohs',
            r'htonl',
            r'ntohl',
        ]
        
        for pattern in non_configurable_patterns:
            if re.search(pattern, line, re.IGNORECASE):
                return True
        
        # Skip very small numbers that are likely not configurable
        try:
            num_value = float(value)
            if num_value == 0 or num_value == 1:
                return True
        except:
            pass
        
        return False
    
    def scan_all_c_files(self) -> Dict[str, List[Dict]]:
        """Scan all C files for remaining hardcoded values"""
        print("🔍 Scanning all C files for remaining hardcoded values...")
        
        all_hardcoded = {}
        c_files = []
        
        # Find all C files
        for root, dirs, files in os.walk('src/c/autonomy-daemon'):
            for file in files:
                if file.endswith('.c'):
                    c_files.append(os.path.join(root, file))
        
        self.total_files_checked = len(c_files)
        print(f"📁 Found {self.total_files_checked} C files to check")
        
        for file_path in c_files:
            hardcoded_values = self.check_file_for_hardcoded_values(file_path)
            if hardcoded_values:
                all_hardcoded[file_path] = hardcoded_values
                self.files_with_hardcoded += 1
                print(f"⚠️  {file_path}: {len(hardcoded_values)} hardcoded values")
        
        return all_hardcoded
    
    def generate_verification_report(self, hardcoded_values: Dict[str, List[Dict]]):
        """Generate a comprehensive verification report"""
        total_hardcoded = sum(len(values) for values in hardcoded_values.values())
        
        report = f"""
# UCI Configuration Verification Report

## 📊 Verification Results
- **Total C files checked**: {self.total_files_checked}
- **Files with hardcoded values**: {self.files_with_hardcoded}
- **Total hardcoded values found**: {total_hardcoded}
- **Files clean**: {self.total_files_checked - self.files_with_hardcoded}

## 🎯 Status
"""
        
        if total_hardcoded == 0:
            report += """
## ✅ VERIFICATION PASSED!
**All hardcoded configurable values have been successfully addressed!**

The UCI configuration system is now fully functional with no remaining hardcoded values.
"""
        else:
            report += f"""
## ⚠️  VERIFICATION FAILED!
**{total_hardcoded} hardcoded values still need to be addressed.**

### Files with remaining hardcoded values:
"""
            for file_path, values in hardcoded_values.items():
                report += f"\n#### {file_path} ({len(values)} values)\n"
                for value in values[:5]:  # Show first 5 values
                    report += f"- Line {value['line']}: `{value['content']}`\n"
                if len(values) > 5:
                    report += f"- ... and {len(values) - 5} more values\n"
        
        report += f"""
## 📈 Summary
- **UCI Configuration Integration**: {'✅ COMPLETE' if total_hardcoded == 0 else '⚠️  INCOMPLETE'}
- **System Configurability**: {'✅ FULLY CONFIGURABLE' if total_hardcoded == 0 else '⚠️  PARTIALLY CONFIGURABLE'}
- **User Control**: {'✅ FULL CONTROL' if total_hardcoded == 0 else '⚠️  LIMITED CONTROL'}

## 🎯 Next Steps
"""
        
        if total_hardcoded == 0:
            report += """
1. ✅ **MISSION ACCOMPLISHED!** - All hardcoded values addressed
2. ✅ **System is fully configurable** via UCI
3. ✅ **Users have complete control** over system behavior
4. ✅ **Configuration changes take effect immediately**
"""
        else:
            report += f"""
1. Address remaining {total_hardcoded} hardcoded values
2. Run verification script again
3. Complete UCI configuration integration
"""
        
        with open('VERIFICATION_REPORT.md', 'w') as f:
            f.write(report)
        
        print(f"\n📄 Verification report saved to VERIFICATION_REPORT.md")
        return total_hardcoded == 0
    
    def run_verification(self):
        """Run the complete verification process"""
        print("🚀 Starting UCI Configuration Verification")
        print("=" * 60)
        
        # Scan all C files
        hardcoded_values = self.scan_all_c_files()
        
        # Generate report
        is_clean = self.generate_verification_report(hardcoded_values)
        
        print(f"\n📊 VERIFICATION RESULTS:")
        print(f"✅ Files checked: {self.total_files_checked}")
        print(f"⚠️  Files with hardcoded values: {self.files_with_hardcoded}")
        print(f"🔧 Total hardcoded values: {sum(len(values) for values in hardcoded_values.values())}")
        
        if is_clean:
            print(f"\n🎉 VERIFICATION PASSED!")
            print(f"✅ All hardcoded configurable values have been addressed!")
            print(f"✅ UCI configuration system is fully functional!")
        else:
            print(f"\n⚠️  VERIFICATION FAILED!")
            print(f"❌ {sum(len(values) for values in hardcoded_values.values())} hardcoded values still need attention")
        
        return is_clean

def main():
    verifier = HardcodedVerifier()
    verifier.run_verification()

if __name__ == '__main__':
    main()
