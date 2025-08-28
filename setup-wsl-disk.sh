#!/bin/bash
# Setup script for creating a dedicated disk for SDK development in WSL
# This avoids Windows permission issues

set -e

echo "========================================="
echo "WSL SDK Development Environment Setup"
echo "========================================="
echo ""

# Configuration
SDK_DIR="/mnt/sdk"
SDK_DISK_IMAGE="$HOME/rutos-sdk.img"
SDK_SIZE="20G"  # Size of the disk image

echo "Configuration:"
echo "  SDK Directory: $SDK_DIR"
echo "  Disk Image: $SDK_DISK_IMAGE"
echo "  Size: $SDK_SIZE"
echo ""

# Check if already mounted
if mount | grep -q "$SDK_DIR"; then
    echo "SDK directory is already mounted at $SDK_DIR"
    echo "Unmounting..."
    sudo umount "$SDK_DIR" || true
fi

# Create mount point
echo "Creating mount point..."
sudo mkdir -p "$SDK_DIR"

# Create disk image if it doesn't exist
if [ ! -f "$SDK_DISK_IMAGE" ]; then
    echo "Creating disk image ($SDK_SIZE)..."
    dd if=/dev/zero of="$SDK_DISK_IMAGE" bs=1M count=20480 status=progress
    
    echo "Creating ext4 filesystem..."
    mkfs.ext4 "$SDK_DISK_IMAGE"
else
    echo "Disk image already exists at $SDK_DISK_IMAGE"
fi

# Mount the disk image
echo "Mounting disk image..."
sudo mount -o loop "$SDK_DISK_IMAGE" "$SDK_DIR"

# Set permissions
echo "Setting permissions..."
sudo chown -R $(whoami):$(whoami) "$SDK_DIR"

# Create SDK structure
echo "Creating SDK directory structure..."
mkdir -p "$SDK_DIR/rutos-sdk"
mkdir -p "$SDK_DIR/packages"
mkdir -p "$SDK_DIR/workspace"

echo ""
echo "========================================="
echo "Disk Setup Complete!"
echo "========================================="
echo ""
echo "SDK disk mounted at: $SDK_DIR"
echo "Disk image: $SDK_DISK_IMAGE"
echo ""
echo "This provides:"
echo "  ✓ Linux-native filesystem (ext4)"
echo "  ✓ Proper file permissions"
echo "  ✓ No Windows interference"
echo "  ✓ Full SDK compatibility"
echo ""
echo "To make this permanent, add to /etc/fstab:"
echo "  $SDK_DISK_IMAGE $SDK_DIR ext4 loop,defaults 0 0"
echo ""


