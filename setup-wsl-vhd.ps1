# PowerShell script to create and set up a dedicated VHD for WSL development
# This VHD will not be touched by Windows, avoiding permission issues

param(
    [string]$VhdPath = "D:\WSL\rutos-sdk.vhdx",  # Change this path as needed
    [int]$SizeGB = 50,  # Size of VHD in GB
    [string]$MountPoint = "/mnt/sdk"  # Mount point in WSL
)

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "WSL VHD Setup for RUTOS SDK Development" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""

# Check if running as Administrator
if (-NOT ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Host "This script requires Administrator privileges to create VHD." -ForegroundColor Red
    Write-Host "Please run PowerShell as Administrator and try again." -ForegroundColor Yellow
    exit 1
}

# Ensure directory exists
$VhdDir = Split-Path -Parent $VhdPath
if (!(Test-Path $VhdDir)) {
    Write-Host "Creating directory: $VhdDir" -ForegroundColor Yellow
    New-Item -ItemType Directory -Path $VhdDir -Force | Out-Null
}

# Step 1: Create VHD if it doesn't exist
if (Test-Path $VhdPath) {
    Write-Host "VHD already exists at: $VhdPath" -ForegroundColor Yellow
    $response = Read-Host "Do you want to delete and recreate it? (y/n)"
    if ($response -eq 'y') {
        Write-Host "Removing existing VHD..." -ForegroundColor Yellow
        Remove-Item $VhdPath -Force
    } else {
        Write-Host "Using existing VHD." -ForegroundColor Green
    }
}

if (!(Test-Path $VhdPath)) {
    Write-Host "Creating new VHD: $VhdPath ($SizeGB GB)" -ForegroundColor Yellow
    
    # Create VHD using diskpart
    $diskpartScript = @"
create vdisk file="$VhdPath" maximum=$($SizeGB * 1024) type=expandable
select vdisk file="$VhdPath"
attach vdisk
create partition primary
select partition 1
format fs=ntfs quick label="RUTOS_SDK"
assign letter=Z
detach vdisk
exit
"@
    
    $diskpartScript | diskpart | Out-Null
    
    if (Test-Path $VhdPath) {
        Write-Host "VHD created successfully!" -ForegroundColor Green
    } else {
        Write-Host "Failed to create VHD!" -ForegroundColor Red
        exit 1
    }
}

Write-Host ""
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "WSL Configuration" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""

# Create WSL setup script
$wslSetupScript = @'
#!/bin/bash
# WSL Setup Script for RUTOS SDK Development

set -e

VHD_PATH="$1"
MOUNT_POINT="$2"

echo "========================================="
echo "Setting up VHD in WSL"
echo "========================================="
echo ""

# Install required packages
echo "Installing required packages..."
sudo apt-get update
sudo apt-get install -y ntfs-3g build-essential git wget unzip python3 python3-pip

# Create mount point
echo "Creating mount point: $MOUNT_POINT"
sudo mkdir -p "$MOUNT_POINT"

# Mount the VHD
echo "Mounting VHD..."
# First, we need to attach the VHD in Windows and get the device path
# This will be done from PowerShell

# Set proper permissions
echo "Setting permissions..."
sudo chown -R $(whoami):$(whoami) "$MOUNT_POINT" 2>/dev/null || true

echo ""
echo "========================================="
echo "VHD Setup Complete!"
echo "========================================="
echo ""
echo "Mount point: $MOUNT_POINT"
echo ""
echo "Next steps:"
echo "1. Clone the RUTOS SDK to $MOUNT_POINT/rutos-sdk"
echo "2. Set up the SDK environment"
echo "3. Build packages using the SDK's make system"
echo ""
'@

# Save WSL setup script
$wslSetupScript | Out-File -FilePath "setup-wsl-vhd.sh" -Encoding UTF8

Write-Host "Created WSL setup script: setup-wsl-vhd.sh" -ForegroundColor Green
Write-Host ""

# Create the mount script for WSL
$mountScript = @"
#!/bin/bash
# Mount VHD in WSL
# Run this script in WSL to mount the VHD

MOUNT_POINT="$MountPoint"

echo "Mounting VHD to \$MOUNT_POINT..."

# Create mount point if it doesn't exist
sudo mkdir -p "\$MOUNT_POINT"

# Find the VHD device (usually /dev/sdc or similar)
# This assumes the VHD is attached in Windows
echo "Available block devices:"
lsblk

