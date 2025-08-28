# PowerShell script to deploy VUCI package using overlay installation
# Uses SSH key authentication

$DeviceIP = "192.168.80.1"
$DeviceUser = "root"
$SSHKey = "C:\Users\markusla\OneDrive\IT\RUTOS Keys\rusos_private_key_openssh"

Write-Host "=== Deploying VUCI Package via Overlay Installation ===" -ForegroundColor Green

# Step 1: Transfer the installation script
Write-Host "Step 1: Transferring installation script..." -ForegroundColor Yellow
$scpCommand = "scp -i `"$SSHKey`" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null `"install-example-api-overlay.sh`" ${DeviceUser}@${DeviceIP}:/tmp/"
Invoke-Expression $scpCommand

if ($LASTEXITCODE -eq 0) {
    Write-Host "Script transferred successfully!" -ForegroundColor Green
} else {
    Write-Host "Failed to transfer script" -ForegroundColor Red
    exit 1
}

# Step 2: Execute the installation script
Write-Host "Step 2: Executing installation script..." -ForegroundColor Yellow
$sshCommand = "ssh -i `"$SSHKey`" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ${DeviceUser}@${DeviceIP} 'chmod +x /tmp/install-example-api-overlay.sh && /tmp/install-example-api-overlay.sh'"
Invoke-Expression $sshCommand

if ($LASTEXITCODE -eq 0) {
    Write-Host "Installation completed successfully!" -ForegroundColor Green
} else {
    Write-Host "Failed to execute installation script" -ForegroundColor Red
    exit 1
}

# Step 3: Test the installation
Write-Host "Step 3: Testing installation..." -ForegroundColor Yellow
$testCommand = "ssh -i `"$SSHKey`" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ${DeviceUser}@${DeviceIP} 'ubus list | grep example'"
Invoke-Expression $testCommand

Write-Host ""
Write-Host "=== DEPLOYMENT COMPLETE ===" -ForegroundColor Green
Write-Host "VUCI package has been deployed via overlay installation!" -ForegroundColor Green
Write-Host ""
Write-Host "You can now:" -ForegroundColor Cyan
Write-Host "1. Access the UI at: http://$DeviceIP/example/" -ForegroundColor White
Write-Host "2. Test the API: ssh -i `"$SSHKey`" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ${DeviceUser}@${DeviceIP} 'ubus call example status'" -ForegroundColor White
Write-Host "3. The app should now appear in the Web Package Manager" -ForegroundColor White
Write-Host "4. The 'services/Example' error should be resolved" -ForegroundColor White




