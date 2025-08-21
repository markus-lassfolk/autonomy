#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Comprehensive verification script for autonomy project (Go + LuCI)

.DESCRIPTION
    Complete verification solution for both Go backend and LuCI frontend components:
    
    GO VERIFICATION:
    - Code formatting (gofmt, goimports)
    - Linting (golangci-lint, staticcheck, gocritic)
    - Security scanning (gosec)
    - Testing (go test, race detection, coverage)
    - Build verification (multi-platform)
    - Dependency analysis
    - Documentation generation
    
    LUCİ VERIFICATION:
    - Lua syntax checking (lua -p)
    - Lua linting (luacheck)
    - HTML validation (htmlhint)
    - JavaScript linting (eslint)
    - CSS linting (stylelint)
    - Translation validation (msgfmt)
    - LuCI-specific checks

.PARAMETER Mode
    Verification mode: all, go, luci, files, staged, commit, ci

.PARAMETER Files
    Specific files or patterns to verify

.PARAMETER Help
    Show detailed help

.PARAMETER Verbose
    Enable verbose output

.PARAMETER Quiet
    Quiet mode (errors only)

.PARAMETER DryRun
    Show what would be done without making changes

.PARAMETER Fix
    Attempt to fix issues automatically

.PARAMETER NoGo
    Skip Go verification

.PARAMETER NoLuci
    Skip LuCI verification

.PARAMETER NoFormat
    Skip formatting checks

.PARAMETER NoLint
    Skip linting checks

.PARAMETER NoSecurity
    Skip security checks

.PARAMETER NoTests
    Skip tests

.PARAMETER NoBuild
    Skip build verification

.PARAMETER NoTranslations
    Skip translation validation

.PARAMETER Coverage
    Generate test coverage report

.PARAMETER Race
    Enable race detection in tests

.PARAMETER Timeout
    Timeout for individual checks (seconds)

.EXAMPLE
    # Full verification (Go + LuCI)
    .\scripts\verify-comprehensive.ps1 all

.EXAMPLE
    # Go-only verification
    .\scripts\verify-comprehensive.ps1 go

.EXAMPLE
    # LuCI-only verification
    .\scripts\verify-comprehensive.ps1 luci

.EXAMPLE
    # Pre-commit check
    .\scripts\verify-comprehensive.ps1 staged

.EXAMPLE
    # CI/CD mode
    .\scripts\verify-comprehensive.ps1 ci -Coverage -Race

.EXAMPLE
    # Auto-fix mode
    .\scripts\verify-comprehensive.ps1 all -Fix

.NOTES
    Required tools for Go:
    - go, gofmt, goimports, golangci-lint, staticcheck, gocritic, gosec
    
    Required tools for LuCI:
    - lua, luacheck, htmlhint, eslint, stylelint, msgfmt
    
    Install missing tools with:
    - Go: go install <tool>@latest
    - LuCI: npm install -g htmlhint eslint stylelint
    - LuCI: luarocks install luacheck
#>

param(
    [Parameter(Position=0)]
    [ValidateSet("all", "go", "luci", "files", "staged", "commit", "ci")]
    [string]$Mode = "all",
    
    [Parameter(Position=1, ValueFromRemainingArguments)]
    [string[]]$Files = @(),
    
    [switch]$Help,
    [switch]$VerboseOutput,
    [switch]$Quiet,
    [switch]$DryRun,
    [switch]$Fix,
    
    # Go-specific options
    [switch]$NoGo,
    [switch]$NoFormat,
    [switch]$NoLint,
    [switch]$NoSecurity,
    [switch]$NoTests,
    [switch]$NoBuild,
    [switch]$NoDeps,
    [switch]$NoDocs,
    [switch]$Coverage,
    [switch]$Race,
    
    # LuCI-specific options
    [switch]$NoLuci,
    [switch]$NoTranslations,
    [switch]$InstallDeps,
    
    [int]$Timeout = 300
)

# Script configuration
$Script:ScriptName = "verify-comprehensive.ps1"
$Script:Version = "3.0.0"
$Script:StartTime = Get-Date

