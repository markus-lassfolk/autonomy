# PowerShell script to build and deploy standalone VUCI package
# Uses SSH key authentication

$DeviceIP = "192.168.80.1"
$DeviceUser = "root"
$SSHKey = "C:\Users\markusla\OneDrive\IT\RUTOS Keys\rusos_private_key_openssh"

Write-Host "=== Building and Deploying Standalone VUCI Package ===" -ForegroundColor Green

# Step 1: Build the standalone package using WSL
Write-Host "Step 1: Building standalone package..." -ForegroundColor Yellow
wsl bash build-standalone-package.sh

if ($LASTEXITCODE -eq 0) {
    Write-Host "Package built successfully!" -ForegroundColor Green
} else {
    Write-Host "Failed to build package" -ForegroundColor Red
    exit 1
}

# Step 2: Find the built package
Write-Host "Step 2: Finding built package..." -ForegroundColor Yellow
$packageName = "example_package_1.0-1_arm_cortex-a7_neon-vfpv4.ipk"

if (Test-Path $packageName) {
    Write-Host "Found package: $packageName" -ForegroundColor Green
    
    # Step 3: Transfer package to device
    Write-Host "Step 3: Transferring package to device..." -ForegroundColor Yellow
    $scpCommand = "scp -i `"$SSHKey`" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null `"$packageName`" ${DeviceUser}@${DeviceIP}:/tmp/"
    Invoke-Expression $scpCommand
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Package transferred successfully!" -ForegroundColor Green
    } else {
        Write-Host "Failed to transfer package" -ForegroundColor Red
        exit 1
    }
    
    # Step 4: Install package on device
    Write-Host "Step 4: Installing package on device..." -ForegroundColor Yellow
    $sshCommand = "ssh -i `"$SSHKey`" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ${DeviceUser}@${DeviceIP} 'opkg install /tmp/$packageName'"
    Invoke-Expression $sshCommand
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Package installed successfully!" -ForegroundColor Green
    } else {
        Write-Host "Failed to install package" -ForegroundColor Red
        exit 1
    }
    
    # Step 5: Restart services
    Write-Host "Step 5: Restarting services..." -ForegroundColor Yellow
    $restartCommand = "ssh -i `"$SSHKey`" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ${DeviceUser}@${DeviceIP} '/etc/init.d/rpcd restart && /etc/init.d/uhttpd restart'"
    Invoke-Expression $restartCommand
    
    # Step 6: Test the installation
    Write-Host "Step 6: Testing installation..." -ForegroundColor Yellow
    $testCommand = "ssh -i `"$SSHKey`" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ${DeviceUser}@${DeviceIP} 'ubus list | grep example'"
    Invoke-Expression $testCommand
    
    Write-Host ""
    Write-Host "=== DEPLOYMENT COMPLETE ===" -ForegroundColor Green
    Write-Host "Standalone VUCI package has been deployed!" -ForegroundColor Green
    Write-Host ""
    Write-Host "You can now:" -ForegroundColor Cyan
    Write-Host "1. Access the UI at: http://$DeviceIP/example/" -ForegroundColor White
    Write-Host "2. Test the API: ssh -i `"$SSHKey`" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ${DeviceUser}@${DeviceIP} 'ubus call example status'" -ForegroundColor White
    Write-Host "3. The app should now appear in the Web Package Manager" -ForegroundColor White
    Write-Host "4. The 'services/Example' error should be resolved" -ForegroundColor White
    
} else {
    Write-Host "Failed to find built package: $packageName" -ForegroundColor Red
    exit 1
}




