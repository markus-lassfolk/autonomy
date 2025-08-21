#!/bin/bash

# autonomy Build Script
# Builds the autonomy daemon for different architectures

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
BUILD_DIR="build"
VERSION="1.0.0"
CGO_ENABLED=0
LDFLAGS="-s -w"

# Build targets
TARGETS=(
    "linux/amd64"
    "linux/arm"
    "linux/arm64"
    "linux/386"
)

# Helper functions
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

# Show usage
show_usage() {
    cat <<EOF
Usage: $0 [options]

Options:
  -h, --help              Show this help message
  -v, --version VERSION   Set version (default: $VERSION)
  -d, --dir DIR           Set build directory (default: $BUILD_DIR)
  -t, --target TARGET     Build for specific target (e.g., linux/arm)
  -a, --all               Build for all targets
  -c, --clean             Clean build directory before building
  -s, --strip             Strip binaries after building
  -p, --package           Create package files (.tar.gz)

Targets:
  linux/amd64    - 64-bit x86 Linux
  linux/arm      - 32-bit ARM Linux
  linux/arm64    - 64-bit ARM Linux
  linux/386      - 32-bit x86 Linux

Examples:
  $0 --all                    # Build for all targets
  $0 --target linux/arm       # Build for ARM only
  $0 --clean --all            # Clean and build all
  $0 --package --target linux/arm  # Build and package for ARM

EOF
}

# Check if Go is available
check_go() {
    if ! command -v go >/dev/null 2>&1; then
        log_error "Go is not installed or not in PATH"
        exit 1
    fi

    GO_VERSION=$(go version | awk '{print $3}')
    log_info "Using Go version: $GO_VERSION"
}

# Clean build directory
clean_build() {
    if [ -d "$BUILD_DIR" ]; then
        log_info "Cleaning build directory: $BUILD_DIR"
        rm -rf "$BUILD_DIR"
    fi
    mkdir -p "$BUILD_DIR"
}

# Build for a specific target
build_target() {
    local target=$1
    local os=$(echo "$target" | cut -d'/' -f1)
    local arch=$(echo "$target" | cut -d'/' -f2)
    local output_name="autonomyd-${os}-${arch}"

    if [ "$os" = "linux" ] && [ "$arch" = "arm" ]; then
        # Set ARM version for 32-bit ARM
        export GOARM=7
        output_name="autonomyd-${os}-${arch}v7"
    fi

    log_info "Building for $target..."

    export GOOS=$os
    export GOARCH=$arch
    export CGO_ENABLED=0

    local output_path="$BUILD_DIR/$output_name"

    if go build -ldflags "$LDFLAGS -X main.AppVersion=$VERSION" -o "$output_path" ./cmd/autonomyd; then
        log_success "Built: $output_path"

        # Strip binary if requested
        if [ "$STRIP" = "1" ]; then
            if command -v strip >/dev/null 2>&1; then
                strip "$output_path" 2>/dev/null || log_warning "Failed to strip $output_path"
            fi
        fi

        # Show binary info
        if command -v file >/dev/null 2>&1; then
            log_info "Binary info: $(file "$output_path")"
        fi

        # Show binary size
        if command -v wc >/dev/null 2>&1; then
            local size=$(wc -c <"$output_path" | numfmt --to=iec)
            log_info "Binary size: $size"
        fi

        return 0
    else
        log_error "Failed to build for $target"
        return 1
    fi
}

