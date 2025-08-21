#!/bin/bash
# Build script for autonomy packages
# This script builds the autonomy daemon and VuCI web interface packages

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
PACKAGE_DIR="${BUILD_DIR}/packages"
SDK_PATH="${RUTOS_SDK_PATH:-/path/to/rutos-ipq40xx-rutx-sdk}"

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

# Check prerequisites
check_prerequisites() {
    log_info "Checking prerequisites..."
    
    # Check if we're in the right directory
    if [ ! -f "${PROJECT_ROOT}/go.mod" ]; then
        log_error "Not in autonomy project root directory"
        exit 1
    fi
    
    # Check Go installation
    if ! command -v go >/dev/null 2>&1; then
        log_error "Go is not installed or not in PATH"
        exit 1
    fi
    
    # Check Go version
    GO_VERSION=$(go version | awk '{print $3}' | sed 's/go//')
    REQUIRED_VERSION="1.19"
    
    if [ "$(printf '%s\n' "$REQUIRED_VERSION" "$GO_VERSION" | sort -V | head -n1)" != "$REQUIRED_VERSION" ]; then
        log_error "Go version $GO_VERSION is too old. Required: $REQUIRED_VERSION or newer"
        exit 1
    fi
    
    log_success "Prerequisites check passed"
}

# Build Go binary
build_binary() {
    log_info "Building autonomy daemon binary..."
    
    # Create build directory
    mkdir -p "${BUILD_DIR}"
    
    # Build for target architecture (ARM)
    cd "${PROJECT_ROOT}"
    
    # Set Go environment variables for cross-compilation
    export CGO_ENABLED=0
    export GOOS=linux
    export GOARCH=arm
    export GOARM=7
    
    # Build the binary
    go build -ldflags="-s -w" -o "${BUILD_DIR}/autonomyd" ./cmd/autonomyd
    
    if [ $? -eq 0 ]; then
        log_success "Binary built successfully: ${BUILD_DIR}/autonomyd"
    else
        log_error "Failed to build binary"
        exit 1
    fi
}

# Prepare package files
prepare_package_files() {
    log_info "Preparing package files..."
    
    # Create package directory
    mkdir -p "${PACKAGE_DIR}"
    
    # Copy binary to package files
    cp "${BUILD_DIR}/autonomyd" "${PROJECT_ROOT}/package/autonomy/files/"
    
    # Make sure all package files are executable
    chmod +x "${PROJECT_ROOT}/package/autonomy/files/autonomy.init"
    chmod +x "${PROJECT_ROOT}/package/autonomy/files/autonomyctl"
    chmod +x "${PROJECT_ROOT}/package/autonomy/files/99-autonomy-defaults"
    
    log_success "Package files prepared"
}

# Build packages with SDK
build_packages_sdk() {
    log_info "Building packages with RUTOS SDK..."
    
    if [ ! -d "${SDK_PATH}" ]; then
        log_warning "RUTOS SDK not found at ${SDK_PATH}"
        log_info "Please set RUTOS_SDK_PATH environment variable or update the script"
        return 1
    fi
    
    # Navigate to SDK
    cd "${SDK_PATH}"
    
    # Copy packages to SDK
    cp -r "${PROJECT_ROOT}/package/autonomy" "${SDK_PATH}/package/"
    cp -r "${PROJECT_ROOT}/vuci-app-autonomy" "${SDK_PATH}/package/"
    
    # Update package feeds
    ./scripts/feeds update -a
    ./scripts/feeds install -a
    
    # Build autonomy package
    log_info "Building autonomy package..."
    make package/autonomy/compile V=s
    
    # Build VuCI package
    log_info "Building VuCI package..."
    make package/vuci-app-autonomy/compile V=s
    
    # Install packages
    make package/autonomy/install V=s
    make package/vuci-app-autonomy/install V=s
    
    # Find generated IPK files
    AUTONOMY_IPK=$(find bin/packages/ -name "*autonomy*.ipk" | grep -v vuci)
    VUCI_IPK=$(find bin/packages/ -name "*vuci-app-autonomy*.ipk")
    
    if [ -n "$AUTONOMY_IPK" ]; then
        log_success "Autonomy package built: $AUTONOMY_IPK"
        cp "$AUTONOMY_IPK" "${PACKAGE_DIR}/"
    else
        log_error "Failed to find autonomy IPK file"
    fi
    
    if [ -n "$VUCI_IPK" ]; then
        log_success "VuCI package built: $VUCI_IPK"
        cp "$VUCI_IPK" "${PACKAGE_DIR}/"
    else
        log_error "Failed to find VuCI IPK file"
    fi
}

# Build packages manually (without SDK)
build_packages_manual() {
    log_info "Building packages manually..."
    
    # Create package structure
    mkdir -p "${PACKAGE_DIR}/autonomy"
    mkdir -p "${PACKAGE_DIR}/vuci-app-autonomy"
    
    # Copy files
    cp -r "${PROJECT_ROOT}/package/autonomy/"* "${PACKAGE_DIR}/autonomy/"
    cp -r "${PROJECT_ROOT}/vuci-app-autonomy/"* "${PACKAGE_DIR}/vuci-app-autonomy/"
    
    # Create simple IPK structure (for testing)
    log_warning "Manual build creates package structure only"
    log_info "Use RUTOS SDK for full IPK generation"
    
    log_success "Manual package structure created in ${PACKAGE_DIR}"
}

