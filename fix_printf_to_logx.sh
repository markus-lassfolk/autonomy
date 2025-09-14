#!/bin/bash

# Systematic printf/fprintf to LOGX replacement script
# This script replaces all printf/fprintf calls with appropriate LOGX macros

echo "Starting systematic printf/fprintf to LOGX replacement..."

# Create backup directory
mkdir -p backups/printf_fix_$(date +%Y%m%d_%H%M%S)
BACKUP_DIR="backups/printf_fix_$(date +%Y%m%d_%H%M%S)"

# Function to process a single file
process_file() {
    local file="$1"
    local backup_file="$BACKUP_DIR/$(basename "$file")"
    
    echo "Processing: $file"
    
    # Create backup
    cp "$file" "$backup_file"
    
    # Apply sed transformations
    sed -i '
    # Replace fprintf(stderr, "DEBUG: ...") with LOGX_DEBUG_MSG
    s/fprintf(stderr, "DEBUG: \([^"]*\)\\n");/LOGX_DEBUG_MSG("\1");/g
    s/fprintf(stderr, "DEBUG: \([^"]*\)");/LOGX_DEBUG_MSG("\1");/g
    
    # Replace fprintf(stderr, "ERROR: ...") with LOGX_ERROR_MSG
    s/fprintf(stderr, "ERROR: \([^"]*\)\\n");/LOGX_ERROR_MSG("\1");/g
    s/fprintf(stderr, "ERROR: \([^"]*\)");/LOGX_ERROR_MSG("\1");/g
    
    # Replace fprintf(stderr, "WARNING: ...") with LOGX_WARN_MSG
    s/fprintf(stderr, "WARNING: \([^"]*\)\\n");/LOGX_WARN_MSG("\1");/g
    s/fprintf(stderr, "WARNING: \([^"]*\)");/LOGX_WARN_MSG("\1");/g
    
    # Replace fprintf(stderr, "INFO: ...") with LOGX_INFO_MSG
    s/fprintf(stderr, "INFO: \([^"]*\)\\n");/LOGX_INFO_MSG("\1");/g
    s/fprintf(stderr, "INFO: \([^"]*\)");/LOGX_INFO_MSG("\1");/g
    
    # Replace fprintf(stderr, "FATAL: ...") with LOGX_FATAL_MSG
    s/fprintf(stderr, "FATAL: \([^"]*\)\\n");/LOGX_FATAL_MSG("\1");/g
    s/fprintf(stderr, "FATAL: \([^"]*\)");/LOGX_FATAL_MSG("\1");/g
    
    # Replace fprintf(stderr, "WARN: ...") with LOGX_WARN_MSG
    s/fprintf(stderr, "WARN: \([^"]*\)\\n");/LOGX_WARN_MSG("\1");/g
    s/fprintf(stderr, "WARN: \([^"]*\)");/LOGX_WARN_MSG("\1");/g
    
    # Replace fprintf(stderr, "=== ... ===") with LOGX_FATAL_MSG (for crash dumps)
    s/fprintf(stderr, "=== \([^"]*\) ===\\n");/LOGX_FATAL_MSG("=== \1 ===");/g
    s/fprintf(stderr, "=== \([^"]*\) ===");/LOGX_FATAL_MSG("=== \1 ===");/g
    
    # Replace fprintf(stderr, "\\n=== ... ===") with LOGX_FATAL_MSG
    s/fprintf(stderr, "\\n=== \([^"]*\) ===\\n");/LOGX_FATAL_MSG("\\n=== \1 ===");/g
    s/fprintf(stderr, "\\n=== \([^"]*\) ===");/LOGX_FATAL_MSG("\\n=== \1 ===");/g
    
    # Replace fprintf(stderr, "\\n=== ... ===") with LOGX_INFO_MSG (for info sections)
    s/fprintf(stderr, "\\n=== \([^"]*\) ===\\n");/LOGX_INFO_MSG("\\n=== \1 ===");/g
    
    # Replace printf("DEBUG: ...") with LOGX_DEBUG_MSG
    s/printf("DEBUG: \([^"]*\)\\n");/LOGX_DEBUG_MSG("\1");/g
    s/printf("DEBUG: \([^"]*\)");/LOGX_DEBUG_MSG("\1");/g
    
    # Replace printf("ERROR: ...") with LOGX_ERROR_MSG
    s/printf("ERROR: \([^"]*\)\\n");/LOGX_ERROR_MSG("\1");/g
    s/printf("ERROR: \([^"]*\)");/LOGX_ERROR_MSG("\1");/g
    
    # Replace printf("WARNING: ...") with LOGX_WARN_MSG
    s/printf("WARNING: \([^"]*\)\\n");/LOGX_WARN_MSG("\1");/g
    s/printf("WARNING: \([^"]*\)");/LOGX_WARN_MSG("\1");/g
    
    # Replace printf("INFO: ...") with LOGX_INFO_MSG
    s/printf("INFO: \([^"]*\)\\n");/LOGX_INFO_MSG("\1");/g
    s/printf("INFO: \([^"]*\)");/LOGX_INFO_MSG("\1");/g
    
    # Replace printf("FATAL: ...") with LOGX_FATAL_MSG
    s/printf("FATAL: \([^"]*\)\\n");/LOGX_FATAL_MSG("\1");/g
    s/printf("FATAL: \([^"]*\)");/LOGX_FATAL_MSG("\1");/g
    
    # Replace printf("WARN: ...") with LOGX_WARN_MSG
    s/printf("WARN: \([^"]*\)\\n");/LOGX_WARN_MSG("\1");/g
    s/printf("WARN: \([^"]*\)");/LOGX_WARN_MSG("\1");/g
    
    # Handle fprintf with format strings and variables
    s/fprintf(stderr, "\([^"]*\)", \([^)]*\));/LOGX_DEBUG_MSG("\1", \2);/g
    
    # Handle printf with format strings and variables
    s/printf("\([^"]*\)", \([^)]*\));/LOGX_DEBUG_MSG("\1", \2);/g
    
    # Remove fflush(stderr) calls that are no longer needed
    s/fflush(stderr);//g
    
    # Clean up any double newlines that might have been created
    s/LOGX_DEBUG_MSG("\\n\\n/LOGX_DEBUG_MSG("\\n/g
    s/LOGX_ERROR_MSG("\\n\\n/LOGX_ERROR_MSG("\\n/g
    s/LOGX_WARN_MSG("\\n\\n/LOGX_WARN_MSG("\\n/g
    s/LOGX_INFO_MSG("\\n\\n/LOGX_INFO_MSG("\\n/g
    s/LOGX_FATAL_MSG("\\n\\n/LOGX_FATAL_MSG("\\n/g
    ' "$file"
    
    # Check if file was modified
    if ! diff -q "$file" "$backup_file" > /dev/null; then
        echo "  ✓ Modified: $file"
    else
        echo "  - No changes: $file"
        rm "$backup_file"  # Remove backup if no changes
    fi
}

# Find all C files with printf/fprintf
echo "Finding files with printf/fprintf calls..."
FILES=$(find src/c/autonomy-daemon -name "*.c" -exec grep -l "printf\|fprintf" {} \;)

# Process each file
TOTAL_FILES=$(echo "$FILES" | wc -l)
CURRENT=0

echo "Processing $TOTAL_FILES files..."

for file in $FILES; do
    CURRENT=$((CURRENT + 1))
    echo "[$CURRENT/$TOTAL_FILES] Processing: $file"
    process_file "$file"
done

echo ""
echo "=== SUMMARY ==="
echo "Total files processed: $TOTAL_FILES"
echo "Backups created in: $BACKUP_DIR"
echo ""
echo "Files that were modified:"
find "$BACKUP_DIR" -name "*.c" | while read backup; do
    original="${backup#$BACKUP_DIR/}"
    original="src/c/autonomy-daemon/$(find src/c/autonomy-daemon -name "$(basename "$original")" | head -1 | sed 's|src/c/autonomy-daemon/||')"
    if [ -f "$original" ] && ! diff -q "$original" "$backup" > /dev/null; then
        echo "  ✓ $original"
    fi
done

echo ""
echo "=== NEXT STEPS ==="
echo "1. Review the changes in the modified files"
echo "2. Test compilation to ensure no syntax errors"
echo "3. If issues found, restore from backup: cp $BACKUP_DIR/* src/c/autonomy-daemon/"
echo "4. If successful, remove backup directory: rm -rf $BACKUP_DIR"
echo ""
echo "Replacement completed!"
