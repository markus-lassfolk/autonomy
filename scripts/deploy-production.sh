#!/bin/bash

# autonomy Production Deployment Script
# Comprehensive deployment automation with validation, monitoring, and rollback

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
BACKUP_DIR="$PROJECT_ROOT/backups"
LOG_DIR="$PROJECT_ROOT/logs"

# Default values
TARGET_HOST=""
TARGET_USER="root"
SSH_KEY=""
BINARY_NAME="autonomyd"
CONFIG_FILE="/etc/config/autonomy"
SERVICE_NAME="autonomy"
BACKUP_ENABLED=true
VALIDATION_ENABLED=true
MONITORING_ENABLED=true
ROLLBACK_ENABLED=true
DRY_RUN=false
VERBOSE=false

# Logging
LOG_FILE="$LOG_DIR/deploy-$(date +%Y%m%d_%H%M%S).log"

# Helper functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1" | tee -a "$LOG_FILE"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1" | tee -a "$LOG_FILE"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1" | tee -a "$LOG_FILE"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1" | tee -a "$LOG_FILE"
}

log_debug() {
    if [ "$VERBOSE" = true ]; then
        echo -e "${PURPLE}[DEBUG]${NC} $1" | tee -a "$LOG_FILE"
    fi
}

# Show usage
show_usage() {
    cat <<EOF
Usage: $0 [OPTIONS] --host HOST

Production Deployment Script for autonomy

Options:
  -h, --host HOST           Target host IP or hostname (required)
  -u, --user USER           SSH user (default: root)
  -k, --key KEY             SSH private key path
  -b, --binary NAME         Binary name (default: autonomyd)
  -c, --config FILE         Config file path (default: /etc/config/autonomy)
  -s, --service NAME        Service name (default: autonomy)
  --no-backup               Disable configuration backup
  --no-validation           Disable deployment validation
  --no-monitoring           Disable monitoring setup
  --no-rollback             Disable rollback capability
  --dry-run                 Show what would be done without executing
  -v, --verbose             Enable verbose output
  --help                    Show this help message

Examples:
  $0 --host 192.168.1.1 --key ~/.ssh/id_rsa
  $0 --host router.example.com --user admin --dry-run
  $0 --host 192.168.1.1 --no-backup --no-rollback

EOF
}

# Initialize deployment environment
init_deployment() {
    log_info "Initializing deployment environment..."
    
    # Create necessary directories
    mkdir -p "$BUILD_DIR" "$BACKUP_DIR" "$LOG_DIR"
    
    # Check required tools
    local missing_tools=()
    
    for tool in ssh scp go; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            missing_tools+=("$tool")
        fi
    done
    
    if [ ${#missing_tools[@]} -gt 0 ]; then
        log_error "Missing required tools: ${missing_tools[*]}"
        exit 1
    fi
    
    log_success "Deployment environment initialized"
}

# Validate target system
validate_target() {
    log_info "Validating target system: $TARGET_HOST"
    
    # Test SSH connectivity
    local ssh_cmd="ssh"
    if [ -n "$SSH_KEY" ]; then
        ssh_cmd="ssh -i $SSH_KEY"
    fi
    
    if ! $ssh_cmd -o ConnectTimeout=10 -o BatchMode=yes "$TARGET_USER@$TARGET_HOST" "echo 'SSH connection successful'" >/dev/null 2>&1; then
        log_error "Cannot connect to target system via SSH"
        return 1
    fi
    
    # Check system requirements
    local system_info
    system_info=$($ssh_cmd "$TARGET_USER@$TARGET_HOST" "
        echo \"OS: \$(cat /etc/os-release | grep PRETTY_NAME | cut -d'\"' -f2 2>/dev/null || echo 'Unknown')\"
        echo \"Arch: \$(uname -m)\"
        echo \"Memory: \$(free -h | awk '/^Mem:/{print \$2}')\"
        echo \"Disk: \$(df -h / | awk 'NR==2{print \$4}')\"
        echo \"mwan3: \$(which mwan3 2>/dev/null && echo 'Installed' || echo 'Not found')\"
        echo \"ubus: \$(which ubus 2>/dev/null && echo 'Available' || echo 'Not found')\"
    ")
    
    log_info "Target system information:"
    echo "$system_info" | while IFS= read -r line; do
        log_info "  $line"
    done
    
    # Validate requirements
    if ! echo "$system_info" | grep -q "mwan3.*Installed"; then
        log_warning "mwan3 not found - some features may not work"
    fi
    
    if ! echo "$system_info" | grep -q "ubus.*Available"; then
        log_error "ubus not available - required for autonomy operation"
        return 1
    fi
    
    log_success "Target system validation passed"
}

# Build binary
build_binary() {
    log_info "Building autonomy binary..."
    
    # Use existing build script
    if [ -f "$SCRIPT_DIR/build.sh" ]; then
        log_debug "Using existing build script"
        cd "$PROJECT_ROOT"
        
        # Build for target architecture
        local arch
        arch=$(ssh -o ConnectTimeout=10 "$TARGET_USER@$TARGET_HOST" "uname -m")
        
        case "$arch" in
            "armv7l"|"arm")
                ./scripts/build.sh --target linux/arm --strip --package
                BINARY_PATH="$BUILD_DIR/autonomyd-linux-arm"
                ;;
            "aarch64"|"arm64")
                ./scripts/build.sh --target linux/arm64 --strip --package
                BINARY_PATH="$BUILD_DIR/autonomyd-linux-arm64"
                ;;
            "x86_64")
                ./scripts/build.sh --target linux/amd64 --strip --package
                BINARY_PATH="$BUILD_DIR/autonomyd-linux-amd64"
                ;;
            *)
                log_error "Unsupported architecture: $arch"
                return 1
                ;;
        esac
        
        if [ ! -f "$BINARY_PATH" ]; then
            log_error "Build failed - binary not found"
            return 1
        fi
        
        log_success "Binary built successfully: $BINARY_PATH"
    else
        log_error "Build script not found: $SCRIPT_DIR/build.sh"
        return 1
    fi
}

