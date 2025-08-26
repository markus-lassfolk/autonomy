# Deploy Web UI using built IPK packages
# This script deploys the web UI using IPK packages built in WSL

param(
    [string]$TargetIP = "192.168.80.1",
    [string]$SSHKey = "C:\Users\markusla\OneDrive\IT\RUTOS Keys\rusos_private_key_openssh"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot

Write-Host "=== Deploying Web UI with IPK Packages ===" -ForegroundColor Green
Write-Host "Target: $TargetIP" -ForegroundColor Yellow

# Check if SSH key exists
if (-not (Test-Path $SSHKey)) {
    Write-Error "SSH key not found at: $SSHKey"
    exit 1
}

# Find IPK packages
$ApiPackage = Get-ChildItem -Path $ProjectRoot -Filter "*vuci-app-autonomy-api*.ipk" | Select-Object -First 1
$UiPackage = Get-ChildItem -Path $ProjectRoot -Filter "*vuci-app-autonomy-ui*.ipk" | Select-Object -First 1

if (-not $ApiPackage -or -not $UiPackage) {
    Write-Error "IPK packages not found. Please run the WSL build script first:"
    Write-Host "  In WSL terminal: ./build-webui-wsl.sh" -ForegroundColor Yellow
    exit 1
}

Write-Host "Found IPK packages:" -ForegroundColor Green
Write-Host "  API: $($ApiPackage.Name)" -ForegroundColor White
Write-Host "  UI: $($UiPackage.Name)" -ForegroundColor White

# Deploy to RUTX50
Write-Host "Deploying packages to RUTX50..." -ForegroundColor Cyan

$ApiPackageFile = $ApiPackage.FullName
$UiPackageFile = $UiPackage.FullName

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
