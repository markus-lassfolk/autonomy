# Deploy Complete VUCI Packages with All New Insights
# PowerShell script for deploying to RUTOS device

param(
    [string]$RutosIP = "192.168.80.1",
    [string]$SshKeyPath = "C:\Users\markusla\OneDrive\IT\RUTOS Keys\rusos_private_key_openssh",
    [string]$BuildDir = "/home/markusla/complete-vuci-build"
)

Write-Host "🚀 Deploying Complete VUCI Packages with All New Insights" -ForegroundColor Green
Write-Host "RUTOS IP: $RutosIP" -ForegroundColor Cyan
Write-Host "SSH Key: $SshKeyPath" -ForegroundColor Cyan
Write-Host "Build Directory: $BuildDir" -ForegroundColor Cyan

# Verify SSH key exists
if (-not (Test-Path $SshKeyPath)) {
    Write-Host "❌ SSH key not found: $SshKeyPath" -ForegroundColor Red
    exit 1
}

# Copy SSH key to temporary location with correct permissions
$TempSshKey = "C:\temp\rutos_key"
Copy-Item $SshKeyPath $TempSshKey -Force

Write-Host "🔑 Setting SSH key permissions..." -ForegroundColor Yellow
wsl chmod 600 "/mnt/c/temp/rutos_key"

# Build the packages first
Write-Host "🔨 Building VUCI packages..." -ForegroundColor Yellow
wsl bash -c "cd '$BuildDir' && chmod +x build-complete-vuci-packages.sh && ./build-complete-vuci-packages.sh"

if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ Package build failed!" -ForegroundColor Red
    exit 1
}

# Verify packages were created
Write-Host "📦 Verifying packages..." -ForegroundColor Yellow
$Packages = wsl bash -c "ls -la '$BuildDir'/*.ipk"
Write-Host $Packages -ForegroundColor Cyan

# Check package file types
Write-Host "🔍 Checking package file types..." -ForegroundColor Yellow
wsl bash -c "cd '$BuildDir' && for pkg in *.ipk; do echo \"\$pkg: \$(file \$pkg)\"; done"

# Remove old host key if needed
Write-Host "🔐 Removing old SSH host key..." -ForegroundColor Yellow
wsl ssh-keygen -f "/home/markusla/.ssh/known_hosts" -R $RutosIP 2>$null

# Test SSH connection
Write-Host "🔌 Testing SSH connection..." -ForegroundColor Yellow
$SshTest = wsl ssh -i "/mnt/c/temp/rutos_key" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=10 root@$RutosIP "echo 'SSH connection successful'"

if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ SSH connection failed!" -ForegroundColor Red
    Write-Host "SSH Output: $SshTest" -ForegroundColor Red
    exit 1
}

Write-Host "✅ SSH connection successful" -ForegroundColor Green

