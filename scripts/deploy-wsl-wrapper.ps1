# PowerShell Wrapper for Dynamic WSL Deployment
# This script provides a PowerShell interface to the bash deployment script

param(
    [string]$Action = "menu",
    [string]$WSLName = "rutos-openwrt-test",
    [string]$Platform = "both"
)

# Function to print colored output
function Write-Status {
    param([string]$Message)
    Write-Host "[INFO] $Message" -ForegroundColor Cyan
}

function Write-Success {
    param([string]$Message)
    Write-Host "[SUCCESS] $Message" -ForegroundColor Green
}

function Write-Warning {
    param([string]$Message)
    Write-Host "[WARNING] $Message" -ForegroundColor Yellow
}

function Write-Error {
    param([string]$Message)
    Write-Host "[ERROR] $Message" -ForegroundColor Red
}

# Get script directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$BashScript = Join-Path $ScriptDir "deploy-wsl-dynamic.sh"

# Check if bash script exists
if (!(Test-Path $BashScript)) {
    Write-Error "Bash script not found: $BashScript"
    Write-Status "Please ensure the bash script is available: scripts/deploy-wsl-dynamic.sh"
    exit 1
}

# Check if bash is available
try {
    bash --version | Out-Null
} catch {
    Write-Error "Bash is not available. Please install WSL or Git Bash."
    Write-Status "You can install WSL using: wsl --install"
    exit 1
}

# Set environment variables for the bash script
$env:ACTION = $Action
$env:WSL_NAME = $WSLName
$env:PLATFORM = $Platform

Write-Status "Running dynamic WSL deployment..."
Write-Status "Action: $Action"
Write-Status "WSL Name: $WSLName"
Write-Status "Platform: $Platform"
Write-Status "Bash Script: $BashScript"
Write-Host ""

# Convert Windows path to Unix path for bash
$UnixBashScript = $BashScript -replace "\\", "/" -replace "^([A-Z]):", { "/mnt/$($_.Groups[1].Value.ToLower())" }
Write-Status "Unix path: $UnixBashScript"

# Run the bash script
if ($Action -eq "menu") {
    # Interactive mode
    Write-Status "Starting interactive deployment menu..."
    Write-Host "Use the menu options to deploy your packages to WSL." -ForegroundColor Gray
    Write-Host ""
    bash $UnixBashScript
} else {
    # Direct action
    Write-Status "Executing action: $Action"
    bash $UnixBashScript -Action $Action -WSLName $WSLName -Platform $Platform
}

# Check exit code
if ($LASTEXITCODE -eq 0) {
    Write-Success "Deployment completed successfully!"
} else {
    Write-Warning "Deployment completed with warnings or errors (exit code: $LASTEXITCODE)"
}

Write-Host ""
Write-Status "For more information, see: docs/DYNAMIC_WSL_DEPLOYMENT.md"
