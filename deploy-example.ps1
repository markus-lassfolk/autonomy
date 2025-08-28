# PowerShell script to deploy example API to RUTOS device
# Uses SSH key authentication

$DeviceIP = "192.168.80.1"
$DeviceUser = "root"
$SSHKey = "C:\Users\markusla\OneDrive\IT\RUTOS Keys\rusos_private_key_openssh"
$ScriptFile = "install-example-api.sh"

Write-Host "=== Deploying Example API to RUTOS Device ===" -ForegroundColor Green

# Step 1: Transfer the script using SCP
Write-Host "Step 1: Transferring script to device..." -ForegroundColor Yellow
$scpCommand = "scp -i `"$SSHKey`" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null `"$ScriptFile`" ${DeviceUser}@${DeviceIP}:/tmp/"
Write-Host "Executing: $scpCommand"
Invoke-Expression $scpCommand

if ($LASTEXITCODE -eq 0) {
    Write-Host "✓ Script transferred successfully!" -ForegroundColor Green
} else {
    Write-Host "✗ Failed to transfer script" -ForegroundColor Red
    exit 1
}

# Step 2: Execute the script on the device
Write-Host "Step 2: Executing script on device..." -ForegroundColor Yellow
$sshCommand = "ssh -i `"$SSHKey`" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ${DeviceUser}@${DeviceIP} `"chmod +x /tmp/$ScriptFile && /tmp/$ScriptFile`"
Invoke-Expression $sshCommand

if ($LASTEXITCODE -eq 0) {
    Write-Host "Script executed successfully!" -ForegroundColor Green
} else {
    Write-Host "Failed to execute script" -ForegroundColor Red
    exit 1
}

# Step 3: Test the installation
Write-Host "Step 3: Testing installation..." -ForegroundColor Yellow
$testCommand = "ssh -i `"$SSHKey`" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ${DeviceUser}@${DeviceIP} `"ubus list | grep example`"
Write-Host "Testing API availability..."
Invoke-Expression $testCommand

Write-Host ""
Write-Host "=== DEPLOYMENT COMPLETE ===" -ForegroundColor Green
Write-Host "Example API has been installed and configured!" -ForegroundColor Green
Write-Host ""
Write-Host "You can now:" -ForegroundColor Cyan
Write-Host "1. Access the UI at: http://$DeviceIP/example/" -ForegroundColor White
Write-Host "2. Test the API: ssh -i `"$SSHKey`" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ${DeviceUser}@${DeviceIP} 'ubus call example status'" -ForegroundColor White
Write-Host "3. The app should now appear in the Web Package Manager" -ForegroundColor White
Write-Host "4. The 'services/Example' error should be resolved" -ForegroundColor White
