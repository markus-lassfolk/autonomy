# PowerShell script to build and deploy proper VUCI example package
# Uses SSH key authentication and SDK build system

$DeviceIP = "192.168.80.1"
$DeviceUser = "root"
$SSHKey = "C:\Users\markusla\OneDrive\IT\RUTOS Keys\rusos_private_key_openssh"

Write-Host "=== Building and Deploying Proper VUCI Example Package ===" -ForegroundColor Green

# Step 1: Create the packages using WSL
Write-Host "Step 1: Creating packages..." -ForegroundColor Yellow
wsl bash create-proper-example-package.sh

if ($LASTEXITCODE -eq 0) {
    Write-Host "Packages created successfully!" -ForegroundColor Green
} else {
    Write-Host "Failed to create packages" -ForegroundColor Red
    exit 1
}

# Step 2: Build the API package
Write-Host "Step 2: Building API package..." -ForegroundColor Yellow
wsl bash -c "cd /home/markusla/rutos-sdk && make package/vuci-app-example-api/clean && make package/vuci-app-example-api/compile"

if ($LASTEXITCODE -eq 0) {
    Write-Host "API package built successfully!" -ForegroundColor Green
} else {
    Write-Host "Failed to build API package" -ForegroundColor Red
    exit 1
}

# Step 3: Build the UI package
Write-Host "Step 3: Building UI package..." -ForegroundColor Yellow
wsl bash -c "cd /home/markusla/rutos-sdk && make package/vuci-app-example-ui/clean && make package/vuci-app-example-ui/compile"

if ($LASTEXITCODE -eq 0) {
    Write-Host "UI package built successfully!" -ForegroundColor Green
} else {
    Write-Host "Failed to build UI package" -ForegroundColor Red
    exit 1
}

# Step 4: Find the built packages
Write-Host "Step 4: Finding built packages..." -ForegroundColor Yellow
$apiPackage = wsl bash -c "find /home/markusla/rutos-sdk/bin -name 'vuci-app-example-api_*.ipk' | head -1"
$uiPackage = wsl bash -c "find /home/markusla/rutos-sdk/bin -name 'vuci-app-example-ui_*.ipk' | head -1"

if ($apiPackage -and $uiPackage) {
    $apiPackageName = Split-Path $apiPackage -Leaf
    $uiPackageName = Split-Path $uiPackage -Leaf
    
    Write-Host "Found API package: $apiPackageName" -ForegroundColor Green
    Write-Host "Found UI package: $uiPackageName" -ForegroundColor Green
    
    # Copy packages to current directory
    wsl bash -c "cp '$apiPackage' /mnt/j/GithubCursor/autonomy/"
    wsl bash -c "cp '$uiPackage' /mnt/j/GithubCursor/autonomy/"
    
    # Step 5: Transfer packages to device
    Write-Host "Step 5: Transferring packages to device..." -ForegroundColor Yellow
    $scpCommand1 = "scp -i `"$SSHKey`" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null `"$apiPackageName`" ${DeviceUser}@${DeviceIP}:/tmp/"
    $scpCommand2 = "scp -i `"$SSHKey`" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null `"$uiPackageName`" ${DeviceUser}@${DeviceIP}:/tmp/"
    
    Invoke-Expression $scpCommand1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "API package transferred successfully!" -ForegroundColor Green
    } else {
        Write-Host "Failed to transfer API package" -ForegroundColor Red
        exit 1
    }
    
    Invoke-Expression $scpCommand2
    if ($LASTEXITCODE -eq 0) {
        Write-Host "UI package transferred successfully!" -ForegroundColor Green
    } else {
        Write-Host "Failed to transfer UI package" -ForegroundColor Red
        exit 1
    }
    
    # Step 6: Install packages on device
    Write-Host "Step 6: Installing packages on device..." -ForegroundColor Yellow
    $sshCommand1 = "ssh -i `"$SSHKey`" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ${DeviceUser}@${DeviceIP} 'opkg install /tmp/$apiPackageName'"
    $sshCommand2 = "ssh -i `"$SSHKey`" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ${DeviceUser}@${DeviceIP} 'opkg install /tmp/$uiPackageName'"
    
    Invoke-Expression $sshCommand1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "API package installed successfully!" -ForegroundColor Green
    } else {
        Write-Host "Failed to install API package" -ForegroundColor Red
        exit 1
    }
    
    Invoke-Expression $sshCommand2
    if ($LASTEXITCODE -eq 0) {
        Write-Host "UI package installed successfully!" -ForegroundColor Green
    } else {
        Write-Host "Failed to install UI package" -ForegroundColor Red
        exit 1
    }
    
    # Step 7: Restart services
    Write-Host "Step 7: Restarting services..." -ForegroundColor Yellow
    $restartCommand = "ssh -i `"$SSHKey`" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ${DeviceUser}@${DeviceIP} '/etc/init.d/rpcd restart && /etc/init.d/uhttpd restart'"
    Invoke-Expression $restartCommand
    
    # Step 8: Test the installation
    Write-Host "Step 8: Testing installation..." -ForegroundColor Yellow
    $testCommand = "ssh -i `"$SSHKey`" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ${DeviceUser}@${DeviceIP} 'curl -s -k https://localhost/api/example/status'"
    Invoke-Expression $testCommand
    
    Write-Host ""
    Write-Host "=== DEPLOYMENT COMPLETE ===" -ForegroundColor Green
    Write-Host "Proper VUCI example package has been deployed!" -ForegroundColor Green
    Write-Host ""
    Write-Host "You can now:" -ForegroundColor Cyan
    Write-Host "1. Access the UI at: https://$DeviceIP/services/example" -ForegroundColor White
    Write-Host "2. Test the API: ssh -i `"$SSHKey`" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ${DeviceUser}@${DeviceIP} 'curl -k https://localhost/api/example/status'" -ForegroundColor White
    Write-Host "3. The app should now appear in the Web Package Manager" -ForegroundColor White
    Write-Host "4. The 'services/Example' error should be resolved" -ForegroundColor White
    
} else {
    Write-Host "Failed to find built packages" -ForegroundColor Red
    exit 1
}




