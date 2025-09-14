#!/bin/bash

# Advanced crash analysis script for autonomy daemon

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
journalctl --since "5 minutes ago" | grep -i "autonomy\|segfault\|crash" | tail -20
echo ""

# Check for core dumps
echo "=== CORE DUMPS ==="
if [ -f "/tmp/core*" ]; then
    echo "Core dumps found:"
    ls -la /tmp/core*
else
    echo "No core dumps found in /tmp/"
fi
echo ""

# Check memory usage
echo "=== MEMORY STATUS ==="
free -h
echo ""

# Check running processes
echo "=== RUNNING AUTONOMY PROCESSES ==="
ps aux | grep autonomy | grep -v grep
echo ""

# Check UBUS status
echo "=== UBUS STATUS ==="
if command -v ubus &> /dev/null; then
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
ip link show | grep -E "(UP|DOWN)"
echo ""

echo "=== ANALYSIS COMPLETE ==="
echo "For more detailed analysis, check:"
echo "1. /tmp/autonomy_crash_debug.log (if exists)"
echo "2. System logs: journalctl -u autonomy-daemon"
echo "3. UBUS status: ubus list autonomy"
