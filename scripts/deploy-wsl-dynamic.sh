#!/bin/bash

# Dynamic WSL Deployment Script for Autonomy Project
# This script dynamically detects the current drive and builds paths for WSL deployment

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration
WSL_NAME="${WSL_NAME:-rutos-openwrt-test}"
ACTION="${ACTION:-menu}"
PLATFORM="${PLATFORM:-both}"  # openwrt, rutos, or both

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

log_deploy() {
    echo -e "${CYAN}[DEPLOY]${NC} $1"
}

# Get script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Dynamic path detection
get_dynamic_paths() {
    log_info "Detecting dynamic paths..."
    
    # Get current working directory
    CURRENT_DIR=$(pwd)
    
    # Extract drive letter from current directory (if on Windows)
    if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
        # On Windows with Git Bash or similar
        CURRENT_DRIVE=$(echo "$CURRENT_DIR" | sed 's/^\([A-Za-z]\):.*/\1/')
        WSL_MOUNT_PATH="/mnt/${CURRENT_DRIVE,,}"
    else
        # On Linux/WSL, try to detect from /mnt
        if [[ "$CURRENT_DIR" =~ ^/mnt/([a-z])/ ]]; then
            CURRENT_DRIVE="${BASH_REMATCH[1]}"
            WSL_MOUNT_PATH="/mnt/$CURRENT_DRIVE"
        else
            # Fallback for Linux without /mnt
            CURRENT_DRIVE="c"
            WSL_MOUNT_PATH="/mnt/c"
        fi
    fi
    
    # Build paths for both platforms
    BUILD_DIR_OPENWRT="$PROJECT_ROOT/build-openwrt"
    PACKAGE_DIR_OPENWRT="$PROJECT_ROOT/build-openwrt/packages"
    BUILD_DIR_RUTOS="$PROJECT_ROOT/build"
    PACKAGE_DIR_RUTOS="$PROJECT_ROOT/build"
    
    PATHS=(
        "PROJECT_ROOT=$PROJECT_ROOT"
        "CURRENT_DRIVE=$CURRENT_DRIVE"
        "WSL_MOUNT_PATH=$WSL_MOUNT_PATH"
        "BUILD_DIR_OPENWRT=$BUILD_DIR_OPENWRT"
        "PACKAGE_DIR_OPENWRT=$PACKAGE_DIR_OPENWRT"
        "BUILD_DIR_RUTOS=$BUILD_DIR_RUTOS"
        "PACKAGE_DIR_RUTOS=$PACKAGE_DIR_RUTOS"
        "WSL_WORKSPACE=/workspace"
        "WSL_PACKAGE_DIR=/tmp/packages"
    )
    
    log_success "Dynamic paths detected:"
    for path in "${PATHS[@]}"; do
        echo "  $path" | sed 's/=/ = /'
    done
    
    # Export paths for use in functions
    for path in "${PATHS[@]}"; do
        export "$path"
    done
}

# Convert Windows path to WSL path
convert_to_wsl_path() {
    local windows_path="$1"
    local current_drive="$2"
    local wsl_mount_path="$3"
    
    # Remove drive letter and convert backslashes
    local path_without_drive=$(echo "$windows_path" | sed "s/^${current_drive}://")
    local wsl_path=$(echo "$path_without_drive" | sed 's/\\/\//g')
    
    echo "$wsl_mount_path$wsl_path"
}

# Check if WSL instance exists
check_wsl_instance() {
    local instance_name="$1"
    
    if wsl -l -v 2>/dev/null | grep -q "$instance_name"; then
        return 0
    else
        return 1
    fi
}