echo ""
echo "Please identify your VHD device (usually the last one listed)"
echo "It should show as a disk with NTFS filesystem"
echo ""
read -p "Enter device path (e.g., /dev/sdc1): " DEVICE

# Mount with proper permissions
sudo mount -t ntfs-3g -o uid=\$(id -u),gid=\$(id -g),umask=022 "\$DEVICE" "\$MOUNT_POINT"

if [ \$? -eq 0 ]; then
    echo "VHD mounted successfully at \$MOUNT_POINT"
    echo ""
    echo "You can now use this directory for the RUTOS SDK"
    echo "All files will have proper Linux permissions"
else
    echo "Failed to mount VHD"
    exit 1
fi
"@

$mountScript | Out-File -FilePath "mount-vhd-wsl.sh" -Encoding UTF8

Write-Host "Created WSL mount script: mount-vhd-wsl.sh" -ForegroundColor Green
Write-Host ""

# Create SDK setup script
$sdkSetupScript = @'
#!/bin/bash
# Setup RUTOS SDK on VHD

set -e

MOUNT_POINT="/mnt/sdk"
SDK_URL="https://wiki.teltonika-networks.com/gpl/RUTX_R_GPL_00.07.11.2.tar.gz"  # Update this URL as needed

echo "========================================="
echo "RUTOS SDK Setup on VHD"
echo "========================================="
echo ""

# Check if mount point exists
if [ ! -d "$MOUNT_POINT" ]; then
    echo "Error: Mount point $MOUNT_POINT does not exist!"
    echo "Please run mount-vhd-wsl.sh first"
    exit 1
fi

# Check if we have write permissions
if [ ! -w "$MOUNT_POINT" ]; then
    echo "Error: No write permissions to $MOUNT_POINT"
    echo "Please check mount permissions"
    exit 1
fi

cd "$MOUNT_POINT"

# Download SDK if not present
if [ ! -d "rutos-sdk" ]; then
    echo "Downloading RUTOS SDK..."
    if [ ! -f "RUTX_R_GPL.tar.gz" ]; then
        wget -O RUTX_R_GPL.tar.gz "$SDK_URL"
    fi
    
    echo "Extracting SDK..."
    tar -xzf RUTX_R_GPL.tar.gz
    mv RUTX_R_* rutos-sdk
    
    echo "SDK extracted to $MOUNT_POINT/rutos-sdk"
else
    echo "SDK already exists at $MOUNT_POINT/rutos-sdk"
fi

# Set proper permissions
echo "Setting permissions..."
chmod -R 755 "$MOUNT_POINT/rutos-sdk"

# Initialize SDK
cd "$MOUNT_POINT/rutos-sdk"

echo ""
echo "Setting up build environment..."
./scripts/feeds update -a
./scripts/feeds install -a

echo ""
echo "========================================="
echo "SDK Setup Complete!"
echo "========================================="
echo ""
echo "SDK location: $MOUNT_POINT/rutos-sdk"
echo ""
echo "To build packages:"
echo "  cd $MOUNT_POINT/rutos-sdk"
echo "  make menuconfig  # Configure build"
echo "  make package/vuci-app-example-api/compile V=s"
echo "  make package/vuci-app-example-ui/compile V=s"
echo ""
'@

$sdkSetupScript | Out-File -FilePath "setup-sdk-vhd.sh" -Encoding UTF8

Write-Host "Created SDK setup script: setup-sdk-vhd.sh" -ForegroundColor Green
Write-Host ""

Write-Host "=========================================" -ForegroundColor Green
Write-Host "Setup Complete!" -ForegroundColor Green
Write-Host "=========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "1. Run this PowerShell script as Administrator to create the VHD" -ForegroundColor White
Write-Host "2. In WSL, run: ./mount-vhd-wsl.sh" -ForegroundColor White
Write-Host "3. In WSL, run: ./setup-sdk-vhd.sh" -ForegroundColor White
Write-Host "4. Build packages using the SDK on the VHD" -ForegroundColor White
Write-Host ""
Write-Host "The VHD will be created at: $VhdPath" -ForegroundColor Cyan
Write-Host "It will be mounted in WSL at: $MountPoint" -ForegroundColor Cyan
Write-Host ""
Write-Host "This approach ensures:" -ForegroundColor Green
Write-Host "  ✓ No Windows permission interference" -ForegroundColor White
Write-Host "  ✓ Proper Linux file permissions" -ForegroundColor White
Write-Host "  ✓ SDK can work as intended" -ForegroundColor White
Write-Host "  ✓ Clean separation from Windows filesystem" -ForegroundColor White


