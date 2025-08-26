# Build and Deploy Autonomy Web UI to RUTOS
# This script builds the VuCI web UI packages and deploys them to the RUTX50

param(
    [string]$TargetIP = "192.168.80.1",
    [string]$SSHKey = "C:\Users\markusla\OneDrive\IT\RUTOS Keys\rusos_private_key_openssh",
    [string]$RutosSDK = "J:\GithubCursor\rutos-ipq40xx-rutx-sdk"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot

Write-Host "=== Autonomy Web UI Build and Deploy ===" -ForegroundColor Green
Write-Host "Target: $TargetIP" -ForegroundColor Yellow
Write-Host "SDK: $RutosSDK" -ForegroundColor Yellow

# Check if RUTOS SDK exists
if (-not (Test-Path $RutosSDK)) {
    Write-Error "RUTOS SDK not found at: $RutosSDK"
    exit 1
}

# Check if SSH key exists
if (-not (Test-Path $SSHKey)) {
    Write-Error "SSH key not found at: $SSHKey"
    exit 1
}

# Create build directory
$BuildDir = Join-Path $ProjectRoot "build-webui"
if (Test-Path $BuildDir) {
    Remove-Item $BuildDir -Recurse -Force
}
New-Item -ItemType Directory -Path $BuildDir | Out-Null

Write-Host "Building VuCI API package..." -ForegroundColor Cyan

# Copy API package to SDK
$ApiPackageDir = Join-Path $RutosSDK "package\base\vuci-app-autonomy-api"
if (Test-Path $ApiPackageDir) {
    Remove-Item $ApiPackageDir -Recurse -Force
}
New-Item -ItemType Directory -Path $ApiPackageDir | Out-Null

# Copy API files
Copy-Item -Path "vuci-app-autonomy-api\*" -Destination $ApiPackageDir -Recurse -Force

Write-Host "Building VuCI UI package..." -ForegroundColor Cyan

# Copy UI package to SDK
$UiPackageDir = Join-Path $RutosSDK "package\base\vuci-app-autonomy-ui"
if (Test-Path $UiPackageDir) {
    Remove-Item $UiPackageDir -Recurse -Force
}
New-Item -ItemType Directory -Path $UiPackageDir | Out-Null

# Copy UI files
Copy-Item -Path "vuci-app-autonomy-ui\*" -Destination $UiPackageDir -Recurse -Force

# Build packages using WSL
Write-Host "Building packages with RUTOS SDK in WSL..." -ForegroundColor Cyan

# Convert Windows paths to WSL paths
$WslRutosSDK = $RutosSDK -replace 'J:', '/mnt/j' -replace '\\', '/'
$WslBuildDir = $BuildDir -replace 'J:', '/mnt/j' -replace '\\', '/'

# Create WSL build script
$WslBuildScript = @"
#!/bin/bash
set -e

cd "$WslRutosSDK"

# Clean previous builds
echo "Cleaning previous builds..."
make clean 2>/dev/null || true

# Build API package
echo "Building vuci-app-autonomy-api..."
make package/vuci-app-autonomy-api/compile V=s

# Build UI package
echo "Building vuci-app-autonomy-ui..."
make package/vuci-app-autonomy-ui/compile V=s

# Find built packages
API_PKG=\$(find bin/packages -name "*vuci-app-autonomy-api*.ipk" | head -1)
UI_PKG=\$(find bin/packages -name "*vuci-app-autonomy-ui*.ipk" | head -1)

if [ -z "\$API_PKG" ] || [ -z "\$UI_PKG" ]; then
    echo "Failed to find built packages"
    exit 1
fi

echo "Packages built successfully:"
echo "  API: \$(basename \$API_PKG)"
echo "  UI: \$(basename \$UI_PKG)"

# Copy packages to build directory
cp "\$API_PKG" "$WslBuildDir/"
cp "\$UI_PKG" "$WslBuildDir/"

echo "Build completed successfully"
"@

# Save WSL build script
$WslScriptPath = Join-Path $ProjectRoot "build-webui-wsl.sh"
$WslBuildScript | Out-File -FilePath $WslScriptPath -Encoding UTF8

try {
    # Make script executable and run in WSL
    Write-Host "Running build in WSL..." -ForegroundColor Yellow
    $WslScriptWslPath = $WslScriptPath -replace 'J:', '/mnt/j' -replace '\\', '/'
    
    & wsl bash -c "chmod +x '$WslScriptWslPath' && '$WslScriptWslPath'"
    
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to build packages in WSL"
        exit 1
    }
    
    # Find built packages
    $ApiPackage = Get-ChildItem -Path $BuildDir -Filter "*vuci-app-autonomy-api*.ipk" | Select-Object -First 1
    $UiPackage = Get-ChildItem -Path $BuildDir -Filter "*vuci-app-autonomy-ui*.ipk" | Select-Object -First 1
    
    if (-not $ApiPackage -or -not $UiPackage) {
        Write-Error "Failed to find built packages"
        exit 1
    }
    
    Write-Host "Packages built successfully:" -ForegroundColor Green
    Write-Host "  API: $($ApiPackage.Name)" -ForegroundColor White
    Write-Host "  UI: $($UiPackage.Name)" -ForegroundColor White
    
} finally {
    # Clean up WSL script
    if (Test-Path $WslScriptPath) {
        Remove-Item $WslScriptPath -Force
    }
}

