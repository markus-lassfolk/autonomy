# Virtual RUTOS Testing Environment Setup
# This script creates a virtual RUTOS environment for testing before physical deployment

param(
    [string]$Action = "menu",
    [string]$WSLName = "rutos-test",
    [string]$RutosVersion = "7.12.1"
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

Write-Status "Virtual RUTOS Testing Environment Setup"
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

# Option 1: Create dedicated Ubuntu WSL instance for RUTOS testing
function Setup-VirtualRutosWSL {
    Write-Status "Setting up virtual RUTOS testing environment..."

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

    Write-Status "Installing dedicated Ubuntu WSL instance for RUTOS testing..."
    Write-Status "This will create a separate Ubuntu instance named '$WSLName'"
    Write-Status "Your existing Ubuntu installation will remain untouched"

    # Install Ubuntu with a specific name
    wsl --install -d Ubuntu --name $WSLName

    Write-Status "Waiting for Ubuntu installation to complete..."
    Start-Sleep -Seconds 20

    Write-Status "Setting up RUTOS testing environment..."
    wsl -d $WSLName -e bash -c @"
# Update Ubuntu
sudo apt-get update
sudo apt-get upgrade -y

# Install RUTOS build dependencies
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
    qemu-system-arm \
    qemu-utils \
    && sudo rm -rf /var/lib/apt/lists/*

# Create test directories
sudo mkdir -p /etc/config /var/log /tmp/autonomy /usr/bin /opt/rutos

# Create mock RUTOS environment
sudo bash -c 'echo "config system" > /etc/config/system'
sudo bash -c 'echo "    option hostname \"rutos-test\"" >> /etc/config/system'
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

# Create RUTOS-specific mock commands
sudo bash -c 'echo "#!/bin/bash" > /usr/bin/ubus'
sudo bash -c 'echo "echo \"Mock ubus - \$*\"" >> /usr/bin/ubus'
sudo chmod +x /usr/bin/ubus

sudo bash -c 'echo "#!/bin/bash" > /usr/bin/uci'
sudo bash -c 'echo "echo \"Mock uci - \$*\"" >> /usr/bin/uci'
sudo chmod +x /usr/bin/uci

sudo bash -c 'echo "#!/bin/bash" > /usr/bin/opkg'
sudo bash -c 'echo "echo \"Mock opkg - \$*\"" >> /usr/bin/opkg'
sudo chmod +x /usr/bin/opkg

# Create RUTOS-specific commands
sudo bash -c 'echo "#!/bin/bash" > /usr/bin/gpsctl'
sudo bash -c 'echo "echo \"Mock gpsctl - \$*\"" >> /usr/bin/gpsctl'
sudo chmod +x /usr/bin/gpsctl

sudo bash -c 'echo "#!/bin/bash" > /usr/bin/gsmctl'
sudo bash -c 'echo "echo \"Mock gsmctl - \$*\"" >> /usr/bin/gsmctl'
sudo chmod +x /usr/bin/gsmctl

# Create workspace directory
mkdir -p /workspace
echo "Virtual RUTOS environment setup complete!"
echo "This instance simulates RUTOS for testing"
"@

    Write-Success "Virtual RUTOS WSL instance '$WSLName' created successfully!"
    Write-Status "To access: wsl -d $WSLName"
    Write-Status "To mount your project: wsl -d $WSLName -e bash -c 'sudo mount --bind /mnt/c/your-project-path /workspace'"
    Write-Status "Your main Ubuntu installation remains untouched!"
}

# Option 2: Download and setup RUTOS firmware for QEMU
function Setup-RutosQemu {
    Write-Status "Setting up RUTOS QEMU virtual machine..."

    if (!(Test-WSLInstance -InstanceName $WSLName)) {
        Write-Error "WSL instance '$WSLName' not found. Please create it first."
        return
    }

    Write-Status "Downloading RUTOS firmware for QEMU testing..."
    wsl -d $WSLName -e bash -c @"
cd /workspace

# Create RUTOS testing directory
mkdir -p rutos-qemu
cd rutos-qemu

# Download RUTOS firmware (example for RUTX50)
echo "Downloading RUTOS firmware for QEMU testing..."
wget -O rutx50-firmware.bin "https://files.teltonika-networks.com/firmwares/rutx50/RUTX50_R_00.07.12.1.bin"

# Create QEMU startup script
cat > start-rutos-qemu.sh << 'EOF'
#!/bin/bash

# RUTOS QEMU Virtual Machine Startup Script
echo "Starting RUTOS virtual machine..."

# QEMU parameters for ARM-based RUTOS device simulation
qemu-system-arm \
    -M vexpress-a9 \
    -cpu cortex-a9 \
    -m 512M \
    -kernel vmlinuz \
    -initrd initrd.img \
    -append "root=/dev/ram0 console=ttyAMA0" \
    -serial stdio \
    -net nic,model=lan9118 \
    -net user,hostfwd=tcp::2222-:22,hostfwd=tcp::8080-:80 \
    -display gtk \
    -name "RUTOS Virtual Machine"

echo "RUTOS virtual machine started!"
echo "SSH access: ssh root@localhost -p 2222"
echo "Web interface: http://localhost:8080"
EOF

chmod +x start-rutos-qemu.sh

echo "RUTOS QEMU setup completed!"
echo "To start: ./start-rutos-qemu.sh"
"@

    Write-Success "RUTOS QEMU setup completed!"
    Write-Status "Virtual RUTOS machine ready for testing"
}

# Option 3: Build and test RUTOS packages
function Build-AndTestRutosPackages {
    Write-Status "Building and testing RUTOS packages..."

    if (!(Test-WSLInstance -InstanceName $WSLName)) {
        Write-Error "WSL instance '$WSLName' not found. Please create it first."
        return
    }

    # Build RUTOS-compatible packages
    $BuildScriptPath = Join-Path $ProjectRoot "build-rutos-package-fixed.ps1"
    if (Test-Path $BuildScriptPath) {
        Write-Status "Building RUTOS packages..."
        & powershell -ExecutionPolicy Bypass -File $BuildScriptPath -Architecture "arm_cortex-a7_neon-vfpv4"
    }
    else {
        Write-Error "RUTOS build script not found!"
        return
    }

    # Copy packages to WSL and test
    Write-Status "Copying packages to virtual RUTOS and testing..."
    $WSLPath = "/mnt/$(($ProjectRoot -replace ':', '').ToLower())"

    wsl -d $WSLName -e bash -c @"
cd $WSLPath/build

echo "Testing RUTOS package installation in virtual environment..."
echo "Available packages:"
ls -la *.ipk

echo "Installing autonomy package..."
opkg install autonomy_1.0.0_arm_cortex-a7_neon-vfpv4.ipk

echo "Installing LuCI package..."
opkg install luci-app-autonomy_1.0.0_all.ipk

echo "Testing RUTOS-specific commands..."
gpsctl --status
gsmctl --status

echo "Testing service..."
/etc/init.d/autonomy start
/etc/init.d/autonomy status

echo "Testing ubus interface..."
ubus call autonomy status

echo "RUTOS package test completed in virtual environment!"
"@

    Write-Success "RUTOS package testing completed!"
}

# Option 4: Start virtual RUTOS shell
function Start-VirtualRutosShell {
    Write-Status "Starting virtual RUTOS shell..."

    if (!(Test-WSLInstance -InstanceName $WSLName)) {
        Write-Error "WSL instance '$WSLName' not found. Please create it first."
        return
    }

    Write-Status "Starting virtual RUTOS shell for $WSLName..."
    Write-Status "This simulates a RUTOS environment for testing"
    Write-Status "Use 'exit' to return to Windows"
    wsl -d $WSLName
}

# Option 5: Mount project directory to virtual RUTOS
function Mount-ProjectDirectory {
    Write-Status "Mounting project directory to virtual RUTOS..."

    if (!(Test-WSLInstance -InstanceName $WSLName)) {
        Write-Error "WSL instance '$WSLName' not found. Please create it first."
        return
    }

    $WSLPath = "/mnt/$(($ProjectRoot -replace ':', '').ToLower())"

    Write-Status "Mounting $ProjectRoot to /workspace in virtual RUTOS..."
    wsl -d $WSLName -e bash -c "sudo mount --bind $WSLPath /workspace"

    Write-Success "Project directory mounted successfully!"
    Write-Status "Access your project at /workspace in virtual RUTOS"
}

# Option 6: Test RUTOS-specific functionality
function Test-RutosFunctionality {
    Write-Status "Testing RUTOS-specific functionality..."

    if (!(Test-WSLInstance -InstanceName $WSLName)) {
        Write-Error "WSL instance '$WSLName' not found. Please create it first."
        return
    }

    Write-Status "Running RUTOS-specific tests..."
    wsl -d $WSLName -e bash -c @"
cd /workspace

echo "Testing RUTOS-specific commands and configurations..."

# Test GPS functionality
echo "Testing GPS commands..."
gpsctl --status
gpsctl --location

# Test GSM functionality
echo "Testing GSM commands..."
gsmctl --status
gsmctl --signal

# Test UCI configuration
echo "Testing UCI configuration..."
uci show autonomy
uci set autonomy.main.enabled=1
uci commit autonomy

# Test ubus interface
echo "Testing ubus interface..."
ubus call autonomy status
ubus call autonomy cellular_status
ubus call autonomy gps_status

# Test service management
echo "Testing service management..."
/etc/init.d/autonomy start
/etc/init.d/autonomy status
/etc/init.d/autonomy stop

echo "RUTOS-specific functionality tests completed!"
"@

    Write-Success "RUTOS functionality testing completed!"
}

# Option 7: Clean up virtual RUTOS instance
function Cleanup-VirtualRutos {
    Write-Status "Cleaning up virtual RUTOS instance..."

    if (!(Test-WSLInstance -InstanceName $WSLName)) {
        Write-Warning "WSL instance '$WSLName' not found. Nothing to clean up."
        return
    }

    $choice = Read-Host "Are you sure you want to remove the virtual RUTOS instance '$WSLName'? (y/N)"
    if ($choice -eq "y" -or $choice -eq "Y") {
        Write-Status "Removing virtual RUTOS instance..."
        wsl --unregister $WSLName
        Write-Success "Virtual RUTOS instance removed successfully!"
        Write-Status "Your main Ubuntu installation remains untouched."
    } else {
        Write-Status "Cleanup cancelled."
    }
}

# Show menu
function Show-Menu {
    Write-Host ""
    Write-Host "Virtual RUTOS Testing Options:"
    Write-Host "============================="
    Write-Host "1. Create virtual RUTOS WSL instance"
    Write-Host "2. Setup RUTOS QEMU virtual machine"
    Write-Host "3. Build and test RUTOS packages"
    Write-Host "4. Start virtual RUTOS shell"
    Write-Host "5. Mount project directory"
    Write-Host "6. Test RUTOS-specific functionality"
    Write-Host "7. Clean up virtual RUTOS instance"
    Write-Host "8. List WSL instances"
    Write-Host "9. Exit"
    Write-Host ""
}

# List WSL instances
function List-WSLInstances {
    Write-Status "WSL instances:"
    wsl -l -v
    Write-Status ""
    Write-Status "Note: Virtual RUTOS instance is separate from your main Ubuntu"
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
    $BuildScriptPath = Join-Path $ProjectRoot "build-rutos-package-fixed.ps1"
    if (Test-Path $BuildScriptPath) {
        Write-Success "RUTOS build script found"
    }
    else {
        Write-Warning "RUTOS build script not found"
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
            "1" { Setup-VirtualRutosWSL }
            "2" { Setup-RutosQemu }
            "3" { Build-AndTestRutosPackages }
            "4" { Start-VirtualRutosShell }
            "5" { Mount-ProjectDirectory }
            "6" { Test-RutosFunctionality }
            "7" { Cleanup-VirtualRutos }
            "8" { List-WSLInstances }
            "check" { Check-Dependencies }
            default { Write-Error "Invalid action: $Action" }
        }
        return
    }

    # Interactive menu
    while ($true) {
        Show-Menu
        $choice = Read-Host "Select an option (1-9)"

        switch ($choice) {
            "1" { Setup-VirtualRutosWSL }
            "2" { Setup-RutosQemu }
            "3" { Build-AndTestRutosPackages }
            "4" { Start-VirtualRutosShell }
            "5" { Mount-ProjectDirectory }
            "6" { Test-RutosFunctionality }
            "7" { Cleanup-VirtualRutos }
            "8" { List-WSLInstances }
            "9" {
                Write-Success "Exiting..."
                exit 0
            }
            default {
                Write-Error "Invalid option. Please select 1-9."
            }
        }

        Write-Host ""
        Read-Host "Press Enter to continue..."
    }
}

# Run main function
Main
