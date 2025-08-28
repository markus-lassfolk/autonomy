#!/bin/bash
set -e

echo "=== Deploying Example Package to RUTOS Device ==="

# Configuration
DEVICE_IP="192.168.80.1"
DEVICE_USER="root"
SSH_KEY="/mnt/c/Users/markusla/OneDrive/IT/RUTOS Keys/rusos_private_key_openssh"
PACKAGE_NAME="vuci-app-example-ui_1.0_arm_cortex-a7_neon-vfpv4.ipk"

echo "Device IP: $DEVICE_IP"
echo "Package: $PACKAGE_NAME"

# Fix SSH key permissions by copying to temp location
echo "Setting up SSH key..."
TEMP_SSH_KEY="/tmp/rutos_key_$$"
cp "$SSH_KEY" "$TEMP_SSH_KEY"
chmod 600 "$TEMP_SSH_KEY"
SSH_KEY="$TEMP_SSH_KEY"

# Check if package exists
if [ ! -f "$PACKAGE_NAME" ]; then
    echo "ERROR: Package $PACKAGE_NAME not found"
    exit 1
fi

echo "Package found: $PACKAGE_NAME"

# Copy package to device
echo "Copying package to device..."
scp -i "$SSH_KEY" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "$PACKAGE_NAME" "$DEVICE_USER@$DEVICE_IP:/tmp/"

if [ $? -eq 0 ]; then
    echo "Package copied successfully!"
else
    echo "ERROR: Failed to copy package to device"
    exit 1
fi

# Install package on device
echo "Installing package on device..."
ssh -i "$SSH_KEY" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "$DEVICE_USER@$DEVICE_IP" "opkg install /tmp/$PACKAGE_NAME"

if [ $? -eq 0 ]; then
    echo "Package installed successfully!"
else
    echo "ERROR: Failed to install package"
    exit 1
fi

# Restart services
echo "Restarting services..."
ssh -i "$SSH_KEY" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "$DEVICE_USER@$DEVICE_IP" "/etc/init.d/rpcd restart && /etc/init.d/uhttpd restart"

if [ $? -eq 0 ]; then
    echo "Services restarted successfully!"
else
    echo "WARNING: Failed to restart services"
fi

# Test the installation
echo "Testing installation..."

# Check if package is installed
echo "Checking if package is installed..."
ssh -i "$SSH_KEY" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "$DEVICE_USER@$DEVICE_IP" "opkg list-installed | grep vuci-app-example"

# Check if services are running
echo "Checking if services are running..."
ssh -i "$SSH_KEY" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "$DEVICE_USER@$DEVICE_IP" "ps | grep rpcd"
ssh -i "$SSH_KEY" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "$DEVICE_USER@$DEVICE_IP" "ps | grep uhttpd"

# Test ubus API if available
echo "Testing ubus API..."
ssh -i "$SSH_KEY" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "$DEVICE_USER@$DEVICE_IP" "ubus list | grep example" || echo "No example ubus API found"

echo ""
echo "=== DEPLOYMENT COMPLETE ==="
echo "Package deployed successfully!"
echo ""
echo "You can now access the UI at:"
echo "http://$DEVICE_IP/example/"
echo ""
echo "To test the API (if available):"
echo "ssh -i $SSH_KEY -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null $DEVICE_USER@$DEVICE_IP 'ubus call example status'"
echo "ssh -i $SSH_KEY -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null $DEVICE_USER@$DEVICE_IP 'ubus call example info'"
echo ""
echo "To check package status:"
echo "ssh -i $SSH_KEY -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null $DEVICE_USER@$DEVICE_IP 'opkg list-installed | grep vuci-app-example'"
echo ""
echo "To view logs:"
echo "ssh -i $SSH_KEY -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null $DEVICE_USER@$DEVICE_IP 'logread | tail -20'"

# Clean up temporary SSH key
rm -f "$TEMP_SSH_KEY"
