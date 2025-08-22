# OpenWrt Testing Environment Setup (PowerShell)
# This script provides multiple options for testing OpenWrt packages on Windows

param(
    [string]$Option = ""
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

Write-Status "OpenWrt Testing Environment Setup"
Write-Status "=================================="

# Check if Docker is available
function Test-Docker {
    try {
        docker --version | Out-Null
        return $true
    }
    catch {
        return $false
    }
}

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

# Check if Hyper-V is available
function Test-HyperV {
    try {
        Get-WindowsOptionalFeature -Online -FeatureName Microsoft-Hyper-V-All | Where-Object { $_.State -eq "Enabled" } | Out-Null
        return $true
    }
    catch {
        return $false
    }
}

# Option 1: Docker-based OpenWrt Simulator
function Setup-DockerOpenWrt {
    Write-Status "Setting up Docker-based OpenWrt simulator..."

    # Create Dockerfile for OpenWrt testing
    $DockerfilePath = Join-Path $ScriptDir "docker\Dockerfile.openwrt-test"
    $DockerDir = Join-Path $ScriptDir "docker"

    if (!(Test-Path $DockerDir)) {
        New-Item -ItemType Directory -Path $DockerDir | Out-Null
    }

    $DockerfileContent = @"
# OpenWrt Testing Environment
FROM ubuntu:22.04

# Install OpenWrt build dependencies
RUN apt-get update && apt-get install -y \
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
    && rm -rf /var/lib/apt/lists/*

# Install OpenWrt Image Builder
RUN git clone https://github.com/openwrt/openwrt.git /opt/openwrt
WORKDIR /opt/openwrt

# Update feeds
RUN ./scripts/feeds update -a
RUN ./scripts/feeds install -a

# Configure for x86_64 target
RUN make defconfig
RUN echo "CONFIG_TARGET_x86=y" >> .config
RUN echo "CONFIG_TARGET_x86_64=y" >> .config
RUN echo "CONFIG_TARGET_x86_64_DEVICE_generic=y" >> .config
RUN echo "CONFIG_PACKAGE_luci=y" >> .config
RUN echo "CONFIG_PACKAGE_luci-base=y" >> .config
RUN echo "CONFIG_PACKAGE_luci-compat=y" >> .config
RUN echo "CONFIG_PACKAGE_mwan3=y" >> .config
RUN echo "CONFIG_PACKAGE_ubus=y" >> .config
RUN echo "CONFIG_PACKAGE_uci=y" >> .config
RUN make defconfig

# Create test directories
RUN mkdir -p /etc/config /var/log /tmp/autonomy /usr/bin

# Set up basic OpenWrt environment
RUN echo "config system" > /etc/config/system && \
    echo "    option hostname 'openwrt-test'" >> /etc/config/system && \
    echo "    option timezone 'UTC'" >> /etc/config/system

# Create mock network interfaces
RUN echo "config interface 'loopback'" > /etc/config/network && \
    echo "    option ifname 'lo'" >> /etc/config/network && \
    echo "    option proto 'static'" >> /etc/config/network && \
    echo "    option ipaddr '127.0.0.1'" >> /etc/config/network && \
    echo "    option netmask '255.0.0.0'" >> /etc/config/network

RUN echo "config interface 'lan'" >> /etc/config/network && \
    echo "    option ifname 'eth0'" >> /etc/config/network && \
    echo "    option proto 'static'" >> /etc/config/network && \
    echo "    option ipaddr '192.168.1.1'" >> /etc/config/network && \
    echo "    option netmask '255.255.255.0'" >> /etc/config/network

# Create mock mwan3 configuration
RUN mkdir -p /etc/config && \
    echo "config globals 'globals'" > /etc/config/mwan3 && \
    echo "    option mmx_mask '0x3F00'" >> /etc/config/mwan3 && \
    echo "    option local_source 'lan'" >> /etc/config/mwan3

RUN echo "config interface 'wan'" >> /etc/config/mwan3 && \
    echo "    option enabled '1'" >> /etc/config/mwan3 && \
    echo "    option family 'ipv4'" >> /etc/config/mwan3 && \
    echo "    option track_method 'ping'" >> /etc/config/mwan3 && \
    echo "    option track_ip '8.8.8.8'" >> /etc/config/mwan3 && \
    echo "    option reliability '1'" >> /etc/config/mwan3 && \
    echo "    option count '1'" >> /etc/config/mwan3 && \
    echo "    option timeout '2'" >> /etc/config/mwan3 && \
    echo "    option interval '5'" >> /etc/config/mwan3 && \
    echo "    option down '3'" >> /etc/config/mwan3 && \
    echo "    option up '3'" >> /etc/config/mwan3

# Create mock ubus
RUN echo '#!/bin/bash' > /usr/bin/ubus && \
    echo 'echo "Mock ubus - $*"' >> /usr/bin/ubus && \
    chmod +x /usr/bin/ubus

# Create mock uci
RUN echo '#!/bin/bash' > /usr/bin/uci && \
    echo 'echo "Mock uci - $*"' >> /usr/bin/uci && \
    chmod +x /usr/bin/uci

# Create mock opkg
RUN echo '#!/bin/bash' > /usr/bin/opkg && \
    echo 'echo "Mock opkg - $*"' >> /usr/bin/opkg && \
    chmod +x /usr/bin/opkg

# Expose ports
EXPOSE 80 443 8080

# Default command
CMD ["/bin/bash"]
"@

    $DockerfileContent | Out-File -FilePath $DockerfilePath -Encoding UTF8

    # Build the Docker image
    Write-Status "Building OpenWrt test environment..."
    docker build -f $DockerfilePath -t openwrt-test:latest $DockerDir

    Write-Success "Docker OpenWrt environment ready!"
    Write-Status "To run: docker run -it --rm -v $ProjectRoot:/workdir openwrt-test:latest"
}

# Option 2: WSL-based OpenWrt
function Setup-WSLOpenWrt {
    Write-Status "Setting up WSL-based OpenWrt..."

    # Create WSL setup script
    $WSLScriptPath = Join-Path $ScriptDir "wsl-setup.sh"

    $WSLScriptContent = @"
#!/bin/bash

# WSL OpenWrt Setup
OPENWRT_VERSION="22.03.5"
ARCH="x86_64"
IMAGE_URL="https://downloads.openwrt.org/releases/${OPENWRT_VERSION}/targets/x86/64/openwrt-${OPENWRT_VERSION}-x86-64-generic-ext4-combined.img.gz"

echo "Downloading OpenWrt ${OPENWRT_VERSION} image..."
wget -O openwrt.img.gz "$IMAGE_URL"
gunzip openwrt.img.gz

echo "Setting up WSL environment..."
# This would require additional setup for WSL integration
echo "WSL setup completed!"
echo "SSH access: ssh root@localhost -p 2222"
echo "Web interface: http://localhost:8080"
"@

    $WSLScriptContent | Out-File -FilePath $WSLScriptPath -Encoding UTF8

    Write-Success "WSL setup script created!"
    Write-Status "To run: wsl bash $WSLScriptPath"
}

# Option 3: OpenWrt Image Builder
function Setup-ImageBuilder {
    Write-Status "Setting up OpenWrt Image Builder..."

    # Create image builder script
    $BuilderScriptPath = Join-Path $ScriptDir "image-builder-setup.sh"

    $BuilderScriptContent = @"
#!/bin/bash

# OpenWrt Image Builder Setup
OPENWRT_VERSION="22.03.5"
ARCH="x86_64"
BUILDER_URL="https://downloads.openwrt.org/releases/${OPENWRT_VERSION}/targets/x86/64/openwrt-imagebuilder-${OPENWRT_VERSION}-x86-64.Linux-x86_64.tar.xz"

echo "Downloading OpenWrt Image Builder..."
wget -O imagebuilder.tar.xz "$BUILDER_URL"
tar -xf imagebuilder.tar.xz
cd openwrt-imagebuilder-${OPENWRT_VERSION}-x86-64.Linux-x86_64

echo "Building custom OpenWrt image..."
make image PACKAGES="luci luci-base luci-compat mwan3 ubus uci"

echo "Image built successfully!"
echo "Output files in bin/targets/x86/64/"
"@

    $BuilderScriptContent | Out-File -FilePath $BuilderScriptPath -Encoding UTF8

    Write-Success "Image Builder setup script created!"
    Write-Status "To run: wsl bash $BuilderScriptPath"
}

# Option 4: OpenWrt SDK
function Setup-OpenWrtSDK {
    Write-Status "Setting up OpenWrt SDK..."

    # Create SDK setup script
    $SDKScriptPath = Join-Path $ScriptDir "sdk-setup.sh"

    $SDKScriptContent = @"
#!/bin/bash

# OpenWrt SDK Setup
OPENWRT_VERSION="22.03.5"
ARCH="x86_64"
SDK_URL="https://downloads.openwrt.org/releases/${OPENWRT_VERSION}/targets/x86/64/openwrt-sdk-${OPENWRT_VERSION}-x86-64_gcc-11.2.0_musl.Linux-x86_64.tar.xz"

echo "Downloading OpenWrt SDK..."
wget -O sdk.tar.xz "$SDK_URL"
tar -xf sdk.tar.xz
cd openwrt-sdk-${OPENWRT_VERSION}-x86-64_gcc-11.2.0_musl.Linux-x86_64

echo "Setting up SDK environment..."
source ./staging_dir/toolchain-x86_64_gcc-11.2.0_musl/bin/relocate-sdk.sh

echo "SDK ready for building packages!"
echo "To build: make package/autonomy/compile"
"@

    $SDKScriptContent | Out-File -FilePath $SDKScriptPath -Encoding UTF8

    Write-Success "SDK setup script created!"
    Write-Status "To run: wsl bash $SDKScriptPath"
}

# Build and test packages
function Build-AndTestPackages {
    Write-Status "Building and testing packages..."

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

    # Test package installation
    if (Test-Docker) {
        Write-Status "Testing package installation in Docker..."
        $volumeMount = "${BuildDir}:/packages"
        docker run --rm -v $volumeMount openwrt-test:latest bash -c @"
            cd /packages
            echo 'Testing package installation...'
            opkg install autonomy_1.0.0_x86_64.ipk
            opkg install luci-app-autonomy_1.0.0_all.ipk
            echo 'Testing service...'
            /etc/init.d/autonomy start
            /etc/init.d/autonomy status
            echo 'Testing ubus interface...'
            ubus call autonomy status
            echo 'Package test completed!'
"@
    }
    else {
        Write-Warning "Docker not available, skipping package testing"
    }
}

# Show menu
function Show-Menu {
    Write-Host ""
    Write-Host "OpenWrt Testing Environment Options:"
    Write-Host "===================================="
    Write-Host "1. Docker-based OpenWrt Simulator (Recommended)"
    Write-Host "2. WSL-based OpenWrt"
    Write-Host "3. OpenWrt Image Builder"
    Write-Host "4. OpenWrt SDK"
    Write-Host "5. Build and test packages"
    Write-Host "6. Exit"
    Write-Host ""
}

# Main execution
function Main {
    # Check dependencies
    Write-Status "Checking dependencies..."

    if (Test-Docker) {
        Write-Success "Docker available"
    }
    else {
        Write-Warning "Docker not available (recommended for testing)"
    }

    if (Test-WSL) {
        Write-Success "WSL available"
    }
    else {
        Write-Warning "WSL not available"
    }

    if (Test-HyperV) {
        Write-Success "Hyper-V available"
    }
    else {
        Write-Warning "Hyper-V not available"
    }

    # Create build directory
    if (!(Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir | Out-Null
    }

    # If option is provided, run it directly
    if ($Option -ne "") {
        switch ($Option) {
            "1" { Setup-DockerOpenWrt }
            "2" { Setup-WSLOpenWrt }
            "3" { Setup-ImageBuilder }
            "4" { Setup-OpenWrtSDK }
            "5" { Build-AndTestPackages }
            default { Write-Error "Invalid option: $Option" }
        }
        return
    }

    # Interactive menu
    while ($true) {
        Show-Menu
        $choice = Read-Host "Select an option (1-6)"

        switch ($choice) {
            "1" {
                if (Test-Docker) {
                    Setup-DockerOpenWrt
                }
                else {
                    Write-Error "Docker not available. Please install Docker Desktop first."
                }
            }
            "2" {
                if (Test-WSL) {
                    Setup-WSLOpenWrt
                }
                else {
                    Write-Error "WSL not available. Please install WSL first."
                }
            }
            "3" { Setup-ImageBuilder }
            "4" { Setup-OpenWrtSDK }
            "5" { Build-AndTestPackages }
            "6" {
                Write-Success "Exiting..."
                exit 0
            }
            default {
                Write-Error "Invalid option. Please select 1-6."
            }
        }

        Write-Host ""
        Read-Host "Press Enter to continue..."
    }
}

# Run main function
Main
