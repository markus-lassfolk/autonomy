#!/bin/bash

# autonomy Monitoring Setup Script
# Configures comprehensive monitoring, alerting, and health checks

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
CONFIG_DIR="/etc/autonomy"
LOG_DIR="/var/log/autonomy"
CRON_DIR="/etc/cron.d"

# Default values
TARGET_HOST=""
TARGET_USER="root"
SSH_KEY=""
SETUP_ALERTS=true
SETUP_LOGGING=true
SETUP_CRON=true
SETUP_DASHBOARD=true
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

Monitoring Setup Script for autonomy

Options:
  -h, --host HOST           Target host IP or hostname (required)
  -u, --user USER           SSH user (default: root)
  -k, --key KEY             SSH private key path
  --no-alerts               Disable alert setup
  --no-logging              Disable logging setup
  --no-cron                 Disable cron job setup
  --no-dashboard            Disable dashboard setup
  --dry-run                 Show what would be done without executing
  -v, --verbose             Enable verbose output
  --help                    Show this help message

Examples:
  $0 --host 192.168.1.1 --key ~/.ssh/id_rsa
  $0 --host router.example.com --user admin --dry-run
  $0 --host 192.168.1.1 --no-dashboard

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

# Create monitoring directories
create_directories() {
    log_info "Creating monitoring directories..."
    
    local ssh_cmd=$(setup_ssh)
    
    $ssh_cmd "$TARGET_USER@$TARGET_HOST" "
        mkdir -p $CONFIG_DIR
        mkdir -p $LOG_DIR
        mkdir -p $CRON_DIR
        chmod 755 $CONFIG_DIR
        chmod 755 $LOG_DIR
    "
    
    log_success "Monitoring directories created"
}

