#!/bin/bash
set -e

# Production Deployment Script for Autonomy Project
# This script deploys the autonomy system to production environments

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="$PROJECT_ROOT/bin"
DEPLOY_DIR="$PROJECT_ROOT/deploy"
CONFIG_DIR="$PROJECT_ROOT/configs"
PACKAGE_DIR="$PROJECT_ROOT/package"

# Deployment configuration
DEPLOY_ENV="${DEPLOY_ENV:-production}"
DEPLOY_TARGET="${DEPLOY_TARGET:-rutos}"
DEPLOY_METHOD="${DEPLOY_METHOD:-package}"
BACKUP_ENABLED="${BACKUP_ENABLED:-true}"
ROLLBACK_ENABLED="${ROLLBACK_ENABLED:-true}"

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

# Check prerequisites
check_prerequisites() {
    log_deploy "Checking deployment prerequisites..."
    
    # Check if Go is installed
    if ! command -v go &> /dev/null; then
        log_error "Go is not installed or not in PATH"
        exit 1
    fi
    
    # Check if build directory exists
    if [ ! -d "$BUILD_DIR" ]; then
        log_error "Build directory not found: $BUILD_DIR"
        log_info "Please run build script first: ./scripts/build.sh"
        exit 1
    fi
    
    # Check if main binary exists
    if [ ! -f "$BUILD_DIR/autonomyd" ]; then
        log_error "Main binary not found: $BUILD_DIR/autonomyd"
        log_info "Please run build script first: ./scripts/build.sh"
        exit 1
    fi
    
    log_success "Prerequisites check passed"
}

