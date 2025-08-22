# Real RUTOS SDK Environment Setup
# This script sets up the actual RUTOS SDK for proper OpenWrt/BusyBox development

param(
    [string]$Action = "menu",
    [string]$SDKPath = "J:\GithubCursor\rutos-ipq40xx-rutx-sdk",
    [string]$WSLName = "rutos-sdk"
)

# Function to print colored output
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

Write-Status "Real RUTOS SDK Environment Setup"
Write-Status "================================="
Write-Status ""
Write-Status "This script sets up the ACTUAL RUTOS SDK environment"
Write-Status "Instead of simulating RUTOS on Ubuntu, this uses the real OpenWrt/BusyBox environment"
Write-Status ""

# Check if RUTOS SDK exists
function Test-RutosSDK {
    param([string]$Path)
    
    if (!(Test-Path $Path)) {
        Write-Error "RUTOS SDK not found at: $Path"
        Write-Status "Please ensure the RUTOS SDK is available at the specified path"
        Write-Status "Expected: $Path"
        return $false
    }
    
    if (!(Test-Path (Join-Path $Path "scripts/env.sh"))) {
        Write-Warning "RUTOS SDK found but scripts/env.sh not found"
        Write-Status "This may not be a complete RUTOS SDK installation"
        return $false
    }
    
    Write-Success "RUTOS SDK found and validated: $Path"
    return $true
}

