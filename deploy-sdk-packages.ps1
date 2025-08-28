# PowerShell script to deploy SDK-built VUCI packages
# Uses the packages built with proper SDK structure

param(
    [string]$RouterIP = "192.168.80.1",
    [string]$SSHKey = "C:\Users\markusla\OneDrive\IT\RUTOS Keys\rusos_private_key_openssh"
)

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "DEPLOYING SDK-BUILT VUCI PACKAGES" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "SDK Location: /mnt/wsl/SDK" -ForegroundColor Gray
Write-Host "Package Version: 1.0-9" -ForegroundColor Gray
Write-Host ""

# Check if key exists
if (!(Test-Path $SSHKey)) {
    Write-Host "SSH key not found at: $SSHKey" -ForegroundColor Red
    exit 1
}

# Check if packages exist
$ApiPackage = "vuci-app-example-api_1.0-9_arm_cortex-a7_neon-vfpv4.ipk"
$UiPackage = "vuci-app-example-ui_1.0-9_arm_cortex-a7_neon-vfpv4.ipk"

if (!(Test-Path $ApiPackage) -or !(Test-Path $UiPackage)) {
    Write-Host "Packages not found in current directory" -ForegroundColor Red
    Write-Host "Looking for:" -ForegroundColor Yellow
    Write-Host "  - $ApiPackage" -ForegroundColor Gray
    Write-Host "  - $UiPackage" -ForegroundColor Gray
    exit 1
}

Write-Host "Found packages:" -ForegroundColor Green
Get-ChildItem *.ipk | Where-Object { $_.Name -match "example.*1\.0-9" } | ForEach-Object {
    Write-Host "  - $($_.Name) ($([math]::Round($_.Length/1KB, 1)) KB)" -ForegroundColor Gray
}

# Clean up old packages on router
Write-Host ""
Write-Host "Cleaning up old packages on router..." -ForegroundColor Yellow

ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" @"
    opkg remove vuci-app-example-ui vuci-app-example-api 2>/dev/null || true
    rm -f /tmp/vuci-app-example*.ipk
    rm -f /www/cgi-bin/example-api
    rm -rf /usr/share/vuci/menu.d/example.json
    rm -rf /usr/lib/lua/api/services/example.lua
    rm -rf /www/example.html
    rm -rf /tmp/example-api.log
"@

Write-Host "Cleanup complete" -ForegroundColor Green

# Transfer packages
Write-Host ""
Write-Host "Transferring packages to router..." -ForegroundColor Yellow

scp -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null $ApiPackage "root@${RouterIP}:/tmp/"
scp -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null $UiPackage "root@${RouterIP}:/tmp/"

Write-Host "Transfer complete" -ForegroundColor Green

# Install packages
Write-Host ""
Write-Host "Installing packages..." -ForegroundColor Yellow

Write-Host "  Installing API package..." -ForegroundColor Gray
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "cd /tmp && opkg install $ApiPackage"

Write-Host "  Installing UI package..." -ForegroundColor Gray
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "cd /tmp && opkg install $UiPackage"

# Verify installation
Write-Host ""
Write-Host "Verifying installation..." -ForegroundColor Yellow
Write-Host "----------------------------------------" -ForegroundColor Gray

$verification = ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" @"
    echo '=== Installed Packages ==='
    opkg list-installed | grep vuci-app-example
    
    echo ''
    echo '=== File Locations ==='
    echo -n 'API Service: '
    if [ -f /usr/local/usr/lib/lua/api/services/example.lua ]; then
        echo 'Installed ✓'
    else
        echo 'Not found'
    fi
    
    echo -n 'Menu Config: '
    if [ -f /usr/local/usr/share/vuci/menu.d/example.json ]; then
        echo 'Installed ✓'
    else
        echo 'Not found'
    fi
    
    echo -n 'HTML Interface: '
    if [ -f /usr/local/www/example/index.html ]; then
        echo 'Installed ✓'
    else
        echo 'Not found'
    fi
    
    echo -n 'CGI Wrapper: '
    if [ -f /www/cgi-bin/example-api ]; then
        echo 'Created ✓'
    else
        echo 'Not found'
    fi
    
    echo ''
    echo '=== Symlinks ==='
    ls -la /usr/share/vuci/menu.d/example.json 2>/dev/null || echo 'Menu symlink not created'
    ls -la /usr/lib/lua/api/services/example.lua 2>/dev/null || echo 'API symlink not created'
"@

Write-Host $verification

# Restart services
Write-Host ""
Write-Host "Restarting web services..." -ForegroundColor Yellow
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "/etc/init.d/uhttpd restart && /etc/init.d/rpcd restart"

# Final message
Write-Host ""
Write-Host "=========================================" -ForegroundColor Green
Write-Host "DEPLOYMENT COMPLETE!" -ForegroundColor Green
Write-Host "=========================================" -ForegroundColor Green
Write-Host ""
Write-Host "SDK-built packages deployed successfully!" -ForegroundColor Green
Write-Host ""
Write-Host "Test the installation:" -ForegroundColor Yellow
Write-Host ""
Write-Host "1. HTML Interface:" -ForegroundColor Cyan
Write-Host "   http://$RouterIP/example/" -ForegroundColor White
Write-Host "   - Full API testing interface" -ForegroundColor Gray
Write-Host "   - Multiple test buttons" -ForegroundColor Gray
Write-Host "   - Real-time status display" -ForegroundColor Gray
Write-Host ""
Write-Host "2. Direct CGI API:" -ForegroundColor Cyan
Write-Host "   http://$RouterIP/cgi-bin/example-api" -ForegroundColor White
Write-Host "   - Direct API access via CGI" -ForegroundColor Gray
Write-Host ""
Write-Host "3. VUCI Menu (if Vue compiled):" -ForegroundColor Cyan
Write-Host "   Services -> Example" -ForegroundColor White
Write-Host "   - Requires webpack compilation" -ForegroundColor Gray
Write-Host ""
Write-Host "Features:" -ForegroundColor Yellow
Write-Host "  - Built with proper SDK structure" -ForegroundColor White
Write-Host "  - Multiple API endpoints" -ForegroundColor White
Write-Host "  - CGI wrapper for HTTP access" -ForegroundColor White
Write-Host "  - Enhanced testing interface" -ForegroundColor White
Write-Host "  - Automatic route checking" -ForegroundColor White