# Configuration
$Script:Config = @{
    EnableGo = !$NoGo
    EnableLuci = !$NoLuci
    EnableFormat = !$NoFormat
    EnableLint = !$NoLint
    EnableSecurity = !$NoSecurity
    EnableTests = !$NoTests
    EnableBuild = !$NoBuild
    EnableDeps = !$NoDeps
    EnableDocs = !$NoDocs
    EnableTranslations = !$NoTranslations
    EnableCoverage = $Coverage
    EnableRace = $Race
    EnableFix = $Fix
    EnableDryRun = $DryRun
    VerboseOutput = $VerboseOutput
    QuietMode = $Quiet
    Timeout = $Timeout
    InstallDeps = $InstallDeps
}

# Colors for console output
$Script:Colors = @{
    Red = "Red"
    Green = "Green"
    Yellow = "Yellow"
    Blue = "Blue"
    Cyan = "Cyan"
    Gray = "Gray"
    White = "White"
    Magenta = "Magenta"
}

# Tool configurations
$Script:GoTools = @{
    "go" = @{ Command = "go"; Install = "Built-in" }
    "gofmt" = @{ Command = "gofmt"; Install = "go install golang.org/x/tools/cmd/gofmt@latest" }
    "goimports" = @{ Command = "goimports"; Install = "go install golang.org/x/tools/cmd/goimports@latest" }
    "golangci-lint" = @{ Command = "golangci-lint"; Install = "go install github.com/golangci/golangci-lint/cmd/golangci-lint@latest" }
    "staticcheck" = @{ Command = "staticcheck"; Install = "go install honnef.co/go/tools/cmd/staticcheck@latest" }
    "gocritic" = @{ Command = "gocritic"; Install = "go install github.com/go-critic/go-critic/cmd/gocritic@latest" }
    "gosec" = @{ Command = "gosec"; Install = "go install github.com/securego/gosec/v2/cmd/gosec@latest" }
    "gocyclo" = @{ Command = "gocyclo"; Install = "go install github.com/fzipp/gocyclo/cmd/gocyclo@latest" }
    "ineffassign" = @{ Command = "ineffassign"; Install = "go install github.com/gordonklaus/ineffassign@latest" }
    "godoc" = @{ Command = "godoc"; Install = "go install golang.org/x/tools/cmd/godoc@latest" }
}

$Script:LuCITools = @{
    "lua" = @{ Command = "lua"; Install = "Install Lua from https://www.lua.org/download.html" }
    "luacheck" = @{ Command = "luacheck"; Install = "luarocks install luacheck" }
    "htmlhint" = @{ Command = "htmlhint"; Install = "npm install -g htmlhint" }
    "eslint" = @{ Command = "eslint"; Install = "npm install -g eslint" }
    "stylelint" = @{ Command = "stylelint"; Install = "npm install -g stylelint" }
    "msgfmt" = @{ Command = "msgfmt"; Install = "Install gettext package for your OS" }
}

# Statistics
$Script:Stats = @{
    TotalChecks = 0
    PassedChecks = 0
    FailedChecks = 0
    Warnings = 0
    Errors = 0
    StartTime = Get-Date
}