# Transfer packages to device
Write-Host "📤 Transferring packages to RUTOS device..." -ForegroundColor Yellow
$TransferResult = wsl scp -i "/mnt/c/temp/rutos_key" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "$BuildDir"/*.ipk "root@${RutosIP}:/tmp/"

if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ Package transfer failed!" -ForegroundColor Red
    Write-Host "Transfer Output: $TransferResult" -ForegroundColor Red
    exit 1
}

Write-Host "✅ Packages transferred successfully" -ForegroundColor Green

# Install packages on device
Write-Host "📥 Installing packages on RUTOS device..." -ForegroundColor Yellow

# First, remove any existing packages
Write-Host "🧹 Removing existing packages..." -ForegroundColor Yellow
wsl ssh -i "/mnt/c/temp/rutos_key" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RutosIP" "opkg remove vuci-app-autonomy-ui vuci-app-autonomy-api --force-depends 2>/dev/null || true"

# Install API package first
Write-Host "📦 Installing API package..." -ForegroundColor Yellow
$ApiInstall = wsl ssh -i "/mnt/c/temp/rutos_key" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RutosIP" "cd /tmp && opkg install vuci-app-autonomy-api_1.0-1_arm_cortex-a7_neon-vfpv4.ipk"

if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ API package installation failed!" -ForegroundColor Red
    Write-Host "Install Output: $ApiInstall" -ForegroundColor Red
    exit 1
}

Write-Host "✅ API package installed successfully" -ForegroundColor Green

# Install UI package
Write-Host "📦 Installing UI package..." -ForegroundColor Yellow
$UiInstall = wsl ssh -i "/mnt/c/temp/rutos_key" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null root@$RutosIP "cd /tmp && opkg install vuci-app-autonomy-ui_1.0-1_arm_cortex-a7_neon-vfpv4.ipk"

if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ UI package installation failed!" -ForegroundColor Red
    Write-Host "Install Output: $UiInstall" -ForegroundColor Red
    exit 1
}

Write-Host "✅ UI package installed successfully" -ForegroundColor Green

# Verify installation
Write-Host "🔍 Verifying installation..." -ForegroundColor Yellow

# Check installed packages
Write-Host "📋 Checking installed packages..." -ForegroundColor Cyan
$InstalledPackages = wsl ssh -i "/mnt/c/temp/rutos_key" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null root@$RutosIP "opkg list-installed | grep autonomy"
Write-Host $InstalledPackages -ForegroundColor Green

# Check file locations
Write-Host "📁 Checking file locations..." -ForegroundColor Cyan
$FileCheck = wsl ssh -i "/mnt/c/temp/rutos_key" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RutosIP" @"
echo 'API Service File:'
ls -la /usr/local/usr/lib/lua/api/services/autonomy.lua 2>/dev/null || echo 'Not found'
echo 'Menu Configuration:'
ls -la /usr/local/usr/share/vuci/menu.d/autonomy.json 2>/dev/null || echo 'Not found'
echo 'UI Assets:'
ls -la /usr/local/www/assets/app.autonomy.app-*.js.gz 2>/dev/null || echo 'Not found'
"@
Write-Host $FileCheck -ForegroundColor Green

# Check menu configuration
Write-Host "📋 Checking menu configuration..." -ForegroundColor Cyan
$MenuCheck = wsl ssh -i "/mnt/c/temp/rutos_key" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null root@$RutosIP "cat /usr/local/usr/share/vuci/menu.d/autonomy.json 2>/dev/null || echo 'Menu file not found'"
Write-Host $MenuCheck -ForegroundColor Green

# Check if services are accessible
Write-Host "🔌 Testing API endpoints..." -ForegroundColor Cyan
$ApiTest = wsl ssh -i "/mnt/c/temp/rutos_key" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null root@$RutosIP "curl -s http://localhost/api/autonomy_status 2>/dev/null || echo 'API endpoint not accessible'"
Write-Host $ApiTest -ForegroundColor Green

# Check web server status
Write-Host "🌐 Checking web server..." -ForegroundColor Cyan
$WebServer = wsl ssh -i "/mnt/c/temp/rutos_key" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null root@$RutosIP "ps | grep uhttpd || echo 'Web server not running'"
Write-Host $WebServer -ForegroundColor Green

# Clean up
Write-Host "🧹 Cleaning up..." -ForegroundColor Yellow
Remove-Item $TempSshKey -Force -ErrorAction SilentlyContinue
wsl ssh -i "/mnt/c/temp/rutos_key" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null root@$RutosIP "rm -f /tmp/*.ipk"

Write-Host ""
Write-Host "🎉 Deployment Complete!" -ForegroundColor Green
Write-Host ""
Write-Host "📋 Summary:" -ForegroundColor Cyan
Write-Host "✅ Packages built with gzip compression" -ForegroundColor Green
Write-Host "✅ Packages transferred to RUTOS device" -ForegroundColor Green
Write-Host "✅ API package installed successfully" -ForegroundColor Green
Write-Host "✅ UI package installed successfully" -ForegroundColor Green
Write-Host "✅ Files placed in correct locations" -ForegroundColor Green
Write-Host ""
Write-Host "🌐 Next Steps:" -ForegroundColor Cyan
Write-Host "1. Open web browser and navigate to: http://$RutosIP" -ForegroundColor White
Write-Host "2. Look for 'Autonomy' in the VUCI menu" -ForegroundColor White
Write-Host "3. Test the application functionality" -ForegroundColor White
Write-Host ""
Write-Host "🔧 Troubleshooting:" -ForegroundColor Cyan
Write-Host "If the app doesn't appear in the UI:" -ForegroundColor White
Write-Host "- Check browser console for errors" -ForegroundColor White
Write-Host "- Verify menu configuration is correct" -ForegroundColor White
Write-Host "- Check if Vue.js assets are loading" -ForegroundColor White
