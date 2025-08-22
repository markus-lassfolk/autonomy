# Simple OpenWrt Testing Environment
# This script provides a quick way to test OpenWrt packages

param(
    [string]$Action = "menu"
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

# Option 1: Quick Docker Test
function Setup-DockerTest {
    Write-Status "Setting up Docker-based OpenWrt test..."

    if (!(Test-Docker)) {
        Write-Error "Docker not available. Please install Docker Desktop first."
        return
    }

    # Create simple Dockerfile
    $DockerfilePath = Join-Path $ScriptDir "docker\Dockerfile.simple"
    $DockerDir = Join-Path $ScriptDir "docker"

    if (!(Test-Path $DockerDir)) {
        New-Item -ItemType Directory -Path $DockerDir | Out-Null
    }

    $DockerfileContent = @"
# Simple OpenWrt Test Environment
FROM ubuntu:22.04

# Install basic tools
RUN apt-get update && apt-get install -y \
    bash \
    curl \
    wget \
    git \
    make \
    gcc \
    && rm -rf /var/lib/apt/lists/*

# Create test directories
RUN mkdir -p /etc/config /var/log /tmp/autonomy /usr/bin

# Create mock OpenWrt environment
RUN echo "config system" > /etc/config/system && \
    echo "    option hostname 'openwrt-test'" >> /etc/config/system

# Create mock network config
RUN echo "config interface 'loopback'" > /etc/config/network && \
    echo "    option ifname 'lo'" >> /etc/config/network && \
    echo "    option proto 'static'" >> /etc/config/network && \
    echo "    option ipaddr '127.0.0.1'" >> /etc/config/network

# Create mock commands
RUN echo '#!/bin/bash' > /usr/bin/ubus && \
    echo 'echo "Mock ubus - $*"' >> /usr/bin/ubus && \
    chmod +x /usr/bin/ubus

RUN echo '#!/bin/bash' > /usr/bin/uci && \
    echo 'echo "Mock uci - $*"' >> /usr/bin/uci && \
    chmod +x /usr/bin/uci

RUN echo '#!/bin/bash' > /usr/bin/opkg && \
    echo 'echo "Mock opkg - $*"' >> /usr/bin/opkg && \
    chmod +x /usr/bin/opkg

# Expose ports
EXPOSE 80 8080

# Default command
CMD ["/bin/bash"]
"@

    $DockerfileContent | Out-File -FilePath $DockerfilePath -Encoding UTF8

    # Build the Docker image
    Write-Status "Building simple OpenWrt test environment..."
    docker build -f $DockerfilePath -t openwrt-simple:latest $DockerDir

    Write-Success "Docker OpenWrt environment ready!"
    Write-Status "To run: docker run -it --rm openwrt-simple:latest"
}

# Option 2: Build and Test Packages
function Build-AndTest {
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

    Write-Success "Packages built successfully!"
    Write-Status "Check the build-openwrt directory for your packages"
}

# Option 3: WSL Setup
function Setup-WSL {
    Write-Status "Setting up WSL-based testing..."

    if (!(Test-WSL)) {
        Write-Error "WSL not available. Please install WSL first."
        return
    }

    # Create WSL setup script
    $WSLScriptPath = Join-Path $ScriptDir "wsl-setup.sh"

    $WSLScriptContent = @"
#!/bin/bash

echo "Setting up WSL environment for OpenWrt testing..."

# Install basic tools
sudo apt-get update
sudo apt-get install -y wget curl git make gcc

# Download OpenWrt Image Builder
OPENWRT_VERSION="22.03.5"
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

    $WSLScriptContent | Out-File -FilePath $WSLScriptPath -Encoding UTF8

    Write-Success "WSL setup script created!"
    Write-Status "To run: wsl bash $WSLScriptPath"
}

# Show menu
function Show-Menu {
    Write-Host ""
    Write-Host "OpenWrt Testing Options:"
    Write-Host "======================="
    Write-Host "1. Setup Docker test environment"
    Write-Host "2. Build and test packages"
    Write-Host "3. Setup WSL environment"
    Write-Host "4. Check dependencies"
    Write-Host "5. Exit"
    Write-Host ""
}

# Check dependencies
function Check-Dependencies {
    Write-Status "Checking dependencies..."

    if (Test-Docker) {
        Write-Success "Docker available"
    }
    else {
        Write-Warning "Docker not available (install Docker Desktop)"
    }

    if (Test-WSL) {
        Write-Success "WSL available"
    }
    else {
        Write-Warning "WSL not available (install WSL)"
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
            "1" { Setup-DockerTest }
            "2" { Build-AndTest }
            "3" { Setup-WSL }
            "4" { Check-Dependencies }
            default { Write-Error "Invalid action: $Action" }
        }
        return
    }

    # Interactive menu
    while ($true) {
        Show-Menu
        $choice = Read-Host "Select an option (1-5)"

        switch ($choice) {
            "1" { Setup-DockerTest }
            "2" { Build-AndTest }
            "3" { Setup-WSL }
            "4" { Check-Dependencies }
            "5" {
                Write-Success "Exiting..."
                exit 0
            }
            default {
                Write-Error "Invalid option. Please select 1-5."
            }
        }

        Write-Host ""
        Read-Host "Press Enter to continue..."
    }
}

# Run main function
Main
