#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Go code verification script for the autonomy project

.DESCRIPTION
    This script performs comprehensive Go code verification including formatting,
    linting, security checks, and testing. It can be run on all files, specific
    files, or only staged files for pre-commit verification.

.PARAMETER Mode
    The verification mode to run:
    - "all": Verify all Go files in the project
    - "files": Verify specific files (use -Files parameter)
    - "staged": Verify only staged files (for pre-commit)

.PARAMETER Files
    Specific files to verify (comma-separated or array). Required when Mode is "files".

.PARAMETER SkipTests
    Skip running tests (useful for quick checks).

.PARAMETER Verbose
    Enable verbose output.

.EXAMPLE
    # Verify all files
    .\scripts\verify-go.ps1 -Mode all

.EXAMPLE
    # Verify specific files
    .\scripts\verify-go.ps1 -Mode files -Files "cmd/autonomyd/main.go,pkg/logx/logger.go"

.EXAMPLE
    # Verify staged files (pre-commit)
    .\scripts\verify-go.ps1 -Mode staged

.EXAMPLE
    # Quick check without tests
    .\scripts\verify-go.ps1 -Mode all -SkipTests
#>

param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("all", "files", "staged")]
    [string]$Mode,
    
    [Parameter(Mandatory = $false)]
    [string[]]$Files = @(),
    
    [Parameter(Mandatory = $false)]
    [switch]$SkipTests,
    
    [Parameter(Mandatory = $false)]
    [switch]$ShowVerbose
)

# Set error action preference
$ErrorActionPreference = "Stop"

# No color variables needed - using direct color names

# Function to write colored output
function Write-ColorOutput {
    param(
        [string]$Message,
        [string]$Color = "White"
    )
    switch ($Color) {
        "Red" { Write-Host $Message -ForegroundColor Red }
        "Green" { Write-Host $Message -ForegroundColor Green }
        "Yellow" { Write-Host $Message -ForegroundColor Yellow }
        "Blue" { Write-Host $Message -ForegroundColor Blue }
        default { Write-Host $Message -ForegroundColor White }
    }
}

# Function to check if a command exists
function Test-Command {
    param([string]$Command)
    try {
        Get-Command $Command -ErrorAction Stop | Out-Null
        return $true
    }
    catch {
        return $false
    }
}

# Function to get Go files to verify
function Get-GoFilesToVerify {
    param([string]$Mode, [string[]]$Files)
    
    switch ($Mode) {
        "all" {
            Write-ColorOutput "Finding all Go files..." "Blue"
            return Get-ChildItem -Recurse -Include "*.go" | ForEach-Object { $_.FullName }
        }
        "files" {
            if ($Files.Count -eq 0) {
                throw "Files parameter is required when Mode is 'files'"
            }
            Write-ColorOutput "Using specified files: $($Files -join ', ')" "Blue"
            return $Files
        }
        "staged" {
            Write-ColorOutput "Finding staged Go files..." "Blue"
            $stagedFiles = git diff --cached --name-only --diff-filter=ACM | Where-Object { $_ -like "*.go" }
            if ($stagedFiles.Count -eq 0) {
                Write-ColorOutput "No staged Go files found" "Yellow"
                return @()
            }
            Write-ColorOutput "Staged Go files: $($stagedFiles -join ', ')" "Blue"
            return $stagedFiles
        }
    }
}

