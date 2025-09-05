#!/usr/bin/env python3
"""
Script to fix missing standard library includes in C header files.
"""

import os
import re
import glob

def get_required_includes(content):
    """Determine which standard library includes are needed based on content."""
    includes = set()
    
    # Check for common types and functions
    if re.search(r'\b(time_t|struct tm|clock_t)\b', content):
        includes.add('#include <time.h>')
    
    if re.search(r'\b(bool|true|false)\b', content):
        includes.add('#include <stdbool.h>')
    
    if re.search(r'\b(uint\d+_t|int\d+_t|size_t|ssize_t)\b', content):
        includes.add('#include <stdint.h>')
    
    if re.search(r'\b(pthread_t|pthread_mutex_t|pthread_cond_t|pthread_attr_t)\b', content):
        includes.add('#include <pthread.h>')
    
    if re.search(r'\b(printf|fprintf|sprintf|snprintf|fopen|fclose|FILE)\b', content):
        includes.add('#include <stdio.h>')
    
    if re.search(r'\b(malloc|free|calloc|realloc|NULL)\b', content):
        includes.add('#include <stdlib.h>')
    
    if re.search(r'\b(strlen|strcpy|strncpy|strcat|strncat|strcmp|strncmp|memcpy|memset)\b', content):
        includes.add('#include <string.h>')
    
    if re.search(r'\b(sin|cos|tan|sqrt|pow|fabs|floor|ceil|round)\b', content):
        includes.add('#include <math.h>')
    
    if re.search(r'\b(offsetof|NULL)\b', content):
        includes.add('#include <stddef.h>')
    
    return includes

def fix_header_file(filepath):
    """Fix missing includes in a header file."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
    except UnicodeDecodeError:
        print(f"⚠️  Skipping {filepath} - encoding issue")
        return False
    
    # Skip if already has include guard
    if not re.search(r'#ifndef\s+\w+_H', content):
        return False
    
    # Get required includes
    required_includes = get_required_includes(content)
    
    # Check what's already included
    existing_includes = set()
    for line in content.split('\n'):
        if re.match(r'#include\s+<[^>]+>', line.strip()):
            existing_includes.add(line.strip())
    
    # Find missing includes
    missing_includes = required_includes - existing_includes
    
    if not missing_includes:
        return False
    
    # Find the best place to insert includes (after existing includes)
    lines = content.split('\n')
    insert_pos = 0
    
    # Find the last include statement
    for i, line in enumerate(lines):
        if re.match(r'#include\s+<[^>]+>', line.strip()):
            insert_pos = i + 1
    
    # Insert missing includes
    for include in sorted(missing_includes):
        lines.insert(insert_pos, include)
        insert_pos += 1
    
    # Write back
    new_content = '\n'.join(lines)
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(new_content)
    
    print(f"✅ Fixed {filepath}: Added {len(missing_includes)} includes")
    for include in sorted(missing_includes):
        print(f"   + {include}")
    
    return True

def main():
    """Main function."""
    print("🔧 Fixing missing standard library includes in C header files...")
    
    # Find all C header files
    header_files = []
    for root, dirs, files in os.walk('src/c'):
        for file in files:
            if file.endswith('.h'):
                header_files.append(os.path.join(root, file))
    
    fixed_count = 0
    for header_file in header_files:
        if fix_header_file(header_file):
            fixed_count += 1
    
    print(f"\n🎉 Fixed {fixed_count} header files")

if __name__ == '__main__':
    main()
