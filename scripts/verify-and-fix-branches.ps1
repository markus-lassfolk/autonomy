# Verify and Fix Branch Organization Script
# This script checks the current state and fixes any issues

Write-Host "🔍 Verifying Branch Organization" -ForegroundColor Green
Write-Host "===============================" -ForegroundColor Green

# Check current branch
$currentBranch = git branch --show-current
Write-Host "ℹ️  Current branch: $currentBranch" -ForegroundColor Blue

# Define what should be on each branch
$mainBranchFiles = @(
    ".github",
    "scripts", 
    "docs",
    "configs",
    "README.md",
    "ARCHITECTURE.md",
    "ROADMAP.md",
    "STATUS.md",
    "PROJECT_INSTRUCTION.md",
    "AUTONOMOUS_WORKFLOWS_IMPLEMENTATION.md",
    "AUTONOMOUS_WORKFLOWS_COMPLETE.md",
    "github-todo.md",
    "ENHANCED_MONITORING_PWA_SUMMARY.md",
    "RUTOS_INTEGRATION_SUMMARY.md",
    "IMPLEMENTATION_SUMMARY.md",
    "azure",
    "luci",
    "package",
    "uci-schema",
    "vuci-app-autonomy",
    ".gitignore",
    ".cursorinstructions",
    ".cursorrules"
)

$mainDevBranchFiles = @(
    "pkg",
    "cmd",
    "test",
    "go.mod",
    "go.sum",
    "Makefile",
    "TODO.md",
    "etc"
)

function Test-MainBranch {
    Write-Host "`n🔍 Testing Main Branch (Infrastructure)" -ForegroundColor Cyan
    Write-Host "=====================================" -ForegroundColor Cyan
    
    git checkout main
    
    $issues = @()
    $correct = @()
    
    # Check for infrastructure files (should be present)
    foreach ($file in $mainBranchFiles) {
        if (Test-Path $file) {
            $correct += $file
            Write-Host "✅ $file" -ForegroundColor Green
        } else {
            $issues += "Missing: $file"
            Write-Host "❌ Missing: $file" -ForegroundColor Red
        }
    }
    
    # Check for project files (should NOT be present)
    foreach ($file in $mainDevBranchFiles) {
        if (Test-Path $file) {
            $issues += "Should not be present: $file"
            Write-Host "❌ Should not be present: $file" -ForegroundColor Red
        } else {
            Write-Host "✅ Correctly absent: $file" -ForegroundColor Green
        }
    }
    
    Write-Host "`n📊 Main Branch Summary:" -ForegroundColor Yellow
    Write-Host "   Correct files: $($correct.Count)" -ForegroundColor Green
    Write-Host "   Issues found: $($issues.Count)" -ForegroundColor Red
    
    return $issues
}

function Test-MainDevBranch {
    Write-Host "`n🔍 Testing Main-Dev Branch (Project Code)" -ForegroundColor Cyan
    Write-Host "=========================================" -ForegroundColor Cyan
    
    git checkout main-dev
    
    $issues = @()
    $correct = @()
    
    # Check for project files (should be present)
    foreach ($file in $mainDevBranchFiles) {
        if (Test-Path $file) {
            $correct += $file
            Write-Host "✅ $file" -ForegroundColor Green
        } else {
            $issues += "Missing: $file"
            Write-Host "❌ Missing: $file" -ForegroundColor Red
        }
    }
    
    # Check for infrastructure files (should NOT be present)
    foreach ($file in $mainBranchFiles) {
        if (Test-Path $file) {
            $issues += "Should not be present: $file"
            Write-Host "❌ Should not be present: $file" -ForegroundColor Red
        } else {
            Write-Host "✅ Correctly absent: $file" -ForegroundColor Green
        }
    }
    
    Write-Host "`n📊 Main-Dev Branch Summary:" -ForegroundColor Yellow
    Write-Host "   Correct files: $($correct.Count)" -ForegroundColor Green
    Write-Host "   Issues found: $($issues.Count)" -ForegroundColor Red
    
    return $issues
}

function Fix-BranchIssues {
    param($mainIssues, $mainDevIssues)
    
    Write-Host "`n🔧 Fixing Branch Issues" -ForegroundColor Yellow
    Write-Host "======================" -ForegroundColor Yellow
    
    if ($mainIssues.Count -eq 0 -and $mainDevIssues.Count -eq 0) {
        Write-Host "✅ No issues found! Branches are properly organized." -ForegroundColor Green
        return
    }
    
    Write-Host "❌ Issues detected. Manual intervention required." -ForegroundColor Red
    Write-Host "`n📋 Issues Summary:" -ForegroundColor Cyan
    
    if ($mainIssues.Count -gt 0) {
        Write-Host "`nMain Branch Issues:" -ForegroundColor Red
        foreach ($issue in $mainIssues) {
            Write-Host "  - $issue" -ForegroundColor Red
        }
    }
    
    if ($mainDevIssues.Count -gt 0) {
        Write-Host "`nMain-Dev Branch Issues:" -ForegroundColor Red
        foreach ($issue in $mainDevIssues) {
            Write-Host "  - $issue" -ForegroundColor Red
        }
    }
    
    Write-Host "`n💡 Recommended Actions:" -ForegroundColor Yellow
    Write-Host "1. Manually move files between branches" -ForegroundColor White
    Write-Host "2. Use git mv to move files properly" -ForegroundColor White
    Write-Host "3. Commit changes to each branch" -ForegroundColor White
    Write-Host "4. Push both branches" -ForegroundColor White
}

# Main execution
Write-Host "`n🚀 Starting branch verification..." -ForegroundColor Blue

$mainIssues = Test-MainBranch
$mainDevIssues = Test-MainDevBranch

Fix-BranchIssues -mainIssues $mainIssues -mainDevIssues $mainDevIssues

Write-Host "`n✅ Verification complete!" -ForegroundColor Green