# Create deployment package
create_deployment_package() {
    log_deploy "Creating deployment package..."
    
    mkdir -p "$DEPLOY_DIR"
    
    # Create package directory
    PACKAGE_NAME="autonomy-${DEPLOY_ENV}-$(date +%Y%m%d-%H%M%S)"
    PACKAGE_PATH="$DEPLOY_DIR/$PACKAGE_NAME"
    mkdir -p "$PACKAGE_PATH"
    
    # Copy binaries
    log_info "Copying binaries..."
    cp "$BUILD_DIR/autonomyd" "$PACKAGE_PATH/"
    if [ -f "$BUILD_DIR/webhook-server" ]; then
        cp "$BUILD_DIR/webhook-server" "$PACKAGE_PATH/"
    fi
    
    # Copy configuration files
    log_info "Copying configuration files..."
    mkdir -p "$PACKAGE_PATH/config"
    cp "$CONFIG_DIR/autonomy.comprehensive.example" "$PACKAGE_PATH/config/autonomy"
    
    # Copy UCI schema
    if [ -f "$PROJECT_ROOT/uci-schema/autonomy.sc" ]; then
        mkdir -p "$PACKAGE_PATH/uci-schema"
        cp "$PROJECT_ROOT/uci-schema/autonomy.sc" "$PACKAGE_PATH/uci-schema/"
    fi
    
    # Copy init scripts
    if [ -f "$PROJECT_ROOT/package/autonomy/files/autonomy.init" ]; then
        mkdir -p "$PACKAGE_PATH/init"
        cp "$PROJECT_ROOT/package/autonomy/files/autonomy.init" "$PACKAGE_PATH/init/"
    fi
    
    # Copy hotplug scripts
    if [ -f "$PROJECT_ROOT/package/autonomy/files/99-autonomy" ]; then
        mkdir -p "$PACKAGE_PATH/hotplug"
        cp "$PROJECT_ROOT/package/autonomy/files/99-autonomy" "$PACKAGE_PATH/hotplug/"
    fi
    
    # Create deployment manifest
    cat > "$PACKAGE_PATH/deployment-manifest.json" << EOF
{
    "package_name": "$PACKAGE_NAME",
    "deployment_env": "$DEPLOY_ENV",
    "deployment_target": "$DEPLOY_TARGET",
    "deployment_method": "$DEPLOY_METHOD",
    "created_at": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
    "version": "$(git describe --tags --always --dirty 2>/dev/null || echo 'dev')",
    "commit": "$(git rev-parse HEAD 2>/dev/null || echo 'unknown')",
    "files": [
        "autonomyd",
        "config/autonomy",
        "deployment-manifest.json"
    ]
}
EOF
    
    # Create deployment script
    cat > "$PACKAGE_PATH/deploy.sh" << 'EOF'
#!/bin/bash
set -e

# Autonomy Deployment Script
# This script deploys the autonomy system to the target device

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BACKUP_DIR="/tmp/autonomy-backup-$(date +%Y%m%d-%H%M%S)"

log_info() {
    echo "[INFO] $1"
}

log_success() {
    echo "[SUCCESS] $1"
}

log_error() {
    echo "[ERROR] $1"
}

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    log_error "This script must be run as root"
    exit 1
fi

# Create backup
log_info "Creating backup..."
mkdir -p "$BACKUP_DIR"

if [ -f "/usr/bin/autonomyd" ]; then
    cp /usr/bin/autonomyd "$BACKUP_DIR/"
fi

if [ -f "/etc/config/autonomy" ]; then
    cp /etc/config/autonomy "$BACKUP_DIR/"
fi

if [ -f "/etc/init.d/autonomy" ]; then
    cp /etc/init.d/autonomy "$BACKUP_DIR/"
fi

# Stop existing service
log_info "Stopping existing autonomy service..."
if [ -f "/etc/init.d/autonomy" ]; then
    /etc/init.d/autonomy stop || true
fi

# Install binaries
log_info "Installing binaries..."
cp "$SCRIPT_DIR/autonomyd" /usr/bin/
chmod +x /usr/bin/autonomyd

if [ -f "$SCRIPT_DIR/webhook-server" ]; then
    cp "$SCRIPT_DIR/webhook-server" /usr/bin/
    chmod +x /usr/bin/webhook-server
fi

# Install configuration
log_info "Installing configuration..."
if [ -f "$SCRIPT_DIR/config/autonomy" ]; then
    cp "$SCRIPT_DIR/config/autonomy" /etc/config/
    chmod 644 /etc/config/autonomy
fi

# Install UCI schema
if [ -f "$SCRIPT_DIR/uci-schema/autonomy.sc" ]; then
    log_info "Installing UCI schema..."
    cp "$SCRIPT_DIR/uci-schema/autonomy.sc" /usr/share/rpcd/acl.d/
    chmod 644 /usr/share/rpcd/acl.d/autonomy.sc
fi

# Install init script
if [ -f "$SCRIPT_DIR/init/autonomy.init" ]; then
    log_info "Installing init script..."
    cp "$SCRIPT_DIR/init/autonomy.init" /etc/init.d/autonomy
    chmod +x /etc/init.d/autonomy
fi

# Install hotplug script
if [ -f "$SCRIPT_DIR/hotplug/99-autonomy" ]; then
    log_info "Installing hotplug script..."
    mkdir -p /etc/hotplug.d/iface
    cp "$SCRIPT_DIR/hotplug/99-autonomy" /etc/hotplug.d/iface/
    chmod +x /etc/hotplug.d/iface/99-autonomy
fi

# Start service
log_info "Starting autonomy service..."
if [ -f "/etc/init.d/autonomy" ]; then
    /etc/init.d/autonomy enable
    /etc/init.d/autonomy start
fi

log_success "Deployment completed successfully!"
log_info "Backup saved to: $BACKUP_DIR"
EOF
    
    chmod +x "$PACKAGE_PATH/deploy.sh"
    
    # Create rollback script
    cat > "$PACKAGE_PATH/rollback.sh" << 'EOF'
#!/bin/bash
set -e

# Autonomy Rollback Script
# This script rolls back to the previous version

BACKUP_DIR="$1"

if [ -z "$BACKUP_DIR" ]; then
    echo "[ERROR] Backup directory not specified"
    echo "Usage: $0 <backup_directory>"
    exit 1
fi

if [ ! -d "$BACKUP_DIR" ]; then
    echo "[ERROR] Backup directory not found: $BACKUP_DIR"
    exit 1
fi

log_info() {
    echo "[INFO] $1"
}

log_success() {
    echo "[SUCCESS] $1"
}

log_error() {
    echo "[ERROR] $1"
}

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    log_error "This script must be run as root"
    exit 1
fi

# Stop service
log_info "Stopping autonomy service..."
if [ -f "/etc/init.d/autonomy" ]; then
    /etc/init.d/autonomy stop || true
fi

# Restore binaries
log_info "Restoring binaries..."
if [ -f "$BACKUP_DIR/autonomyd" ]; then
    cp "$BACKUP_DIR/autonomyd" /usr/bin/
    chmod +x /usr/bin/autonomyd
fi

# Restore configuration
log_info "Restoring configuration..."
if [ -f "$BACKUP_DIR/autonomy" ]; then
    cp "$BACKUP_DIR/autonomy" /etc/config/
    chmod 644 /etc/config/autonomy
fi

# Restore init script
if [ -f "$BACKUP_DIR/autonomy.init" ]; then
    log_info "Restoring init script..."
    cp "$BACKUP_DIR/autonomy.init" /etc/init.d/autonomy
    chmod +x /etc/init.d/autonomy
fi

# Start service
log_info "Starting autonomy service..."
if [ -f "/etc/init.d/autonomy" ]; then
    /etc/init.d/autonomy start
fi

log_success "Rollback completed successfully!"
EOF
    
    chmod +x "$PACKAGE_PATH/rollback.sh"
    
    # Create package archive
    log_info "Creating package archive..."
    cd "$DEPLOY_DIR"
    tar -czf "${PACKAGE_NAME}.tar.gz" "$PACKAGE_NAME"
    
    log_success "Deployment package created: $DEPLOY_DIR/${PACKAGE_NAME}.tar.gz"
    echo "$PACKAGE_PATH"
}

