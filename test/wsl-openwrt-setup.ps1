# WSL-based OpenWrt Testing Environment Setup
# This script creates a dedicated WSL instance for OpenWrt testing

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

Write-Status "WSL OpenWrt Testing Environment Setup"
Write-Status "======================================"

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

# Get available WSL distributions
function Get-WSLDistributions {
    try {
        $distros = wsl --list --online
        return $distros
    }
    catch {
        return @()
    }
}

# Option 1: Create WSL instance with Alpine Linux (lightweight)
function Setup-AlpineWSL {
    Write-Status "Setting up Alpine Linux WSL instance for OpenWrt testing..."

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

    Write-Status "Installing Alpine Linux WSL instance..."
    wsl --install -d Alpine

    Write-Status "Waiting for Alpine installation to complete..."
    Start-Sleep -Seconds 10

    Write-Status "Setting up Alpine environment..."
    wsl -d $WSLName -e sh -c @"
# Update Alpine
apk update
apk upgrade

# Install OpenWrt build dependencies
apk add --no-cache \
    bash \
    curl \
    wget \
    git \
    make \
    gcc \
    g++ \
    musl-dev \
    linux-headers \
    pkgconfig \
    cmake \
    python3 \
    py3-pip \
    tar \
    gzip \
    bzip2 \
    xz \
    patch \
    diffutils \
    findutils \
    grep \
    sed \
    jq \
    && rm -rf /var/cache/apk/*

# Create test directories
mkdir -p /etc/config /var/log /tmp/autonomy /usr/bin

# Create mock OpenWrt environment
echo "config system" > /etc/config/system
echo "    option hostname 'openwrt-test'" >> /etc/config/system
echo "    option timezone 'UTC'" >> /etc/config/system

# Create mock network config
echo "config interface 'loopback'" > /etc/config/network
echo "    option ifname 'lo'" >> /etc/config/network
echo "    option proto 'static'" >> /etc/config/network
echo "    option ipaddr '127.0.0.1'" >> /etc/config/network

# Create mock mwan3 config
echo "config globals 'globals'" > /etc/config/mwan3
echo "    option mmx_mask '0x3F00'" >> /etc/config/mwan3
echo "    option local_source 'lan'" >> /etc/config/mwan3

echo "config interface 'wan'" >> /etc/config/mwan3
echo "    option enabled '1'" >> /etc/config/mwan3
echo "    option family 'ipv4'" >> /etc/config/mwan3
echo "    option track_method 'ping'" >> /etc/config/mwan3
echo "    option track_ip '8.8.8.8'" >> /etc/config/mwan3
echo "    option reliability '1'" >> /etc/config/mwan3
echo "    option count '1'" >> /etc/config/mwan3
echo "    option timeout '2'" >> /etc/config/mwan3
echo "    option interval '5'" >> /etc/config/mwan3
echo "    option down '3'" >> /etc/config/mwan3
echo "    option up '3'" >> /etc/config/mwan3

# Create mock commands
echo '#!/bin/bash' > /usr/bin/ubus
echo 'echo "Mock ubus - $*"' >> /usr/bin/ubus
chmod +x /usr/bin/ubus

echo '#!/bin/bash' > /usr/bin/uci
echo 'echo "Mock uci - $*"' >> /usr/bin/uci
chmod +x /usr/bin/uci

echo '#!/bin/bash' > /usr/bin/opkg
echo 'echo "Mock opkg - $*"' >> /usr/bin/opkg
chmod +x /usr/bin/opkg

# Create workspace directory
mkdir -p /workspace
echo "Alpine WSL environment setup complete!"
"@

    Write-Success "Alpine WSL instance '$WSLName' created successfully!"
    Write-Status "To access: wsl -d $WSLName"
    Write-Status "To mount your project: wsl -d $WSLName -e sh -c 'mount --bind /mnt/c/your-project-path /workspace'"
}

# Option 2: Create WSL instance with Ubuntu (full-featured)
function Setup-UbuntuWSL {
    Write-Status "Setting up Ubuntu WSL instance for OpenWrt testing..."

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

    Write-Status "Installing Ubuntu WSL instance..."
    wsl --install -d Ubuntu

    Write-Status "Waiting for Ubuntu installation to complete..."
    Start-Sleep -Seconds 15

    Write-Status "Setting up Ubuntu environment..."
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
echo "Ubuntu WSL environment setup complete!"
"@

    Write-Success "Ubuntu WSL instance '$WSLName' created successfully!"
    Write-Status "To access: wsl -d $WSLName"
    Write-Status "To mount your project: wsl -d $WSLName -e bash -c 'sudo mount --bind /mnt/c/your-project-path /workspace'"
}

# Option 3: Download and setup OpenWrt Image Builder
function Setup-OpenWrtImageBuilder {
    Write-Status "Setting up OpenWrt Image Builder in WSL..."

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
    Write-Status "Custom OpenWrt images created in WSL workspace"
}

# Option 4: Build and test packages in WSL
function Build-AndTestPackages {
    Write-Status "Building and testing packages in WSL..."

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
    Write-Status "Copying packages to WSL and testing..."
    $WSLPath = "/mnt/$(($ProjectRoot -replace ':', '').ToLower())"

    wsl -d $WSLName -e bash -c @"
cd $WSLPath/build-openwrt

echo "Testing package installation..."
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

echo "Package test completed!"
"@

    Write-Success "Package testing completed in WSL!"
}

# Option 5: Start WSL shell
function Start-WSLShell {
    Write-Status "Starting WSL shell..."

    if (!(Test-WSLInstance -InstanceName $WSLName)) {
        Write-Error "WSL instance '$WSLName' not found. Please create it first."
        return
    }

    Write-Status "Starting WSL shell for $WSLName..."
    Write-Status "Use 'exit' to return to Windows"
    wsl -d $WSLName
}

# Option 6: Mount project directory
function Mount-ProjectDirectory {
    Write-Status "Mounting project directory to WSL..."

    if (!(Test-WSLInstance -InstanceName $WSLName)) {
        Write-Error "WSL instance '$WSLName' not found. Please create it first."
        return
    }

    $WSLPath = "/mnt/$(($ProjectRoot -replace ':', '').ToLower())"

    Write-Status "Mounting $ProjectRoot to /workspace in WSL..."
    wsl -d $WSLName -e bash -c "sudo mount --bind $WSLPath /workspace"

    Write-Success "Project directory mounted successfully!"
    Write-Status "Access your project at /workspace in WSL"
}

# Show menu
function Show-Menu {
    Write-Host ""
    Write-Host "WSL OpenWrt Testing Options:"
    Write-Host "============================"
    Write-Host "1. Create Alpine Linux WSL instance (Lightweight)"
    Write-Host "2. Create Ubuntu WSL instance (Full-featured)"
    Write-Host "3. Setup OpenWrt Image Builder"
    Write-Host "4. Build and test packages"
    Write-Host "5. Start WSL shell"
    Write-Host "6. Mount project directory"
    Write-Host "7. List WSL instances"
    Write-Host "8. Exit"
    Write-Host ""
}

# List WSL instances
function List-WSLInstances {
    Write-Status "WSL instances:"
    wsl -l -v
}

# Check dependencies
function Check-Dependencies {
    Write-Status "Checking dependencies..."

    if (Test-WSL) {
        Write-Success "WSL available"
        Write-Status "Available distributions:"
        Get-WSLDistributions
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
            "1" { Setup-AlpineWSL }
            "2" { Setup-UbuntuWSL }
            "3" { Setup-OpenWrtImageBuilder }
            "4" { Build-AndTestPackages }
            "5" { Start-WSLShell }
            "6" { Mount-ProjectDirectory }
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
            "1" { Setup-AlpineWSL }
            "2" { Setup-UbuntuWSL }
            "3" { Setup-OpenWrtImageBuilder }
            "4" { Build-AndTestPackages }
            "5" { Start-WSLShell }
            "6" { Mount-ProjectDirectory }
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
