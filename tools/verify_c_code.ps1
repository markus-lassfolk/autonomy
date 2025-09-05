# PowerShell wrapper for C Code Verification Script
# ================================================
# This script provides a Windows-friendly interface to the Python C code verifier
# and handles common Windows-specific issues like path separators and Python detection.

param(
    [string]$Directory = "src\c",
    [switch]$Verbose,
    [switch]$Strict,
    [string]$Output,
    [string[]]$Exclude = @(),
    [string[]]$Include = @(),
    [switch]$Help
)

# Function to display help
function Show-Help {
    Write-Host "C Code Verification Tool - PowerShell Wrapper" -ForegroundColor Cyan
    Write-Host "=============================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Usage: .\verify_c_code.ps1 [OPTIONS] [DIRECTORY]" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Options:" -ForegroundColor Green
    Write-Host "  -Directory, -d    Directory to verify (default: src\c)" -ForegroundColor White
    Write-Host "  -Verbose, -v      Verbose output" -ForegroundColor White
    Write-Host "  -Strict, -s       Enable strict checking" -ForegroundColor White
    Write-Host "  -Output, -o       Output report to file" -ForegroundColor White
    Write-Host "  -Exclude, -e      Exclude files matching pattern (can be used multiple times)" -ForegroundColor White
    Write-Host "  -Include, -i      Only include files matching pattern (can be used multiple times)" -ForegroundColor White
    Write-Host "  -Help, -h         Show this help" -ForegroundColor White
    Write-Host ""
    Write-Host "Examples:" -ForegroundColor Green
    Write-Host "  .\verify_c_code.ps1" -ForegroundColor White
    Write-Host "  .\verify_c_code.ps1 -Directory src\c -Verbose" -ForegroundColor White
    Write-Host "  .\verify_c_code.ps1 -Strict -Output report.txt" -ForegroundColor White
    Write-Host "  .\verify_c_code.ps1 -Exclude '*.test.c' -Exclude 'test_*.c'" -ForegroundColor White
    Write-Host ""
    Write-Host "Requirements:" -ForegroundColor Green
    Write-Host "  - Python 3.6 or higher" -ForegroundColor White
    Write-Host "  - GCC or Clang compiler (for syntax checking)" -ForegroundColor White
    Write-Host "  - PowerShell 5.1 or higher" -ForegroundColor White
}

# Show help if requested
if ($Help) {
    Show-Help
    exit 0
}

# Get the script directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PythonScript = Join-Path $ScriptDir "verify_c_code.py"

# Check if Python script exists
if (-not (Test-Path $PythonScript)) {
    Write-Error "Python verification script not found at: $PythonScript"
    Write-Host "Please ensure verify_c_code.py is in the same directory as this PowerShell script." -ForegroundColor Red
    exit 1
}

# Check if Python is available
try {
    $PythonVersion = python --version 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "Python not found"
    }
    Write-Host "Using Python: $PythonVersion" -ForegroundColor Green
} catch {
    Write-Error "Python is not installed or not in PATH"
    Write-Host "Please install Python 3.6 or higher and ensure it's in your PATH." -ForegroundColor Red
    Write-Host "Download from: https://www.python.org/downloads/" -ForegroundColor Yellow
    exit 1
}

# Check if GCC or Clang is available for syntax checking
$CompilerFound = $false
try {
    $GccVersion = gcc --version 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Using GCC: $($GccVersion[0])" -ForegroundColor Green
        $CompilerFound = $true
    }
} catch {
    # GCC not found, try Clang
    try {
        $ClangVersion = clang --version 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "Using Clang: $($ClangVersion[0])" -ForegroundColor Green
            $CompilerFound = $true
        }
    } catch {
        # Neither found
    }
}

if (-not $CompilerFound) {
    Write-Warning "No C compiler (GCC or Clang) found. Syntax checking will be limited."
    Write-Host "For full functionality, install:" -ForegroundColor Yellow
    Write-Host "  - MinGW-w64 (includes GCC)" -ForegroundColor White
    Write-Host "  - Or Visual Studio Build Tools (includes Clang)" -ForegroundColor White
    Write-Host "  - Or WSL with build-essential package" -ForegroundColor White
}

# Convert Windows paths to Unix-style for Python script
$UnixDirectory = $Directory -replace '\\', '/'

# Build command arguments
$Args = @()

# Add directory
$Args += $UnixDirectory

# Add flags
if ($Verbose) { $Args += "-v" }
if ($Strict) { $Args += "-s" }
if ($Output) { $Args += "-o"; $Args += $Output }

# Add exclude patterns
foreach ($Pattern in $Exclude) {
    $Args += "-e"
    $Args += $Pattern
}

# Add include patterns
foreach ($Pattern in $Include) {
    $Args += "-i"
    $Args += $Pattern
}

# Display what we're about to run
Write-Host "🔍 Starting C Code Verification..." -ForegroundColor Cyan
Write-Host "Directory: $Directory" -ForegroundColor White
if ($Verbose) { Write-Host "Verbose: Enabled" -ForegroundColor White }
if ($Strict) { Write-Host "Strict: Enabled" -ForegroundColor White }
if ($Output) { Write-Host "Output: $Output" -ForegroundColor White }
if ($Exclude.Count -gt 0) { Write-Host "Exclude: $($Exclude -join ', ')" -ForegroundColor White }
if ($Include.Count -gt 0) { Write-Host "Include: $($Include -join ', ')" -ForegroundColor White }
Write-Host ""

# Run the Python script
try {
    $Process = Start-Process -FilePath "python" -ArgumentList $Args -NoNewWindow -Wait -PassThru -RedirectStandardOutput "temp_output.txt" -RedirectStandardError "temp_error.txt"
    
    # Read output
    if (Test-Path "temp_output.txt") {
        $Output = Get-Content "temp_output.txt" -Raw
        Write-Host $Output
        Remove-Item "temp_output.txt" -Force
    }
    
    # Read errors
    if (Test-Path "temp_error.txt") {
        $Error = Get-Content "temp_error.txt" -Raw
        if ($Error.Trim()) {
            Write-Host "Errors:" -ForegroundColor Red
            Write-Host $Error -ForegroundColor Red
        }
        Remove-Item "temp_error.txt" -Force
    }
    
    # Exit with the same code as the Python script
    exit $Process.ExitCode
    
} catch {
    Write-Error "Failed to run verification script: $_"
    exit 1
}

# Cleanup function (in case of interruption)
function Cleanup {
    if (Test-Path "temp_output.txt") { Remove-Item "temp_output.txt" -Force }
    if (Test-Path "temp_error.txt") { Remove-Item "temp_error.txt" -Force }
}

# Register cleanup on script exit
Register-EngineEvent -SourceIdentifier PowerShell.Exiting -Action { Cleanup }
