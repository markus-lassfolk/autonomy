#!/bin/bash

# Script to replace all LOGX calls with printf equivalents
# This prevents crashes caused by LOGX macros

echo "🔧 Replacing all LOGX calls with printf equivalents..."

# Find all .c files in the autonomy-daemon directory
find /mnt/s/autonomy/src/c/autonomy-daemon -name "*.c" -type f | while read file; do
    echo "Processing: $file"
    
    # Create a backup
    cp "$file" "$file.bak"
    
    # Replace LOGX_DEBUG_MSG with printf
    sed -i 's/LOGX_DEBUG_MSG(/printf("DEBUG: /g' "$file"
    sed -i 's/);$/\\n");/g' "$file"
    
    # Replace LOGX_INFO_MSG with printf
    sed -i 's/LOGX_INFO_MSG(/printf("INFO: /g' "$file"
    sed -i 's/);$/\\n");/g' "$file"
    
    # Replace LOGX_ERROR_MSG with printf
    sed -i 's/LOGX_ERROR_MSG(/printf("ERROR: /g' "$file"
    sed -i 's/);$/\\n");/g' "$file"
    
    # Replace LOGX_WARN_MSG with printf
    sed -i 's/LOGX_WARN_MSG(/printf("WARN: /g' "$file"
    sed -i 's/);$/\\n");/g' "$file"
    
    # Fix any double newlines that might have been created
    sed -i 's/\\n\\n");/\\n");/g' "$file"
    
    echo "  ✅ Processed: $file"
done

echo "🎉 LOGX replacement completed!"
echo "📝 All LOGX calls have been replaced with printf equivalents"
echo "💾 Backup files (.bak) have been created for safety"
