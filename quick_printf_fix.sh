#!/bin/bash

# Quick one-liner sed command to fix printf/fprintf to LOGX
# This is a simplified version for immediate use

echo "Running quick printf/fprintf to LOGX replacement..."

# Create backup
mkdir -p backups/quick_fix_$(date +%Y%m%d_%H%M%S)
BACKUP_DIR="backups/quick_fix_$(date +%Y%m%d_%H%M%S)"

# Find and process all files
find src/c/autonomy-daemon -name "*.c" -exec grep -l "printf\|fprintf" {} \; | while read file; do
    echo "Processing: $file"
    
    # Create backup
    cp "$file" "$BACKUP_DIR/$(basename "$file")"
    
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
    s/fprintf(stderr, "WARN: \([^"]*\)\\n");/LOGX_WARN_MSG("\1");/g
    s/fprintf(stderr, "WARN: \([^"]*\)");/LOGX_WARN_MSG("\1");/g
    
    # Replace fprintf(stderr, "INFO: ...") with LOGX_INFO_MSG
    s/fprintf(stderr, "INFO: \([^"]*\)\\n");/LOGX_INFO_MSG("\1");/g
    s/fprintf(stderr, "INFO: \([^"]*\)");/LOGX_INFO_MSG("\1");/g
    
    # Replace fprintf(stderr, "FATAL: ...") with LOGX_FATAL_MSG
    s/fprintf(stderr, "FATAL: \([^"]*\)\\n");/LOGX_FATAL_MSG("\1");/g
    s/fprintf(stderr, "FATAL: \([^"]*\)");/LOGX_FATAL_MSG("\1");/g
    
    # Replace printf("DEBUG: ...") with LOGX_DEBUG_MSG
    s/printf("DEBUG: \([^"]*\)\\n");/LOGX_DEBUG_MSG("\1");/g
    s/printf("DEBUG: \([^"]*\)");/LOGX_DEBUG_MSG("\1");/g
    
    # Replace printf("ERROR: ...") with LOGX_ERROR_MSG
    s/printf("ERROR: \([^"]*\)\\n");/LOGX_ERROR_MSG("\1");/g
    s/printf("ERROR: \([^"]*\)");/LOGX_ERROR_MSG("\1");/g
    
    # Replace printf("WARNING: ...") with LOGX_WARN_MSG
    s/printf("WARNING: \([^"]*\)\\n");/LOGX_WARN_MSG("\1");/g
    s/printf("WARNING: \([^"]*\)");/LOGX_WARN_MSG("\1");/g
    s/printf("WARN: \([^"]*\)\\n");/LOGX_WARN_MSG("\1");/g
    s/printf("WARN: \([^"]*\)");/LOGX_WARN_MSG("\1");/g
    
    # Replace printf("INFO: ...") with LOGX_INFO_MSG
    s/printf("INFO: \([^"]*\)\\n");/LOGX_INFO_MSG("\1");/g
    s/printf("INFO: \([^"]*\)");/LOGX_INFO_MSG("\1");/g
    
    # Replace printf("FATAL: ...") with LOGX_FATAL_MSG
    s/printf("FATAL: \([^"]*\)\\n");/LOGX_FATAL_MSG("\1");/g
    s/printf("FATAL: \([^"]*\)");/LOGX_FATAL_MSG("\1");/g
    
    # Handle crash dump patterns
    s/fprintf(stderr, "=== \([^"]*\) ===\\n");/LOGX_FATAL_MSG("=== \1 ===");/g
    s/fprintf(stderr, "=== \([^"]*\) ===");/LOGX_FATAL_MSG("=== \1 ===");/g
    s/fprintf(stderr, "\\n=== \([^"]*\) ===\\n");/LOGX_FATAL_MSG("\\n=== \1 ===");/g
    s/fprintf(stderr, "\\n=== \([^"]*\) ===");/LOGX_FATAL_MSG("\\n=== \1 ===");/g
    
    # Remove fflush calls
    s/fflush(stderr);//g
    s/fflush(stdout);//g
    ' "$file"
    
    echo "  ✓ Processed: $file"
done

echo ""
echo "Quick replacement completed!"
echo "Backups created in: $BACKUP_DIR"
echo ""
echo "Next steps:"
echo "1. Test compilation: /mnt/wsl/SDK/build_autonomy_daemon.sh"
echo "2. If issues found, restore: cp $BACKUP_DIR/* src/c/autonomy-daemon/"
echo "3. If successful, clean up: rm -rf $BACKUP_DIR"
