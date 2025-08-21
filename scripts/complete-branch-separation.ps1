# Complete Branch Separation Script for Autonomy Project (PowerShell)
# This script properly separates files between main (infrastructure) and main-dev (project) branches

Write-Host "🌿 Complete Branch Separation for Autonomy Project" -ForegroundColor Green
Write-Host "=================================================" -ForegroundColor Green

# Check if we're in a git repository
if (-not (Test-Path ".git")) {
    Write-Host "❌ Not in a git repository. Please run this script from the project root." -ForegroundColor Red
    exit 1
}

# Check current branch
$currentBranch = git branch --show-current
Write-Host "ℹ️  Current branch: $currentBranch" -ForegroundColor Blue

# Files that should ONLY be on MAIN branch (infrastructure)
$mainOnlyFiles = @(
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

# Files that should ONLY be on MAIN-DEV branch (project code)
$mainDevOnlyFiles = @(
    "pkg",
    "cmd",
    "test",
    "go.mod",
    "go.sum",
    "Makefile",
    "TODO.md",
    "etc"
)

function Clean-MainBranch {
    Write-Host "🧹 Cleaning main branch to contain only infrastructure files..." -ForegroundColor Blue
    
    # Switch to main branch
    git checkout main
    
    # Remove project files from main branch
    foreach ($file in $mainDevOnlyFiles) {
        if (Test-Path $file) {
            git rm -r --cached $file
            Write-Host "🗑️  Removed $file from main branch" -ForegroundColor Yellow
        }
    }
    
    # Add infrastructure files
    foreach ($file in $mainOnlyFiles) {
        if (Test-Path $file) {
            git add $file
            Write-Host "✅ Added $file to main branch" -ForegroundColor Green
        } else {
            Write-Host "⚠️  File $file not found, skipping" -ForegroundColor Yellow
        }
    }
    
    # Commit changes
    $status = git status --porcelain
    if ($status) {
        git commit -m "🏗️ Complete infrastructure separation on main branch

- Removed all project source code files
- Kept only infrastructure, workflows, and documentation
- Clean separation between infrastructure and project code

This branch now contains only infrastructure and documentation files."
        Write-Host "✅ Committed infrastructure separation to main branch" -ForegroundColor Green
    } else {
        Write-Host "ℹ️  No changes to commit on main branch" -ForegroundColor Blue
    }
}

function Clean-MainDevBranch {
    Write-Host "🧹 Cleaning main-dev branch to contain only project files..." -ForegroundColor Blue
    
    # Switch to main-dev branch
    git checkout main-dev
    
    # Remove infrastructure files from main-dev branch
    foreach ($file in $mainOnlyFiles) {
        if (Test-Path $file) {
            git rm -r --cached $file
            Write-Host "🗑️  Removed $file from main-dev branch" -ForegroundColor Yellow
        }
    }
    
    # Add project files
    foreach ($file in $mainDevOnlyFiles) {
        if (Test-Path $file) {
            git add $file
            Write-Host "✅ Added $file to main-dev branch" -ForegroundColor Green
        } else {
            Write-Host "⚠️  File $file not found, skipping" -ForegroundColor Yellow
        }
    }
    
    # Commit changes
    $status = git status --porcelain
    if ($status) {
        git commit -m "🚀 Complete project code separation on main-dev branch

- Removed all infrastructure and documentation files
- Kept only project source code and dependencies
- Clean separation between infrastructure and project code

This branch now contains only project source code and development files."
        Write-Host "✅ Committed project separation to main-dev branch" -ForegroundColor Green
    } else {
        Write-Host "ℹ️  No changes to commit on main-dev branch" -ForegroundColor Blue
    }
}

function Show-BranchStructure {
    Write-Host ""
    Write-Host "📋 Next steps:" -ForegroundColor Cyan
    Write-Host "1. Push both branches:" -ForegroundColor White
    Write-Host "   git push origin main" -ForegroundColor Gray
    Write-Host "   git push origin main-dev" -ForegroundColor Gray
    Write-Host ""
    Write-Host "2. Set up branch protection rules in GitHub" -ForegroundColor White
    Write-Host "3. Configure default branch settings" -ForegroundColor White
    Write-Host "4. Test the synchronization workflow" -ForegroundColor White
    Write-Host ""
    Write-Host "🌿 Branch structure:" -ForegroundColor Cyan
    Write-Host "   main: Infrastructure, workflows, documentation" -ForegroundColor White
    Write-Host "   main-dev: Project source code, tests, dependencies" -ForegroundColor White
}

# Main execution
Write-Host ""
Write-Host "ℹ️  Starting complete branch separation..." -ForegroundColor Blue

# Check if branches exist
$branches = git branch --list
$hasMain = $branches -match "main$" -and $branches -notmatch "main-dev"
$hasMainDev = $branches -match "main-dev"

if (-not $hasMain) {
    Write-Host "❌ Main branch does not exist" -ForegroundColor Red
    exit 1
}

if (-not $hasMainDev) {
    Write-Host "❌ Main-dev branch does not exist" -ForegroundColor Red
    exit 1
}

Write-Host "✅ Both branches exist, proceeding with separation..." -ForegroundColor Green

# Clean and separate files
Clean-MainBranch
Clean-MainDevBranch

# Show next steps
Show-BranchStructure
