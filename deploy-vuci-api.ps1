# PowerShell script to deploy VUCI API service
# Uses SSH key authentication

$DeviceIP = "192.168.80.1"
$DeviceUser = "root"
$SSHKey = "C:\Users\markusla\OneDrive\IT\RUTOS Keys\rusos_private_key_openssh"

Write-Host "=== Deploying VUCI API Service ===" -ForegroundColor Green

# Step 1: Transfer the installation script
Write-Host "Step 1: Transferring installation script..." -ForegroundColor Yellow
$scpCommand = "scp -i `"$SSHKey`" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null `"create-vuci-api.sh`" ${DeviceUser}@${DeviceIP}:/tmp/"
Invoke-Expression $scpCommand

if ($LASTEXITCODE -eq 0) {
    Write-Host "Script transferred successfully!" -ForegroundColor Green
} else {
    Write-Host "Failed to transfer script" -ForegroundColor Red
    exit 1
}

# Step 2: Execute the installation script
Write-Host "Step 2: Executing installation script..." -ForegroundColor Yellow
$sshCommand = "ssh -i `"$SSHKey`" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ${DeviceUser}@${DeviceIP} 'chmod +x /tmp/create-vuci-api.sh && /tmp/create-vuci-api.sh'"
Invoke-Expression $sshCommand

if ($LASTEXITCODE -eq 0) {
    Write-Host "Installation completed successfully!" -ForegroundColor Green
} else {
    Write-Host "Failed to execute installation script" -ForegroundColor Red
    exit 1
}

# Step 3: Test the API
Write-Host "Step 3: Testing API..." -ForegroundColor Yellow
$testCommand = "ssh -i `"$SSHKey`" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ${DeviceUser}@${DeviceIP} 'curl -s -k https://localhost/api/example/status'"
Invoke-Expression $testCommand

Write-Host ""
Write-Host "=== DEPLOYMENT COMPLETE ===" -ForegroundColor Green
Write-Host "VUCI API service has been deployed!" -ForegroundColor Green
Write-Host ""
Write-Host "You can now:" -ForegroundColor Cyan
Write-Host "1. Access the UI at: https://$DeviceIP/example/" -ForegroundColor White
Write-Host "2. Test the API: ssh -i `"$SSHKey`" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ${DeviceUser}@${DeviceIP} 'curl -k https://localhost/api/example/status'" -ForegroundColor White
Write-Host "3. The app should now appear in the Web Package Manager" -ForegroundColor White
Write-Host "4. The 'services/Example' error should be resolved" -ForegroundColor White




