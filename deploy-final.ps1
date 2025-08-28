# Final VUCI Package Deployment
# PowerShell script for deploying to RUTOS device

param(
    [string]$RutosIP = "192.168.80.1",
    [string]$SshKeyPath = "C:\Users\markusla\OneDrive\IT\RUTOS Keys\rusos_private_key_openssh"
)

Write-Host "Deploying VUCI Packages to RUTOS" -ForegroundColor Green
Write-Host "RUTOS IP: $RutosIP" -ForegroundColor Cyan

# Verify SSH key exists
if (-not (Test-Path $SshKeyPath)) {
    Write-Host "SSH key not found: $SshKeyPath" -ForegroundColor Red
    exit 1
}

# Copy SSH key to temporary location
$TempSshKey = "C:\temp\rutos_key"
Copy-Item $SshKeyPath $TempSshKey -Force
Write-Host "Setting SSH key permissions..." -ForegroundColor Yellow
wsl chmod 600 "/mnt/c/temp/rutos_key"

# Remove old host key
Write-Host "Removing old SSH host key..." -ForegroundColor Yellow
wsl ssh-keygen -f "/home/markusla/.ssh/known_hosts" -R $RutosIP 2>$null

# Test SSH connection
Write-Host "Testing SSH connection..." -ForegroundColor Yellow
$SshTest = wsl ssh -i "/mnt/c/temp/rutos_key" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=10 "root@$RutosIP" "echo SSH connection successful"

if ($LASTEXITCODE -ne 0) {
    Write-Host "SSH connection failed!" -ForegroundColor Red
    exit 1
}

Write-Host "SSH connection successful" -ForegroundColor Green

# Transfer packages
Write-Host "Transferring packages..." -ForegroundColor Yellow
$TransferResult = wsl scp -i "/mnt/c/temp/rutos_key" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "/home/markusla/complete-vuci-build/*.ipk" "root@${RutosIP}:/tmp/"

if ($LASTEXITCODE -ne 0) {
    Write-Host "Package transfer failed!" -ForegroundColor Red
    exit 1
}

Write-Host "Packages transferred" -ForegroundColor Green

# Remove existing packages
Write-Host "Removing existing packages..." -ForegroundColor Yellow
wsl ssh -i "/mnt/c/temp/rutos_key" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RutosIP" "opkg remove vuci-app-autonomy-ui vuci-app-autonomy-api --force-depends 2>/dev/null || true"

# Install API package
Write-Host "Installing API package..." -ForegroundColor Yellow
$ApiInstall = wsl ssh -i "/mnt/c/temp/rutos_key" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RutosIP" "cd /tmp && opkg install vuci-app-autonomy-api_1.0-1_arm_cortex-a7_neon-vfpv4.ipk"

if ($LASTEXITCODE -ne 0) {
    Write-Host "API package installation failed!" -ForegroundColor Red
    Write-Host "Install Output: $ApiInstall" -ForegroundColor Red
    exit 1
}

Write-Host "API package installed" -ForegroundColor Green

# Install UI package
Write-Host "Installing UI package..." -ForegroundColor Yellow
$UiInstall = wsl ssh -i "/mnt/c/temp/rutos_key" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RutosIP" "cd /tmp && opkg install vuci-app-autonomy-ui_1.0-1_arm_cortex-a7_neon-vfpv4.ipk"

if ($LASTEXITCODE -ne 0) {
    Write-Host "UI package installation failed!" -ForegroundColor Red
    Write-Host "Install Output: $UiInstall" -ForegroundColor Red
    exit 1
}

Write-Host "UI package installed" -ForegroundColor Green

# Verify installation
Write-Host "Verifying installation..." -ForegroundColor Yellow

$InstalledPackages = wsl ssh -i "/mnt/c/temp/rutos_key" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RutosIP" "opkg list-installed | grep autonomy"
Write-Host "Installed packages: $InstalledPackages" -ForegroundColor Green

$FileCheck = wsl ssh -i "/mnt/c/temp/rutos_key" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RutosIP" "ls -la /usr/local/usr/lib/lua/api/services/autonomy.lua /usr/local/usr/share/vuci/menu.d/autonomy.json /usr/local/www/assets/app.autonomy.app-*.js.gz 2>/dev/null || echo Some files not found"
Write-Host "File check: $FileCheck" -ForegroundColor Green

$MenuCheck = wsl ssh -i "/mnt/c/temp/rutos_key" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RutosIP" "cat /usr/local/usr/share/vuci/menu.d/autonomy.json 2>/dev/null || echo Menu file not found"
Write-Host "Menu configuration: $MenuCheck" -ForegroundColor Green

# Clean up
Remove-Item $TempSshKey -Force -ErrorAction SilentlyContinue
wsl ssh -i "/mnt/c/temp/rutos_key" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RutosIP" "rm -f /tmp/*.ipk"

Write-Host ""
Write-Host "Deployment Complete!" -ForegroundColor Green
Write-Host "Open browser: http://$RutosIP" -ForegroundColor Cyan
Write-Host "Look for Autonomy in the VUCI menu" -ForegroundColor Cyan