# Create package repository
create_repository() {
    log_info "Creating package repository..."
    
    if [ ! -d "${PACKAGE_DIR}" ]; then
        log_error "Package directory not found"
        return 1
    fi
    
    # Check if we have IPK files
    IPK_FILES=$(find "${PACKAGE_DIR}" -name "*.ipk" 2>/dev/null || true)
    
    if [ -z "$IPK_FILES" ]; then
        log_warning "No IPK files found for repository creation"
        return 1
    fi
    
    # Create repository structure
    REPO_DIR="${BUILD_DIR}/repository"
    mkdir -p "${REPO_DIR}"
    
    # Copy IPK files
    cp "${PACKAGE_DIR}"/*.ipk "${REPO_DIR}/"
    
    # Generate Packages file (if opkg-make-index is available)
    if command -v opkg-make-index >/dev/null 2>&1; then
        cd "${REPO_DIR}"
        opkg-make-index . > Packages
        log_success "Package repository created at ${REPO_DIR}"
    else
        log_warning "opkg-make-index not found, skipping Packages file generation"
        log_info "Repository files copied to ${REPO_DIR}"
    fi
}

# Show build summary
show_summary() {
    log_info "Build Summary:"
    echo "=================="
    
    if [ -f "${BUILD_DIR}/autonomyd" ]; then
        echo "✅ Binary: ${BUILD_DIR}/autonomyd"
        echo "   Size: $(du -h "${BUILD_DIR}/autonomyd" | cut -f1)"
    else
        echo "❌ Binary: Not built"
    fi
    
    echo ""
    echo "📦 Packages:"
    
    if [ -d "${PACKAGE_DIR}" ]; then
        echo "   Directory: ${PACKAGE_DIR}"
        ls -la "${PACKAGE_DIR}" 2>/dev/null || echo "   (empty)"
    else
        echo "   Directory: Not created"
    fi
    
    echo ""
    echo "🌐 Repository:"
    
    if [ -d "${BUILD_DIR}/repository" ]; then
        echo "   Directory: ${BUILD_DIR}/repository"
        ls -la "${BUILD_DIR}/repository" 2>/dev/null || echo "   (empty)"
    else
        echo "   Directory: Not created"
    fi
    
    echo ""
    echo "📋 Next Steps:"
    echo "1. Copy IPK files to your RUTOS device"
    echo "2. Install packages: opkg install autonomy_*.ipk"
    echo "3. Install VuCI: opkg install vuci-app-autonomy_*.ipk"
    echo "4. Configure and start the service"
}

# Clean build artifacts
clean_build() {
    log_info "Cleaning build artifacts..."
    
    if [ -d "${BUILD_DIR}" ]; then
        rm -rf "${BUILD_DIR}"
        log_success "Build directory cleaned"
    fi
    
    # Clean Go build cache
    go clean -cache
    log_success "Go cache cleaned"
}

# Show usage
show_usage() {
    cat << EOF
Usage: $0 [OPTIONS] COMMAND

Commands:
  build       Build binary and packages (default)
  clean       Clean build artifacts
  binary      Build only the Go binary
  packages    Build only the packages
  repository  Create package repository
  summary     Show build summary

Options:
  --sdk       Use RUTOS SDK for building (requires SDK_PATH)
  --manual    Build packages manually (without SDK)
  --clean     Clean before building
  --help      Show this help message

Environment Variables:
  RUTOS_SDK_PATH    Path to RUTOS SDK (default: /path/to/rutos-ipq40xx-rutx-sdk)

Examples:
  $0 build                    # Build everything
  $0 build --sdk              # Build with RUTOS SDK
  $0 build --manual           # Build manually
  $0 clean                    # Clean build artifacts
  $0 summary                  # Show build summary

EOF
}

# Main function
main() {
    local command="build"
    local use_sdk=false
    local use_manual=false
    local clean_first=false
    
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            build|clean|binary|packages|repository|summary)
                command="$1"
                shift
                ;;
            --sdk)
                use_sdk=true
                shift
                ;;
            --manual)
                use_manual=true
                shift
                ;;
            --clean)
                clean_first=true
                shift
                ;;
            --help|-h)
                show_usage
                exit 0
                ;;
            *)
                log_error "Unknown option: $1"
                show_usage
                exit 1
                ;;
        esac
    done
    
    # Execute command
    case $command in
        build)
            check_prerequisites
            
            if [ "$clean_first" = true ]; then
                clean_build
            fi
            
            build_binary
            prepare_package_files
            
            if [ "$use_sdk" = true ]; then
                build_packages_sdk
            elif [ "$use_manual" = true ]; then
                build_packages_manual
            else
                # Try SDK first, fall back to manual
                if ! build_packages_sdk; then
                    log_warning "SDK build failed, trying manual build"
                    build_packages_manual
                fi
            fi
            
            create_repository
            show_summary
            ;;
        clean)
            clean_build
            ;;
        binary)
            check_prerequisites
            build_binary
            ;;
        packages)
            prepare_package_files
            
            if [ "$use_sdk" = true ]; then
                build_packages_sdk
            elif [ "$use_manual" = true ]; then
                build_packages_manual
            else
                if ! build_packages_sdk; then
                    build_packages_manual
                fi
            fi
            ;;
        repository)
            create_repository
            ;;
        summary)
            show_summary
            ;;
        *)
            log_error "Unknown command: $command"
            show_usage
            exit 1
            ;;
    esac
}

# Run main function
main "$@"