# Backup current configuration
backup_config() {
    if [ "$BACKUP_ENABLED" = false ]; then
        log_info "Skipping configuration backup (disabled)"
        return 0
    fi
    
    log_info "Backing up current configuration..."
    
    local backup_file="$BACKUP_DIR/config-backup-$(date +%Y%m%d_%H%M%S).tar.gz"
    local ssh_cmd="ssh"
    if [ -n "$SSH_KEY" ]; then
        ssh_cmd="ssh -i $SSH_KEY"
    fi
    
    # Create backup on target system
    $ssh_cmd "$TARGET_USER@$TARGET_HOST" "
        mkdir -p /tmp/autonomy-backup
        if [ -f $CONFIG_FILE ]; then
            cp $CONFIG_FILE /tmp/autonomy-backup/
        fi
        if [ -f /usr/sbin/$BINARY_NAME ]; then
            cp /usr/sbin/$BINARY_NAME /tmp/autonomy-backup/
        fi
        if [ -f /etc/init.d/$SERVICE_NAME ]; then
            cp /etc/init.d/$SERVICE_NAME /tmp/autonomy-backup/
        fi
        tar -czf /tmp/autonomy-backup.tar.gz -C /tmp autonomy-backup/
    "
    
    # Download backup
    local scp_cmd="scp"
    if [ -n "$SSH_KEY" ]; then
        scp_cmd="scp -i $SSH_KEY"
    fi
    
    $scp_cmd "$TARGET_USER@$TARGET_HOST:/tmp/autonomy-backup.tar.gz" "$backup_file"
    
    # Clean up on target
    $ssh_cmd "$TARGET_USER@$TARGET_HOST" "rm -rf /tmp/autonomy-backup*"
    
    log_success "Configuration backed up to: $backup_file"
    BACKUP_FILE="$backup_file"
}

# Deploy binary and configuration
deploy_files() {
    log_info "Deploying files to target system..."
    
    local ssh_cmd="ssh"
    local scp_cmd="scp"
    if [ -n "$SSH_KEY" ]; then
        ssh_cmd="ssh -i $SSH_KEY"
        scp_cmd="scp -i $SSH_KEY"
    fi
    
    # Stop service if running
    log_debug "Stopping service if running..."
    $ssh_cmd "$TARGET_USER@$TARGET_HOST" "/etc/init.d/$SERVICE_NAME stop 2>/dev/null || true"
    
    # Deploy binary
    log_debug "Deploying binary..."
    $scp_cmd "$BINARY_PATH" "$TARGET_USER@$TARGET_HOST:/usr/sbin/$BINARY_NAME"
    $ssh_cmd "$TARGET_USER@$TARGET_HOST" "chmod +x /usr/sbin/$BINARY_NAME"
    
    # Deploy init script if it exists
    if [ -f "$SCRIPT_DIR/autonomy.init" ]; then
        log_debug "Deploying init script..."
        $scp_cmd "$SCRIPT_DIR/autonomy.init" "$TARGET_USER@$TARGET_HOST:/etc/init.d/$SERVICE_NAME"
        $ssh_cmd "$TARGET_USER@$TARGET_HOST" "chmod +x /etc/init.d/$SERVICE_NAME"
    fi
    
    # Deploy control script if it exists
    if [ -f "$SCRIPT_DIR/autonomyctl" ]; then
        log_debug "Deploying control script..."
        $scp_cmd "$SCRIPT_DIR/autonomyctl" "$TARGET_USER@$TARGET_HOST:/usr/sbin/autonomyctl"
        $ssh_cmd "$TARGET_USER@$TARGET_HOST" "chmod +x /usr/sbin/autonomyctl"
    fi
    
    log_success "Files deployed successfully"
}

