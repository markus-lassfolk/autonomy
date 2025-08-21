#!/bin/bash
set -e

# Build script for Autonomy Project
# This script builds the autonomy daemon and related components

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="$PROJECT_ROOT/bin"
MAIN_PACKAGE="cmd/autonomysysmgmt"
WEBHOOK_SERVER="scripts/webhook-server.go"
VERSION=$(git describe --tags --always --dirty 2>/dev/null || echo "dev")
BUILD_TIME=$(date -u '+%Y-%m-%d_%H:%M:%S_UTC')
LDFLAGS="-X main.Version=$VERSION -X main.BuildTime=$BUILD_TIME"

# Build targets
TARGETS=(
    "linux/amd64"
    "linux/arm64"
    "linux/arm"
    "linux/mips"
    "linux/mipsle"
)

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

# Check if Go is installed
check_go() {
    if ! command -v go &> /dev/null; then
        log_error "Go is not installed or not in PATH"
        exit 1
    fi
    
    GO_VERSION=$(go version | awk '{print $3}')
    log_info "Using Go version: $GO_VERSION"
}

# Create build directory
create_build_dir() {
    log_info "Creating build directory: $BUILD_DIR"
    mkdir -p "$BUILD_DIR"
}

# Build main binary
build_main() {
    log_info "Building main autonomy daemon..."
    
    cd "$PROJECT_ROOT"
    
    # Build for current platform
    go build -ldflags "$LDFLAGS" -o "$BUILD_DIR/autonomyd" "$MAIN_PACKAGE"
    
    if [ $? -eq 0 ]; then
        log_success "Main binary built successfully: $BUILD_DIR/autonomyd"
        chmod +x "$BUILD_DIR/autonomyd"
    else
        log_error "Failed to build main binary"
        exit 1
    fi
}

# Build webhook server
build_webhook_server() {
    log_info "Building webhook server..."
    
    cd "$PROJECT_ROOT"
    
    if [ -f "$WEBHOOK_SERVER" ]; then
        go build -ldflags "$LDFLAGS" -o "$BUILD_DIR/webhook-server" "$WEBHOOK_SERVER"
        
        if [ $? -eq 0 ]; then
            log_success "Webhook server built successfully: $BUILD_DIR/webhook-server"
            chmod +x "$BUILD_DIR/webhook-server"
        else
            log_warning "Failed to build webhook server"
        fi
    else
        log_warning "Webhook server source not found: $WEBHOOK_SERVER"
    fi
}

# Cross-compile for multiple platforms
build_cross_platform() {
    log_info "Building cross-platform binaries..."
    
    cd "$PROJECT_ROOT"
    
    for target in "${TARGETS[@]}"; do
        IFS='/' read -r GOOS GOARCH <<< "$target"
        
        log_info "Building for $GOOS/$GOARCH..."
        
        BINARY_NAME="autonomyd-${GOOS}-${GOARCH}"
        if [ "$GOOS" = "windows" ]; then
            BINARY_NAME="${BINARY_NAME}.exe"
        fi
        
        GOOS=$GOOS GOARCH=$GOARCH go build -ldflags "$LDFLAGS" -o "$BUILD_DIR/$BINARY_NAME" "$MAIN_PACKAGE"
        
        if [ $? -eq 0 ]; then
            log_success "Built: $BINARY_NAME"
            chmod +x "$BUILD_DIR/$BINARY_NAME"
        else
            log_warning "Failed to build for $GOOS/$GOARCH"
        fi
    done
}

# Run tests
run_tests() {
    log_info "Running tests..."
    
    cd "$PROJECT_ROOT"
    
    go test -v ./...
    
    if [ $? -eq 0 ]; then
        log_success "All tests passed"
    else
        log_error "Some tests failed"
        exit 1
    fi
}

# Run linting
run_lint() {
    log_info "Running linter..."
    
    cd "$PROJECT_ROOT"
    
    if command -v golangci-lint &> /dev/null; then
        golangci-lint run
    else
        log_warning "golangci-lint not found, skipping linting"
    fi
}

# Build packages
build_packages() {
    log_info "Building packages..."
    
    cd "$PROJECT_ROOT"
    
    if [ -f "Makefile" ]; then
        make package
    else
        log_warning "Makefile not found, skipping package build"
    fi
}

# Show build information
show_build_info() {
    log_info "Build Information:"
    echo "  Version: $VERSION"
    echo "  Build Time: $BUILD_TIME"
    echo "  Build Directory: $BUILD_DIR"
    echo "  Main Package: $MAIN_PACKAGE"
    echo "  Go Version: $(go version)"
    echo ""
    
    log_info "Built binaries:"
    ls -la "$BUILD_DIR"/* 2>/dev/null || echo "  No binaries found"
}

# Clean build artifacts
clean() {
    log_info "Cleaning build artifacts..."
    rm -rf "$BUILD_DIR"
    log_success "Build directory cleaned"
}

# Show usage
usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -h, --help          Show this help message"
    echo "  -c, --clean         Clean build artifacts"
    echo "  -t, --test          Run tests"
    echo "  -l, --lint          Run linter"
    echo "  -x, --cross         Build cross-platform binaries"
    echo "  -p, --package       Build packages"
    echo "  -a, --all           Build everything (default)"
    echo ""
    echo "Examples:"
    echo "  $0                  Build main binary"
    echo "  $0 -t               Run tests only"
    echo "  $0 -x               Build cross-platform binaries"
    echo "  $0 -a               Build everything"
}

# Main function
main() {
    local clean_only=false
    local test_only=false
    local lint_only=false
    local cross_only=false
    local package_only=false
    local build_all=true
    
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                usage
                exit 0
                ;;
            -c|--clean)
                clean_only=true
                build_all=false
                shift
                ;;
            -t|--test)
                test_only=true
                build_all=false
                shift
                ;;
            -l|--lint)
                lint_only=true
                build_all=false
                shift
                ;;
            -x|--cross)
                cross_only=true
                build_all=false
                shift
                ;;
            -p|--package)
                package_only=true
                build_all=false
                shift
                ;;
            -a|--all)
                build_all=true
                shift
                ;;
            *)
                log_error "Unknown option: $1"
                usage
                exit 1
                ;;
        esac
    done
    
    log_info "Starting Autonomy build process..."
    
    # Check prerequisites
    check_go
    
    # Execute requested actions
    if [ "$clean_only" = true ]; then
        clean
        exit 0
    fi
    
    if [ "$test_only" = true ]; then
        run_tests
        exit 0
    fi
    
    if [ "$lint_only" = true ]; then
        run_lint
        exit 0
    fi
    
    if [ "$cross_only" = true ]; then
        create_build_dir
        build_cross_platform
        show_build_info
        exit 0
    fi
    
    if [ "$package_only" = true ]; then
        build_packages
        exit 0
    fi
    
    # Default: build all
    if [ "$build_all" = true ]; then
        create_build_dir
        run_tests
        run_lint
        build_main
        build_webhook_server
        build_cross_platform
        build_packages
        show_build_info
    fi
    
    log_success "Build process completed successfully!"
}

# Run main function with all arguments
main "$@"