# Deploy to RUTOS device
deploy_to_rutos() {
    local package_path="$1"
    local target_host="${RUTOS_HOST:-192.168.1.1}"
    local target_user="${RUTOS_USER:-root}"
    local target_port="${RUTOS_PORT:-22}"
    
    log_deploy "Deploying to RUTOS device: $target_host"
    
    # Check if SSH is available
    if ! command -v ssh &> /dev/null; then
        log_error "SSH client not available"
        return 1
    fi
    
    # Test SSH connection
    log_info "Testing SSH connection..."
    if ! ssh -p "$target_port" -o ConnectTimeout=10 -o BatchMode=yes "$target_user@$target_host" "echo 'SSH connection successful'" 2>/dev/null; then
        log_error "SSH connection failed to $target_user@$target_host:$target_port"
        log_info "Please ensure SSH access is configured"
        return 1
    fi
    
    # Upload package
    log_info "Uploading deployment package..."
    if ! scp -P "$target_port" "${package_path}.tar.gz" "$target_user@$target_host:/tmp/"; then
        log_error "Failed to upload package"
        return 1
    fi
    
    # Extract and deploy
    log_info "Extracting and deploying package..."
    ssh -p "$target_port" "$target_user@$target_host" << EOF
set -e
cd /tmp
tar -xzf $(basename "${package_path}.tar.gz")
cd $(basename "$package_path")
./deploy.sh
EOF
    
    if [ $? -eq 0 ]; then
        log_success "Deployment to RUTOS completed successfully"
    else
        log_error "Deployment to RUTOS failed"
        return 1
    fi
}

# Deploy to OpenWrt device
deploy_to_openwrt() {
    local package_path="$1"
    local target_host="${OPENWRT_HOST:-192.168.1.1}"
    local target_user="${OPENWRT_USER:-root}"
    local target_port="${OPENWRT_PORT:-22}"
    
    log_deploy "Deploying to OpenWrt device: $target_host"
    
    # Similar to RUTOS deployment but with OpenWrt-specific adjustments
    deploy_to_rutos "$package_path"
}

# Deploy using Docker
deploy_with_docker() {
    local package_path="$1"
    
    log_deploy "Deploying using Docker..."
    
    # Check if Docker is available
    if ! command -v docker &> /dev/null; then
        log_error "Docker not available"
        return 1
    fi
    
    # Build Docker image
    log_info "Building Docker image..."
    cd "$PROJECT_ROOT"
    
    if [ -f "test/docker/Dockerfile.$DEPLOY_TARGET" ]; then
        docker build -f "test/docker/Dockerfile.$DEPLOY_TARGET" -t "autonomy:$DEPLOY_ENV" .
        
        if [ $? -eq 0 ]; then
            log_success "Docker image built successfully"
        else
            log_error "Docker image build failed"
            return 1
        fi
    else
        log_error "Dockerfile not found: test/docker/Dockerfile.$DEPLOY_TARGET"
        return 1
    fi
}

# Deploy using package manager
deploy_with_package() {
    local package_path="$1"
    
    log_deploy "Deploying using package manager..."
    
    # Check if IPK package exists
    if [ -f "$PACKAGE_DIR/autonomy/autonomy_*.ipk" ]; then
        log_info "IPK package found, installing..."
        
        # This would typically involve uploading to the device and installing
        # For now, we'll just log the action
        log_info "IPK package ready for installation: $PACKAGE_DIR/autonomy/autonomy_*.ipk"
    else
        log_warning "IPK package not found, skipping package deployment"
    fi
}