# Setup logging configuration
setup_logging() {
    if [ "$SETUP_LOGGING" = false ]; then
        log_info "Skipping logging setup (disabled)"
        return 0
    fi
    
    log_info "Setting up logging configuration..."
    
    local ssh_cmd=$(setup_ssh)
    local scp_cmd=$(setup_scp)
    
    # Create logrotate configuration
    cat > /tmp/autonomy-logrotate <<EOF
$LOG_DIR/*.log {
    daily
    missingok
    rotate 7
    compress
    delaycompress
    notifempty
    create 644 root root
    postrotate
        /etc/init.d/autonomy reload >/dev/null 2>&1 || true
    endscript
}
EOF
    
    $scp_cmd /tmp/autonomy-logrotate "$TARGET_USER@$TARGET_HOST:/etc/logrotate.d/autonomy"
    rm -f /tmp/autonomy-logrotate
    
    # Create log directory with proper permissions
    $ssh_cmd "$TARGET_USER@$TARGET_HOST" "
        chmod 644 /etc/logrotate.d/autonomy
        chown root:root /etc/logrotate.d/autonomy
    "
    
    log_success "Logging configuration setup completed"
}

# Setup alert configuration
setup_alerts() {
    if [ "$SETUP_ALERTS" = false ]; then
        log_info "Skipping alert setup (disabled)"
        return 0
    fi
    
    log_info "Setting up alert configuration..."
    
    local ssh_cmd=$(setup_ssh)
    local scp_cmd=$(setup_scp)
    
    # Create alert configuration template
    cat > /tmp/autonomy-alerts.conf <<EOF
# autonomy Alert Configuration
# This file configures alert thresholds and notification settings

# Health check thresholds
HEALTH_CHECK_INTERVAL=30
HEALTH_CHECK_TIMEOUT=10
HEALTH_CHECK_RETRIES=3

# Service status alerts
SERVICE_DOWN_ALERT=true
SERVICE_RESTART_ALERT=true
SERVICE_CRASH_ALERT=true

# Performance alerts
HIGH_CPU_THRESHOLD=80
HIGH_MEMORY_THRESHOLD=85
HIGH_DISK_THRESHOLD=90
LOW_DISK_THRESHOLD=10

# Network alerts
INTERFACE_DOWN_ALERT=true
FAILOVER_ALERT=true
CONNECTION_LOSS_ALERT=true

# Notification settings
NOTIFICATION_COOLDOWN=300
NOTIFICATION_RETRY_INTERVAL=60
NOTIFICATION_MAX_RETRIES=3

# Email notifications
EMAIL_ENABLED=false
EMAIL_SMTP_SERVER=""
EMAIL_SMTP_PORT=587
EMAIL_USERNAME=""
EMAIL_PASSWORD=""
EMAIL_FROM=""
EMAIL_TO=""

# Pushover notifications
PUSHOVER_ENABLED=false
PUSHOVER_TOKEN=""
PUSHOVER_USER=""

# Slack notifications
SLACK_ENABLED=false
SLACK_WEBHOOK_URL=""
SLACK_CHANNEL="#alerts"

# Discord notifications
DISCORD_ENABLED=false
DISCORD_WEBHOOK_URL=""
DISCORD_USERNAME="autonomy Alert"

# Telegram notifications
TELEGRAM_ENABLED=false
TELEGRAM_BOT_TOKEN=""
TELEGRAM_CHAT_ID=""

# Webhook notifications
WEBHOOK_ENABLED=false
WEBHOOK_URL=""
WEBHOOK_METHOD="POST"
WEBHOOK_HEADERS="Content-Type: application/json"
EOF
    
    $scp_cmd /tmp/autonomy-alerts.conf "$TARGET_USER@$TARGET_HOST:$CONFIG_DIR/alerts.conf"
    rm -f /tmp/autonomy-alerts.conf
    
    $ssh_cmd "$TARGET_USER@$TARGET_HOST" "
        chmod 644 $CONFIG_DIR/alerts.conf
        chown root:root $CONFIG_DIR/alerts.conf
    "
    
    log_success "Alert configuration setup completed"
}

# Setup cron jobs
setup_cron() {
    if [ "$SETUP_CRON" = false ]; then
        log_info "Skipping cron setup (disabled)"
        return 0
    fi
    
    log_info "Setting up cron jobs..."
    
    local ssh_cmd=$(setup_ssh)
    
    # Create cron configuration
    cat > /tmp/autonomy-cron <<EOF
# autonomy Monitoring Cron Jobs

# Health check every 2 minutes
*/2 * * * * root /usr/local/bin/autonomy-monitor --health-check >> $LOG_DIR/health.log 2>&1

# System status check every 5 minutes
*/5 * * * * root /usr/local/bin/autonomy-monitor --system-status >> $LOG_DIR/system.log 2>&1

# Performance monitoring every 10 minutes
*/10 * * * * root /usr/local/bin/autonomy-monitor --performance >> $LOG_DIR/performance.log 2>&1

# Log rotation (daily at 2 AM)
0 2 * * * root /usr/sbin/logrotate /etc/logrotate.d/autonomy

# Cleanup old logs (weekly on Sunday at 3 AM)
0 3 * * 0 root find $LOG_DIR -name "*.log.*" -mtime +30 -delete

# Backup configuration (daily at 1 AM)
0 1 * * * root /usr/local/bin/autonomy-monitor --backup-config >> $LOG_DIR/backup.log 2>&1
EOF
    
    local scp_cmd=$(setup_scp)
    $scp_cmd /tmp/autonomy-cron "$TARGET_USER@$TARGET_HOST:$CRON_DIR/autonomy"
    rm -f /tmp/autonomy-cron
    
    $ssh_cmd "$TARGET_USER@$TARGET_HOST" "
        chmod 644 $CRON_DIR/autonomy
        chown root:root $CRON_DIR/autonomy
        /etc/init.d/cron restart 2>/dev/null || true
    "
    
    log_success "Cron jobs setup completed"
}