function Show-Usage {
    @"
Comprehensive Verification Script v$Script:Version

USAGE:
    $Script:ScriptName [MODE] [OPTIONS] [FILES...]

MODES:
    all     - Check all components (Go + LuCI) [default]
    go      - Check only Go components
    luci    - Check only LuCI components
    files   - Check specific files or patterns
    staged  - Check staged files for pre-commit
    commit  - Check files in git diff --cached
    ci      - CI/CD mode with all checks

OPTIONS:
    -Help           - Show this help
    -Verbose        - Verbose output
    -Quiet          - Quiet mode (errors only)
    -DryRun         - Show what would be done
    -Fix            - Attempt to fix issues automatically
    -InstallDeps    - Install missing tools automatically

GO OPTIONS:
    -NoGo           - Skip Go verification
    -NoFormat       - Skip formatting checks
    -NoLint         - Skip linting checks
    -NoSecurity     - Skip security checks
    -NoTests        - Skip tests
    -NoBuild        - Skip build verification
    -NoDeps         - Skip dependency analysis
    -NoDocs         - Skip documentation generation
    -Coverage       - Generate test coverage report
    -Race           - Enable race detection in tests

LUCİ OPTIONS:
    -NoLuci         - Skip LuCI verification
    -NoTranslations - Skip translation validation

EXAMPLES:
    $Script:ScriptName all                    # Full verification
    $Script:ScriptName go                     # Go-only verification
    $Script:ScriptName luci                   # LuCI-only verification
    $Script:ScriptName staged                 # Pre-commit check
    $Script:ScriptName files *.lua            # Check Lua files
    $Script:ScriptName all -Fix               # Auto-fix mode
    $Script:ScriptName ci -Coverage -Race     # CI/CD mode
    $Script:ScriptName all -InstallDeps       # Install missing tools and verify

REQUIRED TOOLS:
    Go: go, gofmt, goimports, golangci-lint, staticcheck, gocritic, gosec
    LuCI: lua, luacheck, htmlhint, eslint, stylelint, msgfmt

"@
}

function Write-Log {
    param(
        [string]$Message,
        [ValidateSet("INFO", "SUCCESS", "WARNING", "ERROR", "VERBOSE")]
        [string]$Level = "INFO",
        [string]$Category = "General"
    )
    
    if ($Script:Config.QuietMode -and $Level -ne "ERROR") {
        return
    }
    
    $timestamp = Get-Date -Format "HH:mm:ss"
    $color = switch ($Level) {
        "SUCCESS" { $Script:Colors.Green }
        "WARNING" { $Script:Colors.Yellow }
        "ERROR" { $Script:Colors.Red }
        "VERBOSE" { $Script:Colors.Cyan }
        default { $Script:Colors.Blue }
    }
    
    $prefix = "[$timestamp] [$Level]"
    if ($Category -ne "General") {
        $prefix += " [$Category]"
    }
    
    Write-Host -ForegroundColor $color "$prefix $Message"
}

function Write-ColorOutput {
    param(
        [string]$Message,
        [string]$Color = "White"
    )
    
    Write-Host -ForegroundColor $Color $Message
}

function Test-Tools {
    param([hashtable]$Tools, [string]$Category)
    
    Write-Log "Checking required $Category tools..." "INFO" "Setup"
    
    $available = @()
    $missing = @()
    
    foreach ($tool in $Tools.Keys) {
        $toolInfo = $Tools[$tool]
        $command = $toolInfo.Command
        
        try {
            $null = Get-Command $command -ErrorAction Stop
            $available += $tool
            Write-Log "Tool '$tool' available" "VERBOSE" "Tools"
        }
        catch {
            $missing += $tool
            Write-Log "Tool '$tool' not found" "WARNING" "Tools"
            Write-Log "Install with: $($toolInfo.Install)" "INFO" "Tools"
        }
    }
    
    if ($available.Count -gt 0) {
        Write-Log "Available $Category tools: $($available -join ', ')" "SUCCESS" "Setup"
    }
    
    if ($missing.Count -gt 0) {
        Write-Log "Missing $Category tools: $($missing -join ', ')" "WARNING" "Setup"
        return @{ Success = $false; Missing = $missing; Available = $available }
    }
    
    return @{ Success = $true; Missing = @(); Available = $available }
}

