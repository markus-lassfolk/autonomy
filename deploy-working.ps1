# PowerShell script to deploy working VUCI packages
# Deploys all three packages: API, UI, and Registration helper

param(
    [string]$RouterIP = "192.168.80.1",
    [string]$SSHKey = "C:\Users\markusla\OneDrive\IT\RUTOS Keys\rusos_private_key_openssh"
)

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "DEPLOYING WORKING VUCI PACKAGES" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""

# Check if key exists
if (!(Test-Path $SSHKey)) {
    Write-Host "SSH key not found at: $SSHKey" -ForegroundColor Red
    exit 1
}

# Get packages from WSL
Write-Host "Getting packages from WSL..." -ForegroundColor Yellow
$TempDir = "$env:TEMP\vuci-working-$(Get-Date -Format 'yyyyMMddHHmmss')"
New-Item -ItemType Directory -Path $TempDir -Force | Out-Null

# Copy version 1.0-6 packages
wsl cp /home/markusla/vuci-packages/*_1.0-6_*.ipk /mnt/c/Users/markusla/AppData/Local/Temp/
$PackagePattern = "*_1.0-6_*.ipk"
$SourcePath = "$env:TEMP"
Get-ChildItem -Path $SourcePath -Filter $PackagePattern | ForEach-Object {
    Copy-Item $_.FullName -Destination $TempDir
}

Write-Host "Packages copied to: $TempDir" -ForegroundColor Green
Get-ChildItem $TempDir

# Clean up old packages
Write-Host ""
Write-Host "Cleaning up old packages on router..." -ForegroundColor Yellow

ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "opkg remove vuci-app-example-register vuci-app-example-ui vuci-app-example-api 2>/dev/null || true"
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "rm -f /tmp/vuci-app-example*.ipk"
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "rm -rf /overlay/root/upper/usr/share/vuci/menu.d/example.json"
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "rm -rf /overlay/root/upper/usr/lib/lua/api/services/example.lua"
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "rm -rf /overlay/root/upper/www/example.html"
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "rm -f /www/cgi-bin/example-api"
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "rm -f /tmp/example-api.log"

Write-Host "Cleanup complete" -ForegroundColor Green

# Transfer packages
Write-Host ""
Write-Host "Transferring packages to router..." -ForegroundColor Yellow
Get-ChildItem "$TempDir\*.ipk" | ForEach-Object {
    Write-Host "  Copying: $($_.Name)" -ForegroundColor Gray
    scp -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null $_.FullName "root@${RouterIP}:/tmp/"
}

# Install packages in correct order
Write-Host ""
Write-Host "Installing packages..." -ForegroundColor Yellow

Write-Host "  1. Installing API package..." -ForegroundColor Gray
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "cd /tmp && opkg install vuci-app-example-api_1.0-6_arm_cortex-a7_neon-vfpv4.ipk"

Write-Host "  2. Installing UI package..." -ForegroundColor Gray
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "cd /tmp && opkg install vuci-app-example-ui_1.0-6_arm_cortex-a7_neon-vfpv4.ipk"

Write-Host "  3. Installing registration helper..." -ForegroundColor Gray
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "cd /tmp && opkg install vuci-app-example-register_1.0-6_arm_cortex-a7_neon-vfpv4.ipk"

# Verify installation
Write-Host ""
Write-Host "Verifying installation..." -ForegroundColor Yellow
Write-Host "----------------------------------------" -ForegroundColor Gray

Write-Host "Installed packages:" -ForegroundColor Cyan
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "opkg list-installed | grep vuci-app-example"

Write-Host ""
Write-Host "Checking symlinks:" -ForegroundColor Cyan
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "echo 'Menu symlink:'; ls -la /usr/share/vuci/menu.d/example.json 2>/dev/null || echo 'Not found'; echo ''; echo 'API symlink:'; ls -la /usr/lib/lua/api/services/example.lua 2>/dev/null || echo 'Not found'; echo ''; echo 'HTML symlink:'; ls -la /www/example.html 2>/dev/null || echo 'Not found'"

Write-Host ""
Write-Host "Checking CGI handler:" -ForegroundColor Cyan
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "ls -la /www/cgi-bin/example-api 2>/dev/null || echo 'CGI handler not found'"

Write-Host ""
Write-Host "Testing API directly:" -ForegroundColor Cyan
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "lua -e 'local api = require(\"api.services.example\"); if api and api.test then print(\"API module loaded successfully\") else print(\"API module failed to load\") end' 2>&1"

# Restart services
Write-Host ""
Write-Host "Restarting web server..." -ForegroundColor Yellow
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "/etc/init.d/uhttpd restart"

# Clean up temp directory
Remove-Item -Path $TempDir -Recurse -Force

# Final message
Write-Host ""
Write-Host "=========================================" -ForegroundColor Green
Write-Host "DEPLOYMENT COMPLETE!" -ForegroundColor Green
Write-Host "=========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Test the installation:" -ForegroundColor Yellow
Write-Host ""
Write-Host "1. HTML Interface (WORKING):" -ForegroundColor Green
Write-Host "   http://$RouterIP/vuci-app-example/" -ForegroundColor White
Write-Host "   - Test all buttons to check API connectivity" -ForegroundColor Gray
Write-Host ""
Write-Host "2. Direct HTML Link:" -ForegroundColor Yellow
Write-Host "   http://$RouterIP/example.html" -ForegroundColor White
Write-Host "   - Should work if symlink was created" -ForegroundColor Gray
Write-Host ""
Write-Host "3. VUCI Menu Entry:" -ForegroundColor Yellow
Write-Host "   Services -> Example" -ForegroundColor White
Write-Host "   - Still needs webpack compilation to work" -ForegroundColor Gray
Write-Host ""
Write-Host "The HTML interface now includes:" -ForegroundColor Cyan
Write-Host "  ✓ Multiple API test buttons" -ForegroundColor White
Write-Host "  ✓ Real-time response logging" -ForegroundColor White
Write-Host "  ✓ Debug information display" -ForegroundColor White
Write-Host "  ✓ Route checking functionality" -ForegroundColor White