# Setup monitoring dashboard
setup_dashboard() {
    if [ "$SETUP_DASHBOARD" = false ]; then
        log_info "Skipping dashboard setup (disabled)"
        return 0
    fi
    
    log_info "Setting up monitoring dashboard..."
    
    local ssh_cmd=$(setup_ssh)
    local scp_cmd=$(setup_scp)
    
    # Create simple status dashboard
    cat > /tmp/autonomy-dashboard.html <<EOF
<!DOCTYPE html>
<html>
<head>
    <title>autonomy Status Dashboard</title>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f5f5f5; }
        .container { max-width: 1200px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .header { text-align: center; margin-bottom: 30px; }
        .status-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; margin-bottom: 30px; }
        .status-card { background: #f8f9fa; padding: 20px; border-radius: 6px; border-left: 4px solid #007bff; }
        .status-card.error { border-left-color: #dc3545; }
        .status-card.warning { border-left-color: #ffc107; }
        .status-card.success { border-left-color: #28a745; }
        .metric { display: flex; justify-content: space-between; margin: 10px 0; }
        .metric-label { font-weight: bold; }
        .metric-value { font-family: monospace; }
        .refresh-btn { background: #007bff; color: white; border: none; padding: 10px 20px; border-radius: 4px; cursor: pointer; }
        .refresh-btn:hover { background: #0056b3; }
        .timestamp { text-align: center; color: #666; font-size: 0.9em; margin-top: 20px; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>autonomy Status Dashboard</h1>
            <button class="refresh-btn" onclick="location.reload()">Refresh</button>
        </div>
        
        <div class="status-grid">
            <div class="status-card" id="service-status">
                <h3>Service Status</h3>
                <div id="service-details">Loading...</div>
            </div>
            
            <div class="status-card" id="system-status">
                <h3>System Status</h3>
                <div id="system-details">Loading...</div>
            </div>
            
            <div class="status-card" id="network-status">
                <h3>Network Status</h3>
                <div id="network-details">Loading...</div>
            </div>
            
            <div class="status-card" id="performance-status">
                <h3>Performance</h3>
                <div id="performance-details">Loading...</div>
            </div>
        </div>
        
        <div class="timestamp" id="last-updated">Last updated: Never</div>
    </div>
    
    <script>
        function updateStatus() {
            // This would be populated with actual API calls to autonomy
            fetch('/cgi-bin/autonomy-status')
                .then(response => response.json())
                .then(data => {
                    updateServiceStatus(data.service);
                    updateSystemStatus(data.system);
                    updateNetworkStatus(data.network);
                    updatePerformanceStatus(data.performance);
                    document.getElementById('last-updated').textContent = 'Last updated: ' + new Date().toLocaleString();
                })
                .catch(error => {
                    console.error('Error fetching status:', error);
                });
        }
        
        function updateServiceStatus(data) {
            const card = document.getElementById('service-status');
            const details = document.getElementById('service-details');
            
            card.className = 'status-card ' + (data.running ? 'success' : 'error');
            details.innerHTML = \`
                <div class="metric">
                    <span class="metric-label">Status:</span>
                    <span class="metric-value">\${data.running ? 'Running' : 'Stopped'}</span>
                </div>
                <div class="metric">
                    <span class="metric-label">Uptime:</span>
                    <span class="metric-value">\${data.uptime || 'N/A'}</span>
                </div>
                <div class="metric">
                    <span class="metric-label">Version:</span>
                    <span class="metric-value">\${data.version || 'N/A'}</span>
                </div>
            \`;
        }
        
        function updateSystemStatus(data) {
            const card = document.getElementById('system-status');
            const details = document.getElementById('system-details');
            
            const cpuStatus = data.cpu > 80 ? 'error' : data.cpu > 60 ? 'warning' : 'success';
            const memStatus = data.memory > 85 ? 'error' : data.memory > 70 ? 'warning' : 'success';
            
            card.className = 'status-card ' + (cpuStatus === 'error' || memStatus === 'error' ? 'error' : 
                                              cpuStatus === 'warning' || memStatus === 'warning' ? 'warning' : 'success');
            
            details.innerHTML = \`
                <div class="metric">
                    <span class="metric-label">CPU Usage:</span>
                    <span class="metric-value">\${data.cpu || 0}%</span>
                </div>
                <div class="metric">
                    <span class="metric-label">Memory Usage:</span>
                    <span class="metric-value">\${data.memory || 0}%</span>
                </div>
                <div class="metric">
                    <span class="metric-label">Disk Usage:</span>
                    <span class="metric-value">\${data.disk || 0}%</span>
                </div>
                <div class="metric">
                    <span class="metric-label">Load Average:</span>
                    <span class="metric-value">\${data.load || 'N/A'}</span>
                </div>
            \`;
        }
        
        function updateNetworkStatus(data) {
            const card = document.getElementById('network-status');
            const details = document.getElementById('network-details');
            
            details.innerHTML = \`
                <div class="metric">
                    <span class="metric-label">Active Interfaces:</span>
                    <span class="metric-value">\${data.active_interfaces || 0}</span>
                </div>
                <div class="metric">
                    <span class="metric-label">Primary Interface:</span>
                    <span class="metric-value">\${data.primary_interface || 'N/A'}</span>
                </div>
                <div class="metric">
                    <span class="metric-label">Failover Count:</span>
                    <span class="metric-value">\${data.failover_count || 0}</span>
                </div>
                <div class="metric">
                    <span class="metric-label">Last Failover:</span>
                    <span class="metric-value">\${data.last_failover || 'N/A'}</span>
                </div>
            \`;
        }
        
        function updatePerformanceStatus(data) {
            const card = document.getElementById('performance-status');
            const details = document.getElementById('performance-details');
            
            details.innerHTML = \`
                <div class="metric">
                    <span class="metric-label">Response Time:</span>
                    <span class="metric-value">\${data.response_time || 'N/A'} ms</span>
                </div>
                <div class="metric">
                    <span class="metric-label">Throughput:</span>
                    <span class="metric-value">\${data.throughput || 'N/A'} Mbps</span>
                </div>
                <div class="metric">
                    <span class="metric-label">Error Rate:</span>
                    <span class="metric-value">\${data.error_rate || 0}%</span>
                </div>
                <div class="metric">
                    <span class="metric-label">Health Score:</span>
                    <span class="metric-value">\${data.health_score || 'N/A'}</span>
                </div>
            \`;
        }
        
        // Update status every 30 seconds
        updateStatus();
        setInterval(updateStatus, 30000);
    </script>
</body>
</html>
EOF
    
    $scp_cmd /tmp/autonomy-dashboard.html "$TARGET_USER@$TARGET_HOST:/www/autonomy-dashboard.html"
    rm -f /tmp/autonomy-dashboard.html
    
    $ssh_cmd "$TARGET_USER@$TARGET_HOST" "
        chmod 644 /www/autonomy-dashboard.html
        chown root:root /www/autonomy-dashboard.html
    "
    
    log_success "Dashboard setup completed"
}

# Setup health check script
setup_health_check() {
    log_info "Setting up health check script..."
    
    local ssh_cmd=$(setup_ssh)
    local scp_cmd=$(setup_scp)
    
    # Create health check script
    cat > /tmp/autonomy-health-check.sh <<'EOF'
#!/bin/bash

# autonomy Health Check Script
# Performs comprehensive health checks on the autonomy system

set -e

# Configuration
LOG_FILE="/var/log/autonomy/health.log"
ALERT_CONFIG="/etc/autonomy/alerts.conf"
SERVICE_NAME="autonomy"

# Load alert configuration
if [ -f "$ALERT_CONFIG" ]; then
    source "$ALERT_CONFIG"
fi

# Default values
HEALTH_CHECK_INTERVAL=${HEALTH_CHECK_INTERVAL:-30}
HEALTH_CHECK_TIMEOUT=${HEALTH_CHECK_TIMEOUT:-10}
HEALTH_CHECK_RETRIES=${HEALTH_CHECK_RETRIES:-3}

# Logging function
log() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') health-check: $*" | logger -t autonomy-health
    echo "$(date '+%Y-%m-%d %H:%M:%S') $*" >> "$LOG_FILE"
}

# Check service status
check_service_status() {
    if ! /etc/init.d/$SERVICE_NAME status >/dev/null 2>&1; then
        log "ERROR: Service $SERVICE_NAME is not running"
        return 1
    fi
    
    log "Service $SERVICE_NAME is running"
    return 0
}

# Check ubus interface
check_ubus_interface() {
    if ! ubus list | grep -q "^autonomy$"; then
        log "ERROR: ubus interface 'autonomy' not available"
        return 1
    fi
    
    if ! timeout $HEALTH_CHECK_TIMEOUT ubus call autonomy status >/dev/null 2>&1; then
        log "ERROR: ubus call to autonomy failed"
        return 1
    fi
    
    log "ubus interface is responding"
    return 0
}

# Check system resources
check_system_resources() {
    # Check CPU usage
    local cpu_usage=$(top -bn1 | grep "Cpu(s)" | awk '{print $2}' | cut -d'%' -f1)
    if [ "$cpu_usage" -gt "${HIGH_CPU_THRESHOLD:-80}" ]; then
        log "WARNING: High CPU usage: ${cpu_usage}%"
    fi
    
    # Check memory usage
    local mem_usage=$(free | awk '/Mem:/ {printf "%.0f", $3/$2 * 100}')
    if [ "$mem_usage" -gt "${HIGH_MEMORY_THRESHOLD:-85}" ]; then
        log "WARNING: High memory usage: ${mem_usage}%"
    fi
    
    # Check disk usage
    local disk_usage=$(df / | awk 'NR==2 {print $5}' | cut -d'%' -f1)
    if [ "$disk_usage" -gt "${HIGH_DISK_THRESHOLD:-90}" ]; then
        log "ERROR: High disk usage: ${disk_usage}%"
        return 1
    fi
    
    log "System resources OK - CPU: ${cpu_usage}%, Memory: ${mem_usage}%, Disk: ${disk_usage}%"
    return 0
}

# Check network interfaces
check_network_interfaces() {
    local failed_interfaces=0
    
    # Check if mwan3 is available
    if command -v mwan3 >/dev/null 2>&1; then
        if ! mwan3 status >/dev/null 2>&1; then
            log "ERROR: mwan3 status check failed"
            failed_interfaces=$((failed_interfaces + 1))
        fi
    fi
    
    # Check for network connectivity
    if ! ping -c 1 -W 5 8.8.8.8 >/dev/null 2>&1; then
        log "WARNING: No internet connectivity detected"
    fi
    
    if [ $failed_interfaces -eq 0 ]; then
        log "Network interfaces OK"
        return 0
    else
        return 1
    fi
}

# Main health check function
main() {
    local exit_code=0
    
    log "Starting health check"
    
    # Check service status
    if ! check_service_status; then
        exit_code=1
    fi
    
    # Check ubus interface
    if ! check_ubus_interface; then
        exit_code=1
    fi
    
    # Check system resources
    if ! check_system_resources; then
        exit_code=1
    fi
    
    # Check network interfaces
    if ! check_network_interfaces; then
        exit_code=1
    fi
    
    if [ $exit_code -eq 0 ]; then
        log "Health check passed"
    else
        log "Health check failed"
    fi
    
    exit $exit_code
}

# Run main function
main "$@"
EOF
    
    $scp_cmd /tmp/autonomy-health-check.sh "$TARGET_USER@$TARGET_HOST:/usr/local/bin/autonomy-health-check"
    rm -f /tmp/autonomy-health-check.sh
    
    $ssh_cmd "$TARGET_USER@$TARGET_HOST" "
        chmod +x /usr/local/bin/autonomy-health-check
        chown root:root /usr/local/bin/autonomy-health-check
    "
    
    log_success "Health check script setup completed"
}

# Validate monitoring setup
validate_setup() {
    log_info "Validating monitoring setup..."
    
    local ssh_cmd=$(setup_ssh)
    local validation_passed=true
    
    # Check directories
    if ! $ssh_cmd "$TARGET_USER@$TARGET_HOST" "[ -d $CONFIG_DIR ]"; then
        log_error "Configuration directory not found"
        validation_passed=false
    fi
    
    if ! $ssh_cmd "$TARGET_USER@$TARGET_HOST" "[ -d $LOG_DIR ]"; then
        log_error "Log directory not found"
        validation_passed=false
    fi
    
    # Check configuration files
    if [ "$SETUP_ALERTS" = true ]; then
        if ! $ssh_cmd "$TARGET_USER@$TARGET_HOST" "[ -f $CONFIG_DIR/alerts.conf ]"; then
            log_error "Alert configuration not found"
            validation_passed=false
        fi
    fi
    
    # Check cron jobs
    if [ "$SETUP_CRON" = true ]; then
        if ! $ssh_cmd "$TARGET_USER@$TARGET_HOST" "[ -f $CRON_DIR/autonomy ]"; then
            log_error "Cron configuration not found"
            validation_passed=false
        fi
    fi
    
    # Check health check script
    if ! $ssh_cmd "$TARGET_USER@$TARGET_HOST" "[ -x /usr/local/bin/autonomy-health-check ]"; then
        log_error "Health check script not found or not executable"
        validation_passed=false
    fi
    
    if [ "$validation_passed" = true ]; then
        log_success "Monitoring setup validation passed"
    else
        log_error "Monitoring setup validation failed"
        return 1
    fi
}

# Main setup function
setup_monitoring() {
    log_info "Starting monitoring setup for $TARGET_HOST"
    
    # Create directories
    create_directories
    
    # Setup logging
    setup_logging
    
    # Setup alerts
    setup_alerts
    
    # Setup cron jobs
    setup_cron
    
    # Setup dashboard
    setup_dashboard
    
    # Setup health check
    setup_health_check
    
    # Validate setup
    validate_setup
    
    log_success "Monitoring setup completed successfully!"
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
        --no-alerts)
            SETUP_ALERTS=false
            shift
            ;;
        --no-logging)
            SETUP_LOGGING=false
            shift
            ;;
        --no-cron)
            SETUP_CRON=false
            shift
            ;;
        --no-dashboard)
            SETUP_DASHBOARD=false
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

# Execute setup
if [ "$DRY_RUN" = true ]; then
    log_info "DRY RUN MODE - No changes will be made"
    log_info "Target: $TARGET_USER@$TARGET_HOST"
    log_info "Setup Alerts: $SETUP_ALERTS"
    log_info "Setup Logging: $SETUP_LOGGING"
    log_info "Setup Cron: $SETUP_CRON"
    log_info "Setup Dashboard: $SETUP_DASHBOARD"
else
    setup_monitoring
fi