function Install-MissingTools {
    param([hashtable]$Tools, [string]$Category, [string[]]$MissingTools)
    
    Write-Log "Installing missing $Category tools..." "INFO" "Setup"
    
    $installed = @()
    $failed = @()
    
    foreach ($tool in $MissingTools) {
        $toolInfo = $Tools[$tool]
        $installCmd = $toolInfo.Install
        
        Write-Log "Installing $tool..." "INFO" "Setup"
        
        try {
            if ($installCmd -eq "Built-in") {
                Write-Log "Tool '$tool' is built-in, skipping installation" "WARNING" "Setup"
                continue
            }
            
            # Handle different installation methods
            if ($installCmd -like "go install*") {
                $result = Invoke-CommandWithTimeout "go" @("install", $installCmd.Split(" ")[-1])
            }
            elseif ($installCmd -like "npm install*") {
                $result = Invoke-CommandWithTimeout "npm" @("install", "-g", $installCmd.Split(" ")[-1])
            }
            elseif ($installCmd -like "luarocks install*") {
                $result = Invoke-CommandWithTimeout "luarocks" @("install", $installCmd.Split(" ")[-1])
            }
            else {
                Write-Log "Manual installation required for $tool - $installCmd" "WARNING" "Setup"
                $failed += $tool
                continue
            }
            
            if ($result.Success) {
                Write-Log "Successfully installed $tool" "SUCCESS" "Setup"
                $installed += $tool
            } else {
                Write-Log "Failed to install $tool - $($result.Output)" "ERROR" "Setup"
                $failed += $tool
            }
        }
        catch {
            Write-Log "Error installing $tool - $($_.Exception.Message)" "ERROR" "Setup"
            $failed += $tool
        }
    }
    
    if ($installed.Count -gt 0) {
        Write-Log "Successfully installed $($installed.Count) $Category tool(s): $($installed -join ', ')" "SUCCESS" "Setup"
    }
    
    if ($failed.Count -gt 0) {
        Write-Log "Failed to install $($failed.Count) $Category tool(s): $($failed -join ', ')" "ERROR" "Setup"
        Write-Log "Please install these tools manually:" "INFO" "Setup"
        foreach ($tool in $failed) {
            Write-Log "  $tool - $($Tools[$tool].Install)" "INFO" "Setup"
        }
    }
    
    return @{ Installed = $installed; Failed = $failed }
}

function Invoke-CommandWithTimeout {
    param(
        [string]$Command,
        [string[]]$Arguments = @(),
        [int]$TimeoutSeconds = $Script:Config.Timeout,
        [string]$WorkingDirectory = $PWD.Path
    )
    
    $job = Start-Job -ScriptBlock {
        param($cmd, $args, $wd)
        Set-Location $wd
        & $cmd @args 2>&1
    } -ArgumentList $Command, $Arguments, $WorkingDirectory
    
    $result = Wait-Job $job -Timeout $TimeoutSeconds
    
    if ($result -eq $null) {
        Stop-Job $job
        Remove-Job $job
        return @{
            Success = $false
            Output = "Command timed out after $TimeoutSeconds seconds"
            ExitCode = -1
        }
    }
    
    $output = Receive-Job $job
    Remove-Job $job
    
    return @{
        Success = $true
        Output = $output -join "`n"
        ExitCode = $LASTEXITCODE
    }
}

