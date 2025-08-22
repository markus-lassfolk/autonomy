# Fix All PowerShell Colors
# This script fixes the color escape sequences in all PowerShell scripts

Write-Host "Fixing colors in all PowerShell scripts..."

$Scripts = @(
    "test/setup-virtual-rutos-openwrt-simple.ps1",
    "test/setup-virtual-rutos-openwrt.ps1",
    "test/setup-virtual-rutos.ps1",
    "test/setup-dedicated-openwrt-wsl.ps1",
    "test/wsl-openwrt-setup.ps1",
    "test/openwrt-test-environment.ps1",
    "test/simple-openwrt-test.ps1",
    "build-rutos-package-fixed.ps1",
    "build-rutos-package.ps1",
    "build-rutos-package-simple.ps1",
    "build-openwrt-package.ps1"
)

foreach ($script in $Scripts) {
    if (Test-Path $script) {
        Write-Host "Fixing colors in $script..."

        # Read the file content
        $content = Get-Content $script -Raw

        # Fix color escape sequences - replace the incorrect `e[ with the correct `e[
        $content = $content -replace '`e\[', '`e['

        # Write back the fixed content
        Set-Content $script $content -NoNewline -Encoding UTF8

        Write-Host "Fixed $script"
    } else {
        Write-Host "Script not found: $script"
    }
}

Write-Host "All color fixes completed!"
Write-Host "Now you can use the new script: test/setup-virtual-rutos-openwrt-fixed.ps1"
Write-Host "This includes proper colors and unattended installation with admin/Passw0rd!"