# Main verification function
function Start-GoVerification {
    param([string[]]$GoFiles)
    
    if ($GoFiles.Count -eq 0) {
        Write-ColorOutput "No Go files to verify" "Yellow"
        return
    }
    
    Write-ColorOutput "`n[VERIFICATION STARTED] Verifying $($GoFiles.Count) Go file(s)" "Blue"
    
    $verificationErrors = @()
    $warnings = @()
    
    # Check required tools
    $requiredTools = @("go", "gofmt", "goimports", "golangci-lint", "govet", "staticcheck", "gosec")
    $missingTools = @()
    
    foreach ($tool in $requiredTools) {
        if (-not (Test-Command $tool)) {
            $missingTools += $tool
        }
    }
    
    if ($missingTools.Count -gt 0) {
        Write-ColorOutput "`n$Bold$Yellow[WARNING]$Reset Missing tools: $($missingTools -join ', ')" $Yellow
        Write-ColorOutput "Some verification steps will be skipped" $Yellow
        $warnings += "Missing tools: $($missingTools -join ', ')"
    }
    
    # 1. Formatting check
    if (Test-Command "gofmt") {
        Write-ColorOutput "`n[FORMATTING] Checking code formatting..." "Blue"
        $unformattedFiles = @()
        
        # Use gofmt -l to list files that need formatting
        $gofmtOutput = & (Get-Command gofmt).Source @("-l") + $GoFiles 2>&1
        if ($LASTEXITCODE -eq 0 -and $gofmtOutput) {
            $unformattedFiles = $gofmtOutput -split "`n" | Where-Object { $_ -ne "" }
        }
        
        if ($unformattedFiles.Count -gt 0) {
            Write-ColorOutput "✗ Found $($unformattedFiles.Count) unformatted file(s):" "Red"
            foreach ($file in $unformattedFiles) {
                Write-ColorOutput "  - $file" "Red"
            }
            $verificationErrors += "Code formatting issues found"
        }
        else {
            Write-ColorOutput "✓ All files are properly formatted" $Green
        }
    }
    
    # 2. Import organization
    if (Test-Command "goimports") {
        Write-ColorOutput "`n[IMPORTS] Checking import organization..." "Blue"
        $importIssues = @()
        
        # Use goimports -l to list files that need import organization
        $goimportsOutput = & (Get-Command goimports).Source @("-l") + $GoFiles 2>&1
        if ($LASTEXITCODE -eq 0 -and $goimportsOutput) {
            $importIssues = $goimportsOutput -split "`n" | Where-Object { $_ -ne "" }
        }
        
        if ($importIssues.Count -gt 0) {
            Write-ColorOutput "✗ Found $($importIssues.Count) file(s) with import issues:" "Red"
            foreach ($file in $importIssues) {
                Write-ColorOutput "  - $file" "Red"
            }
            $verificationErrors += "Import organization issues found"
        }
        else {
            Write-ColorOutput "✓ All imports are properly organized" $Green
        }
    }
    
    # 3. Go vet
    Write-ColorOutput "`n[VET] Running go vet..." "Blue"
    $vetOutput = & go vet ./... 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) {
        Write-ColorOutput "✗ Go vet found issues:" "Red"
        Write-ColorOutput $vetOutput "Red"
        $verificationErrors += "Go vet issues found"
    }
    else {
        Write-ColorOutput "✓ Go vet passed" $Green
    }
    
    # 4. Staticcheck
    if (Test-Command "staticcheck") {
        Write-ColorOutput "`n[STATICCHECK] Running staticcheck..." "Blue"
        $staticcheckOutput = & staticcheck ./... 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) {
            Write-ColorOutput "✗ Staticcheck found issues:" "Red"
            Write-ColorOutput $staticcheckOutput "Red"
            $verificationErrors += "Staticcheck issues found"
        }
        else {
            Write-ColorOutput "✓ Staticcheck passed" $Green
        }
    }
    
    # 5. Security check
    if (Test-Command "gosec") {
        Write-ColorOutput "`n[SECURITY] Running gosec..." "Blue"
        $gosecOutput = Start-Process -FilePath "gosec" -ArgumentList "./..." -Wait -NoNewWindow -PassThru -RedirectStandardOutput "temp_gosec.txt" -RedirectStandardError "temp_gosec_err.txt"
        if (Test-Path "temp_gosec.txt") {
            $gosecOutput = Get-Content "temp_gosec.txt" -Raw
            Remove-Item "temp_gosec.txt" -ErrorAction SilentlyContinue
        }
        if (Test-Path "temp_gosec_err.txt") {
            Remove-Item "temp_gosec_err.txt" -ErrorAction SilentlyContinue
        }
        if ($LASTEXITCODE -ne 0) {
            Write-ColorOutput "✗ Gosec found security issues:" "Red"
            Write-ColorOutput $gosecOutput "Red"
            $verificationErrors += "Security issues found"
        }
        else {
            Write-ColorOutput "✓ Gosec passed" $Green
        }
    }
    
    # 6. Linting
    if (Test-Command "golangci-lint") {
        Write-ColorOutput "`n[LINTING] Running golangci-lint..." "Blue"
        $lintOutput = & golangci-lint run 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) {
            Write-ColorOutput "✗ Golangci-lint found issues:" "Red"
            Write-ColorOutput $lintOutput "Red"
            $verificationErrors += "Linting issues found"
        }
        else {
            Write-ColorOutput "✓ Golangci-lint passed" $Green
        }
    }
    
    # 7. Tests (if not skipped)
    if (-not $SkipTests) {
        Write-ColorOutput "`n[TESTING] Running tests..." "Blue"
        $testOutput = & go test -race -v ./... 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) {
            Write-ColorOutput "✗ Tests failed:" "Red"
            Write-ColorOutput $testOutput "Red"
            $verificationErrors += "Tests failed"
        }
        else {
            Write-ColorOutput "✓ All tests passed" $Green
        }
    }
    else {
        Write-ColorOutput "`n$Bold$Yellow[TESTING]$Reset Skipped (use -SkipTests to skip)" $Yellow
    }
    
    # Summary
    Write-ColorOutput "`n[SUMMARY]" "Blue"
    if ($verificationErrors.Count -eq 0) {
        Write-ColorOutput "✓ All verification checks passed!" $Green
        if ($warnings.Count -gt 0) {
            Write-ColorOutput "`nWarnings:" $Yellow
            foreach ($warning in $warnings) {
                Write-ColorOutput "  - $warning" $Yellow
            }
        }
        exit 0
    }
    else {
        Write-ColorOutput "✗ Verification failed with $($verificationErrors.Count) error(s):" "Red"
        foreach ($verificationError in $verificationErrors) {
            Write-ColorOutput "  - $verificationError" $Red
        }
        if ($warnings.Count -gt 0) {
            Write-ColorOutput "`nWarnings:" $Yellow
            foreach ($warning in $warnings) {
                Write-ColorOutput "  - $warning" $Yellow
            }
        }
        exit 1
    }
}

# Main execution
try {
    Write-ColorOutput "[autonomy GO VERIFICATION]" "Blue"
    Write-ColorOutput "Mode: $Mode" "Blue"
    if ($ShowVerbose) {
        Write-ColorOutput "Verbose: Enabled" "Blue"
    }
    
    $goFiles = Get-GoFilesToVerify -Mode $Mode -Files $Files
    Start-GoVerification -GoFiles $goFiles
}
catch {
    Write-ColorOutput "`n$Bold$Red[ERROR]$Reset $($_.Exception.Message)" $Red
    exit 1
}

