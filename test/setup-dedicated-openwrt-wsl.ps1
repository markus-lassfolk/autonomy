# Dedicated OpenWrt WSL Testing Environment Setup
# This script creates a separate WSL instance specifically for OpenWrt testing

param(
    [string]$Action = "menu",
    [string]$WSLName = "openwrt-test",
    [string]$OpenWrtVersion = "22.03.5"
)

# Colors for output
$Red = "`e[31m"
$Green = "`e[32m"
$Yellow = "`e[33m"
$Blue = "`e[34m"
$Reset = "`e[0m"

# Function to print colored output
function Write-Status {
    param([string]$Message)
    Write-Host "$Blue[INFO]$Reset $Message"
}

function Write-Success {
    param([string]$Message)
    Write-Host "$Green[SUCCESS]$Reset $Message"
}

function Write-Warning {
    param([string]$Message)
    Write-Host "$Yellow[WARNING]$Reset $Message"
}

function Write-Error {
    param([string]$Message)
    Write-Host "$Red[ERROR]$Reset $Message"
}

# Script configuration
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$BuildDir = Join-Path $ProjectRoot "build-openwrt"

Write-Status "Dedicated OpenWrt WSL Testing Environment Setup"
Write-Status "=============================================="

# Check if WSL is available
function Test-WSL {
    try {
        wsl --version | Out-Null
        return $true
    }
    catch {
        return $false
    }
}

# Check if WSL instance exists
function Test-WSLInstance {
    param([string]$InstanceName)
    try {
        wsl -l -v | Select-String $InstanceName | Out-Null
        return $true
    }
    catch {
        return $false
    }
}