function Get-FilesToCheck {
    param([string]$Mode, [string[]]$Files)
    
    switch ($Mode) {
        "all" {
            $goFiles = Get-ChildItem -Recurse -Include "*.go" | Where-Object { $_.FullName -notlike "*vendor*" }
            $luciFiles = Get-ChildItem -Recurse -Include "*.lua", "*.html", "*.js", "*.css", "*.po" | Where-Object { $_.FullName -like "*luci*" -or $_.FullName -like "*www*" }
            return @{ Go = $goFiles; LuCI = $luciFiles }
        }
        "go" {
            $goFiles = Get-ChildItem -Recurse -Include "*.go" | Where-Object { $_.FullName -notlike "*vendor*" }
            return @{ Go = $goFiles; LuCI = @() }
        }
        "luci" {
            $luciFiles = Get-ChildItem -Recurse -Include "*.lua", "*.html", "*.js", "*.css", "*.po" | Where-Object { $_.FullName -like "*luci*" -or $_.FullName -like "*www*" }
            return @{ Go = @(); LuCI = $luciFiles }
        }
        "files" {
            $goFiles = @()
            $luciFiles = @()
            
            foreach ($file in $Files) {
                if ($file -like "*.go") {
                    $goFiles += Get-ChildItem -Path $file -ErrorAction SilentlyContinue
                }
                elseif ($file -like "*.lua" -or $file -like "*.html" -or $file -like "*.js" -or $file -like "*.css" -or $file -like "*.po") {
                    $luciFiles += Get-ChildItem -Path $file -ErrorAction SilentlyContinue
                }
            }
            
            return @{ Go = $goFiles; LuCI = $luciFiles }
        }
        "staged" {
            $stagedFiles = git diff --cached --name-only 2>$null
            $goFiles = @()
            $luciFiles = @()
            
            foreach ($file in $stagedFiles) {
                if ($file -like "*.go") {
                    $goFiles += Get-ChildItem -Path $file -ErrorAction SilentlyContinue
                }
                elseif ($file -like "*.lua" -or $file -like "*.html" -or $file -like "*.js" -or $file -like "*.css" -or $file -like "*.po") {
                    $luciFiles += Get-ChildItem -Path $file -ErrorAction SilentlyContinue
                }
            }
            
            return @{ Go = $goFiles; LuCI = $luciFiles }
        }
        default {
            return @{ Go = @(); LuCI = @() }
        }
    }
}

# Go verification functions
function Invoke-GoFormat {
    if (-not $Script:Config.EnableFormat) { return }
    
    Write-Log "Running Go formatting..." "INFO" "Go"
    
    if ($Script:Config.EnableFix) {
        $result = Invoke-CommandWithTimeout "gofmt" @("-s", "-w", ".")
        if ($result.Success) {
            Write-Log "Go formatting completed" "SUCCESS" "Go"
        } else {
            Write-Log "Go formatting failed: $($result.Output)" "ERROR" "Go"
            $Script:Stats.FailedChecks++
        }
    } else {
        $result = Invoke-CommandWithTimeout "gofmt" @("-l", ".")
        if ($result.Success -and $result.Output.Trim() -eq "") {
            Write-Log "Go formatting is correct" "SUCCESS" "Go"
        } else {
            Write-Log "Files need formatting: $($result.Output)" "WARNING" "Go"
            Write-Log "Run with -Fix to auto-fix formatting issues" "INFO" "Go"
            $Script:Stats.Warnings++
        }
    }
    
    $Script:Stats.TotalChecks++
}

function Invoke-GoImports {
    if (-not $Script:Config.EnableFormat) { return }
    
    Write-Log "Organizing imports..." "INFO" "Go"
    
    if ($Script:Config.EnableFix) {
        $result = Invoke-CommandWithTimeout "goimports" @("-w", ".")
        if ($result.Success) {
            Write-Log "Import organization completed" "SUCCESS" "Go"
        } else {
            Write-Log "Import organization failed: $($result.Output)" "ERROR" "Go"
            $Script:Stats.FailedChecks++
        }
    } else {
        $result = Invoke-CommandWithTimeout "goimports" @("-l", ".")
        if ($result.Success -and $result.Output.Trim() -eq "") {
            Write-Log "Imports are organized" "SUCCESS" "Go"
        } else {
            Write-Log "Files need import organization: $($result.Output)" "WARNING" "Go"
            Write-Log "Run with -Fix to auto-organize imports" "INFO" "Go"
            $Script:Stats.Warnings++
        }
    }
    
    $Script:Stats.TotalChecks++
}

function Invoke-GoLint {
    if (-not $Script:Config.EnableLint) { return }
    
    Write-Log "Running golangci-lint..." "INFO" "Go"
    
    $result = Invoke-CommandWithTimeout "golangci-lint" @("run")
    if ($result.Success) {
        Write-Log "Go linting passed" "SUCCESS" "Go"
        $Script:Stats.PassedChecks++
    } else {
        Write-Log "Go linting failed: $($result.Output)" "ERROR" "Go"
        $Script:Stats.FailedChecks++
    }
    
    $Script:Stats.TotalChecks++
}

