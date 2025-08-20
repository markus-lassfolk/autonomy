#!/bin/bash

# autonomy Backup and Recovery Script
# Comprehensive backup, recovery, and disaster recovery procedures

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
BACKUP_DIR="/var/backups/autonomy"
REMOTE_BACKUP_DIR=""
CONFIG_DIR="/etc/autonomy"
LOG_DIR="/var/log/autonomy"
SERVICE_NAME="autonomy"
BINARY_NAME="autonomyd"

# Default values
TARGET_HOST=""
TARGET_USER="root"
SSH_KEY=""
BACKUP_TYPE="full"
RECOVERY_MODE=false
TEST_RECOVERY=false
COMPRESS_BACKUP=true
ENCRYPT_BACKUP=false
BACKUP_PASSPHRASE=""
VERBOSE=false
DRY_RUN=false

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

log_debug() {
    if [ "$VERBOSE" = true ]; then
        echo -e "${PURPLE}[DEBUG]${NC} $1"
    fi
}

# Show usage
show_usage() {
    cat <<EOF
Usage: $0 [OPTIONS] --host HOST

Backup and Recovery Script for autonomy

Commands:
  backup                     Create backup (default)
  recover                    Recover from backup
  test-recovery             Test recovery procedure
  list-backups              List available backups
  verify-backup BACKUP      Verify backup integrity

Options:
  -h, --host HOST           Target host IP or hostname (required)
  -u, --user USER           SSH user (default: root)
  -k, --key KEY             SSH private key path
  -t, --type TYPE           Backup type: full|config|binary (default: full)
  -d, --dir DIR             Backup directory (default: /var/backups/autonomy)
  -r, --remote-dir DIR      Remote backup directory
  --compress                Compress backup (default: true)
  --encrypt                 Encrypt backup with passphrase
  --passphrase PASS         Encryption passphrase
  --dry-run                 Show what would be done without executing
  -v, --verbose             Enable verbose output
  --help                    Show this help message

Examples:
  $0 backup --host 192.168.1.1 --key ~/.ssh/id_rsa
  $0 recover --host 192.168.1.1 --backup config-backup-20250120.tar.gz
  $0 test-recovery --host 192.168.1.1 --dry-run
  $0 list-backups --host 192.168.1.1

EOF
}

# Setup SSH connection
setup_ssh() {
    local ssh_cmd="ssh"
    if [ -n "$SSH_KEY" ]; then
        ssh_cmd="ssh -i $SSH_KEY"
    fi
    echo "$ssh_cmd"
}

# Setup SCP connection
setup_scp() {
    local scp_cmd="scp"
    if [ -n "$SSH_KEY" ]; then
        scp_cmd="scp -i $SSH_KEY"
    fi
    echo "$scp_cmd"
}

# Create backup directories
create_backup_dirs() {
    log_info "Creating backup directories..."
    
    local ssh_cmd=$(setup_ssh)
    
    $ssh_cmd "$TARGET_USER@$TARGET_HOST" "
        mkdir -p $BACKUP_DIR
        mkdir -p $BACKUP_DIR/configs
        mkdir -p $BACKUP_DIR/binaries
        mkdir -p $BACKUP_DIR/logs
        chmod 755 $BACKUP_DIR
        chmod 755 $BACKUP_DIR/configs
        chmod 755 $BACKUP_DIR/binaries
        chmod 755 $BACKUP_DIR/logs
    "
    
    log_success "Backup directories created"
}

