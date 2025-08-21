#!/bin/bash

# Security check script for file permissions and secret scanning
# Usage: ./scripts/check-security.sh [file_list] [options]

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Parse arguments
AUTO_FIX_CHMOD=false
FILE_LIST=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --auto-fix-chmod)
            AUTO_FIX_CHMOD=true
            shift
            ;;
        *)
            if [ -z "$FILE_LIST" ]; then
                FILE_LIST="$1"
            fi
            shift
            ;;
    esac
done

# Function to check file permissions
check_file_permissions() {
    local file="$1"
    
    if [ ! -f "$file" ]; then
        log_warning "File not found: $file"
        return 0
    fi
    
    # Check if it's a shell script
    if [[ "$file" == *.sh ]] || [[ "$file" == *.bash ]]; then
        if [ ! -x "$file" ]; then
            log_warning "Shell script not executable: $file"
            if [ "$AUTO_FIX_CHMOD" = true ]; then
                chmod +x "$file"
                log_success "Fixed permissions for: $file"
            fi
        else
            log_success "Shell script permissions OK: $file"
        fi
    fi
    
    # Check for sensitive files
    if [[ "$file" == *".env"* ]] || [[ "$file" == *"secret"* ]] || [[ "$file" == *"password"* ]]; then
        log_warning "Potentially sensitive file detected: $file"
    fi
}

# Function to scan for secrets
scan_for_secrets() {
    local file="$1"
    
    if [ ! -f "$file" ]; then
        return 0
    fi
    
    # Basic secret patterns
    local patterns=(
        "password.*=.*['\"]"
        "secret.*=.*['\"]"
        "token.*=.*['\"]"
        "key.*=.*['\"]"
        "api_key.*=.*['\"]"
        "private_key.*=.*['\"]"
    )
    
    for pattern in "${patterns[@]}"; do
        if grep -q -i "$pattern" "$file" 2>/dev/null; then
            log_warning "Potential secret found in $file (pattern: $pattern)"
        fi
    done
}

# Main execution
main() {
    log_info "Starting security checks..."
    
    # If no file list provided, check common script locations
    if [ -z "$FILE_LIST" ]; then
        log_info "No file list provided, checking common script locations..."
        
        # Check scripts directory
        if [ -d "scripts" ]; then
            find scripts -name "*.sh" -o -name "*.bash" | while read -r file; do
                check_file_permissions "$file"
                scan_for_secrets "$file"
            done
        fi
        
        # Check package files
        if [ -d "package" ]; then
            find package -name "*.sh" -o -name "*.bash" | while read -r file; do
                check_file_permissions "$file"
                scan_for_secrets "$file"
            done
        fi
    else
        log_info "Checking provided file list..."
        echo "$FILE_LIST" | while read -r file; do
            if [ -n "$file" ]; then
                check_file_permissions "$file"
                scan_for_secrets "$file"
            fi
        done
    fi
    
    log_success "Security checks completed"
}

# Run main function
main "$@"
