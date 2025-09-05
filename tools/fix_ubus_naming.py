#!/usr/bin/env python3
"""
Script to standardize ubus context parameter naming from 'ctx' to 'uctx'.
"""

import os
import re
import glob

def fix_ubus_naming(filepath):
    """Fix ubus context parameter naming in a file."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
    except UnicodeDecodeError:
        print(f"⚠️  Skipping {filepath} - encoding issue")
        return False
    
    original_content = content
    
    # Replace 'struct ubus_context *ctx' with 'struct ubus_context *uctx'
    content = re.sub(
        r'struct ubus_context \*ctx\b',
        'struct ubus_context *uctx',
        content
    )
    
    # Replace function parameter 'ctx' with 'uctx' in ubus function signatures
    # This is more complex - we need to be careful not to replace 'ctx' in other contexts
    content = re.sub(
        r'(\w+\([^)]*struct ubus_context \*uctx[^)]*),\s*ctx\b',
        r'\1, uctx',
        content
    )
    
    # Also handle the case where ctx is the first parameter after uctx
    content = re.sub(
        r'(\w+\([^)]*struct ubus_context \*uctx[^)]*),\s*ctx\b',
        r'\1, uctx',
        content
    )
    
    if content != original_content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"✅ Fixed ubus naming in {filepath}")
        return True
    
    return False

def main():
    """Main function."""
    print("🔧 Standardizing ubus context parameter naming...")
    
    # Find all C header files
    header_files = []
    for root, dirs, files in os.walk('src/c'):
        for file in files:
            if file.endswith(('.h', '.c')):
                header_files.append(os.path.join(root, file))
    
    fixed_count = 0
    for header_file in header_files:
        if fix_ubus_naming(header_file):
            fixed_count += 1
    
    print(f"\n🎉 Fixed {fixed_count} files")

if __name__ == '__main__':
    main()
