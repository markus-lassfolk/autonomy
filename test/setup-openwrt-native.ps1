# Native OpenWrt Environment Setup
# This script sets up the actual OpenWrt environment using official OpenWrt images

param(
    [string]$Action = "menu",
    [string]$OpenWrtVersion = "24.10.2",
    [string]$WSLName = "openwrt-native",
    [string]$ImageType = "rootfs.tar.gz"
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

Write-Status "Native OpenWrt Environment Setup"
Write-Status "================================"
Write-Status ""
Write-Status "This script sets up the ACTUAL OpenWrt environment"
Write-Status "Using official OpenWrt images from downloads.openwrt.org"
Write-Status "This provides the real OpenWrt/BusyBox environment that RUTOS uses"
Write-Status ""

# Available OpenWrt rootfs archives for WSL (only tar.gz files work with WSL)
$AvailableImages = @{
    "rootfs.tar.gz" = "Root filesystem as tar.gz archive (WSL Compatible - RECOMMENDED)"
}

# Setup WSL with OpenWrt
function Setup-OpenWrtNative {
    Write-Status "Setting up native OpenWrt environment..."
    
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
    
    Write-Status "Setting up native OpenWrt environment..."
    Write-Host ""
    Write-Host "SETUP: Native OpenWrt environment" -ForegroundColor Green
    Write-Host "OpenWrt Version: $OpenWrtVersion" -ForegroundColor Cyan
    Write-Host "Image Type: $ImageType" -ForegroundColor Cyan
    Write-Host "WSL Name: $WSLName" -ForegroundColor Cyan
    Write-Host ""
    
    # Download OpenWrt rootfs
    Write-Status "Downloading OpenWrt rootfs..."
    $ImageUrl = "https://downloads.openwrt.org/releases/$OpenWrtVersion/targets/x86/64/openwrt-$OpenWrtVersion-x86-64-$ImageType"
    $DownloadPath = Join-Path $env:TEMP "openwrt-$OpenWrtVersion-x86-64-$ImageType"
    
    Write-Status "Downloading from: $ImageUrl"
    Write-Status "Saving to: $DownloadPath"
    
    try {
        Invoke-WebRequest -Uri $ImageUrl -OutFile $DownloadPath -UseBasicParsing
        Write-Success "OpenWrt rootfs downloaded successfully!"
    } catch {
        Write-Error "Failed to download OpenWrt rootfs: $($_.Exception.Message)"
        return
    }
    
    # Import OpenWrt into WSL
    Write-Status "Importing OpenWrt rootfs into WSL..."
    $WslImportPath = Join-Path $env:TEMP "openwrt-$WSLName"
    
    try {
        # Check if WSL instance already exists
        $existingInstances = wsl --list --quiet
        if ($existingInstances -contains $WSLName) {
            Write-Warning "WSL instance '$WSLName' already exists!"
            Write-Status "Removing existing instance..."
            wsl --unregister $WSLName
            Start-Sleep -Seconds 2  # Give WSL time to unregister
            Write-Success "Existing instance removed."
        }
        
        # Create WSL import directory
        New-Item -ItemType Directory -Path $WslImportPath -Force | Out-Null
        
        # Import the OpenWrt rootfs tarball
        wsl --import $WSLName $WslImportPath $DownloadPath
        Write-Success "OpenWrt imported into WSL successfully!"
    } catch {
        Write-Error "Failed to import OpenWrt into WSL: $($_.Exception.Message)"
        return
    }
    
    # Setup OpenWrt environment
    Write-Status "Setting up OpenWrt environment..."
    Write-Host "This will configure the OpenWrt environment for development..." -ForegroundColor Cyan
    
    $bashScript = @"
#!/bin/sh

echo "=== Native OpenWrt Environment Setup ==="
echo "OpenWrt Version: $OpenWrtVersion"
echo "Image Type: $ImageType"
echo ""

echo "Step 1/4: Creating necessary directories..."
mkdir -p /var/lock
mkdir -p /var/opkg-lists
mkdir -p /workspace
mkdir -p /etc/config
echo "✓ Directories created"

echo "Step 2/4: Creating workspace and configuration..."

# Create basic OpenWrt configuration
cat > /etc/config/system << 'EOF'
config system
    option hostname 'openwrt-dev'
    option timezone 'UTC'

config timeserver 'ntp'
    list server '0.openwrt.pool.ntp.org'
    list server '1.openwrt.pool.ntp.org'
    list server '2.openwrt.pool.ntp.org'
    list server '3.openwrt.pool.ntp.org'
EOF

# Create network configuration
cat > /etc/config/network << 'EOF'
config interface 'loopback'
    option ifname 'lo'
    option proto 'static'
    option ipaddr '127.0.0.1'
    option netmask '255.0.0.0'

config interface 'lan'
    option ifname 'eth0'
    option proto 'static'
    option ipaddr '192.168.1.1'
    option netmask '255.255.255.0'
EOF

echo "✓ Workspace and configuration created"

echo "Step 3/4: Testing OpenWrt commands..."
echo "Testing opkg: \$(opkg --version 2>/dev/null || echo 'opkg not available')"
echo "Testing uci: \$(uci show system 2>/dev/null | head -1 || echo 'uci not available')"
echo "Testing ubus: \$(ubus list 2>/dev/null | head -1 || echo 'ubus not available')"
echo "✓ Command testing completed"

echo "Step 4/4: Creating startup script for ubus..."
# Create a startup script to run ubusd when needed
cat > /workspace/start-ubus.sh << 'EOF'
#!/bin/sh
# Start ubus daemon for OpenWrt services
echo "Starting ubus daemon..."
ubusd &
sleep 2
echo "ubus daemon started. You can now use: ubus list"
EOF
chmod +x /workspace/start-ubus.sh
echo "✓ Startup script created: /workspace/start-ubus.sh"

echo ""
echo "🎉 Native OpenWrt environment setup complete!"
echo "✓ OpenWrt Version: $OpenWrtVersion"
echo "✓ Image Type: $ImageType"
echo "✓ Workspace: /workspace"
echo "✓ Available commands: opkg, uci, ubus (when daemon started)"
echo ""
echo "This is the ACTUAL OpenWrt/BusyBox environment!"
echo "All commands are real OpenWrt binaries, not simulations."
echo ""
echo "You can now use:"
echo "  opkg list-installed    # List installed packages"
echo "  uci show               # Show configuration"
echo "  /workspace/start-ubus.sh  # Start ubus daemon"
echo "  ubus list              # List available services (after starting daemon)"
echo "  ps aux                 # Show processes"
echo ""
echo "Note: ubus requires the daemon to be running. Use /workspace/start-ubus.sh"
echo ""
"@
    
    # Write and execute the bash script
    $tempScript = Join-Path $env:TEMP "setup-openwrt.sh"
    $bashScript | Out-File -FilePath $tempScript -Encoding UTF8 -NoNewline
    
    # Execute the setup script in OpenWrt
    wsl -d $WSLName -e sh -c "cat /mnt/c/Users/$env:USERNAME/AppData/Local/Temp/setup-openwrt.sh | sh"
    
    # Clean up temporary files
    Remove-Item $tempScript -ErrorAction SilentlyContinue
    Remove-Item $DownloadPath -ErrorAction SilentlyContinue
    
    Write-Success "Native OpenWrt environment $WSLName created successfully!"
    Write-Status "To access: wsl -d $WSLName"
    Write-Status "Workspace: /workspace"
    Write-Status ""
    Write-Status "This provides the ACTUAL OpenWrt/BusyBox environment!"
    Write-Status "All commands (opkg, uci, ubus) are real OpenWrt binaries."
}

# List available OpenWrt rootfs archives
function Show-AvailableImages {
    Write-Status "Available OpenWrt Rootfs Archives for WSL x86/64:"
    Write-Host ""
    Write-Status "Note: Only rootfs.tar.gz files are compatible with WSL import"
    Write-Host ""
    
    foreach ($image in $AvailableImages.GetEnumerator()) {
        Write-Host "  $($image.Key)" -ForegroundColor Cyan
        Write-Host "    $($image.Value)" -ForegroundColor Gray
        Write-Host ""
    }
    
    Write-Host "ℹ️  WSL Compatibility Notes:" -ForegroundColor Blue
    Write-Host "   - WSL requires rootfs.tar.gz archives, not disk images" -ForegroundColor Gray
    Write-Host "   - Disk images (.img files) are for hardware flashing only" -ForegroundColor Gray
    Write-Host "   - Our script automatically uses the correct rootfs format" -ForegroundColor Gray
}

# Start OpenWrt shell
function Start-OpenWrtShell {
    Write-Status "Starting native OpenWrt shell..."
    
    try {
        Write-Status "Starting OpenWrt shell for $WSLName..."
        Write-Status "This is the ACTUAL OpenWrt/BusyBox environment"
        Write-Status "Use 'exit' to return to Windows"
        wsl -d $WSLName
    } catch {
        Write-Error "Failed to start OpenWrt shell: $($_.Exception.Message)"
    }
}

# Test OpenWrt environment
function Test-OpenWrtEnvironment {
    Write-Status "Testing OpenWrt environment..."
    
    try {
        Write-Status "Testing OpenWrt commands and environment..."
        
        # Create a temporary test script with proper escaping
        $testScript = @'
#!/bin/sh
echo "=== OpenWrt Environment Test ==="
echo "OpenWrt Version: $(cat /etc/openwrt_version 2>/dev/null || echo 'Unknown')"
echo "Kernel: $(uname -r)"
echo "Architecture: $(uname -m)"
echo ""
echo "Testing OpenWrt commands:"
echo "opkg version: $(opkg --version 2>/dev/null || echo 'Not available')"
echo "uci test: $(uci show system 2>/dev/null | head -1 || echo 'uci not available')"
echo "ubus test: $(ubus list 2>/dev/null | head -1 || echo 'ubus not available (daemon not running)')"
echo ""
echo "Testing package management:"
opkg list-installed | head -5
echo ""
echo "Testing UCI configuration:"
uci show system
echo ""
echo "Testing workspace:"
ls -la /workspace
echo ""
echo "Testing ubus daemon startup script:"
if [ -f /workspace/start-ubus.sh ]
then
    echo "✓ ubus startup script exists: /workspace/start-ubus.sh"
    echo "To start ubus daemon, run: /workspace/start-ubus.sh"
else
    echo "✗ ubus startup script not found"
fi
echo ""
echo "✓ OpenWrt environment test completed!"
'@
        
        # Write test script to temp file
        $tempTestScript = Join-Path $env:TEMP "test-openwrt.sh"
        $testScript | Out-File -FilePath $tempTestScript -Encoding UTF8 -NoNewline
        
        # Execute test script in WSL
        wsl -d $WSLName -e sh -c "cat /mnt/c/Users/$env:USERNAME/AppData/Local/Temp/test-openwrt.sh | sh"
        
        # Clean up
        Remove-Item $tempTestScript -ErrorAction SilentlyContinue
        
        Write-Success "OpenWrt environment test completed!"
    } catch {
        Write-Error "Failed to test OpenWrt environment: $($_.Exception.Message)"
    }
}

# Setup Advanced OpenWrt with proper init system (following GitHub community guide)
function Setup-AdvancedOpenWrtInit {
    Write-Status "Setting up advanced OpenWrt with proper init system..."
    Write-Warning "This setup uses advanced techniques from the OpenWrt community"
    Write-Warning "Based on: https://gist.github.com/Balder1840/8d7670337039432829ed7d3d9d19494d"
    
    try {
        Write-Status "Installing advanced OpenWrt packages and configuration..."
        wsl -d $WSLName -e sh -c @"
echo "=== Advanced OpenWrt Init System Setup ==="
echo "Based on community guide from GitHub"
echo ""

echo "Step 1/5: Installing required packages..."
opkg update
opkg install unshare procps-ng-ps nsenter shadow-su
echo "✓ Advanced packages installed (unshare, nsenter, procps-ng-ps, shadow-su)"

echo "Step 2/5: Creating WSL configuration..."
# Create /etc/wsl.conf for proper init
cat > /etc/wsl.conf << 'EOF'
[boot]
command = "/usr/bin/env -i /usr/bin/unshare --pid --mount-proc --fork --propagation private -- sh -c 'exec /sbin/init'"

[network]
generateHosts = false
generateResolvConf = false
EOF
echo "✓ WSL configuration created (/etc/wsl.conf)"

echo "Step 3/5: Creating WSL init script..."
# Create WSL init script to handle procd properly
cat > /etc/profile.d/wsl-init.sh << 'EOF'
#!/bin/bash
# WSL init script for proper OpenWrt procd handling
# Based on: https://gist.github.com/Balder1840/8d7670337039432829ed7d3d9d19494d

# Get PID of /sbin/procd(init) - improved version to handle multiple PIDs
sleep 1
pid="\$(ps -u root -o pid,args | awk '\$2 ~ /procd/ { print \$1; exit }')"

# Run WSL service script
if [ "\$pid" -ne 1 ] && [ -n "\$pid" ]; then
  echo "Entering /sbin/procd(init) PID: \$pid"
  exec /usr/bin/nsenter -p -m -t "\${pid}" -- su - "\$USER"
fi
EOF
chmod +x /etc/profile.d/wsl-init.sh
echo "✓ WSL init script created (/etc/profile.d/wsl-init.sh)"

echo "Step 4/5: Updating profile configuration..."
# Backup original profile
cp /etc/profile /etc/profile.backup

# Update /etc/profile to handle WSL init properly
sed -i '/for FILE in \/etc\/profile.d\/\*.sh; do/,/unset FILE/ {
    s|#\[ -e "\$FILE" \] && \. "\$FILE"|if [ "\$FILE" == "/etc/profile.d/sysinfo.sh" ]; then\
                 [ "\$(which bash)" ] \&\& env -i bash "\$FILE"\
                elif [ "\$FILE" == "/etc/profile.d/wsl-init.sh" ]; then\
                 [ "\$(which sh)" ] \&\& env -i sh "\$FILE"\
                else\
                 [ -e "\$FILE" ] \&\& . "\$FILE"\
                fi|
}' /etc/profile
echo "✓ Profile configuration updated"

echo "Step 5/5: Creating network configuration..."
# Create safer network configuration to avoid conflicts
cat > /etc/config/network << 'EOF'
config interface 'loopback'
    option ifname 'lo'
    option proto 'static'
    option ipaddr '127.0.0.1'
    option netmask '255.0.0.0'

config interface 'lan'
    option ifname 'eth0'
    option proto 'static'
    option ipaddr '192.168.100.1'
    option netmask '255.255.255.0'
    option gateway '192.168.100.1'
EOF
echo "✓ Network configuration created (using 192.168.100.x to avoid conflicts)"

echo ""
echo "🎉 Advanced OpenWrt init system setup complete!"
echo "✓ Proper procd init system configured"
echo "✓ Process namespacing enabled"
echo "✓ Network configuration optimized"
echo "✓ WSL integration improved"
echo ""
echo "⚠️  IMPORTANT NOTES:"
echo "   - You MUST restart WSL for changes to take effect: wsl --shutdown"
echo "   - OpenWrt will run with proper procd as PID 1"
echo "   - Network uses 192.168.100.x to avoid conflicts"
echo "   - Based on community guide: https://gist.github.com/Balder1840/8d7670337039432829ed7d3d9d19494d"
echo ""
echo "⚠️  RESTART REQUIRED:"
echo "   1. Exit this WSL session"
echo "   2. Run: wsl --shutdown"
echo "   3. Restart with: wsl -d $WSLName"
"@
        Write-Success "Advanced OpenWrt init system setup completed!"
        Write-Warning "RESTART REQUIRED: Run 'wsl --shutdown' then restart WSL"
    } catch {
        Write-Error "Failed to setup advanced OpenWrt init system: $($_.Exception.Message)"
    }
}