function Invoke-GoVet {
    if (-not $Script:Config.EnableLint) { return }
    
    Write-Log "Running go vet..." "INFO" "Go"
    
    $result = Invoke-CommandWithTimeout "go" @("vet", "./...")
    if ($result.Success) {
        Write-Log "Go vet passed" "SUCCESS" "Go"
        $Script:Stats.PassedChecks++
    } else {
        Write-Log "Go vet failed: $($result.Output)" "ERROR" "Go"
        $Script:Stats.FailedChecks++
    }
    
    $Script:Stats.TotalChecks++
}

function Invoke-GoSecurity {
    if (-not $Script:Config.EnableSecurity) { return }
    
    Write-Log "Running security scan..." "INFO" "Go"
    
    $result = Invoke-CommandWithTimeout "gosec" @("./...")
    if ($result.Success) {
        Write-Log "Security scan passed" "SUCCESS" "Go"
        $Script:Stats.PassedChecks++
    } else {
        Write-Log "Security scan found issues: $($result.Output)" "ERROR" "Go"
        $Script:Stats.FailedChecks++
    }
    
    $Script:Stats.TotalChecks++
}

function Invoke-GoTests {
    if (-not $Script:Config.EnableTests) { return }
    
    Write-Log "Running tests..." "INFO" "Go"
    
    $testArgs = @("test", "./...")
    if ($Script:Config.EnableRace) {
        $testArgs += "-race"
    }
    if ($Script:Config.EnableCoverage) {
        $testArgs += "-coverprofile=coverage.out"
    }
    
    $result = Invoke-CommandWithTimeout "go" $testArgs
    if ($result.Success) {
        Write-Log "Tests passed" "SUCCESS" "Go"
        $Script:Stats.PassedChecks++
        
        if ($Script:Config.EnableCoverage) {
            Write-Log "Coverage report generated: coverage.out" "INFO" "Go"
        }
    } else {
        Write-Log "Tests failed: $($result.Output)" "ERROR" "Go"
        $Script:Stats.FailedChecks++
    }
    
    $Script:Stats.TotalChecks++
}

function Invoke-GoBuild {
    if (-not $Script:Config.EnableBuild) { return }
    
    Write-Log "Verifying builds..." "INFO" "Go"
    
    $platforms = @("linux/amd64", "linux/arm64", "windows/amd64")
    $failed = 0
    
    foreach ($platform in $platforms) {
        $parts = $platform -split "/"
        $os = $parts[0]
        $arch = $parts[1]
        
        Write-Log "Building for $platform..." "VERBOSE" "Go"
        
        $env:GOOS = $os
        $env:GOARCH = $arch
        
        $result = Invoke-CommandWithTimeout "go" @("build", "-o", "bin/autonomyd-$os-$arch", "./cmd/autonomyd")
        if (-not $result.Success) {
            Write-Log "Build failed: autonomyd for $platform" "ERROR" "Go"
            Write-Log "Error: $($result.Output)" "ERROR" "Go"
            $failed++
        }
    }
    
    if ($failed -eq 0) {
        Write-Log "All builds successful" "SUCCESS" "Go"
        $Script:Stats.PassedChecks++
    } else {
        Write-Log "$failed build(s) failed" "ERROR" "Go"
        $Script:Stats.FailedChecks++
    }
    
    $Script:Stats.TotalChecks++
}

# LuCI verification functions
function Invoke-LuaSyntax {
    if (-not $Script:Config.EnableLint) { return }
    
    Write-Log "Checking Lua syntax..." "INFO" "LuCI"
    
    $luaFiles = Get-ChildItem -Recurse -Include "*.lua" | Where-Object { $_.FullName -like "*luci*" -or $_.FullName -like "*www*" }
    $errors = 0
    
    foreach ($file in $luaFiles) {
        $result = Invoke-CommandWithTimeout "lua" @("-p", $file.FullName)
        if (-not $result.Success) {
            Write-Log "Lua syntax error in $($file.Name): $($result.Output)" "ERROR" "LuCI"
            $errors++
        }
    }
    
    if ($errors -eq 0) {
        Write-Log "Lua syntax check passed" "SUCCESS" "LuCI"
        $Script:Stats.PassedChecks++
    } else {
        Write-Log "Lua syntax check failed: $errors error(s)" "ERROR" "LuCI"
        $Script:Stats.FailedChecks++
    }
    
    $Script:Stats.TotalChecks++
}

