# PowerShell script to deploy debug VUCI packages with verbose logging
# Shows detailed installation information

param(
    [string]$RouterIP = "192.168.80.1",
    [string]$SSHKey = "C:\Users\markusla\OneDrive\IT\RUTOS Keys\rusos_private_key_openssh"
)

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "DEBUG VUCI PACKAGE DEPLOYMENT" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""

# Check if key exists
if (!(Test-Path $SSHKey)) {
    Write-Host "SSH key not found at: $SSHKey" -ForegroundColor Red
    exit 1
}

# Get packages from WSL
Write-Host "Getting packages from WSL..." -ForegroundColor Yellow
$TempDir = "$env:TEMP\vuci-debug-$(Get-Date -Format 'yyyyMMddHHmmss')"
New-Item -ItemType Directory -Path $TempDir -Force | Out-Null

# Copy debug packages (version 1.0-5) from WSL
wsl cp /home/markusla/vuci-packages/*_1.0-5_*.ipk /mnt/c/Users/markusla/AppData/Local/Temp/
$PackagePattern = "vuci-app-example*1.0-5*.ipk"
$SourcePath = "$env:TEMP"
Get-ChildItem -Path $SourcePath -Filter $PackagePattern | ForEach-Object {
    Copy-Item $_.FullName -Destination $TempDir
}

Write-Host "Debug packages copied to: $TempDir" -ForegroundColor Green
Get-ChildItem $TempDir

# Clean up old packages on router
Write-Host ""
Write-Host "Cleaning up old packages on router..." -ForegroundColor Yellow

ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "opkg remove vuci-app-example-ui vuci-app-example-api 2>/dev/null || true"
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "rm -f /tmp/vuci-app-example*.ipk"
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "rm -rf /usr/share/vuci/menu.d/example.json /www/vuci-app-example /www/example.html"
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "rm -f /usr/lib/lua/api/services/example.lua"
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "rm -rf /usr/share/vuci/components/Example.vue"

Write-Host "Cleanup complete" -ForegroundColor Green

# Transfer packages
Write-Host ""
Write-Host "Transferring debug packages to router..." -ForegroundColor Yellow
Get-ChildItem "$TempDir\*.ipk" | ForEach-Object {
    Write-Host "  Copying: $($_.Name)" -ForegroundColor Gray
    scp -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null $_.FullName "root@${RouterIP}:/tmp/"
}

# Install packages with verbose output
Write-Host ""
Write-Host "Installing packages with debug logging..." -ForegroundColor Yellow
Write-Host "=========================================" -ForegroundColor Cyan

Write-Host ""
Write-Host "Installing API package (watch for debug output):" -ForegroundColor Yellow
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "cd /tmp && opkg install -V2 vuci-app-example-api_1.0-5_arm_cortex-a7_neon-vfpv4.ipk 2>&1"

Write-Host ""
Write-Host "Installing UI package (watch for debug output):" -ForegroundColor Yellow
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "cd /tmp && opkg install -V2 vuci-app-example-ui_1.0-5_arm_cortex-a7_neon-vfpv4.ipk 2>&1"

# Additional verification
Write-Host ""
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "POST-INSTALLATION VERIFICATION" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan

Write-Host ""
Write-Host "Checking where opkg installed the files:" -ForegroundColor Yellow
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "echo '=== API Package Files ==='; opkg files vuci-app-example-api; echo ''; echo '=== UI Package Files ==='; opkg files vuci-app-example-ui"

Write-Host ""
Write-Host "Checking filesystem for installed files:" -ForegroundColor Yellow
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "echo '=== Searching for example.json ==='; find / -name 'example.json' 2>/dev/null | grep -v proc | grep -v sys; echo ''; echo '=== Searching for example.lua ==='; find / -name 'example.lua' 2>/dev/null | grep -v proc | grep -v sys; echo ''; echo '=== Searching for vuci-app-example ==='; find / -type d -name 'vuci-app-example' 2>/dev/null | grep -v proc | grep -v sys"

Write-Host ""
Write-Host "Checking opkg installation prefix:" -ForegroundColor Yellow
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "opkg info vuci-app-example-ui | grep -E 'Package|Status|Root'"

# Restart services
Write-Host ""
Write-Host "Restarting web server..." -ForegroundColor Yellow
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "/etc/init.d/uhttpd restart"

# Clean up temp directory
Remove-Item -Path $TempDir -Recurse -Force

# Final message
Write-Host ""
Write-Host "=========================================" -ForegroundColor Green
Write-Host "DEBUG DEPLOYMENT COMPLETE!" -ForegroundColor Green
Write-Host "=========================================" -ForegroundColor Green
Write-Host ""
Write-Host "The debug output above should show:" -ForegroundColor Yellow
Write-Host "  - Where opkg is installing files" -ForegroundColor White
Write-Host "  - Any path or permission issues" -ForegroundColor White
Write-Host "  - The actual vs expected file locations" -ForegroundColor White
Write-Host ""
Write-Host "Test URLs:" -ForegroundColor Yellow
Write-Host "  http://$RouterIP/vuci-app-example/" -ForegroundColor White
Write-Host "  http://$RouterIP/example.html" -ForegroundColor White


