#!/bin/bash

# Build script for LuCI autonomy application
# This script builds the LuCI package for OpenWrt

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
LUCI_DIR="$SCRIPT_DIR/luci-app-autonomy"
BUILD_DIR="$SCRIPT_DIR/build"
PACKAGE_NAME="luci-app-autonomy"

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check dependencies
check_dependencies() {
    print_status "Checking build dependencies..."

    local missing_deps=()

    # Check for required tools
    for tool in make tar gzip; do
        if ! command -v "$tool" &>/dev/null; then
            missing_deps+=("$tool")
        fi
    done

    if [ ${#missing_deps[@]} -ne 0 ]; then
        print_error "Missing required tools: ${missing_deps[*]}"
        print_error "Please install the missing dependencies and try again."
        exit 1
    fi

    print_success "All dependencies satisfied"
}

# Function to clean build directory
clean_build() {
    print_status "Cleaning build directory..."

    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
    fi

    mkdir -p "$BUILD_DIR"
    print_success "Build directory cleaned"
}

# Function to create package structure
create_package_structure() {
    print_status "Creating package structure..."

    # Create package directory
    local package_dir="$BUILD_DIR/$PACKAGE_NAME"
    mkdir -p "$package_dir"

    # Copy files from source
    if [ -d "$LUCI_DIR" ]; then
        cp -r "$LUCI_DIR"/* "$package_dir/"
        print_success "Package structure created"
    else
        print_error "Source directory not found: $LUCI_DIR"
        exit 1
    fi
}

# Function to create control file
create_control_file() {
    print_status "Creating package control file..."

    local control_file="$BUILD_DIR/$PACKAGE_NAME/CONTROL/control"
    mkdir -p "$(dirname "$control_file")"

    cat >"$control_file" <<EOF
Package: $PACKAGE_NAME
Version: 1.0.0-1
Depends: luci-compat, autonomyd
Section: luci
Architecture: all
Installed-Size: 1024
Description: LuCI web interface for autonomy multi-interface failover daemon
 This package provides a comprehensive web-based management interface
 for monitoring and configuring the autonomy failover system.
 .
 Features:
 - Real-time system status and control
 - Member interface monitoring
 - Telemetry data visualization
 - Configuration management
 - Log viewing and management
EOF

    print_success "Control file created"
}

# Function to create postinst script
create_postinst_script() {
    print_status "Creating post-installation script..."

    local postinst_file="$BUILD_DIR/$PACKAGE_NAME/CONTROL/postinst"

    cat >"$postinst_file" <<'EOF'
#!/bin/sh

# Post-installation script for luci-app-autonomy

# Set executable permissions
chmod +x /etc/init.d/autonomy
chmod +x /etc/hotplug.d/iface/99-autonomy

# Restart uhttpd to load new LuCI modules
/etc/init.d/uhttpd restart

# Enable autonomy service if not already enabled
if [ ! -L /etc/rc.d/S90autonomy ]; then
    /etc/init.d/autonomy enable
fi

echo "LuCI autonomy application installed successfully."
echo "Access the interface at: http://<router-ip>/cgi-bin/luci/admin/network/autonomy"
EOF

    chmod +x "$postinst_file"
    print_success "Post-installation script created"
}

# Function to create prerm script
create_prerm_script() {
    print_status "Creating pre-removal script..."

    local prerm_file="$BUILD_DIR/$PACKAGE_NAME/CONTROL/prerm"

    cat >"$prerm_file" <<'EOF'
#!/bin/sh

# Pre-removal script for luci-app-autonomy

# Stop and disable autonomy service
/etc/init.d/autonomy stop
/etc/init.d/autonomy disable

# Restart uhttpd to unload LuCI modules
/etc/init.d/uhttpd restart

echo "LuCI autonomy application removed successfully."
EOF

    chmod +x "$prerm_file"
    print_success "Pre-removal script created"
}

# Function to build the package
build_package() {
    print_status "Building package..."

    local package_dir="$BUILD_DIR/$PACKAGE_NAME"
    local ipk_file="$BUILD_DIR/${PACKAGE_NAME}_1.0.0-1_all.ipk"

    # Create data.tar.gz
    cd "$package_dir"
    tar -czf data.tar.gz --exclude=CONTROL .

    # Create control.tar.gz
    tar -czf control.tar.gz CONTROL/

    # Create debian-binary
    echo "2.0" >debian-binary

    # Create IPK file
    ar -r "$ipk_file" debian-binary control.tar.gz data.tar.gz

    # Clean up temporary files
    rm -f data.tar.gz control.tar.gz debian-binary

    print_success "Package built: $ipk_file"
}

# Function to verify package
verify_package() {
    print_status "Verifying package..."

    local ipk_file="$BUILD_DIR/${PACKAGE_NAME}_1.0.0-1_all.ipk"

    if [ ! -f "$ipk_file" ]; then
        print_error "Package file not found: $ipk_file"
        exit 1
    fi

    # Check file size
    local size=$(stat -c%s "$ipk_file")
    if [ "$size" -lt 1000 ]; then
        print_warning "Package seems too small ($size bytes)"
    else
        print_success "Package size: $size bytes"
    fi

    # List package contents
    print_status "Package contents:"
    ar -t "$ipk_file"

    print_success "Package verification completed"
}

# Function to show usage
show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -c, --clean     Clean build directory before building"
    echo "  -v, --verify    Verify the built package"
    echo "  -h, --help      Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0              Build the package"
    echo "  $0 -c           Clean and build"
    echo "  $0 -v           Build and verify"
    echo "  $0 -c -v        Clean, build, and verify"
}

# Main function
main() {
    local clean_build_flag=false
    local verify_package_flag=false

    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -c | --clean)
                clean_build_flag=true
                shift
                ;;
            -v | --verify)
                verify_package_flag=true
                shift
                ;;
            -h | --help)
                show_usage
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                show_usage
                exit 1
                ;;
        esac
    done

    print_status "Starting LuCI autonomy package build..."
    print_status "Project root: $PROJECT_ROOT"
    print_status "Build directory: $BUILD_DIR"

    # Check dependencies
    check_dependencies

    # Clean build directory if requested
    if [ "$clean_build_flag" = true ]; then
        clean_build
    else
        # Create build directory if it doesn't exist
        mkdir -p "$BUILD_DIR"
    fi

    # Build process
    create_package_structure
    create_control_file
    create_postinst_script
    create_prerm_script
    build_package

    # Verify package if requested
    if [ "$verify_package_flag" = true ]; then
        verify_package
    fi

    print_success "Build completed successfully!"
    print_status "Package location: $BUILD_DIR/${PACKAGE_NAME}_1.0.0-1_all.ipk"
    print_status "To install: opkg install $BUILD_DIR/${PACKAGE_NAME}_1.0.0-1_all.ipk"
}

# Run main function with all arguments
main "$@"