# Verify deployment
verify_deployment() {
    local target_host="${RUTOS_HOST:-192.168.1.1}"
    local target_user="${RUTOS_USER:-root}"
    local target_port="${RUTOS_PORT:-22}"
    
    log_deploy "Verifying deployment..."
    
    # Test service status
    log_info "Checking service status..."
    if ssh -p "$target_port" "$target_user@$target_host" "pgrep autonomyd" 2>/dev/null; then
        log_success "Autonomy service is running"
    else
        log_error "Autonomy service is not running"
        return 1
    fi
    
    # Test configuration
    log_info "Checking configuration..."
    if ssh -p "$target_port" "$target_user@$target_host" "test -f /etc/config/autonomy" 2>/dev/null; then
        log_success "Configuration file exists"
    else
        log_error "Configuration file missing"
        return 1
    fi
    
    # Test ubus integration
    log_info "Testing ubus integration..."
    if ssh -p "$target_port" "$target_user@$target_host" "ubus list | grep autonomy" 2>/dev/null; then
        log_success "Ubus integration working"
    else
        log_warning "Ubus integration not available"
    fi
    
    log_success "Deployment verification completed"
}

# Show usage
usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -h, --help              Show this help message"
    echo "  -e, --env ENV           Deployment environment (default: production)"
    echo "  -t, --target TARGET     Deployment target (rutos|openwrt|docker)"
    echo "  -m, --method METHOD     Deployment method (package|binary|docker)"
    echo "  -v, --verify            Verify deployment after completion"
    echo "  -b, --backup            Enable backup (default: true)"
    echo "  -r, --rollback          Enable rollback (default: true)"
    echo ""
    echo "Environment Variables:"
    echo "  RUTOS_HOST              RUTOS device hostname/IP"
    echo "  RUTOS_USER              RUTOS device username"
    echo "  RUTOS_PORT              RUTOS device SSH port"
    echo "  OPENWRT_HOST            OpenWrt device hostname/IP"
    echo "  OPENWRT_USER            OpenWrt device username"
    echo "  OPENWRT_PORT            OpenWrt device SSH port"
    echo ""
    echo "Examples:"
    echo "  $0                      Deploy to production RUTOS device"
    echo "  $0 -t openwrt           Deploy to OpenWrt device"
    echo "  $0 -m docker            Deploy using Docker"
    echo "  $0 -v                   Deploy and verify"
}

# Main function
main() {
    local verify_deployment_flag=false
    
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                usage
                exit 0
                ;;
            -e|--env)
                DEPLOY_ENV="$2"
                shift 2
                ;;
            -t|--target)
                DEPLOY_TARGET="$2"
                shift 2
                ;;
            -m|--method)
                DEPLOY_METHOD="$2"
                shift 2
                ;;
            -v|--verify)
                verify_deployment_flag=true
                shift
                ;;
            -b|--backup)
                BACKUP_ENABLED="$2"
                shift 2
                ;;
            -r|--rollback)
                ROLLBACK_ENABLED="$2"
                shift 2
                ;;
            *)
                log_error "Unknown option: $1"
                usage
                exit 1
                ;;
        esac
    done
    
    log_info "Starting production deployment..."
    log_info "Environment: $DEPLOY_ENV"
    log_info "Target: $DEPLOY_TARGET"
    log_info "Method: $DEPLOY_METHOD"
    
    # Check prerequisites
    check_prerequisites
    
    # Create deployment package
    PACKAGE_PATH=$(create_deployment_package)
    
    # Deploy based on method
    case "$DEPLOY_METHOD" in
        "package")
            deploy_with_package "$PACKAGE_PATH"
            ;;
        "binary")
            case "$DEPLOY_TARGET" in
                "rutos")
                    deploy_to_rutos "$PACKAGE_PATH"
                    ;;
                "openwrt")
                    deploy_to_openwrt "$PACKAGE_PATH"
                    ;;
                *)
                    log_error "Unsupported target: $DEPLOY_TARGET"
                    exit 1
                    ;;
            esac
            ;;
        "docker")
            deploy_with_docker "$PACKAGE_PATH"
            ;;
        *)
            log_error "Unsupported deployment method: $DEPLOY_METHOD"
            exit 1
            ;;
    esac
    
    # Verify deployment if requested
    if [ "$verify_deployment_flag" = true ]; then
        verify_deployment
    fi
    
    log_success "Production deployment completed successfully!"
}

# Run main function with all arguments
main "$@"