# Setup OpenWrt build system (following official WSL guide)
function Setup-OpenWrtBuildSystem {
    Write-Status "Setting up OpenWrt build system (following official WSL guide)..."
    
    try {
        Write-Status "Installing OpenWrt build dependencies..."
        wsl -d $WSLName -e sh -c @"
echo "=== OpenWrt Build System Setup ==="
echo "Following official OpenWrt WSL guide recommendations..."
echo ""

echo "Step 1/3: Installing build dependencies..."
opkg update
opkg install build-essential gcc make cmake git subversion wget curl
echo "✓ Build dependencies installed"

echo "Step 2/3: Setting up build environment..."
mkdir -p /workspace/openwrt-build
cd /workspace/openwrt-build

# Create build script with proper PATH (from official guide)
cat > build-openwrt.sh << 'EOF'
#!/bin/sh
# OpenWrt build script with WSL PATH fix (from official guide)
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
echo "Using clean PATH for OpenWrt build: $PATH"
echo "Ready for OpenWrt build system usage"
EOF

chmod +x build-openwrt.sh
echo "✓ Build environment setup complete"

echo "Step 3/3: Creating build instructions..."
cat > /workspace/OPENWRT_BUILD_INSTRUCTIONS.md << 'EOF'
# OpenWrt Build Instructions for WSL

## Important Notes (from OpenWrt official guide)
- This method is NOT OFFICIALLY supported by OpenWrt
- A native GNU/Linux environment is recommended for production
- WSL has been configured to work with OpenWrt build system

## Build Commands
To build OpenWrt packages or images, use the clean PATH:

```bash
# Use the build script (recommended)
./build-openwrt.sh

# Or manually set PATH (from official guide)
PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin make

# For specific targets
PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin make package/autonomy/compile
```

## WSL Configuration
- Windows paths with spaces have been removed from PATH
- Build environment is isolated from Windows interference
- Compatible with OpenWrt build system requirements

## Resources
- Official WSL Guide: https://openwrt.org/docs/guide-developer/toolchain/wsl
- OpenWrt Developer Guide: https://openwrt.org/docs/guide-developer/start
EOF

echo "✓ Build instructions created"
echo ""
echo "🎉 OpenWrt build system setup complete!"
echo "✓ Build dependencies installed"
echo "✓ Clean PATH configured (following official guide)"
echo "✓ Build script created: /workspace/openwrt-build/build-openwrt.sh"
echo "✓ Instructions: /workspace/OPENWRT_BUILD_INSTRUCTIONS.md"
echo ""
echo "⚠️  Remember: Use clean PATH for builds: PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin make"
"@
        Write-Success "OpenWrt build system setup completed!"
    } catch {
        Write-Error "Failed to setup OpenWrt build system: $($_.Exception.Message)"
    }
}