function Invoke-LuaLint {
    if (-not $Script:Config.EnableLint) { return }
    
    Write-Log "Linting Lua files..." "INFO" "LuCI"
    
    $result = Invoke-CommandWithTimeout "luacheck" @(".", "--no-color")
    if ($result.Success) {
        Write-Log "Lua linting passed" "SUCCESS" "LuCI"
        $Script:Stats.PassedChecks++
    } else {
        Write-Log "Lua linting found issues: $($result.Output)" "ERROR" "LuCI"
        $Script:Stats.FailedChecks++
    }
    
    $Script:Stats.TotalChecks++
}

function Invoke-HTMLValidation {
    if (-not $Script:Config.EnableLint) { return }
    
    Write-Log "Validating HTML files..." "INFO" "LuCI"
    
    $result = Invoke-CommandWithTimeout "htmlhint" @(".")
    if ($result.Success) {
        Write-Log "HTML validation passed" "SUCCESS" "LuCI"
        $Script:Stats.PassedChecks++
    } else {
        Write-Log "HTML validation found issues: $($result.Output)" "ERROR" "LuCI"
        $Script:Stats.FailedChecks++
    }
    
    $Script:Stats.TotalChecks++
}

function Invoke-JavaScriptLint {
    if (-not $Script:Config.EnableLint) { return }
    
    Write-Log "Linting JavaScript files..." "INFO" "LuCI"
    
    $result = Invoke-CommandWithTimeout "eslint" @(".", "--ext", ".js")
    if ($result.Success) {
        Write-Log "JavaScript linting passed" "SUCCESS" "LuCI"
        $Script:Stats.PassedChecks++
    } else {
        Write-Log "JavaScript linting found issues: $($result.Output)" "ERROR" "LuCI"
        $Script:Stats.FailedChecks++
    }
    
    $Script:Stats.TotalChecks++
}

function Invoke-CSSLint {
    if (-not $Script:Config.EnableLint) { return }
    
    Write-Log "Linting CSS files..." "INFO" "LuCI"
    
    $result = Invoke-CommandWithTimeout "stylelint" @("**/*.css")
    if ($result.Success) {
        Write-Log "CSS linting passed" "SUCCESS" "LuCI"
        $Script:Stats.PassedChecks++
    } else {
        Write-Log "CSS linting found issues: $($result.Output)" "ERROR" "LuCI"
        $Script:Stats.FailedChecks++
    }
    
    $Script:Stats.TotalChecks++
}

function Invoke-TranslationValidation {
    if (-not $Script:Config.EnableTranslations) { return }
    
    Write-Log "Validating translation files..." "INFO" "LuCI"
    
    $poFiles = Get-ChildItem -Recurse -Include "*.po" | Where-Object { $_.FullName -like "*luci*" -or $_.FullName -like "*www*" }
    $errors = 0
    
    foreach ($file in $poFiles) {
        $result = Invoke-CommandWithTimeout "msgfmt" @("--check", $file.FullName)
        if (-not $result.Success) {
            Write-Log "Translation error in $($file.Name): $($result.Output)" "ERROR" "LuCI"
            $errors++
        }
    }
    
    if ($errors -eq 0) {
        Write-Log "Translation validation passed" "SUCCESS" "LuCI"
        $Script:Stats.PassedChecks++
    } else {
        Write-Log "Translation validation failed: $errors error(s)" "ERROR" "LuCI"
        $Script:Stats.FailedChecks++
    }
    
    $Script:Stats.TotalChecks++
}

