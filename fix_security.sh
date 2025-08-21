#!/bin/bash

# Fix shell injection vulnerabilities in GitHub Actions workflows

# Function to fix shell injection in a file
fix_shell_injection() {
    local file="$1"
    echo "Fixing shell injection in $file"
    
    # Create backup
    cp "$file" "$file.backup"
    
    # Fix github.event.inputs patterns
    sed -i "s/\${{ github\.event\.inputs\.\([^}]*\) }}/\${{ env.\1_INPUT }}/g" "$file"
    
    # Add env sections where needed
    if grep -q "github\.event\.inputs" "$file"; then
        echo "  env:" >> "$file.tmp"
        echo "    BENCHMARK_TYPE_INPUT: \${{ github.event.inputs.benchmark_type || 
all }}" >> "$file.tmp"
        echo "    TEST_TYPE_INPUT: \${{ github.event.inputs.test_type || full }}" >> "$file.tmp"
        echo "    TEST_PAYLOAD_INPUT: \${{ github.event.inputs.test_payload }}" >> "$file.tmp"
        echo "    UPDATE_TYPE_INPUT: \${{ github.event.inputs.update_type || all }}" >> "$file.tmp"
    fi
}

# Fix all workflow files
for file in .github/workflows/*.yml; do
    if [ -f "$file" ]; then
        fix_shell_injection "$file"
    fi
done

echo "Shell injection fixes completed"