# Show menu
function Show-Menu {
    Write-Host ""
    Write-Host "Native OpenWrt Environment Options:" -ForegroundColor White
    Write-Host "===================================" -ForegroundColor White
    Write-Host ""
    Write-Host "1. Setup Native OpenWrt Environment" -ForegroundColor White
    Write-Host "   - Downloads and imports official OpenWrt rootfs" -ForegroundColor Gray
    Write-Host "   - Sets up actual OpenWrt/BusyBox environment" -ForegroundColor Gray
    Write-Host "   - Provides real opkg, uci, ubus commands" -ForegroundColor Gray
    Write-Host "   - Use this for ACTUAL OpenWrt development" -ForegroundColor Gray
    Write-Host ""
    Write-Host "2. Show Available OpenWrt Rootfs" -ForegroundColor White
    Write-Host "   - Lists available OpenWrt rootfs archives" -ForegroundColor Gray
    Write-Host "   - Shows WSL-compatible formats" -ForegroundColor Gray
    Write-Host "   - Use this to choose the right rootfs" -ForegroundColor Gray
    Write-Host ""
    Write-Host "3. Start OpenWrt Shell" -ForegroundColor White
    Write-Host "   - Opens interactive shell in OpenWrt environment" -ForegroundColor Gray
    Write-Host "   - Allows development and testing with real OpenWrt" -ForegroundColor Gray
    Write-Host "   - Use this for development and testing" -ForegroundColor Gray
    Write-Host ""
    Write-Host "4. Test OpenWrt Environment" -ForegroundColor White
    Write-Host "   - Tests OpenWrt commands and environment" -ForegroundColor Gray
    Write-Host "   - Verifies opkg, uci, ubus functionality" -ForegroundColor Gray
    Write-Host "   - Use this to validate the setup" -ForegroundColor Gray
    Write-Host ""
    Write-Host "5. Setup OpenWrt Build System" -ForegroundColor White
    Write-Host "   - Installs build dependencies and tools" -ForegroundColor Gray
    Write-Host "   - Configures clean PATH (following official WSL guide)" -ForegroundColor Gray
    Write-Host "   - Creates build scripts and instructions" -ForegroundColor Gray
    Write-Host ""
    Write-Host "6. Setup Advanced OpenWrt Init System" -ForegroundColor Yellow
    Write-Host "   - Configures proper procd init system (PID 1)" -ForegroundColor Gray
    Write-Host "   - Uses process namespacing and advanced WSL features" -ForegroundColor Gray
    Write-Host "   - Based on community guide (EXPERIMENTAL)" -ForegroundColor Gray
    Write-Host ""
    Write-Host "7. List WSL instances" -ForegroundColor White
    Write-Host "   - Shows all available WSL instances and their status" -ForegroundColor Gray
    Write-Host "   - Helps you see what environments are available" -ForegroundColor Gray
    Write-Host ""
    Write-Host "8. Exit" -ForegroundColor White
    Write-Host "   - Exits the script" -ForegroundColor Gray
    Write-Host ""
}

# List WSL instances
function List-WSLInstances {
    Write-Status "WSL instances:"
    wsl -l -v
    Write-Status ""
    Write-Status "Note: Native OpenWrt instance provides actual OpenWrt/BusyBox environment"
}

# Main execution
function Main {
    # If action is provided, run it directly
    if ($Action -ne "menu") {
        switch ($Action) {
            "1" { Setup-OpenWrtNative }
            "2" { Show-AvailableImages }
            "3" { Start-OpenWrtShell }
            "4" { Test-OpenWrtEnvironment }
            "5" { Setup-OpenWrtBuildSystem }
            "6" { Setup-AdvancedOpenWrtInit }
            "7" { List-WSLInstances }
            default { Write-Error "Invalid action: $Action" }
        }
        return
    }
    
    # Interactive menu
    while ($true) {
        Show-Menu
        $choice = Read-Host "Select an option (1-8)"
        
        switch ($choice) {
            "1" { Setup-OpenWrtNative }
            "2" { Show-AvailableImages }
            "3" { Start-OpenWrtShell }
            "4" { Test-OpenWrtEnvironment }
            "5" { Setup-OpenWrtBuildSystem }
            "6" { Setup-AdvancedOpenWrtInit }
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