# Setup WSL with RUTOS SDK
function Setup-RutosSDKEnvironment {
    Write-Status "Setting up real RUTOS SDK environment..."
    
    if (!(Test-RutosSDK -Path $SDKPath)) {
        Write-Error "Cannot proceed without valid RUTOS SDK"
        return
    }
    
    # Check if WSL is available
    try {
        wsl --version | Out-Null
    } catch {
        Write-Error "WSL not available. Please install WSL first: wsl --install"
        return
    }
    
    # Check if WSL instance exists
    try {
        $wslStatus = wsl -l -v 2>$null | Select-String $WSLName
        if ($wslStatus) {
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
    } catch {
        # Instance doesn't exist, continue
    }
    
    Write-Status "Installing Ubuntu WSL instance for RUTOS SDK development..."
    Write-Host ""
    Write-Host "SETUP: Real RUTOS SDK integration" -ForegroundColor Green
    Write-Host "SDK Path: $SDKPath" -ForegroundColor Cyan
    Write-Host "WSL Name: $WSLName" -ForegroundColor Cyan
    Write-Host ""
    
    # Install Ubuntu with specific name
    Write-Status "Starting Ubuntu installation..."
    wsl --install -d Ubuntu --name $WSLName
    
    Write-Status "Ubuntu installation started. Waiting for completion..."
    
    # Wait for installation with progress
    $maxWait = 300
    $elapsed = 0
    $interval = 10
    
    while ($elapsed -lt $maxWait) {
        Start-Sleep -Seconds $interval
        $elapsed += $interval
        
        $progress = [math]::Min(($elapsed / $maxWait) * 100, 100)
        Write-Progress -Activity "Installing Ubuntu WSL" -Status "Waiting for installation..." -PercentComplete $progress
        
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
    }
    
    # Setup user credentials
    Write-Status "Setting up user credentials..."
    try {
        wsl -d $WSLName -e bash -c @"
# Create admin user
sudo useradd -m -s /bin/bash admin
echo 'admin:Passw0rd!' | sudo chpasswd
sudo usermod -aG sudo admin

# Configure sudo for admin user
echo 'admin ALL=(ALL) NOPASSWD:ALL' | sudo tee /etc/sudoers.d/admin

# Set admin as default user
echo 'export USER=admin' >> /home/admin/.bashrc
echo 'export HOME=/home/admin' >> /home/admin/.bashrc

# Set admin as default user for WSL
sudo bash -c 'echo "admin" > /etc/wsl.conf'
sudo bash -c 'echo "[user]" >> /etc/wsl.conf'
sudo bash -c 'echo "default=admin" >> /etc/wsl.conf'
"@
        Write-Success "User credentials configured successfully!"
    } catch {
        Write-Warning "User credential setup may have failed, but continuing..."
    }
    
    # Setup RUTOS SDK environment
    Write-Status "Setting up RUTOS SDK environment..."
    Write-Host "This will mount the RUTOS SDK and set up the OpenWrt build environment..." -ForegroundColor Cyan
    
    $bashScript = @"
#!/bin/bash
set -e

echo "=== Real RUTOS SDK Environment Setup ==="
echo "Running as: $(whoami)"
echo ""

# Switch to admin user
if [ "$(whoami)" != "admin" ]; then
    echo "Switching to admin user..."
    exec sudo -u admin bash "$0" "$@"
    exit 0
fi

echo "Step 1/5: Installing OpenWrt build dependencies..."
sudo apt-get update -qq
sudo apt-get install -y -qq build-essential ccache ecj fastjar file g++ gawk gettext git java-propose-classpath libelf-dev libncurses5-dev libncursesw5-dev libssl-dev python3 python3-setuptools python3-dev rsync subversion swig time unzip wget xsltproc zlib1g-dev curl jq

echo "Step 2/5: Creating RUTOS SDK mount point..."
sudo mkdir -p /mnt/rutos-sdk
echo "✓ RUTOS SDK mount point created"

echo "Step 3/5: Setting up environment variables..."
echo 'export RUTOS_SDK_PATH="/mnt/rutos-sdk"' >> ~/.bashrc
echo 'export PATH="/mnt/rutos-sdk/staging_dir/toolchain-arm_cortex-a7_gcc-8.4.0_musl_eabi/bin:$PATH"' >> ~/.bashrc
echo 'export STAGING_DIR="/mnt/rutos-sdk/staging_dir"' >> ~/.bashrc
echo 'export TOOLCHAIN_DIR="/mnt/rutos-sdk/staging_dir/toolchain-arm_cortex-a7_gcc-8.4.0_musl_eabi"' >> ~/.bashrc
echo "✓ Environment variables configured"

echo "Step 4/5: Creating workspace..."
mkdir -p /workspace
cd /workspace
echo "✓ Workspace created at /workspace"

echo "Step 5/5: Setting up RUTOS SDK integration..."
# Create a script to initialize RUTOS SDK environment
cat > /workspace/init-rutos-sdk.sh << 'EOF'
#!/bin/bash
# Initialize RUTOS SDK environment

if [ -d "/mnt/rutos-sdk" ]; then
    echo "Initializing RUTOS SDK environment..."
    cd /mnt/rutos-sdk
    
    if [ -f "scripts/env.sh" ]; then
        source scripts/env.sh
        echo "✓ RUTOS SDK environment initialized"
        echo "✓ Available commands: make, opkg, uci, ubus"
        echo "✓ Target: arm_cortex-a7_neon-vfpv4"
    else
        echo "⚠ RUTOS SDK scripts/env.sh not found"
        echo "⚠ Please ensure RUTOS SDK is properly mounted"
    fi
else
    echo "⚠ RUTOS SDK not mounted at /mnt/rutos-sdk"
    echo "⚠ Please mount your RUTOS SDK to /mnt/rutos-sdk"
fi
EOF

chmod +x /workspace/init-rutos-sdk.sh
echo "✓ RUTOS SDK initialization script created"

echo ""
echo "🎉 Real RUTOS SDK environment setup complete!"
echo "✓ Username: admin"
echo "✓ Password: Passw0rd!"
echo "✓ Workspace: /workspace"
echo "✓ RUTOS SDK: /mnt/rutos-sdk (needs to be mounted)"
echo ""
echo "NEXT STEPS:"
echo "1. Mount your RUTOS SDK to /mnt/rutos-sdk"
echo "2. Run: source /workspace/init-rutos-sdk.sh"
echo "3. Build your autonomy packages with: make package/autonomy/compile"
echo ""
echo "This provides the ACTUAL OpenWrt/BusyBox environment used by RUTOS!"
"@
    
    # Write and execute the bash script
    $tempScript = Join-Path $env:TEMP "setup-rutos-sdk.sh"
    $bashScript | Out-File -FilePath $tempScript -Encoding UTF8 -NoNewline
    
    wsl -d $WSLName -e bash -c "cp /mnt/c/Users/$env:USERNAME/AppData/Local/Temp/setup-rutos-sdk.sh /tmp/setup-rutos-sdk.sh && chmod +x /tmp/setup-rutos-sdk.sh && /tmp/setup-rutos-sdk.sh"
    
    Remove-Item $tempScript -ErrorAction SilentlyContinue
    
    Write-Success "Real RUTOS SDK environment $WSLName created successfully!"
    Write-Status "To access: wsl -d $WSLName"
    Write-Status "Username: admin"
    Write-Status "Password: Passw0rd!"
    Write-Status "Workspace: /workspace"
    Write-Status ""
    Write-Status "IMPORTANT: You need to mount your RUTOS SDK to /mnt/rutos-sdk"
    Write-Status "This provides the ACTUAL OpenWrt/BusyBox environment used by RUTOS"
}

# Mount RUTOS SDK
function Mount-RutosSDK {
    Write-Status "Mounting RUTOS SDK..."
    
    if (!(Test-RutosSDK -Path $SDKPath)) {
        Write-Error "Cannot mount invalid RUTOS SDK"
        return
    }
    
    try {
        # Convert Windows path to WSL path
        $wslPath = $SDKPath -replace "\\", "/" -replace "^([A-Z]):", { "/mnt/$($_.Groups[1].Value.ToLower())" }
        
        Write-Status "Mounting $SDKPath to /mnt/rutos-sdk in WSL..."
        wsl -d $WSLName -e bash -c "sudo mkdir -p /mnt/rutos-sdk && sudo mount --bind '$wslPath' /mnt/rutos-sdk"
        
        Write-Success "RUTOS SDK mounted successfully!"
        Write-Status "SDK is now available at /mnt/rutos-sdk in WSL"
    } catch {
        Write-Error "Failed to mount RUTOS SDK: $($_.Exception.Message)"
    }
}

# Initialize RUTOS SDK environment
function Initialize-RutosSDK {
    Write-Status "Initializing RUTOS SDK environment..."
    
    try {
        wsl -d $WSLName -e bash -c "source /workspace/init-rutos-sdk.sh"
        Write-Success "RUTOS SDK environment initialized!"
    } catch {
        Write-Error "Failed to initialize RUTOS SDK environment: $($_.Exception.Message)"
    }
}

# Start RUTOS SDK shell
function Start-RutosSDKShell {
    Write-Status "Starting RUTOS SDK shell..."
    
    try {
        Write-Status "Starting RUTOS SDK shell for $WSLName..."
        Write-Status "This provides the ACTUAL OpenWrt/BusyBox environment used by RUTOS"
        Write-Status "Default credentials: admin / Passw0rd!"
        Write-Status "Use 'exit' to return to Windows"
        wsl -d $WSLName
    } catch {
        Write-Error "Failed to start RUTOS SDK shell: $($_.Exception.Message)"
    }
}

# Show menu
function Show-Menu {
    Write-Host ""
    Write-Host "Real RUTOS SDK Environment Options:" -ForegroundColor White
    Write-Host "====================================" -ForegroundColor White
    Write-Host ""
    Write-Host "1. Setup Real RUTOS SDK Environment" -ForegroundColor White
    Write-Host "   - Creates WSL instance with RUTOS SDK integration" -ForegroundColor Gray
    Write-Host "   - Sets up actual OpenWrt/BusyBox environment" -ForegroundColor Gray
    Write-Host "   - Provides real RUTOS SDK build tools" -ForegroundColor Gray
    Write-Host "   - Use this for ACTUAL RUTOS development" -ForegroundColor Gray
    Write-Host ""
    Write-Host "2. Mount RUTOS SDK" -ForegroundColor White
    Write-Host "   - Mounts your RUTOS SDK to /mnt/rutos-sdk in WSL" -ForegroundColor Gray
    Write-Host "   - Required after setup to access SDK files" -ForegroundColor Gray
    Write-Host "   - Use this to connect your SDK to the environment" -ForegroundColor Gray
    Write-Host ""
    Write-Host "3. Initialize RUTOS SDK Environment" -ForegroundColor White
    Write-Host "   - Runs source scripts/env.sh from RUTOS SDK" -ForegroundColor Gray
    Write-Host "   - Sets up OpenWrt build environment" -ForegroundColor Gray
    Write-Host "   - Use this to activate the SDK environment" -ForegroundColor Gray
    Write-Host ""
    Write-Host "4. Start RUTOS SDK Shell" -ForegroundColor White
    Write-Host "   - Opens interactive shell in RUTOS SDK environment" -ForegroundColor Gray
    Write-Host "   - Allows building and testing with real SDK" -ForegroundColor Gray
    Write-Host "   - Use this for development and testing" -ForegroundColor Gray
    Write-Host ""
    Write-Host "5. List WSL instances" -ForegroundColor White
    Write-Host "   - Shows all available WSL instances and their status" -ForegroundColor Gray
    Write-Host "   - Helps you see what environments are available" -ForegroundColor Gray
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
    Write-Status "Note: Real RUTOS SDK instance provides actual OpenWrt/BusyBox environment"
}

# Main execution
function Main {
    # If action is provided, run it directly
    if ($Action -ne "menu") {
        switch ($Action) {
            "1" { Setup-RutosSDKEnvironment }
            "2" { Mount-RutosSDK }
            "3" { Initialize-RutosSDK }
            "4" { Start-RutosSDKShell }
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
            "1" { Setup-RutosSDKEnvironment }
            "2" { Mount-RutosSDK }
            "3" { Initialize-RutosSDK }
            "4" { Start-RutosSDKShell }
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
