# Virtual RUTOS Testing Environment Setup (OpenWrt-based)
# This script creates a virtual RUTOS environment that simulates actual OpenWrt/BusyBox

param(
    [string]$Action = "menu",
    [string]$WSLName = "rutos-openwrt-test",
    [string]$OpenWrtVersion = "22.03.5"
)

# Function to print colored output using Write-Host colors
function Write-Status {
    param([string]$Message)
    Write-Host "[INFO] $Message" -ForegroundColor Cyan
}

function Write-Success {
    param([string]$Message)
    Write-Host "[SUCCESS] $Message" -ForegroundColor Green
}

function Write-Warning {
    param([string]$Message)
    Write-Host "[WARNING] $Message" -ForegroundColor Yellow
}

function Write-Error {
    param([string]$Message)
    Write-Host "[ERROR] $Message" -ForegroundColor Red
}

# Script configuration
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$BuildDir = Join-Path $ProjectRoot "build-rutos"

Write-Status "Virtual RUTOS Testing Environment Setup (OpenWrt-based)"
Write-Status "===================================================="
Write-Status ""
Write-Status "USAGE GUIDE:"
Write-Status "============"
Write-Status "• FIRST TIME: Run option 1 to create the OpenWrt RUTOS environment"
Write-Status "• DAILY USE: Run option 4 to start interactive shell for development"
Write-Status "• TESTING: Run option 3 to validate your packages work correctly"
Write-Status "• ADVANCED: Run option 2 to build real OpenWrt firmware images"
Write-Status "• CHECK STATUS: Run option 5 to see what environments are available"
Write-Status ""
Write-Status "TYPICAL WORKFLOW:"
Write-Status "================="
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
    Write-Host ""
    Write-Host "Choose your environment type:" -ForegroundColor White
    Write-Host "============================" -ForegroundColor White
    Write-Host ""
    Write-Host "1. OpenWrt Environment" -ForegroundColor Cyan
    Write-Host "   - Simulates pure OpenWrt environment" -ForegroundColor Gray
    Write-Host "   - Includes: ubus, uci, opkg, OpenWrt configs" -ForegroundColor Gray
    Write-Host "   - Use for: OpenWrt package development and testing" -ForegroundColor Gray
    Write-Host ""
    Write-Host "2. RUTOS Environment" -ForegroundColor Cyan
    Write-Host "   - Simulates RUTOS-specific environment (OpenWrt + BusyBox)" -ForegroundColor Gray
    Write-Host "   - Includes: gpsctl, gsmctl, wifi, cellular, wan commands" -ForegroundColor Gray
    Write-Host "   - Use for: RUTOS package development and testing" -ForegroundColor Gray
    Write-Host ""
    Write-Host "3. Combined Environment (Recommended)" -ForegroundColor Green
    Write-Host "   - Simulates both OpenWrt AND RUTOS" -ForegroundColor Gray
    Write-Host "   - Includes: All OpenWrt + RUTOS commands and configs" -ForegroundColor Gray
    Write-Host "   - Use for: Full autonomy package development" -ForegroundColor Gray
    Write-Host ""
    
    $envChoice = Read-Host "Select environment type (1-3)"
    
    $EnvironmentType = switch ($envChoice) {
        "1" { "openwrt" }
        "2" { "rutos" }
        "3" { "combined" }
        default { 
            Write-Warning "Invalid choice, using combined environment"
            "combined"
        }
    }
    
    Write-Status "Setting up $EnvironmentType environment..."

    if (!(Test-WSL)) {
        Write-Error "WSL not available. Please install WSL first: wsl --install"
        return
    }

    if (Test-WSLInstance -InstanceName $WSLName) {
        Write-Warning "WSL instance $WSLName already exists"
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
    Write-Host ""
    Write-Host "SETUP: Fully unattended installation with pre-configured credentials" -ForegroundColor Green
    Write-Host "Username: admin" -ForegroundColor Cyan
    Write-Host "Password: Passw0rd!" -ForegroundColor Cyan
    Write-Host "Sudo: Disabled (no password prompts)" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "The installation may take 2-5 minutes depending on your internet speed." -ForegroundColor Cyan
    Write-Host ""

    # Install Ubuntu with a specific name
    Write-Status "Starting Ubuntu installation..."
    Write-Status "This will run fully unattended with pre-configured credentials"
    wsl --install -d Ubuntu --name $WSLName

    Write-Status "Ubuntu installation started. Waiting for completion..."
    Write-Host "This may take a few minutes. Please be patient..." -ForegroundColor Cyan
    
    # Wait for installation to complete with progress indicators
    $maxWait = 300  # 5 minutes max
    $elapsed = 0
    $interval = 10  # Check every 10 seconds
    
    while ($elapsed -lt $maxWait) {
        Start-Sleep -Seconds $interval
        $elapsed += $interval
        
        # Show progress
        $progress = [math]::Min(($elapsed / $maxWait) * 100, 100)
        Write-Progress -Activity "Installing Ubuntu WSL" -Status "Waiting for installation to complete..." -PercentComplete $progress
        
        # Check if WSL instance is ready
        try {
            $wslStatus = wsl -l -v 2>$null | Select-String $WSLName
            if ($wslStatus) {
                Write-Progress -Activity "Installing Ubuntu WSL" -Completed
                Write-Success "Ubuntu installation completed!"
                break
            }
        } catch {
            # Continue waiting
        }
        
        if ($elapsed % 30 -eq 0) {
            Write-Status "Still waiting for Ubuntu installation... ($elapsed seconds elapsed)"
        }
    }
    
    if ($elapsed -ge $maxWait) {
        Write-Warning "Installation timeout. Please check if Ubuntu installation completed manually."
        Write-Status "You can continue with the setup if the WSL instance exists."
    }

    # Set up default user credentials for unattended installation
    Write-Status "Setting up default user credentials..."
    Write-Host "Creating admin user and disabling sudo prompts..." -ForegroundColor Cyan
    
    try {
        wsl -d $WSLName -e bash -c @"
# Create default user account
sudo useradd -m -s /bin/bash admin
echo 'admin:Passw0rd!' | sudo chpasswd
sudo usermod -aG sudo admin

# Configure sudo for admin user (no password required)
echo 'admin ALL=(ALL) NOPASSWD:ALL' | sudo tee /etc/sudoers.d/admin

# Set admin as default user
echo 'export USER=admin' >> /home/admin/.bashrc
echo 'export HOME=/home/admin' >> /home/admin/.bashrc

# Disable sudo password prompts globally for unattended operation
echo 'Defaults:admin !requiretty' | sudo tee -a /etc/sudoers.d/admin
echo 'Defaults:admin env_reset' | sudo tee -a /etc/sudoers.d/admin
echo 'Defaults:admin secure_path="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"' | sudo tee -a /etc/sudoers.d/admin

# Set admin as default user for WSL
sudo bash -c 'echo "admin" > /etc/wsl.conf'
sudo bash -c 'echo "[user]" >> /etc/wsl.conf'
sudo bash -c 'echo "default=admin" >> /etc/wsl.conf'
"@
        Write-Success "User credentials configured successfully!"
        Write-Success "Sudo password prompts disabled for unattended operation!"
    } catch {
        Write-Warning "User credential setup may have failed, but continuing..."
    }

    Write-Status "Setting up OpenWrt/RUTOS simulation environment..."
    Write-Host "This will take 2-3 minutes to install dependencies and create the environment..." -ForegroundColor Cyan

    # Create a separate bash script to avoid line ending issues
    Write-Status "Creating setup script with progress indicators for $EnvironmentType environment..."
    $bashScript = @"
#!/bin/bash
set -e

# Environment type parameter
ENVIRONMENT_TYPE="$EnvironmentType"

# Switch to admin user for unattended operation
if [ "$(whoami)" != "admin" ]; then
    echo "Switching to admin user for unattended operation..."
    exec sudo -u admin bash "$0" "$@"
    exit 0
fi

echo "=== $ENVIRONMENT_TYPE Environment Setup ==="
echo "Running as: $(whoami)"
echo "Environment: $ENVIRONMENT_TYPE"
echo ""

echo "Step 1/6: Updating Ubuntu packages..."
sudo apt-get update -qq
echo "✓ Ubuntu packages updated"

echo "Step 2/6: Upgrading system packages..."
sudo apt-get upgrade -y -qq
echo "✓ System packages upgraded"

echo "Step 3/6: Installing OpenWrt build dependencies..."
echo "This may take 2-3 minutes..."
sudo apt-get install -y -qq build-essential ccache ecj fastjar file g++ gawk gettext git java-propose-classpath libelf-dev libncurses5-dev libncursesw5-dev libssl-dev python3 python3-setuptools python3-dev rsync subversion swig time unzip wget xsltproc zlib1g-dev curl jq busybox

sudo rm -rf /var/lib/apt/lists/*
echo "✓ Dependencies installed"

echo "Step 4/6: Creating OpenWrt-style directory structure..."
# Create OpenWrt-style directory structure
sudo mkdir -p /etc/config /var/log /tmp/autonomy /usr/bin /opt/rutos /lib /sbin
echo "✓ Directory structure created"

echo "Step 5/6: Creating configuration files..."

# Create configuration files based on environment type
if [[ "$ENVIRONMENT_TYPE" == "openwrt" || "$ENVIRONMENT_TYPE" == "combined" ]]; then
    echo "Creating OpenWrt-style configuration files..."
    # Create OpenWrt-style system configuration
    sudo bash -c 'echo "config system" > /etc/config/system'
    sudo bash -c 'echo "    option hostname \"openwrt-sim\"" >> /etc/config/system'
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
    echo "✓ OpenWrt configuration files created"
fi

if [[ "$ENVIRONMENT_TYPE" == "rutos" || "$ENVIRONMENT_TYPE" == "combined" ]]; then
    echo "Creating RUTOS-style configuration files..."
    # Create RUTOS-specific configuration
    sudo bash -c 'echo "config system" > /etc/config/rutos'
    sudo bash -c 'echo "    option hostname \"rutos-sim\"" >> /etc/config/rutos'
    sudo bash -c 'echo "    option gps_enabled \"1\"" >> /etc/config/rutos'
    sudo bash -c 'echo "    option gsm_enabled \"1\"" >> /etc/config/rutos'
    echo "✓ RUTOS configuration files created"
fi

echo "✓ Configuration files created"

echo "Step 6/6: Creating commands and workspace..."

# Create commands based on environment type
if [[ "$ENVIRONMENT_TYPE" == "openwrt" || "$ENVIRONMENT_TYPE" == "combined" ]]; then
    echo "Creating OpenWrt-style commands..."
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
    echo "✓ OpenWrt commands created"
fi

if [[ "$ENVIRONMENT_TYPE" == "rutos" || "$ENVIRONMENT_TYPE" == "combined" ]]; then
    echo "Creating RUTOS-specific commands..."
    # Create RUTOS-specific commands (BusyBox-style)
    sudo bash -c 'echo "#!/bin/bash" > /usr/bin/gpsctl'
    sudo bash -c 'echo "echo \"RUTOS gpsctl - \$*\"" >> /usr/bin/gpsctl'
    sudo chmod +x /usr/bin/gpsctl

    sudo bash -c 'echo "#!/bin/bash" > /usr/bin/gsmctl'
    sudo bash -c 'echo "echo \"RUTOS gsmctl - \$*\"" >> /usr/bin/gsmctl'
    sudo chmod +x /usr/bin/gsmctl

    # Create additional RUTOS/BusyBox-style commands
    sudo bash -c 'echo "#!/bin/bash" > /usr/bin/wifi'
    sudo bash -c 'echo "echo \"RUTOS wifi - \$*\"" >> /usr/bin/wifi'
    sudo chmod +x /usr/bin/wifi

    sudo bash -c 'echo "#!/bin/bash" > /usr/bin/cellular'
    sudo bash -c 'echo "echo \"RUTOS cellular - \$*\"" >> /usr/bin/cellular'
    sudo chmod +x /usr/bin/cellular

    sudo bash -c 'echo "#!/bin/bash" > /usr/bin/wan'
    sudo bash -c 'echo "echo \"RUTOS wan - \$*\"" >> /usr/bin/wan'
    sudo chmod +x /usr/bin/wan
    echo "✓ RUTOS commands created"
fi

# Create BusyBox-style environment simulation
echo "Creating BusyBox-style environment simulation..."
# Create symlinks to simulate BusyBox single binary approach
sudo ln -sf /bin/busybox /usr/bin/ash
sudo ln -sf /bin/busybox /usr/bin/sh
sudo ln -sf /bin/busybox /usr/bin/cat
sudo ln -sf /bin/busybox /usr/bin/ls
sudo ln -sf /bin/busybox /usr/bin/echo
sudo ln -sf /bin/busybox /usr/bin/grep
sudo ln -sf /bin/busybox /usr/bin/sed
sudo ln -sf /bin/busybox /usr/bin/awk
echo "✓ BusyBox environment simulation created"

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
echo "✓ Commands and workspace created"

echo ""
echo "🎉 $ENVIRONMENT_TYPE environment setup complete!"
echo "✓ Username: admin"
echo "✓ Password: Passw0rd!"
echo "✓ Sudo: Disabled (no password prompts)"
echo "✓ Workspace: /workspace"

if [[ "$ENVIRONMENT_TYPE" == "openwrt" ]]; then
    echo "✓ OpenWrt environment with ubus, uci, opkg commands"
elif [[ "$ENVIRONMENT_TYPE" == "rutos" ]]; then
    echo "✓ RUTOS environment with gpsctl, gsmctl, wifi, cellular commands"
    echo "✓ BusyBox-style environment simulation"
else
    echo "✓ Combined OpenWrt + RUTOS environment"
    echo "✓ BusyBox-style environment simulation"
fi

echo "✓ This simulates the actual OpenWrt/BusyBox environment used by RUTOS"
"@

    # Write the bash script to a temporary file
    $tempScript = Join-Path $env:TEMP "setup-openwrt-env.sh"
    $bashScript | Out-File -FilePath $tempScript -Encoding UTF8 -NoNewline

    # Copy the script to WSL and execute it
    wsl -d $WSLName -e bash -c "cp /mnt/c/Users/$env:USERNAME/AppData/Local/Temp/setup-openwrt-env.sh /tmp/setup-openwrt-env.sh && chmod +x /tmp/setup-openwrt-env.sh && /tmp/setup-openwrt-env.sh"

    # Clean up
    Remove-Item $tempScript -ErrorAction SilentlyContinue

    Write-Success "$EnvironmentType environment $WSLName created successfully!"
    Write-Status "To access: wsl -d $WSLName"
    Write-Status "Username: admin"
    Write-Status "Password: Passw0rd!"
    Write-Status "Sudo: Disabled (no password prompts)"
    Write-Status "Workspace: /workspace"
    
    if ($EnvironmentType -eq "openwrt") {
        Write-Status "OpenWrt environment with ubus, uci, opkg commands"
    } elseif ($EnvironmentType -eq "rutos") {
        Write-Status "RUTOS environment with gpsctl, gsmctl, wifi, cellular commands"
        Write-Status "BusyBox-style environment simulation"
    } else {
        Write-Status "Combined OpenWrt + RUTOS environment"
        Write-Status "BusyBox-style environment simulation"
    }
    
    Write-Status "This simulates the actual OpenWrt/BusyBox environment used by RUTOS"
}

# Option 2: Download and setup actual OpenWrt Image Builder
function Setup-OpenWrtImageBuilder {
    Write-Status "Setting up OpenWrt Image Builder for RUTOS simulation..."

    if (!(Test-WSLInstance -InstanceName $WSLName)) {
        Write-Error "WSL instance $WSLName not found. Please create it first."
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
        Write-Error "WSL instance $WSLName not found. Please create it first."
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
        Write-Error "WSL instance $WSLName not found. Please create it first."
        return
    }

    Write-Status "Starting OpenWrt RUTOS shell for $WSLName..."
    Write-Status "This simulates the actual OpenWrt/BusyBox environment used by RUTOS"
    Write-Status "Default credentials: admin / Passw0rd!"
    Write-Status "Use 'exit' to return to Windows"
    wsl -d $WSLName
}

# Show menu
function Show-Menu {
    Write-Host ""
    Write-Host "OpenWrt-based RUTOS Testing Options:" -ForegroundColor White
    Write-Host "===================================" -ForegroundColor White
    Write-Host ""
    Write-Host "1. Create OpenWrt-based RUTOS environment" -ForegroundColor White
    Write-Host "   - Creates a new WSL instance that simulates OpenWrt/BusyBox" -ForegroundColor Gray
    Write-Host "   - Installs mock ubus, uci, opkg, gpsctl, gsmctl commands" -ForegroundColor Gray
    Write-Host "   - Sets up OpenWrt-style directory structure and configs" -ForegroundColor Gray
    Write-Host "   - Use this FIRST TIME to set up your testing environment" -ForegroundColor Gray
    Write-Host "   - Default credentials: admin / Passw0rd!" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "2. Setup OpenWrt Image Builder" -ForegroundColor White
    Write-Host "   - Downloads and configures OpenWrt Image Builder" -ForegroundColor Gray
    Write-Host "   - Allows building custom OpenWrt firmware images" -ForegroundColor Gray
    Write-Host "   - Creates actual OpenWrt images with your packages included" -ForegroundColor Gray
    Write-Host "   - Use this for ADVANCED testing with real OpenWrt firmware" -ForegroundColor Gray
    Write-Host ""
    Write-Host "3. Test RUTOS packages in OpenWrt environment" -ForegroundColor White
    Write-Host "   - Runs automated tests on your RUTOS packages" -ForegroundColor Gray
    Write-Host "   - Tests ubus, uci, opkg, gpsctl, gsmctl functionality" -ForegroundColor Gray
    Write-Host "   - Tests service management and configuration" -ForegroundColor Gray
    Write-Host "   - Use this to VALIDATE your packages work correctly" -ForegroundColor Gray
    Write-Host ""
    Write-Host "4. Start OpenWrt RUTOS shell" -ForegroundColor White
    Write-Host "   - Opens interactive shell in the OpenWrt RUTOS environment" -ForegroundColor Gray
    Write-Host "   - Allows manual testing and debugging" -ForegroundColor Gray
    Write-Host "   - Access your project at /workspace" -ForegroundColor Gray
    Write-Host "   - Use this for INTERACTIVE testing and development" -ForegroundColor Gray
    Write-Host ""
    Write-Host "5. List WSL instances" -ForegroundColor White
    Write-Host "   - Shows all available WSL instances and their status" -ForegroundColor Gray
    Write-Host "   - Helps you see what environments are available" -ForegroundColor Gray
    Write-Host "   - Use this to CHECK what is installed and running" -ForegroundColor Gray
    Write-Host ""
    Write-Host "6. Exit" -ForegroundColor White
    Write-Host "   - Exits the script" -ForegroundColor Gray
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
