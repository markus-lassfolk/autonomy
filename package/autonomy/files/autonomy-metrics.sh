#!/bin/sh
# autonomy-metrics.sh - Metrics collection script for autonomy daemon
# This script collects system performance metrics and stores them for historical analysis

METRICS_LOG="/var/log/autonomy-metrics.log"
METRICS_DIR="/var/lib/autonomy/metrics"
RRD_DIR="/var/lib/autonomy/rrd"
LOCK_FILE="/var/run/autonomy-metrics.lock"

# Ensure directories exist
mkdir -p "$METRICS_DIR" "$RRD_DIR"

# Prevent multiple instances
if [ -f "$LOCK_FILE" ]; then
    PID=$(cat "$LOCK_FILE")
    if kill -0 "$PID" 2>/dev/null; then
        exit 0
    fi
fi
echo $$ > "$LOCK_FILE"

# Cleanup on exit
trap 'rm -f "$LOCK_FILE"' EXIT

# Get current timestamp
TIMESTAMP=$(date +%s)

# Collect CPU usage
CPU_USAGE=$(top -bn1 | grep 'Cpu(s)' | awk '{print $2}' | cut -d'%' -f1)
echo "CPU $TIMESTAMP $CPU_USAGE" >> "$METRICS_LOG"

# Collect memory usage
MEMORY_USAGE=$(free | grep Mem | awk '{printf "%.1f", $3/$2 * 100.0}')
echo "MEM $TIMESTAMP $MEMORY_USAGE" >> "$METRICS_LOG"

# Collect disk usage
DISK_USAGE=$(df / | tail -1 | awk '{print $5}' | cut -d'%' -f1)
echo "DISK $TIMESTAMP $DISK_USAGE" >> "$METRICS_LOG"

# Collect network interface statistics
for iface in $(ip link show | grep '^[0-9]' | awk '{print $2}' | cut -d':' -f1); do
    if [ -d "/sys/class/net/$iface" ]; then
        RX_BYTES=$(cat "/sys/class/net/$iface/statistics/rx_bytes" 2>/dev/null || echo "0")
        TX_BYTES=$(cat "/sys/class/net/$iface/statistics/tx_bytes" 2>/dev/null || echo "0")
        RX_ERRORS=$(cat "/sys/class/net/$iface/statistics/rx_errors" 2>/dev/null || echo "0")
        TX_ERRORS=$(cat "/sys/class/net/$iface/statistics/tx_errors" 2>/dev/null || echo "0")
        RX_DROPPED=$(cat "/sys/class/net/$iface/statistics/rx_dropped" 2>/dev/null || echo "0")
        TX_DROPPED=$(cat "/sys/class/net/$iface/statistics/tx_dropped" 2>/dev/null || echo "0")
        
        echo "NET $TIMESTAMP $iface $RX_BYTES $TX_BYTES $RX_ERRORS $TX_ERRORS $RX_DROPPED $TX_DROPPED" >> "$METRICS_LOG"
    fi
done

# Collect autonomy daemon specific metrics
if [ -f "/var/run/autonomyd.pid" ]; then
    AUTONOMY_PID=$(cat "/var/run/autonomyd.pid")
    if kill -0 "$AUTONOMY_PID" 2>/dev/null; then
        # Get process CPU and memory usage
        PS_OUTPUT=$(ps -o pid,ppid,etime,pcpu,pmem,comm -p "$AUTONOMY_PID" | tail -1)
        if [ -n "$PS_OUTPUT" ]; then
            CPU_PCT=$(echo "$PS_OUTPUT" | awk '{print $4}')
            MEM_PCT=$(echo "$PS_OUTPUT" | awk '{print $5}')
            echo "PROC $TIMESTAMP $AUTONOMY_PID $CPU_PCT $MEM_PCT" >> "$METRICS_LOG"
        fi
        
        # Get autonomy daemon status via ubus
        if command -v ubus >/dev/null 2>&1; then
            AUTONOMY_STATUS=$(ubus call autonomy status 2>/dev/null | grep -o '"current_interface":"[^"]*"' | cut -d'"' -f4)
            if [ -n "$AUTONOMY_STATUS" ]; then
                echo "STATUS $TIMESTAMP $AUTONOMY_STATUS" >> "$METRICS_LOG"
            fi
        fi
    fi
fi

