#!/bin/bash

# Performance Monitoring Script for autonomy
# Tracks memory usage, CPU usage, and network efficiency

set -e

# Configuration
LOG_FILE="/var/log/autonomy-performance.log"
PID_FILE="/var/run/autonomyd.pid"
MONITOR_INTERVAL=30  # seconds
MAX_LOG_SIZE=10485760  # 10MB

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Performance thresholds
MEMORY_WARN_MB=20
MEMORY_CRIT_MB=25
CPU_WARN_PERCENT=10
CPU_CRIT_PERCENT=20

log() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') - $1" | tee -a "$LOG_FILE"
}

log_performance() {
    local level="$1"
    local message="$2"
    local color="$3"
    
    echo -e "${color}$(date '+%Y-%m-%d %H:%M:%S') - [$level] $message${NC}" | tee -a "$LOG_FILE"
}

check_autonomyd_running() {
    if [ ! -f "$PID_FILE" ]; then
        log_performance "ERROR" "autonomyd PID file not found" "$RED"
        return 1
    fi
    
    local pid=$(cat "$PID_FILE")
    if ! kill -0 "$pid" 2>/dev/null; then
        log_performance "ERROR" "autonomyd process not running (PID: $pid)" "$RED"
        return 1
    fi
    
    echo "$pid"
}

get_memory_usage() {
    local pid="$1"
    if [ -z "$pid" ]; then
        echo "0"
        return
    fi
    
    # Get RSS memory usage in KB
    local rss=$(ps -o rss= -p "$pid" 2>/dev/null | tr -d ' ')
    if [ -z "$rss" ]; then
        echo "0"
        return
    fi
    
    # Convert to MB
    echo "$((rss / 1024))"
}

get_cpu_usage() {
    local pid="$1"
    if [ -z "$pid" ]; then
        echo "0"
        return
    fi
    
    # Get CPU usage percentage
    local cpu=$(ps -o %cpu= -p "$pid" 2>/dev/null | tr -d ' ')
    if [ -z "$cpu" ]; then
        echo "0"
        return
    fi
    
    echo "$cpu"
}

get_network_stats() {
    # Get network interface statistics
    local interface="wan"
    if [ -n "$1" ]; then
        interface="$1"
    fi
    
    # Get bytes sent/received
    local rx_bytes=$(cat "/sys/class/net/$interface/statistics/rx_bytes" 2>/dev/null || echo "0")
    local tx_bytes=$(cat "/sys/class/net/$interface/statistics/tx_bytes" 2>/dev/null || echo "0")
    
    echo "$rx_bytes $tx_bytes"
}

get_ubus_stats() {
    # Get ubus API call statistics
    local status_response
    status_response=$(ubus call autonomy status 2>/dev/null || echo "{}")
    
    # Extract member count
    local member_count=$(echo "$status_response" | grep -o '"rank":\[[^]]*\]' | grep -o '\[.*\]' | jq 'length' 2>/dev/null || echo "0")
    
    echo "$member_count"
}

check_performance_thresholds() {
    local memory_mb="$1"
    local cpu_percent="$2"
    local member_count="$3"
    
    # Memory checks
    if [ "$memory_mb" -ge "$MEMORY_CRIT_MB" ]; then
        log_performance "CRITICAL" "Memory usage critical: ${memory_mb}MB (threshold: ${MEMORY_CRIT_MB}MB)" "$RED"
        return 1
    elif [ "$memory_mb" -ge "$MEMORY_WARN_MB" ]; then
        log_performance "WARNING" "Memory usage high: ${memory_mb}MB (threshold: ${MEMORY_WARN_MB}MB)" "$YELLOW"
    else
        log_performance "INFO" "Memory usage normal: ${memory_mb}MB" "$GREEN"
    fi
    
    # CPU checks
    if [ "$(echo "$cpu_percent >= $CPU_CRIT_PERCENT" | bc -l 2>/dev/null || echo "0")" -eq 1 ]; then
        log_performance "CRITICAL" "CPU usage critical: ${cpu_percent}% (threshold: ${CPU_CRIT_PERCENT}%)" "$RED"
        return 1
    elif [ "$(echo "$cpu_percent >= $CPU_WARN_PERCENT" | bc -l 2>/dev/null || echo "0")" -eq 1 ]; then
        log_performance "WARNING" "CPU usage high: ${cpu_percent}% (threshold: ${CPU_WARN_PERCENT}%)" "$YELLOW"
    else
        log_performance "INFO" "CPU usage normal: ${cpu_percent}%" "$GREEN"
    fi
    
    # Member count check
    if [ "$member_count" -gt 0 ]; then
        log_performance "INFO" "Active members: $member_count" "$BLUE"
    else
        log_performance "WARNING" "No active members detected" "$YELLOW"
    fi
    
    return 0
}

rotate_log() {
    if [ -f "$LOG_FILE" ] && [ "$(stat -c%s "$LOG_FILE" 2>/dev/null || echo "0")" -gt "$MAX_LOG_SIZE" ]; then
        mv "$LOG_FILE" "${LOG_FILE}.old"
        log "Log file rotated"
    fi
}

main() {
    log "Performance monitoring started"
    
    # Initialize network stats
    local last_rx=0
    local last_tx=0
    local last_check_time=$(date +%s)
    
    while true; do
        # Rotate log if needed
        rotate_log
        
        # Check if autonomyd is running
        local pid
        pid=$(check_autonomyd_running)
        if [ $? -ne 0 ]; then
            sleep "$MONITOR_INTERVAL"
            continue
        fi
        
        # Get performance metrics
        local memory_mb
        memory_mb=$(get_memory_usage "$pid")
        
        local cpu_percent
        cpu_percent=$(get_cpu_usage "$pid")
        
        local member_count
        member_count=$(get_ubus_stats)
        
        # Get network statistics
        local network_stats
        network_stats=$(get_network_stats)
        local current_rx=$(echo "$network_stats" | cut -d' ' -f1)
        local current_tx=$(echo "$network_stats" | cut -d' ' -f2)
        
        # Calculate network throughput
        local current_time=$(date +%s)
        local time_diff=$((current_time - last_check_time))
        if [ "$time_diff" -gt 0 ]; then
            local rx_diff=$((current_rx - last_rx))
            local tx_diff=$((current_tx - last_tx))
            local rx_mbps=$(echo "scale=2; $rx_diff * 8 / 1024 / 1024 / $time_diff" | bc -l 2>/dev/null || echo "0")
            local tx_mbps=$(echo "scale=2; $tx_diff * 8 / 1024 / 1024 / $time_diff" | bc -l 2>/dev/null || echo "0")
            
            log_performance "INFO" "Network throughput - RX: ${rx_mbps}Mbps, TX: ${tx_mbps}Mbps" "$BLUE"
        fi
        
        # Update last values
        last_rx="$current_rx"
        last_tx="$current_tx"
        last_check_time="$current_time"
        
        # Check performance thresholds
        check_performance_thresholds "$memory_mb" "$cpu_percent" "$member_count"
        
        # Log summary
        log "Performance summary - Memory: ${memory_mb}MB, CPU: ${cpu_percent}%, Members: ${member_count}"
        
        sleep "$MONITOR_INTERVAL"
    done
}

# Handle script termination
trap 'log "Performance monitoring stopped"; exit 0' INT TERM

# Check dependencies
if ! command -v bc >/dev/null 2>&1; then
    echo "Error: 'bc' command not found. Please install it for CPU percentage calculations."
    exit 1
fi

if ! command -v jq >/dev/null 2>&1; then
    echo "Warning: 'jq' command not found. Member count detection may not work properly."
fi

# Start monitoring
main
