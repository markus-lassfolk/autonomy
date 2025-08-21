#!/usr/bin/env pwsh

<#
.SYNOPSIS
    Comprehensive test runner for autonomy project

.DESCRIPTION
    Runs all tests including unit tests, integration tests, performance tests,
    and validation tests. Generates detailed reports and handles test failures.

.PARAMETER TestType
    Type of tests to run: "all", "unit", "integration", "performance", "validation"

.PARAMETER OutputDir
    Directory to store test results and reports

.PARAMETER Verbose
    Enable verbose output

.EXAMPLE
    .\run-comprehensive-tests.ps1 -TestType "all" -OutputDir "test-results"

.EXAMPLE
    .\run-comprehensive-tests.ps1 -TestType "integration" -Verbose
#>

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet("all", "unit", "integration", "performance", "validation")]
    [string]$TestType = "all",
    
    [Parameter(Mandatory=$false)]
    [string]$OutputDir = "test-results",
    
    [Parameter(Mandatory=$false)]
    [switch]$Verbose,
    
    [Parameter(Mandatory=$false)]
    [switch]$GenerateReport,
    
    [Parameter(Mandatory=$false)]
    [switch]$StopOnFailure
)

# Set error action preference
$ErrorActionPreference = if ($StopOnFailure) { "Stop" } else { "Continue" }

