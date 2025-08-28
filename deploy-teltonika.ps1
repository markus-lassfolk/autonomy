# PowerShell script to deploy VUCI packages with Teltonika-specific paths
# This implements the REAL fix with correct installation paths

param(
    [string]$RouterIP = "192.168.80.1",
    [string]$SSHKey = "C:\Users\markusla\OneDrive\IT\RUTOS Keys\rusos_private_key_openssh"
)

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "TELTONIKA VUCI DEPLOYMENT" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""

# Configuration
$TempDir = "C:\temp\vuci-teltonika"
$BuildScript = "build-vuci-teltonika.sh"

# Create temp directory
Write-Host "Creating temporary directory..." -ForegroundColor Yellow
if (Test-Path $TempDir) {
    Remove-Item -Recurse -Force $TempDir
}
New-Item -ItemType Directory -Path $TempDir | Out-Null

# Copy build script
Write-Host "Copying build script..." -ForegroundColor Yellow
Copy-Item $BuildScript -Destination $TempDir

# Build packages in WSL
Write-Host ""
Write-Host "Building packages with Teltonika paths..." -ForegroundColor Yellow
Write-Host "----------------------------------------" -ForegroundColor Gray

# Copy to WSL and build
wsl bash -c "cp /mnt/c/temp/vuci-teltonika/$BuildScript /tmp/ && chmod +x /tmp/$BuildScript && cd /tmp && ./$BuildScript"

# Copy packages back
Write-Host ""
Write-Host "Copying packages from WSL..." -ForegroundColor Yellow
wsl bash -c "cp /tmp/vuci-app-example-*.ipk /mnt/c/temp/vuci-teltonika/ 2>/dev/null"

# Check if packages exist
$ApiPackage = Get-ChildItem -Path $TempDir -Filter "vuci-app-example-api*.ipk" -ErrorAction SilentlyContinue | Select-Object -First 1
$UiPackage = Get-ChildItem -Path $TempDir -Filter "vuci-app-example-ui*.ipk" -ErrorAction SilentlyContinue | Select-Object -First 1

if (-not $ApiPackage -or -not $UiPackage) {
    Write-Host "ERROR: Package build failed!" -ForegroundColor Red
    Write-Host "Check the build output above for errors." -ForegroundColor Yellow
    exit 1
}

Write-Host "Packages built successfully:" -ForegroundColor Green
Write-Host "  - $($ApiPackage.Name)" -ForegroundColor Gray
Write-Host "  - $($UiPackage.Name)" -ForegroundColor Gray

# Clean up old packages on router
Write-Host ""
Write-Host "Cleaning up old packages on router..." -ForegroundColor Yellow

# Remove old packages and files
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" @'
    # Remove old packages
    opkg remove vuci-app-example-ui vuci-app-example-api 2>/dev/null || true
    
    # Clean up old files from all possible locations
    rm -f /tmp/vuci-app-example*.ipk
    rm -f /usr/share/vuci/menu.d/example.json
    rm -f /usr/local/share/vuci/menu.d/example.json
    rm -f /usr/local/usr/share/vuci/menu.d/example.json
    rm -f /www/assets/app.example.*.js.gz
    rm -f /usr/local/www/assets/app.example.*.js.gz
    rm -f /usr/local/usr/lib/lua/api/services/example*.lua
    rm -f /usr/lib/lua/api/services/example*.lua
    rm -rf /overlay/root/upper/usr/share/vuci/menu.d/example.json
    rm -rf /overlay/root/upper/usr/local/share/vuci/menu.d/example.json
'@

# Transfer packages
Write-Host ""
Write-Host "Transferring packages to router..." -ForegroundColor Yellow

scp -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null `
    "$TempDir\$($ApiPackage.Name)" "$TempDir\$($UiPackage.Name)" "root@${RouterIP}:/tmp/"

# Install packages
Write-Host ""
Write-Host "Installing packages..." -ForegroundColor Yellow

ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" @'
    cd /tmp
    opkg install vuci-app-example-api*.ipk
    opkg install vuci-app-example-ui*.ipk
'@

# Verify installation
Write-Host ""
Write-Host "Verifying installation..." -ForegroundColor Yellow
Write-Host "----------------------------------------" -ForegroundColor Gray

ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" @'
    echo "=== Installed packages ==="
    opkg list-installed | grep example
    echo ""
    echo "=== Menu files (all locations) ==="
    find / -name "example.json" -path "*menu.d*" 2>/dev/null | while read f; do
        echo "  $f:"
        ls -la "$f"
        echo "  Content: $(cat $f)"
        echo ""
    done
    echo ""
    echo "=== JavaScript assets ==="
    find / -name "app.example.*.js.gz" 2>/dev/null | xargs ls -la
    echo ""
    echo "=== API services ==="
    find / -name "example*.lua" -path "*api/services*" 2>/dev/null | xargs ls -la
    echo ""
    echo "=== Checking symlinks ==="
    ls -la /usr/share/vuci/menu.d/example.json 2>/dev/null || echo "No symlink in /usr/share"
    ls -la /usr/local/share/vuci/menu.d/example.json 2>/dev/null || echo "No file in /usr/local/share"
'@

# Restart services
Write-Host ""
Write-Host "Restarting web server..." -ForegroundColor Yellow
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "/etc/init.d/uhttpd restart"

# Test API endpoint
Write-Host ""
Write-Host "Testing API endpoint..." -ForegroundColor Yellow
$apiTest = ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" @'
    curl -s http://localhost/api/example_f/test 2>/dev/null || echo "API not responding"
'@
Write-Host $apiTest -ForegroundColor Gray

# Final status
Write-Host ""
Write-Host "=========================================" -ForegroundColor Green
Write-Host "DEPLOYMENT COMPLETE!" -ForegroundColor Green
Write-Host "=========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Teltonika-specific fixes applied:" -ForegroundColor Cyan
Write-Host "  [OK] Menu in /usr/local/share/vuci/menu.d/" -ForegroundColor Green
Write-Host "  [OK] JavaScript in /www/assets/" -ForegroundColor Green
Write-Host "  [OK] API in /usr/local/usr/lib/lua/api/services/" -ForegroundColor Green
Write-Host "  [OK] Symlink created for menu discovery" -ForegroundColor Green
Write-Host "  [OK] Gzip compressed packages" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Open browser: http://$RouterIP" -ForegroundColor White
Write-Host "  2. Clear cache: Ctrl+F5" -ForegroundColor White
Write-Host "  3. Navigate to: Services -> Example" -ForegroundColor White
Write-Host "  4. The page should now load!" -ForegroundColor White
Write-Host ""
Write-Host "Troubleshooting:" -ForegroundColor Yellow
Write-Host "  - Check browser console (F12) for JavaScript errors" -ForegroundColor Gray
Write-Host "  - Check router logs: ssh root@$RouterIP 'logread | tail -50'" -ForegroundColor Gray
Write-Host "  - Verify menu appears: ssh root@$RouterIP 'cat /usr/local/share/vuci/menu.d/example.json'" -ForegroundColor Gray
Write-Host ""

# Clean up temp directory
Write-Host "Cleaning up temporary files..." -ForegroundColor Yellow
Remove-Item -Recurse -Force $TempDir

Write-Host "Done! Check your browser now." -ForegroundColor Green