# Deploy to RUTX50
Write-Host "Deploying packages to RUTX50..." -ForegroundColor Cyan

$ApiPackageFile = Join-Path $BuildDir $ApiPackage.Name
$UiPackageFile = Join-Path $BuildDir $UiPackage.Name

# Upload packages
Write-Host "Uploading API package..." -ForegroundColor Yellow
& scp -i $SSHKey -o StrictHostKeyChecking=no $ApiPackageFile "root@${TargetIP}:/tmp/"

if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to upload API package"
    exit 1
}

Write-Host "Uploading UI package..." -ForegroundColor Yellow
& scp -i $SSHKey -o StrictHostKeyChecking=no $UiPackageFile "root@${TargetIP}:/tmp/"

if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to upload UI package"
    exit 1
}

# Install packages
Write-Host "Installing packages..." -ForegroundColor Yellow

$InstallCommands = @"
# Install API package
opkg install /tmp/$($ApiPackage.Name)

# Install UI package
opkg install /tmp/$($UiPackage.Name)

# Start API service
/etc/init.d/autonomy-api enable
/etc/init.d/autonomy-api start

# Restart uhttpd to load new UI
/etc/init.d/uhttpd restart

# Clean up
rm -f /tmp/$($ApiPackage.Name) /tmp/$($UiPackage.Name)

echo "Web UI installation completed!"
echo "Access the UI at: http://$TargetIP/cgi-bin/luci/admin/autonomy"
"@

& ssh -i $SSHKey -o StrictHostKeyChecking=no "root@${TargetIP}" $InstallCommands

if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to install packages"
    exit 1
}

Write-Host "=== Web UI Deployment Completed ===" -ForegroundColor Green
Write-Host "Access the Autonomy Web UI at:" -ForegroundColor Yellow
Write-Host "  http://$TargetIP/cgi-bin/luci/admin/autonomy" -ForegroundColor White
Write-Host ""
Write-Host "Features available:" -ForegroundColor Cyan
Write-Host "  - Real-time system status monitoring" -ForegroundColor White
Write-Host "  - Starlink health monitoring" -ForegroundColor White
Write-Host "  - GPS and location services" -ForegroundColor White
Write-Host "  - OpenCELLID data submission" -ForegroundColor White
Write-Host "  - Network failover management" -ForegroundColor White
Write-Host "  - Configuration management" -ForegroundColor White
Write-Host "  - Log viewing and management" -ForegroundColor White
