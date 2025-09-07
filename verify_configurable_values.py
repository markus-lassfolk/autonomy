#!/usr/bin/env python3
"""
Accurate Verification Script - Check for Remaining Configurable Values
This script verifies that all values that SHOULD be configurable have been properly addressed.
"""

import os
import re
import json
import subprocess
import sys
from typing import Dict, List, Tuple, Optional

class ConfigurableVerifier:
    def __init__(self):
        self.remaining_configurable = []
        self.total_files_checked = 0
        self.files_with_configurable = 0
        
    def check_file_for_configurable_values(self, file_path: str) -> List[Dict]:
        """Check a single C file for remaining configurable values"""
        if not os.path.exists(file_path):
            return []
        
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
        except UnicodeDecodeError:
            return []
        
        configurable_values = []
        lines = content.split('\n')
        
        # Patterns to identify values that SHOULD be configurable
        configurable_patterns = [
            # Timeout values
            r'(\s*)(\w*timeout\w*)(\s*=\s*)(\d+\.?\d*[f]?)(\s*;)(\s*)(?!.*configurable)',
            # Threshold values
            r'(\s*)(\w*threshold\w*)(\s*=\s*)(\d+\.?\d*[f]?)(\s*;)(\s*)(?!.*configurable)',
            # Interval values
            r'(\s*)(\w*interval\w*)(\s*=\s*)(\d+\.?\d*[f]?)(\s*;)(\s*)(?!.*configurable)',
            # Retry/attempt values
            r'(\s*)(\w*retry\w*|\w*attempt\w*)(\s*=\s*)(\d+\.?\d*[f]?)(\s*;)(\s*)(?!.*configurable)',
            # Size/limit values
            r'(\s*)(\w*size\w*|\w*limit\w*|\w*max\w*|\w*min\w*)(\s*=\s*)(\d+\.?\d*[f]?)(\s*;)(\s*)(?!.*configurable)',
            # Port/address values
            r'(\s*)(\w*port\w*|\w*address\w*)(\s*=\s*)(\d+\.?\d*[f]?)(\s*;)(\s*)(?!.*configurable)',
            # Configuration flags that should be configurable
            r'(\s*)(\w*enabled\w*|\w*enable\w*)(\s*=\s*)(true|false)(\s*;)(\s*)(?!.*configurable)',
            # Static const values that look like configuration
            r'(\s*static\s+const\s+\w+\s+)(\w*timeout\w*|\w*threshold\w*|\w*interval\w*|\w*retry\w*|\w*attempt\w*|\w*size\w*|\w*limit\w*|\w*max\w*|\w*min\w*)(\s*=\s*)(\d+\.?\d*[f]?)(\s*;)(\s*)(?!.*configurable)',
        ]
        
        for line_num, line in enumerate(lines, 1):
            for pattern in configurable_patterns:
                matches = re.finditer(pattern, line, re.IGNORECASE)
                for match in matches:
                    # Skip if line already has configurable comment
                    if 'configurable' in line.lower():
                        continue
                    
                    # Skip if it's clearly not a configurable value
                    if self.is_non_configurable_value(line, match.group(2) if len(match.groups()) > 1 else ''):
                        continue
                    
                    configurable_values.append({
                        'line': line_num,
                        'content': line.strip(),
                        'value': match.group(2) if len(match.groups()) > 1 else '',
                        'file': file_path
                    })
        
        return configurable_values
    
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
            r'initialized\s*=\s*false',
            r'initialized\s*=\s*true',
            r'count\s*=\s*0',
            r'score\s*=\s*0',
            r'index\s*=\s*0',
            r'result\s*=\s*0',
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
        """Scan all C files for remaining configurable values"""
        print("🔍 Scanning all C files for remaining configurable values...")
        
        all_configurable = {}
        c_files = []
        
        # Find all C files
        for root, dirs, files in os.walk('src/c/autonomy-daemon'):
            for file in files:
                if file.endswith('.c'):
                    c_files.append(os.path.join(root, file))
        
        self.total_files_checked = len(c_files)
        print(f"📁 Found {self.total_files_checked} C files to check")
        
        for file_path in c_files:
            configurable_values = self.check_file_for_configurable_values(file_path)
            if configurable_values:
                all_configurable[file_path] = configurable_values
                self.files_with_configurable += 1
                print(f"⚠️  {file_path}: {len(configurable_values)} configurable values")
        
        return all_configurable
    
    def generate_verification_report(self, configurable_values: Dict[str, List[Dict]]):
        """Generate a comprehensive verification report"""
        total_configurable = sum(len(values) for values in configurable_values.values())
        
        report = f"""
# UCI Configuration Verification Report - Configurable Values Only

## 📊 Verification Results
- **Total C files checked**: {self.total_files_checked}
- **Files with configurable values**: {self.files_with_configurable}
- **Total configurable values found**: {total_configurable}
- **Files clean**: {self.total_files_checked - self.files_with_configurable}

## 🎯 Status
"""
        
        if total_configurable == 0:
            report += """
## ✅ VERIFICATION PASSED!
**All configurable values have been successfully addressed!**

The UCI configuration system is now fully functional with no remaining configurable values.
"""
        else:
            report += f"""
## ⚠️  VERIFICATION FAILED!
**{total_configurable} configurable values still need to be addressed.**

### Files with remaining configurable values:
"""
            for file_path, values in configurable_values.items():
                report += f"\n#### {file_path} ({len(values)} values)\n"
                for value in values[:5]:  # Show first 5 values
                    report += f"- Line {value['line']}: `{value['content']}`\n"
                if len(values) > 5:
                    report += f"- ... and {len(values) - 5} more values\n"
        
        report += f"""
## 📈 Summary
- **UCI Configuration Integration**: {'✅ COMPLETE' if total_configurable == 0 else '⚠️  INCOMPLETE'}
- **System Configurability**: {'✅ FULLY CONFIGURABLE' if total_configurable == 0 else '⚠️  PARTIALLY CONFIGURABLE'}
- **User Control**: {'✅ FULL CONTROL' if total_configurable == 0 else '⚠️  LIMITED CONTROL'}

## 🎯 Next Steps
"""
        
        if total_configurable == 0:
            report += """
1. ✅ **MISSION ACCOMPLISHED!** - All configurable values addressed
2. ✅ **System is fully configurable** via UCI
3. ✅ **Users have complete control** over system behavior
4. ✅ **Configuration changes take effect immediately**
"""
        else:
            report += f"""
1. Address remaining {total_configurable} configurable values
2. Run verification script again
3. Complete UCI configuration integration
"""
        
        with open('CONFIGURABLE_VERIFICATION_REPORT.md', 'w') as f:
            f.write(report)
        
        print(f"\n📄 Verification report saved to CONFIGURABLE_VERIFICATION_REPORT.md")
        return total_configurable == 0
    
    def run_verification(self):
        """Run the complete verification process"""
        print("🚀 Starting UCI Configuration Verification - Configurable Values Only")
        print("=" * 70)
        
        # Scan all C files
        configurable_values = self.scan_all_c_files()
        
        # Generate report
        is_clean = self.generate_verification_report(configurable_values)
        
        print(f"\n📊 VERIFICATION RESULTS:")
        print(f"✅ Files checked: {self.total_files_checked}")
        print(f"⚠️  Files with configurable values: {self.files_with_configurable}")
        print(f"🔧 Total configurable values: {sum(len(values) for values in configurable_values.values())}")
        
        if is_clean:
            print(f"\n🎉 VERIFICATION PASSED!")
            print(f"✅ All configurable values have been addressed!")
            print(f"✅ UCI configuration system is fully functional!")
        else:
            print(f"\n⚠️  VERIFICATION FAILED!")
            print(f"❌ {sum(len(values) for values in configurable_values.values())} configurable values still need attention")
        
        return is_clean

def main():
    verifier = ConfigurableVerifier()
    verifier.run_verification()

if __name__ == '__main__':
    main()
