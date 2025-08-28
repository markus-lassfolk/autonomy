# PowerShell script to deploy manually built VUCI packages
# Uses native PowerShell SSH/SCP with the authentication key

param(
    [string]$RouterIP = "192.168.80.1",
    [string]$SSHKey = "C:\Users\markusla\OneDrive\IT\RUTOS Keys\rusos_private_key_openssh"
)

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "VUCI PACKAGE DEPLOYMENT" -ForegroundColor Cyan
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

ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" @'
    # Remove old packages
    opkg remove vuci-app-example-ui vuci-app-example-api 2>/dev/null || true
    
    # Clean up old files
    rm -f /tmp/vuci-app-example*.ipk
    rm -f /usr/share/vuci/menu.d/example.json
    rm -f /usr/local/share/vuci/menu.d/example.json
    rm -f /www/vuci-app-example/index.html
    rm -f /www/example.html
    rm -rf /www/vuci-app-example
    rm -f /usr/lib/lua/api/services/example.lua
    rm -f /usr/local/usr/lib/lua/api/services/example.lua
    
    # Clear cache
    rm -rf /tmp/luci-* /tmp/vuci-* 2>/dev/null || true
    
    echo "Cleanup complete"
'@

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

ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" @'
    cd /tmp
    
    # Install API package first
    echo "Installing API package..."
    opkg install vuci-app-example-api_*.ipk
    
    # Install UI package
    echo "Installing UI package..."
    opkg install vuci-app-example-ui_*.ipk
    
    echo ""
    echo "Installation complete"
'@

# Verify installation
Write-Host ""
Write-Host "Verifying installation..." -ForegroundColor Yellow
Write-Host "----------------------------------------" -ForegroundColor Gray

ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" @'
    echo "=== Installed packages ==="
    opkg list-installed | grep vuci-app-example
    
    echo ""
    echo "=== Menu file ==="
    if [ -f /usr/share/vuci/menu.d/example.json ]; then
        echo "Location: /usr/share/vuci/menu.d/example.json"
        cat /usr/share/vuci/menu.d/example.json
    else
        echo "Menu file not found in /usr/share/vuci/menu.d/"
    fi
    
    echo ""
    echo "=== API service ==="
    if [ -f /usr/lib/lua/api/services/example.lua ]; then
        echo "API service installed at: /usr/lib/lua/api/services/example.lua"
    else
        echo "API service not found"
    fi
    
    echo ""
    echo "=== HTML fallback ==="
    if [ -f /www/vuci-app-example/index.html ]; then
        echo "HTML page installed at: /www/vuci-app-example/index.html"
        echo "Accessible at: http://192.168.80.1/vuci-app-example/"
    fi
    
    if [ -L /www/example.html ]; then
        echo "Symlink created at: /www/example.html"
    fi
'@

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
Write-Host "2. HTML Fallback: http://$RouterIP/vuci-app-example/" -ForegroundColor White
Write-Host "3. Direct HTML: http://$RouterIP/example.html" -ForegroundColor White
Write-Host ""
Write-Host "The HTML fallback provides a working test interface" -ForegroundColor Cyan
Write-Host "even without webpack compilation of the Vue component." -ForegroundColor Cyan


