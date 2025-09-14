#!/bin/sh

# Simple crash analysis for RUTOS BusyBox
echo "=== AUTONOMY DAEMON CRASH ANALYSIS ==="
echo "Timestamp: $(date)"
echo ""

# Check crash debug file
if [ -f "/tmp/autonomy_crash_debug.log" ]; then
    echo "=== CRASH DEBUG FILE ==="
    cat /tmp/autonomy_crash_debug.log
    echo ""
else
    echo "No crash debug file found"
fi

# Check recent logs
echo "=== RECENT LOGS ==="
logread | grep -i autonomy | tail -10
echo ""

# Check processes
echo "=== AUTONOMY PROCESSES ==="
ps | grep autonomy
echo ""

# Check UBUS
echo "=== UBUS STATUS ==="
ubus list | grep autonomy
echo ""

# Check memory
echo "=== MEMORY ==="
cat /proc/meminfo | head -5
echo ""

echo "=== DONE ==="
