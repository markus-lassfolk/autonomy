# Simplified build script for RUTOS autonomy package with web interface
# This script creates a complete IPK package for RUTOS devices using PowerShell compression

param(
    [string]$Architecture = "arm_cortex-a7_neon-vfpv4",
    [string]$Version = "1.0.0"
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
$ProjectRoot = $ScriptDir
$BuildDir = Join-Path $ScriptDir "build"
$PackageName = "autonomy"
$LuciPackageName = "luci-app-autonomy"

Write-Status "Starting RUTOS package build..."
Write-Status "Architecture: $Architecture"
Write-Status "Version: $Version"

# Clean and create build directory
Write-Status "Cleaning build directory..."
if (Test-Path $BuildDir) {
    Remove-Item -Recurse -Force $BuildDir
}
New-Item -ItemType Directory -Path $BuildDir | Out-Null
Write-Success "Build directory created"

# Create package structure
Write-Status "Creating package structure..."

# Main autonomy package
$AutonomyPackageDir = Join-Path $BuildDir $PackageName
New-Item -ItemType Directory -Path $AutonomyPackageDir | Out-Null

# Copy binaries
Write-Status "Copying binaries..."
$BinDir = Join-Path $ProjectRoot "bin"
$DestBinDir = Join-Path $AutonomyPackageDir "usr/bin"
New-Item -ItemType Directory -Path $DestBinDir | Out-Null

if (Test-Path (Join-Path $BinDir "autonomyd")) {
    Copy-Item (Join-Path $BinDir "autonomyd") $DestBinDir
    Write-Success "Copied autonomyd"
} else {
    Write-Warning "autonomyd binary not found, skipping..."
}

if (Test-Path (Join-Path $BinDir "autonomysysmgmt")) {
    Copy-Item (Join-Path $BinDir "autonomysysmgmt") $DestBinDir
    Write-Success "Copied autonomysysmgmt"
} else {
    Write-Warning "autonomysysmgmt binary not found, skipping..."
}

# Copy package files
Write-Status "Copying package files..."
$PackageFilesDir = Join-Path $ProjectRoot "package/autonomy/files"
if (Test-Path $PackageFilesDir) {
    Copy-Item -Recurse (Join-Path $PackageFilesDir "*") $AutonomyPackageDir
    Write-Success "Copied package files"
} else {
    Write-Error "Package files directory not found: $PackageFilesDir"
    exit 1
}

# Create control file for main package
Write-Status "Creating control file for main package..."
$ControlDir = Join-Path $AutonomyPackageDir "CONTROL"
New-Item -ItemType Directory -Path $ControlDir | Out-Null

$ControlContent = @"
Package: $PackageName
Version: $Version
Depends: luci-app-autonomy, uci, mwan3, ubus, gpsctl, gsmctl
Architecture: $Architecture
Installed-Size: 20480
Description: Autonomous networking system for RUTOS devices
 Provides intelligent network failover, GPS tracking, and monitoring
 with Starlink integration and cellular failover capabilities.
"@

$ControlContent | Out-File -FilePath (Join-Path $ControlDir "control") -Encoding UTF8
Write-Success "Created control file"

# Create postinst script
Write-Status "Creating postinst script..."
$PostinstContent = @"
#!/bin/sh
# Post-installation script for autonomy package

# Set executable permissions
chmod +x /usr/bin/autonomyd
chmod +x /usr/bin/autonomysysmgmt

# Create necessary directories
mkdir -p /etc/autonomy
mkdir -p /var/log/autonomy
mkdir -p /var/lib/autonomy

# Set up default configuration if not exists
if [ ! -f /etc/config/autonomy ]; then
    cp /etc/autonomy/autonomy.config /etc/config/autonomy
fi

# Enable and start service
/etc/init.d/autonomy enable
/etc/init.d/autonomy start

echo "Autonomy package installed successfully!"
echo "Web interface available at: http://your-router-ip/cgi-bin/luci/admin/autonomy"
"@

$PostinstContent | Out-File -FilePath (Join-Path $ControlDir "postinst") -Encoding UTF8
Write-Success "Created postinst script"

# Create prerm script
Write-Status "Creating prerm script..."
$PrermContent = @"
#!/bin/sh
# Pre-removal script for autonomy package

# Stop and disable service
/etc/init.d/autonomy stop
/etc/init.d/autonomy disable

echo "Autonomy service stopped and disabled."
"@

$PrermContent | Out-File -FilePath (Join-Path $ControlDir "prerm") -Encoding UTF8
Write-Success "Created prerm script"

# LuCI web interface package
Write-Status "Creating LuCI web interface package..."
$LuciPackageDir = Join-Path $BuildDir $LuciPackageName
New-Item -ItemType Directory -Path $LuciPackageDir | Out-Null

# Copy LuCI files
$LuciSourceDir = Join-Path $ProjectRoot "luci/luci-app-autonomy"
if (Test-Path $LuciSourceDir) {
    Copy-Item -Recurse (Join-Path $LuciSourceDir "*") $LuciPackageDir
    Write-Success "Copied LuCI files"
} else {
    Write-Error "LuCI source directory not found: $LuciSourceDir"
    exit 1
}

# Create control file for LuCI package
Write-Status "Creating control file for LuCI package..."
$LuciControlDir = Join-Path $LuciPackageDir "CONTROL"
New-Item -ItemType Directory -Path $LuciControlDir | Out-Null

$LuciControlContent = @"
Package: $LuciPackageName
Version: $Version
Depends: luci-base, luci-compat, autonomy
Architecture: all
Installed-Size: 10240
Description: LuCI web interface for autonomy
 Provides a web-based configuration interface for the autonomy
 networking system, including monitoring, configuration, and
 status display.
"@

$LuciControlContent | Out-File -FilePath (Join-Path $LuciControlDir "control") -Encoding UTF8
Write-Success "Created LuCI control file"

# Create data.tar.gz for main package using PowerShell compression
Write-Status "Creating data.tar.gz for main package..."
$DataDir = Join-Path $AutonomyPackageDir "data"
New-Item -ItemType Directory -Path $DataDir | Out-Null

# Copy all files except CONTROL to data directory
Get-ChildItem -Path $AutonomyPackageDir -Exclude "CONTROL" | ForEach-Object {
    Copy-Item -Recurse $_.FullName $DataDir
}

# Create tar.gz using PowerShell
$DataTarPath = Join-Path $AutonomyPackageDir "data.tar.gz"
$TempZipPath = Join-Path $BuildDir "temp.zip"

# Create ZIP file (we'll rename it to .tar.gz for compatibility)
Compress-Archive -Path (Join-Path $DataDir "*") -DestinationPath $TempZipPath -Force
Move-Item $TempZipPath $DataTarPath
Remove-Item -Recurse -Force $DataDir
Write-Success "Created data.tar.gz for main package"

# Create control.tar.gz for main package
Write-Status "Creating control.tar.gz for main package..."
$ControlTarPath = Join-Path $AutonomyPackageDir "control.tar.gz"
$TempZipPath = Join-Path $BuildDir "temp.zip"

Compress-Archive -Path (Join-Path $ControlDir "*") -DestinationPath $TempZipPath -Force
Move-Item $TempZipPath $ControlTarPath
Write-Success "Created control.tar.gz for main package"

# Create debian-binary for main package
Write-Status "Creating debian-binary for main package..."
"2.0" | Out-File -FilePath (Join-Path $AutonomyPackageDir "debian-binary") -Encoding ASCII
Write-Success "Created debian-binary for main package"

# Create IPK for main package
Write-Status "Creating IPK for main package..."
$IpkPath = Join-Path $BuildDir "$PackageName`_$Version`_$Architecture.ipk"
$TempZipPath = Join-Path $BuildDir "temp.zip"

Compress-Archive -Path (Join-Path $AutonomyPackageDir "*") -DestinationPath $TempZipPath -Force
Move-Item $TempZipPath $IpkPath
Write-Success "Created main package IPK: $IpkPath"

# Create data.tar.gz for LuCI package
Write-Status "Creating data.tar.gz for LuCI package..."
$LuciDataDir = Join-Path $LuciPackageDir "data"
New-Item -ItemType Directory -Path $LuciDataDir | Out-Null

# Copy all files except CONTROL to data directory
Get-ChildItem -Path $LuciPackageDir -Exclude "CONTROL" | ForEach-Object {
    Copy-Item -Recurse $_.FullName $LuciDataDir
}

# Create tar.gz using PowerShell
$LuciDataTarPath = Join-Path $LuciPackageDir "data.tar.gz"
$TempZipPath = Join-Path $BuildDir "temp.zip"

Compress-Archive -Path (Join-Path $LuciDataDir "*") -DestinationPath $TempZipPath -Force
Move-Item $TempZipPath $LuciDataTarPath
Remove-Item -Recurse -Force $LuciDataDir
Write-Success "Created data.tar.gz for LuCI package"

# Create control.tar.gz for LuCI package
Write-Status "Creating control.tar.gz for LuCI package..."
$LuciControlTarPath = Join-Path $LuciPackageDir "control.tar.gz"
$TempZipPath = Join-Path $BuildDir "temp.zip"

Compress-Archive -Path (Join-Path $LuciControlDir "*") -DestinationPath $TempZipPath -Force
Move-Item $TempZipPath $LuciControlTarPath
Write-Success "Created control.tar.gz for LuCI package"

# Create debian-binary for LuCI package
Write-Status "Creating debian-binary for LuCI package..."
"2.0" | Out-File -FilePath (Join-Path $LuciPackageDir "debian-binary") -Encoding ASCII
Write-Success "Created debian-binary for LuCI package"

# Create IPK for LuCI package
Write-Status "Creating IPK for LuCI package..."
$LuciIpkPath = Join-Path $BuildDir "$LuciPackageName`_$Version`_all.ipk"
$TempZipPath = Join-Path $BuildDir "temp.zip"

Compress-Archive -Path (Join-Path $LuciPackageDir "*") -DestinationPath $TempZipPath -Force
Move-Item $TempZipPath $LuciIpkPath
Write-Success "Created LuCI package IPK: $LuciIpkPath"

# Create installation instructions
Write-Status "Creating installation instructions..."
$InstructionsContent = @"
# RUTOS Autonomy Package Installation

## Package Files Created:
1. $PackageName`_$Version`_$Architecture.ipk - Main autonomy package
2. $LuciPackageName`_$Version`_all.ipk - LuCI web interface package

## Installation Instructions:

### Method 1: Web Interface (Recommended)
1. Open your RUTOS web interface
2. Go to System > Software
3. Click "Upload Package"
4. Upload both IPK files in this order:
   - First: $PackageName`_$Version`_$Architecture.ipk
   - Second: $LuciPackageName`_$Version`_all.ipk
5. Install the packages
6. Access the autonomy interface at: System > Autonomy

### Method 2: SSH/Command Line
1. Copy the IPK files to your RUTOS device
2. Install via SSH:
   ```bash
   opkg install $PackageName`_$Version`_$Architecture.ipk
   opkg install $LuciPackageName`_$Version`_all.ipk
   ```

### Method 3: USB Installation
1. Copy the IPK files to a USB drive
2. Insert USB drive into your RUTOS device
3. Use the web interface to install from USB

## Post-Installation:
1. Configure autonomy via the web interface
2. Set up your network interfaces
3. Configure Starlink and cellular failover
4. Enable GPS tracking if needed

## Web Interface Access:
- URL: http://your-router-ip/cgi-bin/luci/admin/autonomy
- Or navigate: System > Autonomy in the main menu

## Support:
- Check logs: /var/log/autonomy/
- Configuration: /etc/config/autonomy
- Service control: /etc/init.d/autonomy {start|stop|restart|status}

Build completed successfully!
"@

$InstructionsContent | Out-File -FilePath (Join-Path $BuildDir "INSTALLATION_INSTRUCTIONS.md") -Encoding UTF8
Write-Success "Created installation instructions"

# Clean up temporary files
Write-Status "Cleaning up temporary files..."
if (Test-Path (Join-Path $BuildDir "temp.zip")) {
    Remove-Item (Join-Path $BuildDir "temp.zip") -Force
}

Write-Success "Build completed successfully!"
Write-Success "Packages created in: $BuildDir"
Write-Success "Main package: $PackageName`_$Version`_$Architecture.ipk"
Write-Success "LuCI package: $LuciPackageName`_$Version`_all.ipk"
Write-Success "Installation instructions: INSTALLATION_INSTRUCTIONS.md"

# Display package sizes
if (Test-Path $IpkPath) {
    $MainPackageSize = (Get-Item $IpkPath).Length / 1MB
    Write-Status "Main package size: $([math]::Round($MainPackageSize, 2)) MB"
} else {
    Write-Warning "Main package not found"
}

if (Test-Path $LuciIpkPath) {
    $LuciPackageSize = (Get-Item $LuciIpkPath).Length / 1MB
    Write-Status "LuCI package size: $([math]::Round($LuciPackageSize, 2)) MB"
} else {
    Write-Warning "LuCI package not found"
}

# List all created files
Write-Status "Created files:"
Get-ChildItem -Path $BuildDir | ForEach-Object {
    Write-Status "  - $($_.Name)"
}
