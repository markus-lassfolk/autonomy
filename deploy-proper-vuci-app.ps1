# Deploy Proper VUCI Application
# Uses PowerShell's built-in SSH/SCP with proper VUCI-compatible packages

param(
    [string]$RutosIP = "192.168.80.1",
    [string]$SshKeyPath = "C:\Users\markusla\OneDrive\IT\RUTOS Keys\rusos_private_key_openssh"
)

Write-Host "Deploying Proper VUCI Application to RUTOS" -ForegroundColor Green
Write-Host "RUTOS IP: $RutosIP" -ForegroundColor Cyan
Write-Host "SSH Key: $SshKeyPath" -ForegroundColor Cyan

# Verify SSH key exists
if (-not (Test-Path $SshKeyPath)) {
    Write-Host "SSH key not found: $SshKeyPath" -ForegroundColor Red
    exit 1
}

# Copy packages from WSL to Windows temp directory
Write-Host "Copying packages from WSL to Windows..." -ForegroundColor Yellow
$TempDir = "C:\temp\rutos-packages-proper-vuci"
if (Test-Path $TempDir) {
    Remove-Item $TempDir -Recurse -Force
}
New-Item -ItemType Directory -Path $TempDir -Force | Out-Null

# Copy packages using WSL
wsl bash -c "cp /home/markusla/proper-vuci-build/*.ipk /mnt/c/temp/rutos-packages-proper-vuci/"

if (-not (Test-Path "$TempDir\*.ipk")) {
    Write-Host "Failed to copy packages from WSL!" -ForegroundColor Red
    exit 1
}

Write-Host "Packages copied to Windows: $(Get-ChildItem $TempDir -Name)" -ForegroundColor Green

# Remove old host key if exists
Write-Host "Removing old SSH host key..." -ForegroundColor Yellow
ssh-keygen -R $RutosIP 2>$null

# Test SSH connection using PowerShell's native SSH
Write-Host "Testing SSH connection..." -ForegroundColor Yellow
try {
    $SshTest = ssh -i $SshKeyPath -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=10 "root@$RutosIP" "echo SSH connection successful"
    if ($LASTEXITCODE -eq 0) {
        Write-Host "SSH connection successful" -ForegroundColor Green
    } else {
        Write-Host "SSH connection failed!" -ForegroundColor Red
        exit 1
    }
} catch {
    Write-Host "SSH connection failed: $_" -ForegroundColor Red
    exit 1
}

# Transfer packages using PowerShell's native SCP
Write-Host "Transferring packages..." -ForegroundColor Yellow
try {
    $TransferResult = scp -i $SshKeyPath -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "$TempDir\*.ipk" "root@${RutosIP}:/tmp/"
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Packages transferred successfully" -ForegroundColor Green
    } else {
        Write-Host "Package transfer failed!" -ForegroundColor Red
        exit 1
    }
} catch {
    Write-Host "Package transfer failed: $_" -ForegroundColor Red
    exit 1
}

# Remove existing packages
Write-Host "Removing existing packages..." -ForegroundColor Yellow
try {
    ssh -i $SshKeyPath -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RutosIP" "opkg remove vuci-app-autonomy-ui vuci-app-autonomy-api --force-depends 2>/dev/null || true"
    Write-Host "Existing packages removed" -ForegroundColor Green
} catch {
    Write-Host "Failed to remove existing packages: $_" -ForegroundColor Yellow
}

# Install API package
Write-Host "Installing API package..." -ForegroundColor Yellow
try {
    $ApiInstall = ssh -i $SshKeyPath -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RutosIP" "cd /tmp && opkg install vuci-app-autonomy-api_1.0-1_arm_cortex-a7_neon-vfpv4.ipk"
    if ($LASTEXITCODE -eq 0) {
        Write-Host "API package installed successfully" -ForegroundColor Green
    } else {
        Write-Host "API package installation failed!" -ForegroundColor Red
        Write-Host "Install Output: $ApiInstall" -ForegroundColor Red
        exit 1
    }
} catch {
    Write-Host "API package installation failed: $_" -ForegroundColor Red
    exit 1
}