# Build packages dynamically for both OpenWrt and RUTOS
build_packages() {
    log_info "Building packages with dynamic paths..."
    
    local all_packages=()
    local found_packages=false
    
    # Check OpenWrt packages
    if [[ "$PLATFORM" == "openwrt" || "$PLATFORM" == "both" ]]; then
        log_info "Checking OpenWrt packages..."
        if [[ -d "$BUILD_DIR_OPENWRT" ]]; then
            local openwrt_pattern="$PACKAGE_DIR_OPENWRT/autonomy_*.ipk"
            local openwrt_packages=($openwrt_pattern)
            
            if [[ ${#openwrt_packages[@]} -gt 0 && -f "${openwrt_packages[0]}" ]]; then
                log_success "Found ${#openwrt_packages[@]} OpenWrt package(s):"
                for package in "${openwrt_packages[@]}"; do
                    echo "  $(basename "$package") (OpenWrt)"
                    all_packages+=("$package")
                done
                found_packages=true
            else
                log_warning "No OpenWrt packages found in: $PACKAGE_DIR_OPENWRT"
            fi
        else
            log_warning "OpenWrt build directory not found: $BUILD_DIR_OPENWRT"
        fi
    fi
    
    # Check RUTOS packages
    if [[ "$PLATFORM" == "rutos" || "$PLATFORM" == "both" ]]; then
        log_info "Checking RUTOS packages..."
        if [[ -d "$BUILD_DIR_RUTOS" ]]; then
            local rutos_pattern="$PACKAGE_DIR_RUTOS/autonomy_*.ipk"
            local rutos_packages=($rutos_pattern)
            local luci_pattern="$PACKAGE_DIR_RUTOS/luci-app-autonomy_*.ipk"
            local luci_packages=($luci_pattern)
            
            # Add main RUTOS packages
            if [[ ${#rutos_packages[@]} -gt 0 && -f "${rutos_packages[0]}" ]]; then
                log_success "Found ${#rutos_packages[@]} RUTOS package(s):"
                for package in "${rutos_packages[@]}"; do
                    echo "  $(basename "$package") (RUTOS)"
                    all_packages+=("$package")
                done
                found_packages=true
            fi
            
            # Add LuCI packages
            if [[ ${#luci_packages[@]} -gt 0 && -f "${luci_packages[0]}" ]]; then
                log_success "Found ${#luci_packages[@]} LuCI package(s):"
                for package in "${luci_packages[@]}"; do
                    echo "  $(basename "$package") (LuCI)"
                    all_packages+=("$package")
                done
                found_packages=true
            fi
            
            if [[ ${#rutos_packages[@]} -eq 0 && ${#luci_packages[@]} -eq 0 ]]; then
                log_warning "No RUTOS packages found in: $PACKAGE_DIR_RUTOS"
            fi
        else
            log_warning "RUTOS build directory not found: $BUILD_DIR_RUTOS"
        fi
    fi
    
    if [[ "$found_packages" == false ]]; then
        log_error "No packages found for platform: $PLATFORM"
        log_info "Please build packages first:"
        log_info "  OpenWrt: ./test/build-opkg-package.sh"
        log_info "  RUTOS: ./build-rutos-package.ps1"
        return 1
    fi
    
    # Export packages array
    PACKAGES=("${all_packages[@]}")
    log_success "Total packages to deploy: ${#PACKAGES[@]}"
}

# Copy packages to WSL
copy_packages_to_wsl() {
    log_info "Copying packages to WSL..."
    
    if ! check_wsl_instance "$WSL_NAME"; then
        log_error "WSL instance '$WSL_NAME' not found"
        log_info "Please create the WSL instance first using: ./test/setup-virtual-rutos-openwrt-final.ps1"
        return 1
    fi
    
    # Create package directory in WSL
    log_info "Creating package directory in WSL..."
    wsl -d "$WSL_NAME" -e bash -c "mkdir -p $WSL_PACKAGE_DIR"
    
    # Copy each package to WSL
    for package in "${PACKAGES[@]}"; do
        local package_name=$(basename "$package")
        local source_path="$package"
        
        log_info "Copying $package_name to WSL..."
        echo "  Source: $source_path"
        echo "  WSL Dest: $WSL_PACKAGE_DIR/$package_name"
        
        # Convert Windows path to WSL path
        local wsl_source_path=$(convert_to_wsl_path "$source_path" "$CURRENT_DRIVE" "$WSL_MOUNT_PATH")
        local wsl_dest_path="$WSL_PACKAGE_DIR/$package_name"
        
        echo "  WSL Source: $wsl_source_path"
        
        # Copy using WSL cp command
        if wsl -d "$WSL_NAME" -e bash -c "cp '$wsl_source_path' '$wsl_dest_path'" 2>/dev/null; then
            log_success "Successfully copied $package_name"
        else
            log_error "Failed to copy $package_name"
            return 1
        fi
    done
    
    log_success "All packages copied to WSL successfully"
}

# Install packages in WSL
install_packages_in_wsl() {
    log_info "Installing packages in WSL..."
    
    # List packages in WSL
    log_info "Packages available in WSL:"
    wsl -d "$WSL_NAME" -e bash -c "ls -la $WSL_PACKAGE_DIR/*.ipk 2>/dev/null || echo 'No packages found'"
    
    # Install each package
    for package in "${PACKAGES[@]}"; do
        local package_name=$(basename "$package")
        local wsl_package_path="$WSL_PACKAGE_DIR/$package_name"
        
        log_info "Installing $package_name..."
        
        # Install using opkg
        local install_result
        install_result=$(wsl -d "$WSL_NAME" -e bash -c "opkg install '$wsl_package_path' 2>&1")
        
        if [[ $? -eq 0 ]]; then
            log_success "Successfully installed $package_name"
        else
            log_warning "Installation of $package_name may have failed:"
            echo "$install_result"
        fi
    done
    
    log_success "Package installation completed"
}

# Test installed packages
test_installed_packages() {
    log_info "Testing installed packages..."
    
    # Test basic functionality
    local tests=(
        "Binary Exists:which autonomysysmgmt"
        "Service Script:ls -la /etc/init.d/autonomy"
        "Configuration:ls -la /etc/config/autonomy"
        "LuCI Controller:ls -la /usr/lib/lua/luci/controller/autonomy.lua"
        "Service Status:/etc/init.d/autonomy status"
    )
    
    for test in "${tests[@]}"; do
        local test_name="${test%%:*}"
        local test_command="${test#*:}"
        
        log_info "Testing: $test_name"
        local result
        result=$(wsl -d "$WSL_NAME" -e bash -c "$test_command 2>&1")
        
        if [[ $? -eq 0 ]]; then
            log_success "$test_name: PASS"
            echo "  $result"
        else
            log_warning "$test_name: FAIL"
            echo "  $result"
        fi
    done
    
    log_success "Package testing completed"
}

# Start service in WSL
start_service_in_wsl() {
    log_info "Starting autonomy service in WSL..."
    
    # Enable service to start on boot
    log_info "Enabling service to start on boot..."
    local enable_result
    enable_result=$(wsl -d "$WSL_NAME" -e bash -c "/etc/init.d/autonomy enable 2>&1")
    if [[ $? -eq 0 ]]; then
        log_success "Service enabled for auto-start"
    else
        log_warning "Service enable may have failed: $enable_result"
    fi
    
    # Start the service
    local start_result
    start_result=$(wsl -d "$WSL_NAME" -e bash -c "/etc/init.d/autonomy start 2>&1")
    
    if [[ $? -eq 0 ]]; then
        log_success "Service started successfully"
        echo "$start_result"
    else
        log_warning "Service start may have failed:"
        echo "$start_result"
    fi
    
    # Check service status
    log_info "Checking service status..."
    local status_result
    status_result=$(wsl -d "$WSL_NAME" -e bash -c "/etc/init.d/autonomy status 2>&1")
    echo "$status_result"
    
    # Check if process is running
    local process_result
    process_result=$(wsl -d "$WSL_NAME" -e bash -c "pgrep autonomysysmgmt 2>/dev/null || echo 'Process not found'")
    if [[ "$process_result" != "Process not found" ]]; then
        log_success "Autonomy process is running (PID: $process_result)"
    else
        log_warning "Autonomy process is not running"
    fi
}

# Deploy all-in-one function
deploy_all() {
    log_deploy "Starting complete deployment process..."
    
    # Get dynamic paths
    get_dynamic_paths
    
    # Build packages
    if ! build_packages; then
        return 1
    fi
    
    # Copy packages to WSL
    if ! copy_packages_to_wsl; then
        return 1
    fi
    
    # Install packages in WSL
    if ! install_packages_in_wsl; then
        return 1
    fi
    
    # Test installed packages
    test_installed_packages
    
    # Start service
    start_service_in_wsl
    
    log_success "Complete deployment process finished!"
}

# Show menu
show_menu() {
    echo ""
    echo "Dynamic WSL Deployment Options:"
    echo "=============================="
    echo "Platform: $PLATFORM (openwrt, rutos, or both)"
    echo ""
    echo "1. Deploy All (Complete Process)"
    echo "   - Detects current drive and builds dynamic paths"
    echo "   - Builds packages, copies to WSL, installs, and starts service"
    echo "   - Use this for complete deployment workflow"
    echo ""
    echo "2. Build and Copy Packages"
    echo "   - Builds packages and copies them to WSL"
    echo "   - Use this if you just want to update packages"
    echo ""
    echo "3. Install Packages in WSL"
    echo "   - Installs already copied packages in WSL"
    echo "   - Use this if packages are already in WSL"
    echo ""
    echo "4. Test Installed Packages"
    echo "   - Tests functionality of installed packages"
    echo "   - Use this to validate installation"
    echo ""
    echo "5. Start Service in WSL"
    echo "   - Starts the autonomy service in WSL"
    echo "   - Use this to start the service after installation"
    echo ""
    echo "6. Show Dynamic Paths"
    echo "   - Shows detected paths for current environment"
    echo "   - Use this to verify path detection"
    echo ""
    echo "7. Exit"
    echo "   - Exits the script"
    echo ""
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -Action)
            ACTION="$2"
            shift 2
            ;;
        -WSLName)
            WSL_NAME="$2"
            shift 2
            ;;
        -Platform)
            PLATFORM="$2"
            shift 2
            ;;
        *)
            shift
            ;;
    esac
done

# Main execution
main() {
    # If action is provided, run it directly
    if [[ "$ACTION" != "menu" ]]; then
        get_dynamic_paths
        
        case "$ACTION" in
            "1") deploy_all ;;
            "2") 
                if build_packages; then
                    copy_packages_to_wsl
                fi
                ;;
            "3") 
                if build_packages; then
                    install_packages_in_wsl
                fi
                ;;
            "4") test_installed_packages ;;
            "5") start_service_in_wsl ;;
            "6") 
                # Paths already shown by get_dynamic_paths call above
                ;;
            *) log_error "Invalid action: $ACTION" ;;
        esac
        return
    fi

    # Interactive menu
    while true; do
        show_menu
        read -p "Select an option (1-7): " choice

        case "$choice" in
            "1") deploy_all ;;
            "2") 
                get_dynamic_paths
                if build_packages; then
                    copy_packages_to_wsl
                fi
                ;;
            "3") 
                get_dynamic_paths
                if build_packages; then
                    install_packages_in_wsl
                fi
                ;;
            "4") 
                get_dynamic_paths
                test_installed_packages 
                ;;
            "5") 
                get_dynamic_paths
                start_service_in_wsl 
                ;;
            "6") get_dynamic_paths ;;
            "7")
                log_success "Exiting..."
                exit 0
                ;;
            *)
                log_error "Invalid option. Please select 1-7."
                ;;
        esac

        echo ""
        read -p "Press Enter to continue..."
    done
}

# Run main function
main "$@"
