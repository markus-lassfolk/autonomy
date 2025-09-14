#!/bin/sh

# Advanced crash analysis script for autonomy daemon (BusyBox compatible)

echo "=== AUTONOMY DAEMON CRASH ANALYSIS ==="
echo "Timestamp: $(date)"
echo ""

# Check if crash debug file exists
if [ -f "/tmp/autonomy_crash_debug.log" ]; then
    echo "=== CRASH DEBUG FILE FOUND ==="
    cat /tmp/autonomy_crash_debug.log
    echo ""
else
    echo "No crash debug file found at /tmp/autonomy_crash_debug.log"
fi

# Check system logs for recent crashes
echo "=== RECENT SYSTEM LOGS ==="
echo "Checking for recent autonomy daemon crashes..."
if command -v journalctl >/dev/null 2>&1; then
    journalctl --since "5 minutes ago" 2>/dev/null | grep -i "autonomy\|segfault\|crash" | tail -20
else
    echo "journalctl not available, checking logread instead..."
    logread | grep -i "autonomy\|segfault\|crash" | tail -20
fi
echo ""

# Check for core dumps
echo "=== CORE DUMPS ==="
if ls /tmp/core* >/dev/null 2>&1; then
    echo "Core dumps found:"
    ls -la /tmp/core*
else
    echo "No core dumps found in /tmp/"
fi
echo ""

# Check memory usage
echo "=== MEMORY STATUS ==="
if command -v free >/dev/null 2>&1; then
    free
else
    echo "free command not available"
    cat /proc/meminfo | head -10
fi
echo ""

# Check running processes
echo "=== RUNNING AUTONOMY PROCESSES ==="
ps | grep autonomy | grep -v grep
echo ""

# Check UBUS status
echo "=== UBUS STATUS ==="
if command -v ubus >/dev/null 2>&1; then
    echo "UBUS objects:"
    ubus list | grep autonomy
    echo ""
    echo "UBUS methods for autonomy:"
    ubus list autonomy 2>/dev/null || echo "autonomy object not found"
else
    echo "UBUS not available"
fi
echo ""

# Check network interfaces
echo "=== NETWORK INTERFACES ==="
if command -v ip >/dev/null 2>&1; then
    ip link show | grep -E "(UP|DOWN)"
else
    echo "ip command not available, checking ifconfig..."
    ifconfig | grep -E "(UP|DOWN|RUNNING)"
fi
echo ""

# Check system info
echo "=== SYSTEM INFO ==="
echo "Uptime: $(uptime)"
echo "Load: $(cat /proc/loadavg 2>/dev/null || echo 'N/A')"
echo ""

# Check for any autonomy-related files
echo "=== AUTONOMY FILES ==="
find /tmp /var/log -name "*autonomy*" 2>/dev/null | head -10
echo ""

echo "=== ANALYSIS COMPLETE ==="
echo "For more detailed analysis, check:"
echo "1. /tmp/autonomy_crash_debug.log (if exists)"
echo "2. System logs: logread | grep autonomy"
echo "3. UBUS status: ubus list autonomy"
echo "4. Process info: ps | grep autonomy"
