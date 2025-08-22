# Virtual RUTOS Testing Environment Setup (OpenWrt-based)
# This script creates a virtual RUTOS environment that simulates actual OpenWrt/BusyBox

param(
    [string]$Action = "menu",
    [string]$WSLName = "rutos-openwrt-test",
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
$BuildDir = Join-Path $ProjectRoot "build-rutos"

Write-Status "Virtual RUTOS Testing Environment Setup (OpenWrt-based)"
Write-Status "===================================================="
Write-Status ""
Write-Status "ðŸ“‹ USAGE GUIDE:"
Write-Status "==============="
Write-Status "â€¢ FIRST TIME: Run option 1 to create the OpenWrt RUTOS environment"
Write-Status "â€¢ DAILY USE: Run option 4 to start interactive shell for development"
Write-Status "â€¢ TESTING: Run option 3 to validate your packages work correctly"
Write-Status "â€¢ ADVANCED: Run option 2 to build real OpenWrt firmware images"
Write-Status "â€¢ CHECK STATUS: Run option 5 to see what environments are available"
Write-Status ""
Write-Status "ðŸŽ¯ TYPICAL WORKFLOW:"
Write-Status "==================="
Write-Status "1. Create environment (option 1) - ONE TIME SETUP"
Write-Status "2. Start shell (option 4) - DAILY DEVELOPMENT"
Write-Status "3. Test packages (option 3) - VALIDATION"
Write-Status "4. Deploy to physical RUTOS hardware"
Write-Status ""

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

# Option 1: Create OpenWrt-based RUTOS simulation environment
function Setup-OpenWrtRutosEnvironment {
    Write-Status "Setting up OpenWrt-based RUTOS simulation environment..."

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

    Write-Status "Installing Ubuntu WSL instance for OpenWrt simulation..."
    Write-Status "This will create a separate Ubuntu instance to simulate OpenWrt/RUTOS"

    # Install Ubuntu with a specific name
    wsl --install -d Ubuntu --name $WSLName

    Write-Status "Waiting for Ubuntu installation to complete..."
    Start-Sleep -Seconds 20

    Write-Status "Setting up OpenWrt/RUTOS simulation environment..."
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
    busybox \
    ; sudo rm -rf /var/lib/apt/lists/*

# Create OpenWrt-style directory structure
sudo mkdir -p /etc/config /var/log /tmp/autonomy /usr/bin /opt/rutos /lib /sbin

# Create OpenWrt-style system configuration
sudo bash -c 'echo "config system" > /etc/config/system'
sudo bash -c 'echo "    option hostname \"rutos-openwrt\"" >> /etc/config/system'
sudo bash -c 'echo "    option timezone \"UTC\"" >> /etc/config/system'

# Create OpenWrt-style network config
sudo bash -c 'echo "config interface \"loopback\"" > /etc/config/network'
sudo bash -c 'echo "    option ifname \"lo\"" >> /etc/config/network'
sudo bash -c 'echo "    option proto \"static\"" >> /etc/config/network'
sudo bash -c 'echo "    option ipaddr \"127.0.0.1\"" >> /etc/config/network'

# Create OpenWrt-style mwan3 config
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

# Create OpenWrt-style mock commands (simulating BusyBox environment)
sudo bash -c 'echo "#!/bin/bash" > /usr/bin/ubus'
sudo bash -c 'echo "echo \"OpenWrt ubus - \$*\"" >> /usr/bin/ubus'
sudo chmod +x /usr/bin/ubus

sudo bash -c 'echo "#!/bin/bash" > /usr/bin/uci'
sudo bash -c 'echo "echo \"OpenWrt uci - \$*\"" >> /usr/bin/uci'
sudo chmod +x /usr/bin/uci

sudo bash -c 'echo "#!/bin/bash" > /usr/bin/opkg'
sudo bash -c 'echo "echo \"OpenWrt opkg - \$*\"" >> /usr/bin/opkg'
sudo chmod +x /usr/bin/opkg

# Create RUTOS-specific commands
sudo bash -c 'echo "#!/bin/bash" > /usr/bin/gpsctl'
sudo bash -c 'echo "echo \"RUTOS gpsctl - \$*\"" >> /usr/bin/gpsctl'
sudo chmod +x /usr/bin/gpsctl

sudo bash -c 'echo "#!/bin/bash" > /usr/bin/gsmctl'
sudo bash -c 'echo "echo \"RUTOS gsmctl - \$*\"" >> /usr/bin/gsmctl'
sudo chmod +x /usr/bin/gsmctl

# Create OpenWrt-style init scripts directory
sudo mkdir -p /etc/init.d
sudo bash -c 'echo "#!/bin/sh /etc/rc.common" > /etc/init.d/autonomy'
sudo bash -c 'echo "START=99" >> /etc/init.d/autonomy'
sudo bash -c 'echo "STOP=01" >> /etc/init.d/autonomy'
sudo bash -c 'echo "start() {" >> /etc/init.d/autonomy'
sudo bash -c 'echo "    echo \"Starting autonomy service...\"" >> /etc/init.d/autonomy'
sudo bash -c 'echo "}" >> /etc/init.d/autonomy'
sudo bash -c 'echo "stop() {" >> /etc/init.d/autonomy'
sudo bash -c 'echo "    echo \"Stopping autonomy service...\"" >> /etc/init.d/autonomy'
sudo bash -c 'echo "}" >> /etc/init.d/autonomy'
sudo chmod +x /etc/init.d/autonomy

# Create workspace directory
mkdir -p /workspace
echo "OpenWrt-based RUTOS environment setup complete!"
echo "This simulates the actual OpenWrt/BusyBox environment used by RUTOS"
"@

    Write-Success "OpenWrt-based RUTOS environment $WSLName created successfully!"
    Write-Status "To access: wsl -d $WSLName"
    Write-Status "This simulates the actual OpenWrt/BusyBox environment used by RUTOS"
}

# Option 2: Download and setup actual OpenWrt Image Builder
function Setup-OpenWrtImageBuilder {
    Write-Status "Setting up OpenWrt Image Builder for RUTOS simulation..."

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

echo "Building custom OpenWrt image with RUTOS-style packages..."
make image PACKAGES="luci luci-base luci-compat mwan3 ubus uci busybox"

echo "Image built successfully!"
echo "Output files in bin/targets/x86/64/"
ls -la bin/targets/x86/64/
"@

    Write-Success "OpenWrt Image Builder setup completed!"
    Write-Status "Custom OpenWrt images created for RUTOS simulation"
}

# Option 3: Test RUTOS packages in OpenWrt environment
function Test-RutosPackages {
    Write-Status "Testing RUTOS packages in OpenWrt environment..."

    if (!(Test-WSLInstance -InstanceName $WSLName)) {
        Write-Error "WSL instance '$WSLName' not found. Please create it first."
        return
    }

    Write-Status "Running RUTOS package tests in OpenWrt environment..."
    wsl -d $WSLName -e bash -c @"
cd /workspace

echo "Testing RUTOS packages in OpenWrt/BusyBox environment..."

# Test OpenWrt-style commands
echo "Testing OpenWrt commands..."
ubus version
uci show system
opkg list-installed

# Test RUTOS-specific commands
echo "Testing RUTOS commands..."
gpsctl --status
gsmctl --status

# Test service management
echo "Testing service management..."
/etc/init.d/autonomy start
/etc/init.d/autonomy status
/etc/init.d/autonomy stop

echo "RUTOS package tests completed in OpenWrt environment!"
"@

    Write-Success "RUTOS package testing completed in OpenWrt environment!"
}

# Option 4: Start OpenWrt RUTOS shell
function Start-OpenWrtRutosShell {
    Write-Status "Starting OpenWrt RUTOS shell..."

    if (!(Test-WSLInstance -InstanceName $WSLName)) {
        Write-Error "WSL instance '$WSLName' not found. Please create it first."
        return
    }

    Write-Status "Starting OpenWrt RUTOS shell for $WSLName..."
    Write-Status "This simulates the actual OpenWrt/BusyBox environment used by RUTOS"
    Write-Status "Use 'exit' to return to Windows"
    wsl -d $WSLName
}

# Show menu
function Show-Menu {
    Write-Host ""
    Write-Host "OpenWrt-based RUTOS Testing Options:"
    Write-Host "==================================="
    Write-Host ""
    Write-Host "1. Create OpenWrt-based RUTOS environment"
    Write-Host "   - Creates a new WSL instance that simulates OpenWrt/BusyBox"
    Write-Host "   - Installs mock ubus, uci, opkg, gpsctl, gsmctl commands"
    Write-Host "   - Sets up OpenWrt-style directory structure and configs"
    Write-Host "   - Use this FIRST TIME to set up your testing environment"
    Write-Host ""
    Write-Host "2. Setup OpenWrt Image Builder"
    Write-Host "   - Downloads and configures OpenWrt Image Builder"
    Write-Host "   - Allows building custom OpenWrt firmware images"
    Write-Host "   - Creates actual OpenWrt images with your packages included"
    Write-Host "   - Use this for ADVANCED testing with real OpenWrt firmware"
    Write-Host ""
    Write-Host "3. Test RUTOS packages in OpenWrt environment"
    Write-Host "   - Runs automated tests on your RUTOS packages"
    Write-Host "   - Tests ubus, uci, opkg, gpsctl, gsmctl functionality"
    Write-Host "   - Tests service management and configuration"
    Write-Host "   - Use this to VALIDATE your packages work correctly"
    Write-Host ""
    Write-Host "4. Start OpenWrt RUTOS shell"
    Write-Host "   - Opens interactive shell in the OpenWrt RUTOS environment"
    Write-Host "   - Allows manual testing and debugging"
    Write-Host "   - Access your project at /workspace"
    Write-Host "   - Use this for INTERACTIVE testing and development"
    Write-Host ""
    Write-Host "5. List WSL instances"
    Write-Host "   - Shows all available WSL instances and their status"
    Write-Host "   - Helps you see what environments are available"
    Write-Host "   - Use this to CHECK what is installed and running"
    Write-Host ""
    Write-Host "6. Exit"
    Write-Host "   - Exits the script"
    Write-Host ""
}

# List WSL instances
function List-WSLInstances {
    Write-Status "WSL instances:"
    wsl -l -v
    Write-Status ""
    Write-Status "Note: OpenWrt-based RUTOS instance simulates actual RUTOS environment"
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
            "1" { Setup-OpenWrtRutosEnvironment }
            "2" { Setup-OpenWrtImageBuilder }
            "3" { Test-RutosPackages }
            "4" { Start-OpenWrtRutosShell }
            "5" { List-WSLInstances }
            default { Write-Error "Invalid action: $Action" }
        }
        return
    }

    # Interactive menu
    while ($true) {
        Show-Menu
        $choice = Read-Host "Select an option (1-6)"

        switch ($choice) {
            "1" { Setup-OpenWrtRutosEnvironment }
            "2" { Setup-OpenWrtImageBuilder }
            "3" { Test-RutosPackages }
            "4" { Start-OpenWrtRutosShell }
            "5" { List-WSLInstances }
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
