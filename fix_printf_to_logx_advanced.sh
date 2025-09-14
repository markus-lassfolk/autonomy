#!/bin/bash

# Advanced systematic printf/fprintf to LOGX replacement script
# This script handles complex printf/fprintf patterns with better accuracy

echo "Starting advanced printf/fprintf to LOGX replacement..."

# Create backup directory
mkdir -p backups/printf_fix_advanced_$(date +%Y%m%d_%H%M%S)
BACKUP_DIR="backups/printf_fix_advanced_$(date +%Y%m%d_%H%M%S)"

# Function to add LOGX include if not present
add_logx_include() {
    local file="$1"
    
    # Check if logx.h is already included
    if ! grep -q "#include.*logx\.h" "$file"; then
        # Find the best place to add the include
        if grep -q "#include.*types\.h" "$file"; then
            # Add after types.h include
            sed -i '/#include.*types\.h/a #include "../shared/logging/logx.h"' "$file"
        elif grep -q "#include.*stdio\.h" "$file"; then
            # Add after stdio.h include
            sed -i '/#include.*stdio\.h/a #include "../shared/logging/logx.h"' "$file"
        else
            # Add at the beginning after the first include
            sed -i '1a #include "../shared/logging/logx.h"' "$file"
        fi
        echo "  ✓ Added LOGX include to $file"
    fi
}

# Function to process a single file
process_file() {
    local file="$1"
    local backup_file="$BACKUP_DIR/$(basename "$file")"
    
    echo "Processing: $file"
    
    # Create backup
    cp "$file" "$backup_file"
    
    # Add LOGX include if needed
    add_logx_include "$file"
    
    # Create a temporary file for complex replacements
    local temp_file=$(mktemp)
    cp "$file" "$temp_file"
    
    # Apply sed transformations in multiple passes for better accuracy
    
    # Pass 1: Handle simple fprintf(stderr, "LEVEL: message\n") patterns
    sed -i '
    # DEBUG messages
    s/fprintf(stderr, "DEBUG: \([^"]*\)\\n");/LOGX_DEBUG_MSG("\1");/g
    s/fprintf(stderr, "DEBUG: \([^"]*\)");/LOGX_DEBUG_MSG("\1");/g
    
    # ERROR messages  
    s/fprintf(stderr, "ERROR: \([^"]*\)\\n");/LOGX_ERROR_MSG("\1");/g
    s/fprintf(stderr, "ERROR: \([^"]*\)");/LOGX_ERROR_MSG("\1");/g
    
    # WARNING messages
    s/fprintf(stderr, "WARNING: \([^"]*\)\\n");/LOGX_WARN_MSG("\1");/g
    s/fprintf(stderr, "WARNING: \([^"]*\)");/LOGX_WARN_MSG("\1");/g
    s/fprintf(stderr, "WARN: \([^"]*\)\\n");/LOGX_WARN_MSG("\1");/g
    s/fprintf(stderr, "WARN: \([^"]*\)");/LOGX_WARN_MSG("\1");/g
    
    # INFO messages
    s/fprintf(stderr, "INFO: \([^"]*\)\\n");/LOGX_INFO_MSG("\1");/g
    s/fprintf(stderr, "INFO: \([^"]*\)");/LOGX_INFO_MSG("\1");/g
    
    # FATAL messages
    s/fprintf(stderr, "FATAL: \([^"]*\)\\n");/LOGX_FATAL_MSG("\1");/g
    s/fprintf(stderr, "FATAL: \([^"]*\)");/LOGX_FATAL_MSG("\1");/g
    ' "$temp_file"
    
    # Pass 2: Handle printf patterns
    sed -i '
    # DEBUG messages
    s/printf("DEBUG: \([^"]*\)\\n");/LOGX_DEBUG_MSG("\1");/g
    s/printf("DEBUG: \([^"]*\)");/LOGX_DEBUG_MSG("\1");/g
    
    # ERROR messages
    s/printf("ERROR: \([^"]*\)\\n");/LOGX_ERROR_MSG("\1");/g
    s/printf("ERROR: \([^"]*\)");/LOGX_ERROR_MSG("\1");/g
    
    # WARNING messages
    s/printf("WARNING: \([^"]*\)\\n");/LOGX_WARN_MSG("\1");/g
    s/printf("WARNING: \([^"]*\)");/LOGX_WARN_MSG("\1");/g
    s/printf("WARN: \([^"]*\)\\n");/LOGX_WARN_MSG("\1");/g
    s/printf("WARN: \([^"]*\)");/LOGX_WARN_MSG("\1");/g
    
    # INFO messages
    s/printf("INFO: \([^"]*\)\\n");/LOGX_INFO_MSG("\1");/g
    s/printf("INFO: \([^"]*\)");/LOGX_INFO_MSG("\1");/g
    
    # FATAL messages
    s/printf("FATAL: \([^"]*\)\\n");/LOGX_FATAL_MSG("\1");/g
    s/printf("FATAL: \([^"]*\)");/LOGX_FATAL_MSG("\1");/g
    ' "$temp_file"
    
    # Pass 3: Handle crash dump patterns (=== sections)
    sed -i '
    # Crash dump headers - use FATAL for these
    s/fprintf(stderr, "=== \([^"]*\) ===\\n");/LOGX_FATAL_MSG("=== \1 ===");/g
    s/fprintf(stderr, "=== \([^"]*\) ===");/LOGX_FATAL_MSG("=== \1 ===");/g
    s/fprintf(stderr, "\\n=== \([^"]*\) ===\\n");/LOGX_FATAL_MSG("\\n=== \1 ===");/g
    s/fprintf(stderr, "\\n=== \([^"]*\) ===");/LOGX_FATAL_MSG("\\n=== \1 ===");/g
    
    # Info sections - use INFO for these
    s/fprintf(stderr, "\\n=== \([^"]*\) ===\\n");/LOGX_INFO_MSG("\\n=== \1 ===");/g
    ' "$temp_file"
    
    # Pass 4: Handle format strings with variables (more complex patterns)
    # This is a simplified approach - complex format strings may need manual review
    sed -i '
    # Handle fprintf with format strings - default to DEBUG level
    s/fprintf(stderr, "\([^"]*\)", \([^)]*\));/LOGX_DEBUG_MSG("\1", \2);/g
    
    # Handle printf with format strings - default to DEBUG level  
    s/printf("\([^"]*\)", \([^)]*\));/LOGX_DEBUG_MSG("\1", \2);/g
    ' "$temp_file"
    
    # Pass 5: Remove fflush calls that are no longer needed
    sed -i '
    s/fflush(stderr);//g
    s/fflush(stdout);//g
    ' "$temp_file"
    
    # Pass 6: Clean up any formatting issues
    sed -i '
    # Remove double newlines
    s/LOGX_DEBUG_MSG("\\n\\n/LOGX_DEBUG_MSG("\\n/g
    s/LOGX_ERROR_MSG("\\n\\n/LOGX_ERROR_MSG("\\n/g
    s/LOGX_WARN_MSG("\\n\\n/LOGX_WARN_MSG("\\n/g
    s/LOGX_INFO_MSG("\\n\\n/LOGX_INFO_MSG("\\n/g
    s/LOGX_FATAL_MSG("\\n\\n/LOGX_FATAL_MSG("\\n/g
    
    # Clean up any trailing semicolons that might be duplicated
    s/;;/;/g
    ' "$temp_file"
    
    # Copy the processed file back
    cp "$temp_file" "$file"
    rm "$temp_file"
    
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
echo "Advanced replacement completed!"
