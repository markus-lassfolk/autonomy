# PowerShell script to create and deploy proper VUCI package
# Uses SSH key authentication

$DeviceIP = "192.168.80.1"
$DeviceUser = "root"
$SSHKey = "C:\Users\markusla\OneDrive\IT\RUTOS Keys\rusos_private_key_openssh"

Write-Host "=== Creating and Deploying Proper VUCI Package ===" -ForegroundColor Green

# Step 1: Create the package using WSL
Write-Host "Step 1: Creating package..." -ForegroundColor Yellow
wsl bash create-proper-package.sh

if ($LASTEXITCODE -eq 0) {
    Write-Host "Package created successfully!" -ForegroundColor Green
} else {
    Write-Host "Failed to create package" -ForegroundColor Red
    exit 1
}

# Step 2: Build the package
Write-Host "Step 2: Building package..." -ForegroundColor Yellow
wsl bash -c "cd /home/markusla/rutos-sdk && make package/example_package/clean && make package/example_package/compile"

if ($LASTEXITCODE -eq 0) {
    Write-Host "Package built successfully!" -ForegroundColor Green
} else {
    Write-Host "Failed to build package" -ForegroundColor Red
    exit 1
}

# Step 3: Find the built package
Write-Host "Step 3: Finding built package..." -ForegroundColor Yellow
$packagePath = wsl bash -c "find /home/markusla/rutos-sdk/bin -name 'example_package_*.ipk' | head -1"
$packageName = Split-Path $packagePath -Leaf

if ($packagePath) {
    Write-Host "Found package: $packageName" -ForegroundColor Green
    
    # Copy package to current directory
    wsl bash -c "cp '$packagePath' /mnt/j/GithubCursor/autonomy/"
    
    # Step 4: Transfer package to device
    Write-Host "Step 4: Transferring package to device..." -ForegroundColor Yellow
    $scpCommand = "scp -i `"$SSHKey`" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null `"$packageName`" ${DeviceUser}@${DeviceIP}:/tmp/"
    Invoke-Expression $scpCommand
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Package transferred successfully!" -ForegroundColor Green
    } else {
        Write-Host "Failed to transfer package" -ForegroundColor Red
        exit 1
    }
    
    # Step 5: Install package on device
    Write-Host "Step 5: Installing package on device..." -ForegroundColor Yellow
    $sshCommand = "ssh -i `"$SSHKey`" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ${DeviceUser}@${DeviceIP} 'opkg install /tmp/$packageName'"
    Invoke-Expression $sshCommand
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Package installed successfully!" -ForegroundColor Green
    } else {
        Write-Host "Failed to install package" -ForegroundColor Red
        exit 1
    }
    
    # Step 6: Restart services
    Write-Host "Step 6: Restarting services..." -ForegroundColor Yellow
    $restartCommand = "ssh -i `"$SSHKey`" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ${DeviceUser}@${DeviceIP} '/etc/init.d/rpcd restart && /etc/init.d/uhttpd restart'"
    Invoke-Expression $restartCommand
    
    # Step 7: Test the installation
    Write-Host "Step 7: Testing installation..." -ForegroundColor Yellow
    $testCommand = "ssh -i `"$SSHKey`" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ${DeviceUser}@${DeviceIP} 'ubus list | grep example'"
    Invoke-Expression $testCommand
    
    Write-Host ""
    Write-Host "=== DEPLOYMENT COMPLETE ===" -ForegroundColor Green
    Write-Host "Proper VUCI package has been deployed!" -ForegroundColor Green
    Write-Host ""
    Write-Host "You can now:" -ForegroundColor Cyan
    Write-Host "1. Access the UI at: http://$DeviceIP/example/" -ForegroundColor White
    Write-Host "2. Test the API: ssh -i `"$SSHKey`" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ${DeviceUser}@${DeviceIP} 'ubus call example status'" -ForegroundColor White
    Write-Host "3. The app should now appear in the Web Package Manager" -ForegroundColor White
    Write-Host "4. The 'services/Example' error should be resolved" -ForegroundColor White
    
} else {
    Write-Host "Failed to find built package" -ForegroundColor Red
    exit 1
}
