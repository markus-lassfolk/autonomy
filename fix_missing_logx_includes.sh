#!/bin/bash

# Script to fix missing LOGX includes in files that use LOGX macros

echo "Fixing missing LOGX includes..."

# Function to add LOGX include to a file
add_logx_include() {
    local file="$1"
    echo "Adding LOGX include to: $file"
    
    # Check if logx.h is already included
    if grep -q "#include.*logx\.h" "$file"; then
        echo "  LOGX include already present"
        return 0
    fi
    
    # Find the best place to add the include
    if grep -q "#include.*types\.h" "$file"; then
        # Add after types.h include
        sed -i '/#include.*types\.h/a #include "../shared/logging/logx.h"' "$file"
    elif grep -q "#include.*stdio\.h" "$file"; then
        # Add after stdio.h include
        sed -i '/#include.*stdio\.h/a #include "../shared/logging/logx.h"' "$file"
    elif grep -q "#include.*stdlib\.h" "$file"; then
        # Add after stdlib.h include
        sed -i '/#include.*stdlib\.h/a #include "../shared/logging/logx.h"' "$file"
    else
        # Add after the first include
        sed -i '1a #include "../shared/logging/logx.h"' "$file"
    fi
}

# Find all files that use LOGX macros but don't have the include
find src/c/autonomy-daemon -name "*.c" -exec grep -l "LOGX_" {} \; | while read file; do
    if ! grep -q "#include.*logx\.h" "$file"; then
        add_logx_include "$file"
    fi
done

echo "LOGX include fixes completed!"