# Create output directory
if (!(Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

# Initialize test results
$TestResults = @{
    StartTime = Get-Date
    Tests = @()
    Summary = @{
        Total = 0
        Passed = 0
        Failed = 0
        Skipped = 0
        Duration = 0
    }
}

# Function to log messages
function Write-TestLog {
    param(
        [string]$Message,
        [string]$Level = "INFO"
    )
    
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $logMessage = "[$timestamp] [$Level] $Message"
    
    if ($Verbose -or $Level -eq "ERROR" -or $Level -eq "WARN") {
        Write-Host $logMessage
    }
    
    # Add to test results
    $TestResults.Tests += @{
        Timestamp = $timestamp
        Level = $Level
        Message = $Message
    }
}

# Function to run Go tests
function Invoke-GoTests {
    param(
        [string]$Package,
        [string]$TestName,
        [string]$Args = ""
    )
    
    Write-TestLog "Running $TestName tests for package: $Package"
    
    $testStart = Get-Date
    $testOutput = ""
    $testExitCode = 0
    
    try {
        $testCmd = "go test -v -timeout=5m ./$Package $Args"
        Write-TestLog "Executing: $testCmd"
        
        $testOutput = & cmd /c $testCmd 2>&1
        $testExitCode = $LASTEXITCODE
    }
    catch {
        Write-TestLog "Error running tests: $($_.Exception.Message)" -Level "ERROR"
        $testExitCode = 1
    }
    
    $testEnd = Get-Date
    $testDuration = ($testEnd - $testStart).TotalSeconds
    
    # Parse test results
    $passed = 0
    $failed = 0
    $skipped = 0
    
    if ($testOutput) {
        $passed = ($testOutput | Select-String "PASS" | Measure-Object).Count
        $failed = ($testOutput | Select-String "FAIL" | Measure-Object).Count
        $skipped = ($testOutput | Select-String "SKIP" | Measure-Object).Count
    }
    
    # Update summary
    $TestResults.Summary.Total += ($passed + $failed + $skipped)
    $TestResults.Summary.Passed += $passed
    $TestResults.Summary.Failed += $failed
    $TestResults.Summary.Skipped += $skipped
    $TestResults.Summary.Duration += $testDuration
    
    # Log results
    if ($testExitCode -eq 0) {
        Write-TestLog "$TestName tests completed successfully: $passed passed, $failed failed, $skipped skipped (${testDuration}s)" -Level "INFO"
    } else {
        Write-TestLog "$TestName tests failed: $passed passed, $failed failed, $skipped skipped (${testDuration}s)" -Level "ERROR"
    }
    
    # Save test output
    $outputFile = Join-Path $OutputDir "$TestName-output.txt"
    $testOutput | Out-File -FilePath $outputFile -Encoding UTF8
    
    return @{
        Success = ($testExitCode -eq 0)
        Passed = $passed
        Failed = $failed
        Skipped = $skipped
        Duration = $testDuration
        OutputFile = $outputFile
    }
}

# Function to run integration tests
function Invoke-IntegrationTests {
    Write-TestLog "Running integration tests"
    
    $integrationResults = @()
    
    # UCI Integration tests
    $uciResult = Invoke-GoTests -Package "test/integration" -TestName "UCI-Integration" -Args "-run TestUCIIntegration"
    $integrationResults += @{
        Name = "UCI Integration"
        Result = $uciResult
    }
    
    # End-to-end tests
    $e2eResult = Invoke-GoTests -Package "test/integration" -TestName "End-to-End" -Args "-run TestEndToEnd"
    $integrationResults += @{
        Name = "End-to-End"
        Result = $e2eResult
    }
    
    return $integrationResults
}

# Function to run performance tests
function Invoke-PerformanceTests {
    Write-TestLog "Running performance tests"
    
    $perfResults = @()
    
    # UCI Performance tests
    $uciPerfResult = Invoke-GoTests -Package "test/integration" -TestName "UCI-Performance" -Args "-run TestUCIPerformance"
    $perfResults += @{
        Name = "UCI Performance"
        Result = $uciPerfResult
    }
    
    # Memory usage tests
    $memoryResult = Invoke-GoTests -Package "pkg/telem" -TestName "Memory-Usage" -Args "-run TestMemoryUsage"
    $perfResults += @{
        Name = "Memory Usage"
        Result = $memoryResult
    }
    
    # CPU usage tests
    $cpuResult = Invoke-GoTests -Package "pkg/decision" -TestName "CPU-Usage" -Args "-run TestCPUUsage"
    $perfResults += @{
        Name = "CPU Usage"
        Result = $cpuResult
    }
    
    return $perfResults
}

# Function to run validation tests
function Invoke-ValidationTests {
    Write-TestLog "Running validation tests"
    
    $validationResults = @()
    
    # Configuration validation tests
    $configValidationResult = Invoke-GoTests -Package "pkg/uci" -TestName "Config-Validation" -Args "-run TestConfigValidation"
    $validationResults += @{
        Name = "Configuration Validation"
        Result = $configValidationResult
    }
    
    # UCI validation tests
    $uciValidationResult = Invoke-GoTests -Package "pkg/uci" -TestName "UCI-Validation" -Args "-run TestUCIValidation"
    $validationResults += @{
        Name = "UCI Validation"
        Result = $uciValidationResult
    }
    
    return $validationResults
}

# Function to run unit tests
function Invoke-UnitTests {
    Write-TestLog "Running unit tests"
    
    $unitResults = @()
    
    # Core packages
    $packages = @(
        "pkg/collector",
        "pkg/decision", 
        "pkg/gps",
        "pkg/logx",
        "pkg/notifications",
        "pkg/starlink",
        "pkg/telem",
        "pkg/uci",
        "pkg/ubus"
    )
    
    foreach ($package in $packages) {
        $result = Invoke-GoTests -Package $package -TestName "Unit-$package"
        $unitResults += @{
            Name = $package
            Result = $result
        }
    }
    
    return $unitResults
}

# Function to generate test report
function New-TestReport {
    param(
        [string]$OutputPath
    )
    
    Write-TestLog "Generating test report: $OutputPath"
    
    $report = @{
        GeneratedAt = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        Summary = $TestResults.Summary
        TestResults = $TestResults.Tests
        Duration = ($TestResults.Summary.Duration).ToString("F2")
        SuccessRate = if ($TestResults.Summary.Total -gt 0) { 
            (($TestResults.Summary.Passed / $TestResults.Summary.Total) * 100).ToString("F1") 
        } else { "0.0" }
    }
    
    # Convert to JSON and save
    $reportJson = $report | ConvertTo-Json -Depth 10
    $reportJson | Out-File -FilePath $OutputPath -Encoding UTF8
    
    Write-TestLog "Test report saved to: $OutputPath"
}

# Function to generate HTML report
function New-HTMLReport {
    param(
        [string]$OutputPath
    )
    
    Write-TestLog "Generating HTML report: $OutputPath"
    
    $html = @"
<!DOCTYPE html>
<html>
<head>
    <title>autonomy Test Results</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .header { background-color: #f0f0f0; padding: 20px; border-radius: 5px; }
        .summary { display: flex; gap: 20px; margin: 20px 0; }
        .summary-item { 
            background-color: #e8f4fd; 
            padding: 15px; 
            border-radius: 5px; 
            text-align: center;
            flex: 1;
        }
        .passed { background-color: #d4edda; }
        .failed { background-color: #f8d7da; }
        .skipped { background-color: #fff3cd; }
        .log { 
            background-color: #f8f9fa; 
            padding: 10px; 
            border-radius: 3px; 
            font-family: monospace; 
            margin: 5px 0;
        }
        .error { color: #dc3545; }
        .warn { color: #ffc107; }
        .info { color: #17a2b8; }
    </style>
</head>
<body>
    <div class="header">
        <h1>autonomy Test Results</h1>
        <p>Generated at: $($TestResults.StartTime.ToString("yyyy-MM-dd HH:mm:ss"))</p>
    </div>
    
    <div class="summary">
        <div class="summary-item">
            <h3>Total Tests</h3>
            <h2>$($TestResults.Summary.Total)</h2>
        </div>
        <div class="summary-item passed">
            <h3>Passed</h3>
            <h2>$($TestResults.Summary.Passed)</h2>
        </div>
        <div class="summary-item failed">
            <h3>Failed</h3>
            <h2>$($TestResults.Summary.Failed)</h2>
        </div>
        <div class="summary-item skipped">
            <h3>Skipped</h3>
            <h2>$($TestResults.Summary.Skipped)</h2>
        </div>
        <div class="summary-item">
            <h3>Duration</h3>
            <h2>$($TestResults.Summary.Duration.ToString("F1"))s</h2>
        </div>
    </div>
    
    <h2>Test Log</h2>
"@

    foreach ($test in $TestResults.Tests) {
        $cssClass = switch ($test.Level) {
            "ERROR" { "error" }
            "WARN" { "warn" }
            "INFO" { "info" }
            default { "" }
        }
        
        $html += @"
    <div class="log $cssClass">
        <strong>[$($test.Level)]</strong> $($test.Message)
    </div>
"@
    }
    
    $html += @"
</body>
</html>
"@
    
    $html | Out-File -FilePath $OutputPath -Encoding UTF8
    Write-TestLog "HTML report saved to: $OutputPath"
}

# Main execution
try {
    Write-TestLog "Starting comprehensive test suite"
    Write-TestLog "Test type: $TestType"
    Write-TestLog "Output directory: $OutputDir"
    
    # Check if Go is available
    try {
        $goVersion = & go version 2>&1
        Write-TestLog "Go version: $goVersion"
    }
    catch {
        Write-TestLog "Go is not available or not in PATH" -Level "ERROR"
        exit 1
    }
    
    # Run tests based on type
    switch ($TestType) {
        "all" {
            Write-TestLog "Running all test types"
            $unitResults = Invoke-UnitTests
            $integrationResults = Invoke-IntegrationTests
            $performanceResults = Invoke-PerformanceTests
            $validationResults = Invoke-ValidationTests
        }
        "unit" {
            $unitResults = Invoke-UnitTests
        }
        "integration" {
            $integrationResults = Invoke-IntegrationTests
        }
        "performance" {
            $performanceResults = Invoke-PerformanceTests
        }
        "validation" {
            $validationResults = Invoke-ValidationTests
        }
    }
    
    # Calculate final summary
    $endTime = Get-Date
    $totalDuration = ($endTime - $TestResults.StartTime).TotalSeconds
    
    Write-TestLog "Test suite completed in ${totalDuration}s"
    Write-TestLog "Summary: $($TestResults.Summary.Total) total, $($TestResults.Summary.Passed) passed, $($TestResults.Summary.Failed) failed, $($TestResults.Summary.Skipped) skipped"
    
    # Generate reports if requested
    if ($GenerateReport) {
        $jsonReportPath = Join-Path $OutputDir "test-results.json"
        $htmlReportPath = Join-Path $OutputDir "test-results.html"
        
        New-TestReport -OutputPath $jsonReportPath
        New-HTMLReport -OutputPath $htmlReportPath
    }
    
    # Exit with appropriate code
    if ($TestResults.Summary.Failed -gt 0) {
        Write-TestLog "Some tests failed" -Level "ERROR"
        exit 1
    } else {
        Write-TestLog "All tests passed successfully" -Level "INFO"
        exit 0
    }
}
catch {
    Write-TestLog "Test suite failed with error: $($_.Exception.Message)" -Level "ERROR"
    exit 1
}

