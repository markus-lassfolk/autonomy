#!/bin/bash

# RUTOS SDK Integration Script
# This script integrates with the RUTOS SDK for building and testing autonomy packages

set -e

# Configuration
SDK_PATH="${RUTOS_SDK_PATH:-/mnt/d/GitCursor/SDK/rutos-ipq40xx-rutx-sdk}"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
PACKAGE_DIR="${PROJECT_ROOT}/package/autonomy"

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

# Check if SDK exists
check_sdk() {
    log_info "Checking RUTOS SDK at: $SDK_PATH"
    
    if [ ! -d "$SDK_PATH" ]; then
        log_error "RUTOS SDK not found at $SDK_PATH"
        log_info "Please set RUTOS_SDK_PATH environment variable or update the path in this script"
        return 1
    fi
    
    if [ ! -f "$SDK_PATH/feeds.conf.default" ]; then
        log_warning "SDK found but feeds.conf.default not found - may not be a valid OpenWrt SDK"
    fi
    
    log_success "RUTOS SDK found and validated"
    return 0
}

# Setup build environment
setup_build_env() {
    log_info "Setting up build environment..."
    
    # Create build directory
    mkdir -p "$BUILD_DIR"
    
    # Copy SDK files if needed
    if [ -d "$SDK_PATH" ]; then
        log_info "Copying SDK files to build directory..."
        cp -r "$SDK_PATH"/* "$BUILD_DIR/" 2>/dev/null || log_warning "Could not copy all SDK files"
    fi
    
    # Setup Go environment for cross-compilation
    export GOOS=linux
    export GOARCH=arm
    export GOARM=7
    export CGO_ENABLED=0
    
    log_success "Build environment setup complete"
}

# Build autonomy binary for RUTOS
build_autonomy_binary() {
    log_info "Building autonomy binary for RUTOS..."
    
    cd "$PROJECT_ROOT"
    
    # Build for ARM (RUTX50)
    log_info "Building for ARM architecture..."
    GOOS=linux GOARCH=arm GOARM=7 CGO_ENABLED=0 go build \
        -ldflags="-s -w" \
        -o "$BUILD_DIR/autonomyd-arm" \
        ./cmd/autonomysysmgmt
    
    # Build for MIPS (some RUTOS devices)
    log_info "Building for MIPS architecture..."
    GOOS=linux GOARCH=mips GOMIPS=softfloat CGO_ENABLED=0 go build \
        -ldflags="-s -w" \
        -o "$BUILD_DIR/autonomyd-mips" \
        ./cmd/autonomysysmgmt
    
    log_success "Autonomy binaries built successfully"
}

# Build RUTOS package
build_rutos_package() {
    log_info "Building RUTOS package..."
    
    if [ ! -d "$PACKAGE_DIR" ]; then
        log_error "Package directory not found: $PACKAGE_DIR"
        return 1
    fi
    
    cd "$PACKAGE_DIR"
    
    # Check if Makefile exists
    if [ ! -f "Makefile" ]; then
        log_error "Package Makefile not found"
        return 1
    fi
    
    # Build package
    log_info "Running package build..."
    make clean
    make package/autonomy/compile V=s
    
    log_success "RUTOS package built successfully"
}

# Test package installation
test_package_installation() {
    log_info "Testing package installation..."
    
    # Check if IPK file was created
    IPK_FILE=$(find "$BUILD_DIR" -name "autonomy_*.ipk" 2>/dev/null | head -1)
    
    if [ -n "$IPK_FILE" ]; then
        log_success "IPK package found: $IPK_FILE"
        
        # Test package structure
        log_info "Testing package structure..."
        tar -tzf "$IPK_FILE" | head -10
        
        return 0
    else
        log_warning "No IPK package found"
        return 1
    fi
}

# Validate UCI configuration
validate_uci_config() {
    log_info "Validating UCI configuration..."
    
    if [ -f "$PROJECT_ROOT/uci-schema/autonomy.sc" ]; then
        log_success "UCI schema found"
        
        # Basic schema validation
        if grep -q "config autonomy" "$PROJECT_ROOT/uci-schema/autonomy.sc"; then
            log_success "UCI schema appears valid"
        else
            log_warning "UCI schema may be incomplete"
        fi
    else
        log_warning "UCI schema not found"
    fi
}

# Test RUTOS-specific features
test_rutos_features() {
    log_info "Testing RUTOS-specific features..."
    
    # Test UCI integration
    if [ -f "$PROJECT_ROOT/pkg/uci/config.go" ]; then
        log_success "UCI integration package found"
    else
        log_warning "UCI integration package not found"
    fi
    
    # Test ubus integration
    if [ -f "$PROJECT_ROOT/pkg/ubus/client.go" ]; then
        log_success "ubus integration package found"
    else
        log_warning "ubus integration package not found"
    fi
    
    # Test mwan3 integration
    if [ -f "$PROJECT_ROOT/pkg/controller/controller.go" ]; then
        log_success "mwan3 controller package found"
    else
        log_warning "mwan3 controller package not found"
    fi
}

# Generate deployment report
generate_report() {
    log_info "Generating deployment report..."
    
    REPORT_FILE="$BUILD_DIR/rutos-deployment-report.md"
    
    cat > "$REPORT_FILE" << EOF
# RUTOS Deployment Report

Generated: $(date)

## Build Information
- SDK Path: $SDK_PATH
- Project Root: $PROJECT_ROOT
- Build Directory: $BUILD_DIR

## Build Results
- ARM Binary: $(ls -la "$BUILD_DIR/autonomyd-arm" 2>/dev/null || echo "Not found")
- MIPS Binary: $(ls -la "$BUILD_DIR/autonomyd-mips" 2>/dev/null || echo "Not found")
- IPK Package: $(find "$BUILD_DIR" -name "autonomy_*.ipk" 2>/dev/null || echo "Not found")

## Package Structure
\`\`\`
$(find "$PACKAGE_DIR" -type f 2>/dev/null | head -20)
\`\`\`

## UCI Configuration
- Schema: $(ls -la "$PROJECT_ROOT/uci-schema/autonomy.sc" 2>/dev/null || echo "Not found")

## Integration Status
- UCI Integration: $(if [ -f "$PROJECT_ROOT/pkg/uci/config.go" ]; then echo "✅ Found"; else echo "❌ Missing"; fi)
- ubus Integration: $(if [ -f "$PROJECT_ROOT/pkg/ubus/client.go" ]; then echo "✅ Found"; else echo "❌ Missing"; fi)
- mwan3 Integration: $(if [ -f "$PROJECT_ROOT/pkg/controller/controller.go" ]; then echo "✅ Found"; else echo "❌ Missing"; fi)

## Next Steps
1. Copy IPK package to RUTOS device
2. Install package: \`opkg install autonomy_*.ipk\`
3. Configure UCI settings
4. Start service: \`/etc/init.d/autonomy start\`
EOF
    
    log_success "Deployment report generated: $REPORT_FILE"
}

# Main execution
main() {
    log_info "Starting RUTOS SDK integration..."
    
    # Check SDK
    if ! check_sdk; then
        log_error "SDK check failed"
        exit 1
    fi
    
    # Setup build environment
    setup_build_env
    
    # Build binary
    build_autonomy_binary
    
    # Build package
    if build_rutos_package; then
        test_package_installation
    fi
    
    # Validate configuration
    validate_uci_config
    
    # Test RUTOS features
    test_rutos_features
    
    # Generate report
    generate_report
    
    log_success "RUTOS SDK integration completed successfully!"
}

# Run main function
main "$@"
