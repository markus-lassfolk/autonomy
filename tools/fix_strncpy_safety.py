#!/usr/bin/env python3
"""
Fix strncpy Safety Issues Script
================================

This script automatically fixes strncpy safety issues by adding null termination.
"""

import os
import re
import sys
from pathlib import Path

def fix_strncpy_safety_in_file(file_path):
    """Fix strncpy safety issues in a single file"""
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
        
        lines = content.split('\n')
        modified = False
        
        for i, line in enumerate(lines):
            # Look for strncpy calls that might need null termination
            if 'strncpy(' in line and 'sizeof(' in line:
                # Check if the line already has null termination
                if '\\0' in line or 'null' in line.lower():
                    continue
                
                # Extract the destination variable name
                match = re.search(r'strncpy\s*\(\s*([^,]+)\s*,', line)
                if match:
                    dest_var = match.group(1).strip()
                    
                    # Find the next line to add null termination
                    if i + 1 < len(lines):
                        next_line = lines[i + 1]
                        # Add null termination on the next line
                        indent = len(line) - len(line.lstrip())
                        null_line = ' ' * indent + f'{dest_var}[sizeof({dest_var}) - 1] = \'\\0\';'
                        
                        # Insert the null termination line
                        lines.insert(i + 1, null_line)
                        modified = True
                        print(f"  Added null termination for {dest_var} in {file_path}")
        
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
        print("Usage: python3 fix_strncpy_safety.py <directory>")
        sys.exit(1)
    
    directory = sys.argv[1]
    if not os.path.exists(directory):
        print(f"Directory {directory} does not exist")
        sys.exit(1)
    
    print(f"Fixing strncpy safety issues in {directory}")
    
    # Find all C files
    c_files = []
    for root, dirs, files in os.walk(directory):
        for file in files:
            if file.endswith('.c'):
                c_files.append(os.path.join(root, file))
    
    print(f"Found {len(c_files)} C files")
    
    modified_count = 0
    for file_path in c_files:
        if fix_strncpy_safety_in_file(file_path):
            modified_count += 1
    
    print(f"Modified {modified_count} files")

if __name__ == "__main__":
    main()