# Option 1: Create dedicated Ubuntu WSL instance
function Setup-DedicatedUbuntuWSL {
    Write-Status "Setting up dedicated Ubuntu WSL instance for OpenWrt testing..."

    if (!(Test-WSL)) {
        Write-Error "WSL not available. Please install WSL first: wsl --install"
        return
    }

    if (Test-WSLInstance -InstanceName $WSLName) {
        Write-Warning "WSL instance '$WSLName' already exists"
        $choice = Read-Host "Do you want to remove it and recreate? (y/N)"
        if ($choice -eq "y" -or $choice -eq "Y") {
            Write-Status "Removing existing WSL instance..."
            wsl --unregister $WSLName
        } else {
            Write-Status "Using existing WSL instance"
            return
        }
    }

    Write-Status "Installing dedicated Ubuntu WSL instance..."
    Write-Status "This will create a separate Ubuntu instance named '$WSLName'"
    Write-Status "Your existing Ubuntu installation will remain untouched"

    # Install Ubuntu with a specific name
    wsl --install -d Ubuntu --name $WSLName

    Write-Status "Waiting for Ubuntu installation to complete..."
    Start-Sleep -Seconds 20

    Write-Status "Setting up OpenWrt testing environment..."
    wsl -d $WSLName -e bash -c @"
# Update Ubuntu
sudo apt-get update
sudo apt-get upgrade -y

# Install OpenWrt build dependencies
sudo apt-get install -y \
    build-essential \
    ccache \
    ecj \
    fastjar \
    file \
    g++ \
    gawk \
    gettext \
    git \
    java-propose-classpath \
    libelf-dev \
    libncurses5-dev \
    libncursesw5-dev \
    libssl-dev \
    python3 \
    python3-distutils \
    python3-setuptools \
    python3-dev \
    rsync \
    subversion \
    swig \
    time \
    unzip \
    wget \
    xsltproc \
    zlib1g-dev \
    curl \
    jq \
    && sudo rm -rf /var/lib/apt/lists/*

# Create test directories
sudo mkdir -p /etc/config /var/log /tmp/autonomy /usr/bin

# Create mock OpenWrt environment
sudo bash -c 'echo "config system" > /etc/config/system'
sudo bash -c 'echo "    option hostname \"openwrt-test\"" >> /etc/config/system'
sudo bash -c 'echo "    option timezone \"UTC\"" >> /etc/config/system'

# Create mock network config
sudo bash -c 'echo "config interface \"loopback\"" > /etc/config/network'
sudo bash -c 'echo "    option ifname \"lo\"" >> /etc/config/network'
sudo bash -c 'echo "    option proto \"static\"" >> /etc/config/network'
sudo bash -c 'echo "    option ipaddr \"127.0.0.1\"" >> /etc/config/network'

# Create mock mwan3 config
sudo bash -c 'echo "config globals \"globals\"" > /etc/config/mwan3'
sudo bash -c 'echo "    option mmx_mask \"0x3F00\"" >> /etc/config/mwan3'
sudo bash -c 'echo "    option local_source \"lan\"" >> /etc/config/mwan3'

sudo bash -c 'echo "config interface \"wan\"" >> /etc/config/mwan3'
sudo bash -c 'echo "    option enabled \"1\"" >> /etc/config/mwan3'
sudo bash -c 'echo "    option family \"ipv4\"" >> /etc/config/mwan3'
sudo bash -c 'echo "    option track_method \"ping\"" >> /etc/config/mwan3'
sudo bash -c 'echo "    option track_ip \"8.8.8.8\"" >> /etc/config/mwan3'
sudo bash -c 'echo "    option reliability \"1\"" >> /etc/config/mwan3'
sudo bash -c 'echo "    option count \"1\"" >> /etc/config/mwan3'
sudo bash -c 'echo "    option timeout \"2\"" >> /etc/config/mwan3'
sudo bash -c 'echo "    option interval \"5\"" >> /etc/config/mwan3'
sudo bash -c 'echo "    option down \"3\"" >> /etc/config/mwan3'
sudo bash -c 'echo "    option up \"3\"" >> /etc/config/mwan3'

# Create mock commands
sudo bash -c 'echo \"#!/bin/bash\" > /usr/bin/ubus'
sudo bash -c 'echo \"echo \\\"Mock ubus - \$*\\\"\" >> /usr/bin/ubus'
sudo chmod +x /usr/bin/ubus

sudo bash -c 'echo \"#!/bin/bash\" > /usr/bin/uci'
sudo bash -c 'echo \"echo \\\"Mock uci - \$*\\\"\" >> /usr/bin/uci'
sudo chmod +x /usr/bin/uci

sudo bash -c 'echo \"#!/bin/bash\" > /usr/bin/opkg'
sudo bash -c 'echo \"echo \\\"Mock opkg - \$*\\\"\" >> /usr/bin/opkg'
sudo chmod +x /usr/bin/opkg

# Create workspace directory
mkdir -p /workspace
echo "Dedicated Ubuntu WSL environment setup complete!"
echo "This instance is separate from your main Ubuntu installation"
"@

    Write-Success "Dedicated Ubuntu WSL instance '$WSLName' created successfully!"
    Write-Status "To access: wsl -d $WSLName"
    Write-Status "To mount your project: wsl -d $WSLName -e bash -c 'sudo mount --bind /mnt/c/your-project-path /workspace'"
    Write-Status "Your main Ubuntu installation remains untouched!"
}

# Option 2: Setup OpenWrt Image Builder in dedicated WSL
function Setup-OpenWrtImageBuilder {
    Write-Status "Setting up OpenWrt Image Builder in dedicated WSL..."

    if (!(Test-WSLInstance -InstanceName $WSLName)) {
        Write-Error "WSL instance '$WSLName' not found. Please create it first."
        return
    }

    Write-Status "Downloading OpenWrt Image Builder..."
    wsl -d $WSLName -e bash -c @"
cd /workspace

# Download OpenWrt Image Builder
OPENWRT_VERSION="$OpenWrtVersion"
BUILDER_URL="https://downloads.openwrt.org/releases/${OPENWRT_VERSION}/targets/x86/64/openwrt-imagebuilder-${OPENWRT_VERSION}-x86-64.Linux-x86_64.tar.xz"

echo "Downloading OpenWrt Image Builder ${OPENWRT_VERSION}..."
wget -O imagebuilder.tar.xz "$BUILDER_URL"
tar -xf imagebuilder.tar.xz
cd openwrt-imagebuilder-${OPENWRT_VERSION}-x86-64.Linux-x86_64

echo "Building custom OpenWrt image with autonomy packages..."
make image PACKAGES="luci luci-base luci-compat mwan3 ubus uci"

echo "Image built successfully!"
echo "Output files in bin/targets/x86/64/"
ls -la bin/targets/x86/64/
"@

    Write-Success "OpenWrt Image Builder setup completed!"
    Write-Status "Custom OpenWrt images created in dedicated WSL workspace"
}

# Option 3: Build and test packages in dedicated WSL
function Build-AndTestPackages {
    Write-Status "Building and testing packages in dedicated WSL..."

    if (!(Test-WSLInstance -InstanceName $WSLName)) {
        Write-Error "WSL instance '$WSLName' not found. Please create it first."
        return
    }

    # Build OpenWrt-compatible packages
    $BuildScriptPath = Join-Path $ProjectRoot "build-openwrt-package.ps1"
    if (Test-Path $BuildScriptPath) {
        Write-Status "Building OpenWrt packages..."
        & powershell -ExecutionPolicy Bypass -File $BuildScriptPath -Architecture "x86_64"
    }
    else {
        Write-Error "OpenWrt build script not found!"
        return
    }

    # Copy packages to WSL and test
    Write-Status "Copying packages to dedicated WSL and testing..."
    $WSLPath = "/mnt/$(($ProjectRoot -replace ':', '').ToLower())"

    wsl -d $WSLName -e bash -c @"
cd $WSLPath/build-openwrt

echo "Testing package installation in dedicated WSL environment..."
echo "Available packages:"
ls -la *.ipk

echo "Installing autonomy package..."
opkg install autonomy_1.0.0_x86_64.ipk

echo "Installing LuCI package..."
opkg install luci-app-autonomy_1.0.0_all.ipk

echo "Testing service..."
/etc/init.d/autonomy start
/etc/init.d/autonomy status

echo "Testing ubus interface..."
ubus call autonomy status

echo "Package test completed in dedicated WSL environment!"
"@

    Write-Success "Package testing completed in dedicated WSL!"
}

# Option 4: Start dedicated WSL shell
function Start-DedicatedWSLShell {
    Write-Status "Starting dedicated WSL shell..."

    if (!(Test-WSLInstance -InstanceName $WSLName)) {
        Write-Error "WSL instance '$WSLName' not found. Please create it first."
        return
    }

    Write-Status "Starting dedicated WSL shell for $WSLName..."
    Write-Status "This is separate from your main Ubuntu installation"
    Write-Status "Use 'exit' to return to Windows"
    wsl -d $WSLName
}

# Option 5: Mount project directory to dedicated WSL
function Mount-ProjectDirectory {
    Write-Status "Mounting project directory to dedicated WSL..."

    if (!(Test-WSLInstance -InstanceName $WSLName)) {
        Write-Error "WSL instance '$WSLName' not found. Please create it first."
        return
    }

    $WSLPath = "/mnt/$(($ProjectRoot -replace ':', '').ToLower())"

    Write-Status "Mounting $ProjectRoot to /workspace in dedicated WSL..."
    wsl -d $WSLName -e bash -c "sudo mount --bind $WSLPath /workspace"

    Write-Success "Project directory mounted successfully!"
    Write-Status "Access your project at /workspace in dedicated WSL"
}

# Option 6: Clean up dedicated WSL instance
function Cleanup-DedicatedWSL {
    Write-Status "Cleaning up dedicated WSL instance..."

    if (!(Test-WSLInstance -InstanceName $WSLName)) {
        Write-Warning "WSL instance '$WSLName' not found. Nothing to clean up."
        return
    }

    $choice = Read-Host "Are you sure you want to remove the dedicated WSL instance '$WSLName'? (y/N)"
    if ($choice -eq "y" -or $choice -eq "Y") {
        Write-Status "Removing dedicated WSL instance..."
        wsl --unregister $WSLName
        Write-Success "Dedicated WSL instance removed successfully!"
        Write-Status "Your main Ubuntu installation remains untouched."
    } else {
        Write-Status "Cleanup cancelled."
    }
}

# Show menu
function Show-Menu {
    Write-Host ""
    Write-Host "Dedicated OpenWrt WSL Testing Options:"
    Write-Host "====================================="
    Write-Host "1. Create dedicated Ubuntu WSL instance (Separate from main Ubuntu)"
    Write-Host "2. Setup OpenWrt Image Builder"
    Write-Host "3. Build and test packages"
    Write-Host "4. Start dedicated WSL shell"
    Write-Host "5. Mount project directory"
    Write-Host "6. Clean up dedicated WSL instance"
    Write-Host "7. List WSL instances"
    Write-Host "8. Exit"
    Write-Host ""
}

# List WSL instances
function List-WSLInstances {
    Write-Status "WSL instances:"
    wsl -l -v
    Write-Status ""
    Write-Status "Note: Your main Ubuntu installation and the dedicated openwrt-test are separate"
}

# Check dependencies
function Check-Dependencies {
    Write-Status "Checking dependencies..."

    if (Test-WSL) {
        Write-Success "WSL available"
    }
    else {
        Write-Error "WSL not available. Please install WSL first: wsl --install"
    }

    # Check if build scripts exist
    $BuildScriptPath = Join-Path $ProjectRoot "build-openwrt-package.ps1"
    if (Test-Path $BuildScriptPath) {
        Write-Success "OpenWrt build script found"
    }
    else {
        Write-Warning "OpenWrt build script not found"
    }

    # Check current WSL instances
    Write-Status "Current WSL instances:"
    wsl -l -v
}

# Main execution
function Main {
    # Create build directory
    if (!(Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir | Out-Null
    }

    # If action is provided, run it directly
    if ($Action -ne "menu") {
        switch ($Action) {
            "1" { Setup-DedicatedUbuntuWSL }
            "2" { Setup-OpenWrtImageBuilder }
            "3" { Build-AndTestPackages }
            "4" { Start-DedicatedWSLShell }
            "5" { Mount-ProjectDirectory }
            "6" { Cleanup-DedicatedWSL }
            "7" { List-WSLInstances }
            "check" { Check-Dependencies }
            default { Write-Error "Invalid action: $Action" }
        }
        return
    }

    # Interactive menu
    while ($true) {
        Show-Menu
        $choice = Read-Host "Select an option (1-8)"

        switch ($choice) {
            "1" { Setup-DedicatedUbuntuWSL }
            "2" { Setup-OpenWrtImageBuilder }
            "3" { Build-AndTestPackages }
            "4" { Start-DedicatedWSLShell }
            "5" { Mount-ProjectDirectory }
            "6" { Cleanup-DedicatedWSL }
            "7" { List-WSLInstances }
            "8" {
                Write-Success "Exiting..."
                exit 0
            }
            default {
                Write-Error "Invalid option. Please select 1-8."
            }
        }

        Write-Host ""
        Read-Host "Press Enter to continue..."
    }
}

# Run main function
Main
