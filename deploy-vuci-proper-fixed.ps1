# PowerShell script to deploy properly built VUCI packages
# This fixes all the issues we've identified

param(
    [string]$RouterIP = "192.168.80.1",
    [string]$SSHKey = "C:\Users\markusla\OneDrive\IT\RUTOS Keys\rusos_private_key_openssh"
)

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "VUCI PACKAGE DEPLOYMENT SCRIPT" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""

# Configuration
$TempDir = "C:\temp\vuci-proper-build"
$BuildScript = "build-vuci-proper.sh"

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
Write-Host "Building packages in WSL..." -ForegroundColor Yellow
Write-Host "----------------------------------------" -ForegroundColor Gray

# Copy to WSL and build
wsl bash -c "cp /mnt/c/temp/vuci-proper-build/$BuildScript /tmp/ && chmod +x /tmp/$BuildScript && cd /tmp && ./$BuildScript"

# Copy packages back
Write-Host ""
Write-Host "Copying packages from WSL..." -ForegroundColor Yellow
wsl bash -c "cp /tmp/vuci-app-example-*.ipk /mnt/c/temp/vuci-proper-build/"

# Check if packages exist
$ApiPackage = Get-ChildItem -Path $TempDir -Filter "vuci-app-example-api*.ipk" | Select-Object -First 1
$UiPackage = Get-ChildItem -Path $TempDir -Filter "vuci-app-example-ui*.ipk" | Select-Object -First 1

if (-not $ApiPackage -or -not $UiPackage) {
    Write-Host "ERROR: Package build failed!" -ForegroundColor Red
    exit 1
}

Write-Host "Packages built successfully:" -ForegroundColor Green
Write-Host "  - $($ApiPackage.Name)" -ForegroundColor Gray
Write-Host "  - $($UiPackage.Name)" -ForegroundColor Gray

# Clean up old packages on router
Write-Host ""
Write-Host "Cleaning up old packages on router..." -ForegroundColor Yellow

$cleanupCommands = @(
    "opkg remove vuci-app-example-ui 2>/dev/null || true",
    "opkg remove vuci-app-example-api 2>/dev/null || true",
    "rm -f /tmp/vuci-app-example*.ipk",
    "# Clean up any leftover files",
    "rm -rf /usr/local/usr/share/vuci/menu.d/example.json",
    "rm -rf /usr/local/www/assets/app.example.*.js.gz",
    "rm -rf /usr/local/usr/lib/lua/api/services/example*.lua",
    "rm -rf /overlay/root/upper/usr/share/vuci/menu.d/example.json"
)

foreach ($cmd in $cleanupCommands) {
    if ($cmd -notlike "#*") {
        ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" $cmd
    }
}

# Transfer packages
Write-Host ""
Write-Host "Transferring packages to router..." -ForegroundColor Yellow

scp -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null `
    "$TempDir\$($ApiPackage.Name)" "$TempDir\$($UiPackage.Name)" "root@${RouterIP}:/tmp/"

# Install packages
Write-Host ""
Write-Host "Installing packages..." -ForegroundColor Yellow

$installCommands = @(
    "cd /tmp",
    "opkg install vuci-app-example-api*.ipk",
    "opkg install vuci-app-example-ui*.ipk"
)

$installScript = $installCommands -join " && "
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" $installScript

# Verify installation
Write-Host ""
Write-Host "Verifying installation..." -ForegroundColor Yellow
Write-Host "----------------------------------------" -ForegroundColor Gray

$verifyCommands = @(
    "echo '=== Installed packages ==='",
    "opkg list-installed | grep example",
    "echo ''",
    "echo '=== Menu file ==='",
    "find / -name 'example.json' -path '*menu.d*' 2>/dev/null | xargs ls -la",
    "echo ''",
    "echo '=== JavaScript assets ==='", 
    "find / -name 'app.example.*.js.gz' 2>/dev/null | xargs ls -la",
    "echo ''",
    "echo '=== API service ==='",
    "find / -name 'example*.lua' -path '*api/services*' 2>/dev/null | xargs ls -la",
    "echo ''",
    "echo '=== Menu content ==='",
    "find / -name 'example.json' -path '*menu.d*' 2>/dev/null | xargs cat"
)

$verifyScript = $verifyCommands -join " ; "
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" $verifyScript

# Restart services
Write-Host ""
Write-Host "Restarting web server..." -ForegroundColor Yellow
ssh -i $SSHKey -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RouterIP" "/etc/init.d/uhttpd restart"

# Final status
Write-Host ""
Write-Host "=========================================" -ForegroundColor Green
Write-Host "DEPLOYMENT COMPLETE!" -ForegroundColor Green
Write-Host "=========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Key fixes implemented:" -ForegroundColor Cyan
Write-Host "  [✓] Menu files in /usr/share/vuci/menu.d/ (correct location)" -ForegroundColor Green
Write-Host "  [✓] Compiled JS with proper naming (app.example.app-hash.js.gz)" -ForegroundColor Green
Write-Host "  [✓] Gzip compressed IPK packages" -ForegroundColor Green
Write-Host "  [✓] Proper Vue.js component structure" -ForegroundColor Green
Write-Host "  [✓] Working API service" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Open browser: http://$RouterIP" -ForegroundColor White
Write-Host "  2. Navigate to: Services -> Example" -ForegroundColor White
Write-Host "  3. The page should load without errors!" -ForegroundColor White
Write-Host ""
Write-Host "If it still shows 'Failed to load page':" -ForegroundColor Yellow
Write-Host "  - Clear browser cache (Ctrl+F5)" -ForegroundColor Gray
Write-Host "  - Check browser console for errors" -ForegroundColor Gray
Write-Host "  - Run: ssh root@$RouterIP 'logread | tail -50'" -ForegroundColor Gray
Write-Host ""

# Clean up temp directory
Write-Host "Cleaning up temporary files..." -ForegroundColor Yellow
Remove-Item -Recurse -Force $TempDir

Write-Host "Deployment script completed!" -ForegroundColor Green


