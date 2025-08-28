# PowerShell script to create a VHD for WSL with ext4 filesystem
# This creates a VHD that WSL can format and use with Linux permissions

param(
    [string]$VhdPath = "D:\WSL\rutos-sdk.vhdx",
    [int]$SizeGB = 50
)

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "WSL VHD Setup for RUTOS SDK Development" -ForegroundColor Cyan  
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""

# Check if running as Administrator
if (-NOT ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Host "This script requires Administrator privileges." -ForegroundColor Red
    Write-Host "Please run PowerShell as Administrator." -ForegroundColor Yellow
    exit 1
}

# Ensure directory exists
$VhdDir = Split-Path -Parent $VhdPath
if (!(Test-Path $VhdDir)) {
    Write-Host "Creating directory: $VhdDir" -ForegroundColor Yellow
    New-Item -ItemType Directory -Path $VhdDir -Force | Out-Null
}

# Check if VHD exists
if (Test-Path $VhdPath) {
    Write-Host "VHD already exists at: $VhdPath" -ForegroundColor Yellow
    $response = Read-Host "Delete and recreate? (y/n)"
    if ($response -eq 'y') {
        Remove-Item $VhdPath -Force
        Write-Host "Removed existing VHD" -ForegroundColor Green
    } else {
        Write-Host "Keeping existing VHD" -ForegroundColor Green
    }
}

if (!(Test-Path $VhdPath)) {
    Write-Host "Creating new VHD: $VhdPath ($SizeGB GB)" -ForegroundColor Yellow
    
    # Create a raw VHD without formatting (WSL will format as ext4)
    $diskpartScript = @"
create vdisk file="$VhdPath" maximum=$($SizeGB * 1024) type=expandable
exit
"@
    
    $diskpartScript | diskpart | Out-Host
    
    if (Test-Path $VhdPath) {
        Write-Host "VHD created successfully!" -ForegroundColor Green
    } else {
        Write-Host "Failed to create VHD!" -ForegroundColor Red
        exit 1
    }
}

Write-Host ""
Write-Host "=========================================" -ForegroundColor Green
Write-Host "VHD Created Successfully!" -ForegroundColor Green
Write-Host "=========================================" -ForegroundColor Green
Write-Host ""
Write-Host "VHD Path: $VhdPath" -ForegroundColor Cyan
Write-Host "Size: $SizeGB GB" -ForegroundColor Cyan
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "1. Open WSL" -ForegroundColor White
Write-Host "2. Run: wsl --mount --vhd '$VhdPath' --bare" -ForegroundColor White
Write-Host "3. Format and mount in WSL with Linux filesystem" -ForegroundColor White
Write-Host ""


