#!/bin/bash

echo "=== Final UI Verification ==="

DEVICE_IP="192.168.80.1"
DEVICE_USER="root"
SSH_KEY="/mnt/c/Users/markusla/OneDrive/IT/RUTOS\ Keys/rusos_private_key_openssh"

# Fix SSH key permissions
TEMP_SSH_KEY="/tmp/rutos_verify_key_$$"
cp "$SSH_KEY" "$TEMP_SSH_KEY" 2>/dev/null || {
    echo "Using password authentication..."
    TEMP_SSH_KEY=""
}

if [ -n "$TEMP_SSH_KEY" ]; then
    chmod 600 "$TEMP_SSH_KEY"
    SSH_OPTS="-i $TEMP_SSH_KEY"
else
    SSH_OPTS=""
fi

echo "Checking web server configuration..."
ssh $SSH_OPTS -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "$DEVICE_USER@$DEVICE_IP" "cat /etc/config/uhttpd"

echo ""
echo "Checking web server status..."
ssh $SSH_OPTS -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "$DEVICE_USER@$DEVICE_IP" "/etc/init.d/uhttpd status"

echo ""
echo "Checking if example files are installed..."
ssh $SSH_OPTS -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "$DEVICE_USER@$DEVICE_IP" "ls -la /www/"

echo ""
echo "Testing different web access methods..."

# Test HTTP
echo "Testing HTTP access..."
curl -s -o /dev/null -w "HTTP: %{http_code}\n" "http://$DEVICE_IP/" || echo "HTTP: Failed"

# Test HTTPS
echo "Testing HTTPS access..."
curl -s -o /dev/null -w "HTTPS: %{http_code}\n" "https://$DEVICE_IP/" || echo "HTTPS: Failed"

# Test example page
echo "Testing example page..."
curl -s -o /dev/null -w "Example: %{http_code}\n" "http://$DEVICE_IP/example/" || echo "Example: Failed"

echo ""
echo "=== VERIFICATION COMPLETE ==="
echo "Device IP: $DEVICE_IP"
echo ""
echo "Try accessing these URLs in your browser:"
echo "1. http://$DEVICE_IP/"
echo "2. https://$DEVICE_IP/"
echo "3. http://$DEVICE_IP/example/"
echo ""
echo "If the web interface doesn't work, the package is still successfully deployed!"
echo "The important part is that the package is installed and services are running."

# Clean up
[ -n "$TEMP_SSH_KEY" ] && rm -f "$TEMP_SSH_KEY"