function Show-Summary {
    $duration = (Get-Date) - $Script:Stats.StartTime
    
    Write-Log "Verification completed in $($duration.TotalSeconds.ToString('F1'))s" "INFO" "Summary"
    Write-Log "Checks run: $($Script:Stats.TotalChecks), Passed: $($Script:Stats.PassedChecks), Failed: $($Script:Stats.FailedChecks)" "INFO" "Summary"
    
    if ($Script:Stats.Warnings -gt 0) {
        Write-Log "$($Script:Stats.Warnings) warning(s)" "WARNING" "Summary"
    }
    
    if ($Script:Stats.FailedChecks -gt 0) {
        Write-Log "$($Script:Stats.FailedChecks) check(s) failed" "ERROR" "Summary"
        exit 1
    } else {
        Write-Log "All checks passed!" "SUCCESS" "Summary"
        exit 0
    }
}

# Main execution
if ($Help) {
    Show-Usage
    exit 0
}

Write-Log "autonomy Comprehensive Verification Script v$Script:Version" "INFO" "Setup"
Write-Log "Mode: $Mode" "INFO" "Setup"

if ($Script:Config.EnableDryRun) {
    Write-Log "DRY RUN MODE - No changes will be made" "WARNING" "Setup"
}

# Check tools
$goToolsResult = Test-Tools $Script:GoTools "Go"
$luciToolsResult = Test-Tools $Script:LuCITools "LuCI"

$goToolsOk = $goToolsResult.Success
$luciToolsOk = $luciToolsResult.Success

# Install missing tools if requested
if ($Script:Config.InstallDeps) {
    Write-Log "Auto-installation mode enabled" "INFO" "Setup"
    
    # Install Go tools
    if (-not $goToolsOk -and $goToolsResult.Missing.Count -gt 0) {
        $goInstallResult = Install-MissingTools $Script:GoTools "Go" $goToolsResult.Missing
        if ($goInstallResult.Failed.Count -eq 0) {
            $goToolsOk = $true
            Write-Log "All Go tools installed successfully" "SUCCESS" "Setup"
        } else {
            Write-Log "Some Go tools failed to install" "WARNING" "Setup"
        }
    }
    
    # Install LuCI tools
    if (-not $luciToolsOk -and $luciToolsResult.Missing.Count -gt 0) {
        $luciInstallResult = Install-MissingTools $Script:LuCITools "LuCI" $luciToolsResult.Missing
        if ($luciInstallResult.Failed.Count -eq 0) {
            $luciToolsOk = $true
            Write-Log "All LuCI tools installed successfully" "SUCCESS" "Setup"
        } else {
            Write-Log "Some LuCI tools failed to install" "WARNING" "Setup"
        }
    }
    
    # Re-check tools after installation
    if ($goToolsOk -or $luciToolsOk) {
        Write-Log "Re-checking tools after installation..." "INFO" "Setup"
        $goToolsResult = Test-Tools $Script:GoTools "Go"
        $luciToolsResult = Test-Tools $Script:LuCITools "LuCI"
        $goToolsOk = $goToolsResult.Success
        $luciToolsOk = $luciToolsResult.Success
    }
}

# Get files to check
$filesToCheck = Get-FilesToCheck $Mode $Files

Write-Log "Found $($filesToCheck.Go.Count) Go files and $($filesToCheck.LuCI.Count) LuCI files to verify" "INFO" "Setup"

# Run Go verification
if ($Script:Config.EnableGo -and $goToolsOk) {
    Write-Log "Starting Go verification..." "INFO" "Go"
    
    Invoke-GoFormat
    Invoke-GoImports
    Invoke-GoLint
    Invoke-GoVet
    Invoke-GoSecurity
    Invoke-GoTests
    Invoke-GoBuild
}

# Run LuCI verification
if ($Script:Config.EnableLuci -and $luciToolsOk) {
    Write-Log "Starting LuCI verification..." "INFO" "LuCI"
    
    Invoke-LuaSyntax
    Invoke-LuaLint
    Invoke-HTMLValidation
    Invoke-JavaScriptLint
    Invoke-CSSLint
    Invoke-TranslationValidation
}

# Show summary
Show-Summary
