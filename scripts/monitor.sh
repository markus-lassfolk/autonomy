#!/bin/bash

# autonomy Monitoring Script
# This script runs the autonomy daemon in monitoring mode with comprehensive logging

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Default values
CONFIG_FILE="/etc/config/autonomy"
LOG_LEVEL="trace"
MONITOR_MODE=true
VERBOSE=true
FOREGROUND=true
PROFILE=false
AUDIT=false
SYSMGMT=false

# Function to print colored output
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_debug() {
    echo -e "${BLUE}[DEBUG]${NC} $1"
}

print_monitor() {
    echo -e "${PURPLE}[MONITOR]${NC} $1"
}

# Function to show usage
show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -c, --config FILE     Configuration file path (default: /etc/config/autonomy)"
    echo "  -l, --log-level LEVEL Log level: debug|info|warn|error|trace (default: trace)"
    echo "  -m, --monitor         Run in monitoring mode (default: true)"
    echo "  -v, --verbose         Enable verbose logging (default: true)"
    echo "  -f, --foreground      Run in foreground mode (default: true)"
    echo "  -p, --profile         Enable performance profiling"
    echo "  -a, --audit           Enable security auditing"
    echo "  -s, --sysmgmt         Run system management instead of main daemon"
    echo "  -h, --help            Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                                    # Run main daemon in monitoring mode"
    echo "  $0 -s                                 # Run system management in monitoring mode"
    echo "  $0 -l debug -p                        # Run with debug level and profiling"
    echo "  $0 -c /tmp/test.conf -v               # Run with custom config and verbose"
    echo ""
}

# Function to check if daemon is already running
check_daemon_running() {
    if pgrep -f "autonomyd" >/dev/null; then
        print_warning "autonomy daemon is already running"
        read -p "Do you want to stop it and start monitoring mode? (y/N): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            print_status "Stopping existing daemon..."
            pkill -f "autonomyd" || true
            sleep 2
        else
            print_error "Exiting. Please stop the daemon manually first."
            exit 1
        fi
    fi
}

# Function to check dependencies
check_dependencies() {
    print_debug "Checking dependencies..."

    # Check if autonomyd binary exists
    if [ ! -f "/usr/sbin/autonomyd" ]; then
        print_error "autonomyd binary not found at /usr/sbin/autonomyd"
        print_error "Please build and install the daemon first"
        exit 1
    fi

    # Check if autonomysysmgmt binary exists (if needed)
    if [ "$SYSMGMT" = true ] && [ ! -f "/usr/sbin/autonomysysmgmt" ]; then
        print_error "autonomysysmgmt binary not found at /usr/sbin/autonomysysmgmt"
        print_error "Please build and install the system management daemon first"
        exit 1
    fi

    # Check if config file exists
    if [ ! -f "$CONFIG_FILE" ]; then
        print_warning "Configuration file not found at $CONFIG_FILE"
        print_warning "Using default configuration"
    fi

    print_status "Dependencies check passed"
}

# Function to setup monitoring environment
setup_monitoring() {
    print_debug "Setting up monitoring environment..."

    # Create log directory if it doesn't exist
    mkdir -p /tmp/autonomy/logs

    # Set environment variables for monitoring
    export autonomy_MONITOR_MODE=true
    export autonomy_VERBOSE_LOGGING=true

    print_status "Monitoring environment setup complete"
}

# Function to run main daemon in monitoring mode
run_main_daemon() {
    print_monitor "Starting main daemon in monitoring mode..."

    local cmd="/usr/sbin/autonomyd"
    local args=(
        "--config=$CONFIG_FILE"
        "--log-level=$LOG_LEVEL"
        "--foreground"
    )

    if [ "$MONITOR_MODE" = true ]; then
        args+=("--monitor")
    fi

    if [ "$VERBOSE" = true ]; then
        args+=("--verbose")
    fi

    if [ "$PROFILE" = true ]; then
        args+=("--profile")
    fi

    if [ "$AUDIT" = true ]; then
        args+=("--audit")
    fi

    print_debug "Command: $cmd ${args[*]}"

    # Run the daemon
    exec $cmd "${args[@]}"
}

# Function to run system management in monitoring mode
run_sysmgmt() {
    print_monitor "Starting system management in monitoring mode..."

    local cmd="/usr/sbin/autonomysysmgmt"
    local args=(
        "--config=$CONFIG_FILE"
        "--log-level=$LOG_LEVEL"
        "--foreground"
    )

    if [ "$MONITOR_MODE" = true ]; then
        args+=("--monitor")
    fi

    if [ "$VERBOSE" = true ]; then
        args+=("--verbose")
    fi

    print_debug "Command: $cmd ${args[*]}"

    # Run the system management daemon
    exec $cmd "${args[@]}"
}

# Function to show monitoring tips
show_monitoring_tips() {
    echo ""
    print_monitor "=== MONITORING TIPS ==="
    echo ""
    echo "1. Watch for these key log patterns:"
    echo "   - ${CYAN}member discovery${NC}: Member detection and classification"
    echo "   - ${CYAN}metrics collection${NC}: Data collection from interfaces"
    echo "   - ${CYAN}decision making${NC}: Failover decisions and reasoning"
    echo "   - ${CYAN}state changes${NC}: Interface state transitions"
    echo "   - ${CYAN}performance${NC}: Timing and resource usage"
    echo "   - ${CYAN}errors${NC}: Any issues or failures"
    echo ""
    echo "2. Useful commands while monitoring:"
    echo "   - ${GREEN}ubus call autonomy status${NC} - Check current status"
    echo "   - ${GREEN}ubus call autonomy members${NC} - List discovered members"
    echo "   - ${GREEN}ubus call autonomy events${NC} - Show recent events"
    echo "   - ${GREEN}tail -f /var/log/messages${NC} - Watch system logs"
    echo ""
    echo "3. Press Ctrl+C to stop monitoring"
    echo ""
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -c | --config)
            CONFIG_FILE="$2"
            shift 2
            ;;
        -l | --log-level)
            LOG_LEVEL="$2"
            shift 2
            ;;
        -m | --monitor)
            MONITOR_MODE=true
            shift
            ;;
        -v | --verbose)
            VERBOSE=true
            shift
            ;;
        -f | --foreground)
            FOREGROUND=true
            shift
            ;;
        -p | --profile)
            PROFILE=true
            shift
            ;;
        -a | --audit)
            AUDIT=true
            shift
            ;;
        -s | --sysmgmt)
            SYSMGMT=true
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

# Main execution
main() {
    print_status "autonomy Monitoring Script"
    print_status "=========================="

    # Show configuration
    print_debug "Configuration:"
    print_debug "  Config file: $CONFIG_FILE"
    print_debug "  Log level: $LOG_LEVEL"
    print_debug "  Monitor mode: $MONITOR_MODE"
    print_debug "  Verbose: $VERBOSE"
    print_debug "  Foreground: $FOREGROUND"
    print_debug "  Profile: $PROFILE"
    print_debug "  Audit: $AUDIT"
    print_debug "  System Management: $SYSMGMT"

    # Check dependencies
    check_dependencies

    # Check if daemon is already running
    check_daemon_running

    # Setup monitoring environment
    setup_monitoring

    # Show monitoring tips
    show_monitoring_tips

    # Run the appropriate daemon
    if [ "$SYSMGMT" = true ]; then
        run_sysmgmt
    else
        run_main_daemon
    fi
}

# Run main function
main "$@"