# Create full backup
create_full_backup() {
    log_info "Creating full backup..."
    
    local ssh_cmd=$(setup_ssh)
    local timestamp=$(date +%Y%m%d_%H%M%S)
    local backup_file="$BACKUP_DIR/autonomy-full-backup-$timestamp.tar.gz"
    
    # Create backup on target system
    $ssh_cmd "$TARGET_USER@$TARGET_HOST" "
        cd /tmp
        mkdir -p autonomy-backup
        
        # Backup configuration files
        if [ -d $CONFIG_DIR ]; then
            cp -r $CONFIG_DIR autonomy-backup/
        fi
        
        # Backup binary
        if [ -f /usr/sbin/$BINARY_NAME ]; then
            cp /usr/sbin/$BINARY_NAME autonomy-backup/
        fi
        
        # Backup init script
        if [ -f /etc/init.d/$SERVICE_NAME ]; then
            cp /etc/init.d/$SERVICE_NAME autonomy-backup/
        fi
        
        # Backup control script
        if [ -f /usr/sbin/autonomyctl ]; then
            cp /usr/sbin/autonomyctl autonomy-backup/
        fi
        
        # Backup logs (last 7 days)
        if [ -d $LOG_DIR ]; then
            mkdir -p autonomy-backup/logs
            find $LOG_DIR -name '*.log' -mtime -7 -exec cp {} autonomy-backup/logs/ \;
        fi
        
        # Backup system information
        echo \"Backup created: $(date)\" > autonomy-backup/backup-info.txt
        echo \"Hostname: \$(hostname)\" >> autonomy-backup/backup-info.txt
        echo \"OS: \$(cat /etc/os-release | grep PRETTY_NAME | cut -d'\"' -f2 2>/dev/null || echo 'Unknown')\" >> autonomy-backup/backup-info.txt
        echo \"Architecture: \$(uname -m)\" >> autonomy-backup/backup-info.txt
        echo \"Kernel: \$(uname -r)\" >> autonomy-backup/backup-info.txt
        
        # Create archive
        tar -czf $backup_file autonomy-backup/
        rm -rf autonomy-backup
        
        # Encrypt if requested
        if [ \"$ENCRYPT_BACKUP\" = true ] && [ -n \"$BACKUP_PASSPHRASE\" ]; then
            echo \"$BACKUP_PASSPHRASE\" | gpg --batch --yes --passphrase-fd 0 --symmetric $backup_file
            rm $backup_file
            backup_file=\"$backup_file.gpg\"
        fi
        
        echo \"$backup_file\"
    "
    
    log_success "Full backup created: $backup_file"
    echo "$backup_file"
}

# Create configuration backup
create_config_backup() {
    log_info "Creating configuration backup..."
    
    local ssh_cmd=$(setup_ssh)
    local timestamp=$(date +%Y%m%d_%H%M%S)
    local backup_file="$BACKUP_DIR/configs/autonomy-config-backup-$timestamp.tar.gz"
    
    # Create backup on target system
    $ssh_cmd "$TARGET_USER@$TARGET_HOST" "
        cd /tmp
        mkdir -p autonomy-config-backup
        
        # Backup configuration files
        if [ -d $CONFIG_DIR ]; then
            cp -r $CONFIG_DIR autonomy-config-backup/
        fi
        
        # Backup UCI configuration
        if command -v uci >/dev/null 2>&1; then
            uci export autonomy > autonomy-config-backup/uci-autonomy.conf 2>/dev/null || true
        fi
        
        # Create archive
        tar -czf $backup_file autonomy-config-backup/
        rm -rf autonomy-config-backup
        
        # Encrypt if requested
        if [ \"$ENCRYPT_BACKUP\" = true ] && [ -n \"$BACKUP_PASSPHRASE\" ]; then
            echo \"$BACKUP_PASSPHRASE\" | gpg --batch --yes --passphrase-fd 0 --symmetric $backup_file
            rm $backup_file
            backup_file=\"$backup_file.gpg\"
        fi
        
        echo \"$backup_file\"
    "
    
    log_success "Configuration backup created: $backup_file"
    echo "$backup_file"
}

# Create binary backup
create_binary_backup() {
    log_info "Creating binary backup..."
    
    local ssh_cmd=$(setup_ssh)
    local timestamp=$(date +%Y%m%d_%H%M%S)
    local backup_file="$BACKUP_DIR/binaries/autonomy-binary-backup-$timestamp.tar.gz"
    
    # Create backup on target system
    $ssh_cmd "$TARGET_USER@$TARGET_HOST" "
        cd /tmp
        mkdir -p autonomy-binary-backup
        
        # Backup binary
        if [ -f /usr/sbin/$BINARY_NAME ]; then
            cp /usr/sbin/$BINARY_NAME autonomy-binary-backup/
        fi
        
        # Backup init script
        if [ -f /etc/init.d/$SERVICE_NAME ]; then
            cp /etc/init.d/$SERVICE_NAME autonomy-binary-backup/
        fi
        
        # Backup control script
        if [ -f /usr/sbin/autonomyctl ]; then
            cp /usr/sbin/autonomyctl autonomy-binary-backup/
        fi
        
        # Backup system information
        echo \"Binary backup created: $(date)\" > autonomy-binary-backup/backup-info.txt
        echo \"Architecture: \$(uname -m)\" >> autonomy-binary-backup/backup-info.txt
        echo \"OS: \$(cat /etc/os-release | grep PRETTY_NAME | cut -d'\"' -f2 2>/dev/null || echo 'Unknown')\" >> autonomy-binary-backup/backup-info.txt
        
        # Create archive
        tar -czf $backup_file autonomy-binary-backup/
        rm -rf autonomy-binary-backup
        
        # Encrypt if requested
        if [ \"$ENCRYPT_BACKUP\" = true ] && [ -n \"$BACKUP_PASSPHRASE\" ]; then
            echo \"$BACKUP_PASSPHRASE\" | gpg --batch --yes --passphrase-fd 0 --symmetric $backup_file
            rm $backup_file
            backup_file=\"$backup_file.gpg\"
        fi
        
        echo \"$backup_file\"
    "
    
    log_success "Binary backup created: $backup_file"
    echo "$backup_file"
}

# List available backups
list_backups() {
    log_info "Listing available backups..."
    
    local ssh_cmd=$(setup_ssh)
    
    $ssh_cmd "$TARGET_USER@$TARGET_HOST" "
        echo \"=== Full Backups ===\"
        ls -la $BACKUP_DIR/autonomy-full-backup-*.tar.gz* 2>/dev/null || echo \"No full backups found\"
        
        echo \"\n=== Configuration Backups ===\"
        ls -la $BACKUP_DIR/configs/autonomy-config-backup-*.tar.gz* 2>/dev/null || echo \"No configuration backups found\"
        
        echo \"\n=== Binary Backups ===\"
        ls -la $BACKUP_DIR/binaries/autonomy-binary-backup-*.tar.gz* 2>/dev/null || echo \"No binary backups found\"
        
        echo \"\n=== Backup Directory Usage ===\"
        du -sh $BACKUP_DIR
    "
}

# Verify backup integrity
verify_backup() {
    local backup_file="$1"
    
    if [ -z "$backup_file" ]; then
        log_error "Backup file not specified"
        return 1
    fi
    
    log_info "Verifying backup: $backup_file"
    
    local ssh_cmd=$(setup_ssh)
    
    # Check if backup file exists
    if ! $ssh_cmd "$TARGET_USER@$TARGET_HOST" "[ -f $backup_file ]"; then
        log_error "Backup file not found: $backup_file"
        return 1
    fi
    
    # Verify archive integrity
    if [[ "$backup_file" == *.gpg ]]; then
        # Encrypted backup
        if [ -z "$BACKUP_PASSPHRASE" ]; then
            log_error "Passphrase required for encrypted backup"
            return 1
        fi
        
        $ssh_cmd "$TARGET_USER@$TARGET_HOST" "
            echo \"$BACKUP_PASSPHRASE\" | gpg --batch --yes --passphrase-fd 0 --decrypt $backup_file | tar -tz >/dev/null
        "
    else
        # Regular backup
        $ssh_cmd "$TARGET_USER@$TARGET_HOST" "tar -tzf $backup_file >/dev/null"
    fi
    
    if [ $? -eq 0 ]; then
        log_success "Backup verification passed: $backup_file"
    else
        log_error "Backup verification failed: $backup_file"
        return 1
    fi
}

# Recover from backup
recover_from_backup() {
    local backup_file="$1"
    
    if [ -z "$backup_file" ]; then
        log_error "Backup file not specified"
        return 1
    fi
    
    log_info "Recovering from backup: $backup_file"
    
    local ssh_cmd=$(setup_ssh)
    
    # Check if backup file exists
    if ! $ssh_cmd "$TARGET_USER@$TARGET_HOST" "[ -f $backup_file ]"; then
        log_error "Backup file not found: $backup_file"
        return 1
    fi
    
    # Stop service before recovery
    log_info "Stopping autonomy service..."
    $ssh_cmd "$TARGET_USER@$TARGET_HOST" "/etc/init.d/$SERVICE_NAME stop 2>/dev/null || true"
    
    # Create recovery directory
    $ssh_cmd "$TARGET_USER@$TARGET_HOST" "
        cd /tmp
        mkdir -p autonomy-recovery
        
        # Extract backup
        if [[ \"$backup_file\" == *.gpg ]]; then
            echo \"$BACKUP_PASSPHRASE\" | gpg --batch --yes --passphrase-fd 0 --decrypt $backup_file | tar -xzf - -C autonomy-recovery/
        else
            tar -xzf $backup_file -C autonomy-recovery/
        fi
        
        # Find the actual backup directory
        RECOVERY_DIR=\$(find autonomy-recovery -type d -name 'autonomy*backup' | head -1)
        if [ -z \"\$RECOVERY_DIR\" ]; then
            RECOVERY_DIR=autonomy-recovery
        fi
        
        # Restore configuration
        if [ -d \"\$RECOVERY_DIR/$CONFIG_DIR\" ]; then
            mkdir -p $CONFIG_DIR
            cp -r \"\$RECOVERY_DIR/$CONFIG_DIR\"/* $CONFIG_DIR/
            chmod -R 644 $CONFIG_DIR/*
            chmod 755 $CONFIG_DIR
        fi
        
        # Restore binary
        if [ -f \"\$RECOVERY_DIR/$BINARY_NAME\" ]; then
            cp \"\$RECOVERY_DIR/$BINARY_NAME\" /usr/sbin/
            chmod +x /usr/sbin/$BINARY_NAME
        fi
        
        # Restore init script
        if [ -f \"\$RECOVERY_DIR/$SERVICE_NAME\" ]; then
            cp \"\$RECOVERY_DIR/$SERVICE_NAME\" /etc/init.d/
            chmod +x /etc/init.d/$SERVICE_NAME
        fi
        
        # Restore control script
        if [ -f \"\$RECOVERY_DIR/autonomyctl\" ]; then
            cp \"\$RECOVERY_DIR/autonomyctl\" /usr/sbin/
            chmod +x /usr/sbin/autonomyctl
        fi
        
        # Restore UCI configuration if available
        if [ -f \"\$RECOVERY_DIR/uci-autonomy.conf\" ]; then
            uci import autonomy < \"\$RECOVERY_DIR/uci-autonomy.conf\" 2>/dev/null || true
        fi
        
        # Clean up
        rm -rf autonomy-recovery
    "
    
    # Start service
    log_info "Starting autonomy service..."
    $ssh_cmd "$TARGET_USER@$TARGET_HOST" "/etc/init.d/$SERVICE_NAME start"
    
    # Verify recovery
    sleep 5
    if $ssh_cmd "$TARGET_USER@$TARGET_HOST" "/etc/init.d/$SERVICE_NAME status >/dev/null 2>&1"; then
        log_success "Recovery completed successfully"
    else
        log_error "Recovery failed - service not running"
        return 1
    fi
}

# Test recovery procedure
test_recovery() {
    log_info "Testing recovery procedure..."
    
    local ssh_cmd=$(setup_ssh)
    
    # Create test backup
    log_info "Creating test backup..."
    local test_backup=$(create_config_backup)
    
    if [ -z "$test_backup" ]; then
        log_error "Failed to create test backup"
        return 1
    fi
    
    # Verify test backup
    log_info "Verifying test backup..."
    if ! verify_backup "$test_backup"; then
        log_error "Test backup verification failed"
        return 1
    fi
    
    # Test recovery
    log_info "Testing recovery from backup..."
    if ! recover_from_backup "$test_backup"; then
        log_error "Test recovery failed"
        return 1
    fi
    
    # Clean up test backup
    log_info "Cleaning up test backup..."
    $ssh_cmd "$TARGET_USER@$TARGET_HOST" "rm -f $test_backup"
    
    log_success "Recovery test completed successfully"
}

# Setup automated backup
setup_automated_backup() {
    log_info "Setting up automated backup..."
    
    local ssh_cmd=$(setup_ssh)
    local scp_cmd=$(setup_scp)
    
    # Create automated backup script
    cat > /tmp/autonomy-auto-backup.sh <<'EOF'
#!/bin/bash

# autonomy Automated Backup Script
# Runs daily to create configuration backups

set -e

# Configuration
BACKUP_DIR="/var/backups/autonomy"
CONFIG_DIR="/etc/autonomy"
LOG_FILE="/var/log/autonomy/backup.log"
RETENTION_DAYS=30

# Logging function
log() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') auto-backup: $*" | logger -t autonomy-backup
    echo "$(date '+%Y-%m-%d %H:%M:%S') $*" >> "$LOG_FILE"
}

# Create backup
create_backup() {
    local timestamp=$(date +%Y%m%d_%H%M%S)
    local backup_file="$BACKUP_DIR/configs/autonomy-auto-backup-$timestamp.tar.gz"
    
    log "Creating automated backup: $backup_file"
    
    cd /tmp
    mkdir -p autonomy-auto-backup
    
    # Backup configuration
    if [ -d "$CONFIG_DIR" ]; then
        cp -r "$CONFIG_DIR" autonomy-auto-backup/
    fi
    
    # Backup UCI configuration
    if command -v uci >/dev/null 2>&1; then
        uci export autonomy > autonomy-auto-backup/uci-autonomy.conf 2>/dev/null || true
    fi
    
    # Create archive
    tar -czf "$backup_file" autonomy-auto-backup/
    rm -rf autonomy-auto-backup
    
    log "Backup created successfully: $backup_file"
}

# Cleanup old backups
cleanup_old_backups() {
    log "Cleaning up backups older than $RETENTION_DAYS days"
    
    find "$BACKUP_DIR" -name "autonomy-auto-backup-*.tar.gz" -mtime +$RETENTION_DAYS -delete 2>/dev/null || true
    find "$BACKUP_DIR" -name "autonomy-auto-backup-*.tar.gz.gpg" -mtime +$RETENTION_DAYS -delete 2>/dev/null || true
    
    log "Cleanup completed"
}

# Main function
main() {
    log "Starting automated backup"
    
    # Create backup directory if it doesn't exist
    mkdir -p "$BACKUP_DIR/configs"
    
    # Create backup
    create_backup
    
    # Cleanup old backups
    cleanup_old_backups
    
    log "Automated backup completed"
}

# Run main function
main "$@"
EOF
    
    $scp_cmd /tmp/autonomy-auto-backup.sh "$TARGET_USER@$TARGET_HOST:/usr/local/bin/autonomy-auto-backup"
    rm -f /tmp/autonomy-auto-backup.sh
    
    $ssh_cmd "$TARGET_USER@$TARGET_HOST" "
        chmod +x /usr/local/bin/autonomy-auto-backup
        chown root:root /usr/local/bin/autonomy-auto-backup
        
        # Add to crontab (daily at 2 AM)
        (crontab -l 2>/dev/null; echo \"0 2 * * * /usr/local/bin/autonomy-auto-backup\") | crontab -
    "
    
    log_success "Automated backup setup completed"
}

# Main backup function
perform_backup() {
    log_info "Starting backup procedure for $TARGET_HOST"
    
    # Create backup directories
    create_backup_dirs
    
    # Create backup based on type
    case "$BACKUP_TYPE" in
        "full")
            create_full_backup
            ;;
        "config")
            create_config_backup
            ;;
        "binary")
            create_binary_backup
            ;;
        *)
            log_error "Unknown backup type: $BACKUP_TYPE"
            return 1
            ;;
    esac
    
    log_success "Backup procedure completed successfully!"
}

# Main recovery function
perform_recovery() {
    log_info "Starting recovery procedure for $TARGET_HOST"
    
    if [ -z "$RECOVERY_BACKUP" ]; then
        log_error "Recovery backup file not specified"
        return 1
    fi
    
    # Verify backup before recovery
    if ! verify_backup "$RECOVERY_BACKUP"; then
        log_error "Backup verification failed - cannot proceed with recovery"
        return 1
    fi
    
    # Perform recovery
    if ! recover_from_backup "$RECOVERY_BACKUP"; then
        log_error "Recovery failed"
        return 1
    fi
    
    log_success "Recovery procedure completed successfully!"
}

# Parse command line arguments
COMMAND="backup"
RECOVERY_BACKUP=""

while [[ $# -gt 0 ]]; do
    case $1 in
        backup|recover|test-recovery|list-backups|verify-backup)
            COMMAND="$1"
            shift
            ;;
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
        -t|--type)
            BACKUP_TYPE="$2"
            shift 2
            ;;
        -d|--dir)
            BACKUP_DIR="$2"
            shift 2
            ;;
        -r|--remote-dir)
            REMOTE_BACKUP_DIR="$2"
            shift 2
            ;;
        --compress)
            COMPRESS_BACKUP=true
            shift
            ;;
        --encrypt)
            ENCRYPT_BACKUP=true
            shift
            ;;
        --passphrase)
            BACKUP_PASSPHRASE="$2"
            shift 2
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
            if [ "$COMMAND" = "recover" ] || [ "$COMMAND" = "verify-backup" ]; then
                RECOVERY_BACKUP="$1"
            else
                log_error "Unknown option: $1"
                show_usage
                exit 1
            fi
            shift
            ;;
    esac
done

# Validate required parameters
if [ -z "$TARGET_HOST" ]; then
    log_error "Target host is required"
    show_usage
    exit 1
fi

# Execute command
if [ "$DRY_RUN" = true ]; then
    log_info "DRY RUN MODE - No changes will be made"
    log_info "Command: $COMMAND"
    log_info "Target: $TARGET_USER@$TARGET_HOST"
    log_info "Backup Type: $BACKUP_TYPE"
    log_info "Backup Directory: $BACKUP_DIR"
    if [ -n "$RECOVERY_BACKUP" ]; then
        log_info "Recovery Backup: $RECOVERY_BACKUP"
    fi
else
    case "$COMMAND" in
        "backup")
            perform_backup
            ;;
        "recover")
            perform_recovery
            ;;
        "test-recovery")
            test_recovery
            ;;
        "list-backups")
            list_backups
            ;;
        "verify-backup")
            if [ -z "$RECOVERY_BACKUP" ]; then
                log_error "Backup file required for verification"
                exit 1
            fi
            verify_backup "$RECOVERY_BACKUP"
            ;;
        *)
            log_error "Unknown command: $COMMAND"
            show_usage
            exit 1
            ;;
    esac
fi