# Validate deployment
validate_deployment() {
    if [ "$VALIDATION_ENABLED" = false ]; then
        log_info "Skipping deployment validation (disabled)"
        return 0
    fi
    
    log_info "Validating deployment..."
    
    local ssh_cmd="ssh"
    if [ -n "$SSH_KEY" ]; then
        ssh_cmd="ssh -i $SSH_KEY"
    fi
    
    # Check binary
    if ! $ssh_cmd "$TARGET_USER@$TARGET_HOST" "[ -x /usr/sbin/$BINARY_NAME ]"; then
        log_error "Binary validation failed"
        return 1
    fi
    
    # Test binary
    if ! $ssh_cmd "$TARGET_USER@$TARGET_HOST" "/usr/sbin/$BINARY_NAME --version 2>/dev/null || /usr/sbin/$BINARY_NAME --help 2>/dev/null"; then
        log_error "Binary test failed"
        return 1
    fi
    
    # Check init script
    if ! $ssh_cmd "$TARGET_USER@$TARGET_HOST" "[ -x /etc/init.d/$SERVICE_NAME ]"; then
        log_warning "Init script validation failed"
    fi
    
    log_success "Deployment validation passed"
}

# Start service
start_service() {
    log_info "Starting autonomy service..."
    
    local ssh_cmd="ssh"
    if [ -n "$SSH_KEY" ]; then
        ssh_cmd="ssh -i $SSH_KEY"
    fi
    
    # Start service
    $ssh_cmd "$TARGET_USER@$TARGET_HOST" "/etc/init.d/$SERVICE_NAME start"
    
    # Wait for service to start
    sleep 5
    
    # Check service status
    if $ssh_cmd "$TARGET_USER@$TARGET_HOST" "/etc/init.d/$SERVICE_NAME status >/dev/null 2>&1"; then
        log_success "Service started successfully"
    else
        log_error "Service failed to start"
        return 1
    fi
}

# Setup monitoring
setup_monitoring() {
    if [ "$MONITORING_ENABLED" = false ]; then
        log_info "Skipping monitoring setup (disabled)"
        return 0
    fi
    
    log_info "Setting up monitoring..."
    
    local ssh_cmd="ssh"
    if [ -n "$SSH_KEY" ]; then
        ssh_cmd="ssh -i $SSH_KEY"
    fi
    
    # Deploy monitoring script if it exists
    if [ -f "$SCRIPT_DIR/monitor.sh" ]; then
        log_debug "Deploying monitoring script..."
        $scp_cmd "$SCRIPT_DIR/monitor.sh" "$TARGET_USER@$TARGET_HOST:/usr/local/bin/autonomy-monitor"
        $ssh_cmd "$TARGET_USER@$TARGET_HOST" "chmod +x /usr/local/bin/autonomy-monitor"
    fi
    
    # Deploy watchdog script if it exists
    if [ -f "$SCRIPT_DIR/starwatch" ]; then
        log_debug "Deploying watchdog script..."
        $scp_cmd "$SCRIPT_DIR/starwatch" "$TARGET_USER@$TARGET_HOST:/usr/local/bin/starwatch"
        $ssh_cmd "$TARGET_USER@$TARGET_HOST" "chmod +x /usr/local/bin/starwatch"
        
        # Setup cron job for watchdog
        $ssh_cmd "$TARGET_USER@$TARGET_HOST" "
            if [ -f /usr/local/bin/starwatch ]; then
                echo '*/6 * * * * /usr/local/bin/starwatch' | crontab -
            fi
        "
    fi
    
    log_success "Monitoring setup completed"
}

# Perform health check
health_check() {
    log_info "Performing health check..."
    
    local ssh_cmd="ssh"
    if [ -n "$SSH_KEY" ]; then
        ssh_cmd="ssh -i $SSH_KEY"
    fi
    
    # Check service status
    if ! $ssh_cmd "$TARGET_USER@$TARGET_HOST" "/etc/init.d/$SERVICE_NAME status >/dev/null 2>&1"; then
        log_error "Health check failed: service not running"
        return 1
    fi
    
    # Check ubus interface
    if $ssh_cmd "$TARGET_USER@$TARGET_HOST" "ubus list | grep -q autonomy"; then
        log_success "Health check passed: ubus interface available"
    else
        log_warning "Health check warning: ubus interface not available"
    fi
    
    # Check basic functionality
    if $ssh_cmd "$TARGET_USER@$TARGET_HOST" "ubus call autonomy status >/dev/null 2>&1"; then
        log_success "Health check passed: API responding"
    else
        log_warning "Health check warning: API not responding"
    fi
    
    log_success "Health check completed"
}

