#!/bin/bash

# Test script for comprehensive network discovery
# This script tests the new ubus method for detailed interface information

echo "Testing Comprehensive Network Discovery Implementation"
echo "====================================================="

# Check if autonomy daemon is running
if ! pgrep -f "autonomy-daemon" > /dev/null; then
    echo "ERROR: autonomy-daemon is not running"
    echo "Please start the daemon first: ./autonomy-daemon"
    exit 1
fi

echo "✓ autonomy-daemon is running"

# Test the new ubus method
echo ""
echo "Testing ubus call autonomy.network interfaces_detailed..."
echo "--------------------------------------------------------"

# Call the new ubus method
ubus call autonomy.network interfaces_detailed

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ ubus call successful"
else
    echo ""
    echo "✗ ubus call failed"
    exit 1
fi

# Test MWAN3 integration
echo ""
echo "Testing MWAN3 integration..."
echo "----------------------------"

# Check if MWAN3 is available
if ubus list | grep -q "mwan3"; then
    echo "✓ MWAN3 is available"
    
    # Get MWAN3 status
    echo "MWAN3 status:"
    ubus call mwan3 status | head -20
else
    echo "⚠ MWAN3 is not available (this is expected in some configurations)"
fi

# Test network interface discovery
echo ""
echo "Testing network interface discovery..."
echo "--------------------------------------"

# Check network interfaces
echo "Available network interfaces:"
ubus call network.interface dump | grep -E '"interface":|"proto":|"device":' | head -20

echo ""
echo "Testing completed successfully!"
echo ""
echo "Usage examples:"
echo "  ubus call autonomy.network interfaces_detailed"
echo "  ubus call autonomy.network network_status"
echo "  ubus call autonomy.network network_failover"