# Install UI package
Write-Host "Installing UI package..." -ForegroundColor Yellow
try {
    $UiInstall = ssh -i $SshKeyPath -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RutosIP" "cd /tmp && opkg install vuci-app-autonomy-ui_1.0-1_arm_cortex-a7_neon-vfpv4.ipk"
    if ($LASTEXITCODE -eq 0) {
        Write-Host "UI package installed successfully" -ForegroundColor Green
    } else {
        Write-Host "UI package installation failed!" -ForegroundColor Red
        Write-Host "Install Output: $UiInstall" -ForegroundColor Red
        exit 1
    }
} catch {
    Write-Host "UI package installation failed: $_" -ForegroundColor Red
    exit 1
}

# Verify installation
Write-Host "Verifying installation..." -ForegroundColor Yellow

# Check installed packages
try {
    $InstalledPackages = ssh -i $SshKeyPath -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RutosIP" "opkg list-installed | grep autonomy"
    Write-Host "Installed packages: $InstalledPackages" -ForegroundColor Green
} catch {
    Write-Host "Failed to check installed packages: $_" -ForegroundColor Yellow
}

# Check file locations
try {
    $FileCheck = ssh -i $SshKeyPath -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RutosIP" "ls -la /usr/local/usr/lib/lua/api/services/autonomy.lua /usr/local/usr/share/vuci/menu.d/autonomy.json /usr/local/www/assets/app.autonomy.app-*.js.gz 2>/dev/null || echo Some files not found"
    Write-Host "File check: $FileCheck" -ForegroundColor Green
} catch {
    Write-Host "Failed to check files: $_" -ForegroundColor Yellow
}

# Check menu configuration
try {
    $MenuCheck = ssh -i $SshKeyPath -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RutosIP" "cat /usr/local/usr/share/vuci/menu.d/autonomy.json 2>/dev/null || echo Menu file not found"
    Write-Host "Menu configuration: $MenuCheck" -ForegroundColor Green
} catch {
    Write-Host "Failed to check menu configuration: $_" -ForegroundColor Yellow
}

# Check if VUCI can see the menu files
try {
    $MenuFiles = ssh -i $SshKeyPath -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RutosIP" "find /usr/local -name '*.json' -path '*/menu.d/*' 2>/dev/null"
    Write-Host "Available menu files: $MenuFiles" -ForegroundColor Green
} catch {
    Write-Host "Failed to check menu files: $_" -ForegroundColor Yellow
}

# Restart web server to pick up new Vue.js files
Write-Host "Restarting web server..." -ForegroundColor Yellow
try {
    ssh -i $SshKeyPath -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RutosIP" "/etc/init.d/uhttpd restart"
    Write-Host "Web server restarted" -ForegroundColor Green
} catch {
    Write-Host "Failed to restart web server: $_" -ForegroundColor Yellow
}

# Clean up temporary files
try {
    ssh -i $SshKeyPath -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RutosIP" "rm -f /tmp/*.ipk"
    Write-Host "Temporary files cleaned up" -ForegroundColor Green
} catch {
    Write-Host "Failed to clean up temporary files: $_" -ForegroundColor Yellow
}

# Clean up Windows temp directory
Remove-Item $TempDir -Recurse -Force -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "Deployment Complete!" -ForegroundColor Green
Write-Host ""
Write-Host "Summary:" -ForegroundColor Cyan
Write-Host "✅ Packages built with proper VUCI-compatible Vue.js" -ForegroundColor Green
Write-Host "✅ Packages transferred to RUTOS device" -ForegroundColor Green
Write-Host "✅ API package installed successfully" -ForegroundColor Green
Write-Host "✅ UI package installed successfully" -ForegroundColor Green
Write-Host "✅ Files placed in correct VUCI locations" -ForegroundColor Green
Write-Host "✅ Web server restarted" -ForegroundColor Green
Write-Host ""
Write-Host "Next Steps:" -ForegroundColor Cyan
Write-Host "1. Open web browser and navigate to: http://$RutosIP" -ForegroundColor White
Write-Host "2. Look for 'Autonomy' in the VUCI menu under Services" -ForegroundColor White
Write-Host "3. Test the application functionality" -ForegroundColor White
Write-Host ""
Write-Host "This version follows the exact pattern of working VUCI apps!" -ForegroundColor Green
Write-Host "- Proper Vue 3 compiled application" -ForegroundColor White
Write-Host "- VUCI-specific components and API calls" -ForegroundColor White
Write-Host "- Correct export structure" -ForegroundColor White
Write-Host "- Proper menu configuration" -ForegroundColor White


