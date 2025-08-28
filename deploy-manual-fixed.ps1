# PowerShell script to deploy manually built VUCI packages
# Fixed version with proper command execution

param(
    [string]$RouterIP = "192.168.80.1",
    [string]$SSHKey = "C:\Users\markusla\OneDrive\IT\RUTOS Keys\rusos_private_key_openssh"
)

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "VUCI PACKAGE DEPLOYMENT (Fixed)" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""

# Check if key exists
if (!(Test-Path $SSHKey)) {
    Write-Host "SSH key not found at: $SSHKey" -ForegroundColor Red
    exit 1
}

# Get packages from WSL
Write-Host "Getting packages from WSL..." -ForegroundColor Yellow
$TempDir = "$env:TEMP\vuci-deploy-$(Get-Date -Format 'yyyyMMddHHmmss')"
New-Item -ItemType Directory -Path $TempDir -Force | Out-Null

# Copy packages from WSL to Windows temp directory
wsl cp /home/markusla/vuci-packages/*.ipk /mnt/c/Users/markusla/AppData/Local/Temp/
$PackagePattern = "vuci-app-example*.ipk"
$SourcePath = "$env:TEMP"
Get-ChildItem -Path $SourcePath -Filter $PackagePattern | ForEach-Object {
    Copy-Item $_.FullName -Destination $TempDir
}

Write-Host "Packages copied to: $TempDir" -ForegroundColor Green
Get-ChildItem $TempDir

# Clean up old packages on router
Write-Host ""
Write-Host "Cleaning up old packages on router..." -ForegroundColor Yellow

ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "opkg remove vuci-app-example-ui vuci-app-example-api 2>/dev/null || true"
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "rm -f /tmp/vuci-app-example*.ipk"
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "rm -rf /usr/share/vuci/menu.d/example.json /www/vuci-app-example /www/example.html"
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "rm -f /usr/lib/lua/api/services/example.lua"
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "rm -rf /tmp/luci-* /tmp/vuci-*"

Write-Host "Cleanup complete" -ForegroundColor Green

# Transfer packages
Write-Host ""
Write-Host "Transferring packages to router..." -ForegroundColor Yellow
Get-ChildItem "$TempDir\*.ipk" | ForEach-Object {
    Write-Host "  Copying: $($_.Name)" -ForegroundColor Gray
    scp -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null $_.FullName "root@${RouterIP}:/tmp/"
}

# Install packages
Write-Host ""
Write-Host "Installing packages..." -ForegroundColor Yellow

Write-Host "  Installing API package..." -ForegroundColor Gray
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "cd /tmp && opkg install vuci-app-example-api_1.0-4_arm_cortex-a7_neon-vfpv4.ipk"

Write-Host "  Installing UI package..." -ForegroundColor Gray
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "cd /tmp && opkg install vuci-app-example-ui_1.0-4_arm_cortex-a7_neon-vfpv4.ipk"

# Verify installation
Write-Host ""
Write-Host "Verifying installation..." -ForegroundColor Yellow
Write-Host "----------------------------------------" -ForegroundColor Gray

Write-Host "Installed packages:" -ForegroundColor Cyan
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "opkg list-installed | grep vuci-app-example"

Write-Host ""
Write-Host "Menu file:" -ForegroundColor Cyan
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "if [ -f /usr/share/vuci/menu.d/example.json ]; then echo 'Found at: /usr/share/vuci/menu.d/example.json'; cat /usr/share/vuci/menu.d/example.json; else echo 'Not found'; fi"

Write-Host ""
Write-Host "API service:" -ForegroundColor Cyan
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "if [ -f /usr/lib/lua/api/services/example.lua ]; then echo 'Found at: /usr/lib/lua/api/services/example.lua'; else echo 'Not found'; fi"

Write-Host ""
Write-Host "HTML fallback:" -ForegroundColor Cyan
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "if [ -f /www/vuci-app-example/index.html ]; then echo 'Found at: /www/vuci-app-example/index.html'; else echo 'Not found'; fi"

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
Write-Host "1. VUCI Interface: http://$RouterIP -> Services -> Example" -ForegroundColor White
Write-Host "   (May show 'Failed to load' due to missing webpack compilation)" -ForegroundColor Gray
Write-Host ""
Write-Host "2. HTML Fallback: http://$RouterIP/vuci-app-example/" -ForegroundColor White
Write-Host "   (Should work - provides a test interface)" -ForegroundColor Green
Write-Host ""
Write-Host "3. Direct HTML: http://$RouterIP/example.html" -ForegroundColor White
Write-Host "   (Alternative access to the HTML page)" -ForegroundColor Green
Write-Host ""
Write-Host "The HTML fallback page lets you test the API" -ForegroundColor Cyan
Write-Host "even without proper Vue.js compilation." -ForegroundColor Cyan