# Rollback function
rollback() {
    if [ "$ROLLBACK_ENABLED" = false ]; then
        log_error "Rollback disabled - manual intervention required"
        return 1
    fi
    
    log_warning "Rolling back deployment..."
    
    if [ -z "$BACKUP_FILE" ] || [ ! -f "$BACKUP_FILE" ]; then
        log_error "No backup file available for rollback"
        return 1
    fi
    
    local ssh_cmd="ssh"
    local scp_cmd="scp"
    if [ -n "$SSH_KEY" ]; then
        ssh_cmd="ssh -i $SSH_KEY"
        scp_cmd="scp -i $SSH_KEY"
    fi
    
    # Stop service
    $ssh_cmd "$TARGET_USER@$TARGET_HOST" "/etc/init.d/$SERVICE_NAME stop 2>/dev/null || true"
    
    # Upload backup
    $scp_cmd "$BACKUP_FILE" "$TARGET_USER@$TARGET_HOST:/tmp/autonomy-backup.tar.gz"
    
    # Restore from backup
    $ssh_cmd "$TARGET_USER@$TARGET_HOST" "
        cd /tmp
        tar -xzf autonomy-backup.tar.gz
        if [ -f autonomy-backup/$BINARY_NAME ]; then
            cp autonomy-backup/$BINARY_NAME /usr/sbin/
            chmod +x /usr/sbin/$BINARY_NAME
        fi
        if [ -f autonomy-backup/$SERVICE_NAME ]; then
            cp autonomy-backup/$SERVICE_NAME /etc/init.d/
            chmod +x /etc/init.d/$SERVICE_NAME
        fi
        if [ -f autonomy-backup/autonomy ]; then
            cp autonomy-backup/autonomy $CONFIG_FILE
        fi
        rm -rf autonomy-backup*
    "
    
    # Start service
    $ssh_cmd "$TARGET_USER@$TARGET_HOST" "/etc/init.d/$SERVICE_NAME start"
    
    log_success "Rollback completed"
}

# Main deployment function
deploy() {
    log_info "Starting production deployment to $TARGET_HOST"
    
    # Initialize
    init_deployment
    
    # Validate target
    if ! validate_target; then
        log_error "Target validation failed"
        exit 1
    fi
    
    # Build binary
    if ! build_binary; then
        log_error "Build failed"
        exit 1
    fi
    
    # Backup configuration
    if ! backup_config; then
        log_error "Backup failed"
        exit 1
    fi
    
    # Deploy files
    if ! deploy_files; then
        log_error "Deployment failed"
        rollback
        exit 1
    fi
    
    # Validate deployment
    if ! validate_deployment; then
        log_error "Deployment validation failed"
        rollback
        exit 1
    fi
    
    # Start service
    if ! start_service; then
        log_error "Service start failed"
        rollback
        exit 1
    fi
    
    # Setup monitoring
    if ! setup_monitoring; then
        log_warning "Monitoring setup failed (non-critical)"
    fi
    
    # Health check
    if ! health_check; then
        log_error "Health check failed"
        rollback
        exit 1
    fi
    
    log_success "Production deployment completed successfully!"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--host)
            TARGET_HOST="$2"
            shift 2
            ;;
        -u|--user)
            TARGET_USER="$2"
            shift 2
            ;;
        -k|--key)
            SSH_KEY="$2"
            shift 2
            ;;
        -b|--binary)
            BINARY_NAME="$2"
            shift 2
            ;;
        -c|--config)
            CONFIG_FILE="$2"
            shift 2
            ;;
        -s|--service)
            SERVICE_NAME="$2"
            shift 2
            ;;
        --no-backup)
            BACKUP_ENABLED=false
            shift
            ;;
        --no-validation)
            VALIDATION_ENABLED=false
            shift
            ;;
        --no-monitoring)
            MONITORING_ENABLED=false
            shift
            ;;
        --no-rollback)
            ROLLBACK_ENABLED=false
            shift
            ;;
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        --help)
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

# Validate required parameters
if [ -z "$TARGET_HOST" ]; then
    log_error "Target host is required"
    show_usage
    exit 1
fi

# Execute deployment
if [ "$DRY_RUN" = true ]; then
    log_info "DRY RUN MODE - No changes will be made"
    log_info "Target: $TARGET_USER@$TARGET_HOST"
    log_info "Binary: $BINARY_NAME"
    log_info "Config: $CONFIG_FILE"
    log_info "Service: $SERVICE_NAME"
    log_info "Backup: $BACKUP_ENABLED"
    log_info "Validation: $VALIDATION_ENABLED"
    log_info "Monitoring: $MONITORING_ENABLED"
    log_info "Rollback: $ROLLBACK_ENABLED"
else
    deploy
fi
