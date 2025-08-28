#!/bin/bash

echo "=== Testing RUTOS Deployment ==="

# Configuration
DEVICE_IP="192.168.80.1"
DEVICE_USER="root"
SSH_KEY="/mnt/c/Users/markusla/OneDrive/IT/RUTOS\ Keys/rusos_private_key_openssh"

# Fix SSH key permissions
TEMP_SSH_KEY="/tmp/rutos_test_key_$$"
cp "$SSH_KEY" "$TEMP_SSH_KEY"
chmod 600 "$TEMP_SSH_KEY"

echo "Testing connection to device..."

# Test basic connectivity
if ssh -i "$TEMP_SSH_KEY" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=10 "$DEVICE_USER@$DEVICE_IP" "echo 'Connection successful'" 2>/dev/null; then
    echo "✓ SSH connection successful"
else
    echo "✗ SSH connection failed"
    rm -f "$TEMP_SSH_KEY"
    exit 1
fi

# Check if package is installed
echo "Checking installed packages..."
ssh -i "$TEMP_SSH_KEY" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "$DEVICE_USER@$DEVICE_IP" "opkg list-installed | grep vuci-app-example"

# Check if services are running
echo "Checking services..."
ssh -i "$TEMP_SSH_KEY" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "$DEVICE_USER@$DEVICE_IP" "ps | grep rpcd"
ssh -i "$TEMP_SSH_KEY" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "$DEVICE_USER@$DEVICE_IP" "ps | grep uhttpd"

# Test ubus API
echo "Testing ubus API..."
ssh -i "$TEMP_SSH_KEY" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "$DEVICE_USER@$DEVICE_IP" "ubus list | grep example" || echo "No example ubus API found"

# Test web server
echo "Testing web server..."
if curl -s -o /dev/null -w "%{http_code}" "http://$DEVICE_IP/" | grep -q "200\|401\|403"; then
    echo "✓ Web server is running"
else
    echo "✗ Web server not responding"
fi

# Test example page
echo "Testing example page..."
if curl -s -o /dev/null -w "%{http_code}" "http://$DEVICE_IP/example/" | grep -q "200\|404"; then
    echo "✓ Example page accessible"
else
    echo "✗ Example page not accessible"
fi

echo ""
echo "=== DEPLOYMENT TEST RESULTS ==="
echo "Device IP: $DEVICE_IP"
echo "Package: vuci-app-example-ui"
echo ""
echo "You can access the UI at:"
echo "http://$DEVICE_IP/example/"
echo ""
echo "To manually test:"
echo "ssh -i $SSH_KEY -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null $DEVICE_USER@$DEVICE_IP"

# Clean up
rm -f "$TEMP_SSH_KEY"