# Create package
create_package() {
    local target=$1
    local os=$(echo "$target" | cut -d'/' -f1)
    local arch=$(echo "$target" | cut -d'/' -f2)
    local output_name="autonomyd-${os}-${arch}"

    if [ "$os" = "linux" ] && [ "$arch" = "arm" ]; then
        output_name="autonomyd-${os}-${arch}v7"
    fi

    local binary_path="$BUILD_DIR/$output_name"
    local package_name="autonomy-${VERSION}-${os}-${arch}.tar.gz"

    if [ ! -f "$binary_path" ]; then
        log_error "Binary not found: $binary_path"
        return 1
    fi

    log_info "Creating package: $package_name"

    # Create temporary directory for package
    local temp_dir=$(mktemp -d)
    local package_dir="$temp_dir/autonomy-$VERSION"

    mkdir -p "$package_dir/usr/sbin"
    mkdir -p "$package_dir/etc/init.d"
    mkdir -p "$package_dir/etc/config"
    mkdir -p "$package_dir/scripts"

    # Copy binary
    cp "$binary_path" "$package_dir/usr/sbin/autonomyd"
    chmod 755 "$package_dir/usr/sbin/autonomyd"

    # Copy scripts
    cp scripts/autonomyctl "$package_dir/usr/sbin/"
    chmod 755 "$package_dir/usr/sbin/autonomyctl"

    cp scripts/autonomy.init "$package_dir/etc/init.d/autonomy"
    chmod 755 "$package_dir/etc/init.d/autonomy"

    # Copy sample config
    cp configs/autonomy.example "$package_dir/etc/config/autonomy"

    # Create package
    (cd "$temp_dir" && tar -czf "$BUILD_DIR/$package_name" "autonomy-$VERSION")

    # Cleanup
    rm -rf "$temp_dir"

    log_success "Created package: $BUILD_DIR/$package_name"
}

# Main build function
main_build() {
    check_go

    if [ "$CLEAN" = "1" ]; then
        clean_build
    else
        mkdir -p "$BUILD_DIR"
    fi

    local build_count=0
    local success_count=0

    if [ -n "$SPECIFIC_TARGET" ]; then
        # Build specific target
        build_target "$SPECIFIC_TARGET"
        if [ $? -eq 0 ]; then
            success_count=$((success_count + 1))

            if [ "$PACKAGE" = "1" ]; then
                create_package "$SPECIFIC_TARGET"
            fi
        fi
        build_count=$((build_count + 1))
    else
        # Build all targets
        for target in "${TARGETS[@]}"; do
            build_target "$target"
            if [ $? -eq 0 ]; then
                success_count=$((success_count + 1))

                if [ "$PACKAGE" = "1" ]; then
                    create_package "$target"
                fi
            fi
            build_count=$((build_count + 1))
        done
    fi

    # Summary
    echo
    log_info "Build summary: $success_count/$build_count targets built successfully"

    if [ $success_count -eq $build_count ]; then
        log_success "All builds completed successfully!"
    else
        log_warning "Some builds failed. Check the output above for details."
        exit 1
    fi
}

# Parse command line arguments
CLEAN=0
STRIP=0
PACKAGE=0
SPECIFIC_TARGET=""

while [ $# -gt 0 ]; do
    case "$1" in
        -h | --help)
            show_usage
            exit 0
            ;;
        -v | --version)
            VERSION="$2"
            shift 2
            ;;
        -d | --dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        -t | --target)
            SPECIFIC_TARGET="$2"
            shift 2
            ;;
        -a | --all)
            SPECIFIC_TARGET=""
            shift
            ;;
        -c | --clean)
            CLEAN=1
            shift
            ;;
        -s | --strip)
            STRIP=1
            shift
            ;;
        -p | --package)
            PACKAGE=1
            shift
            ;;
        *)
            log_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
done

# Validate specific target if provided
if [ -n "$SPECIFIC_TARGET" ]; then
    valid_target=0
    for target in "${TARGETS[@]}"; do
        if [ "$target" = "$SPECIFIC_TARGET" ]; then
            valid_target=1
            break
        fi
    done

    if [ $valid_target -eq 0 ]; then
        log_error "Invalid target: $SPECIFIC_TARGET"
        echo "Valid targets: ${TARGETS[*]}"
        exit 1
    fi
fi

# Show build configuration
log_info "Build configuration:"
log_info "  Version: $VERSION"
log_info "  Build directory: $BUILD_DIR"
log_info "  CGO enabled: $CGO_ENABLED"
log_info "  Strip binaries: $STRIP"
log_info "  Create packages: $PACKAGE"
if [ -n "$SPECIFIC_TARGET" ]; then
    log_info "  Target: $SPECIFIC_TARGET"
else
    log_info "  Targets: ${TARGETS[*]}"
fi

echo

# Run the build
main_build