# Rotate log file if it gets too large (keep last 1000 lines)
if [ -f "$METRICS_LOG" ] && [ $(wc -l < "$METRICS_LOG") -gt 1000 ]; then
    tail -n 500 "$METRICS_LOG" > "$METRICS_LOG.tmp"
    mv "$METRICS_LOG.tmp" "$METRICS_LOG"
fi

# Create RRD files if they don't exist (requires rrdtool)
if command -v rrdtool >/dev/null 2>&1; then
    # CPU RRD
    if [ ! -f "$RRD_DIR/cpu.rrd" ]; then
        rrdtool create "$RRD_DIR/cpu.rrd" \
            --start now-1d \
            --step 60 \
            DS:cpu:GAUGE:120:0:100 \
            RRA:AVERAGE:0.5:1:1440 \
            RRA:AVERAGE:0.5:5:288 \
            RRA:AVERAGE:0.5:30:144
    fi
    
    # Memory RRD
    if [ ! -f "$RRD_DIR/memory.rrd" ]; then
        rrdtool create "$RRD_DIR/memory.rrd" \
            --start now-1d \
            --step 60 \
            DS:memory:GAUGE:120:0:100 \
            RRA:AVERAGE:0.5:1:1440 \
            RRA:AVERAGE:0.5:5:288 \
            RRA:AVERAGE:0.5:30:144
    fi
    
    # Disk RRD
    if [ ! -f "$RRD_DIR/disk.rrd" ]; then
        rrdtool create "$RRD_DIR/disk.rrd" \
            --start now-1d \
            --step 60 \
            DS:disk:GAUGE:120:0:100 \
            RRA:AVERAGE:0.5:1:1440 \
            RRA:AVERAGE:0.5:5:288 \
            RRA:AVERAGE:0.5:30:144
    fi
    
    # Update RRD files with current values
    rrdtool update "$RRD_DIR/cpu.rrd" "$TIMESTAMP:$CPU_USAGE"
    rrdtool update "$RRD_DIR/memory.rrd" "$TIMESTAMP:$MEMORY_USAGE"
    rrdtool update "$RRD_DIR/disk.rrd" "$TIMESTAMP:$DISK_USAGE"
fi

# Generate alerts based on thresholds
ALERT_LOG="/var/log/autonomy-alerts.log"

# CPU alerts
if [ "$(echo "$CPU_USAGE > 90" | bc -l 2>/dev/null || echo "0")" = "1" ]; then
    echo "CRITICAL $TIMESTAMP CPU usage is critically high: ${CPU_USAGE}%" >> "$ALERT_LOG"
elif [ "$(echo "$CPU_USAGE > 80" | bc -l 2>/dev/null || echo "0")" = "1" ]; then
    echo "WARNING $TIMESTAMP CPU usage is high: ${CPU_USAGE}%" >> "$ALERT_LOG"
fi

# Memory alerts
if [ "$(echo "$MEMORY_USAGE > 90" | bc -l 2>/dev/null || echo "0")" = "1" ]; then
    echo "CRITICAL $TIMESTAMP Memory usage is critically high: ${MEMORY_USAGE}%" >> "$ALERT_LOG"
elif [ "$(echo "$MEMORY_USAGE > 80" | bc -l 2>/dev/null || echo "0")" = "1" ]; then
    echo "WARNING $TIMESTAMP Memory usage is high: ${MEMORY_USAGE}%" >> "$ALERT_LOG"
fi

# Disk alerts
if [ "$(echo "$DISK_USAGE > 95" | bc -l 2>/dev/null || echo "0")" = "1" ]; then
    echo "CRITICAL $TIMESTAMP Disk usage is critically high: ${DISK_USAGE}%" >> "$ALERT_LOG"
elif [ "$(echo "$DISK_USAGE > 90" | bc -l 2>/dev/null || echo "0")" = "1" ]; then
    echo "WARNING $TIMESTAMP Disk usage is high: ${DISK_USAGE}%" >> "$ALERT_LOG"
fi

# Rotate alert log if it gets too large (keep last 100 lines)
if [ -f "$ALERT_LOG" ] && [ $(wc -l < "$ALERT_LOG") -gt 100 ]; then
    tail -n 50 "$ALERT_LOG" > "$ALERT_LOG.tmp"
    mv "$ALERT_LOG.tmp" "$ALERT_LOG"
fi

exit 0
