# Branch Organization Script for Autonomy Project (PowerShell)
# This script helps organize files between main (infrastructure) and main-dev (project) branches

param(
    [switch]$Force
)

Write-Host "🌿 Organizing Autonomy Project Branches" -ForegroundColor Green
Write-Host "======================================" -ForegroundColor Green

# Check if we're in a git repository
if (-not (Test-Path ".git")) {
    Write-Host "❌ Not in a git repository. Please run this script from the project root." -ForegroundColor Red
    exit 1
}

# Check current branch
$currentBranch = git branch --show-current
Write-Host "ℹ️  Current branch: $currentBranch" -ForegroundColor Blue

# Files that should be on MAIN branch (infrastructure)
$mainFiles = @(
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

# Files that should be on MAIN-DEV branch (project code)
$mainDevFiles = @(
    "pkg",
    "cmd",
    "test",
    "go.mod",
    "go.sum",
    "Makefile",
    "TODO.md",
    "etc"
)

function Move-ToMain {
    Write-Host "ℹ️  Moving infrastructure files to main branch..." -ForegroundColor Blue
    
    # Switch to main branch
    git checkout main
    
    # Add and commit infrastructure files
    foreach ($file in $mainFiles) {
        if (Test-Path $file) {
            git add $file
            Write-Host "✅ Added $file to main branch" -ForegroundColor Green
        } else {
            Write-Host "⚠️  File $file not found, skipping" -ForegroundColor Yellow
        }
    }
    
    # Check if there are changes to commit
    $status = git status --porcelain
    if ($status) {
        git commit -m "🏗️ Add infrastructure files to main branch

- GitHub workflows and CI/CD configuration
- Documentation and setup guides
- Build scripts and deployment tools
- Configuration examples and schemas
- Project architecture and roadmap

This branch contains all infrastructure and documentation files."
        Write-Host "✅ Committed infrastructure files to main branch" -ForegroundColor Green
    } else {
        Write-Host "ℹ️  No changes to commit on main branch" -ForegroundColor Blue
    }
}

function Move-ToMainDev {
    Write-Host "ℹ️  Moving project files to main-dev branch..." -ForegroundColor Blue
    
    # Switch to main-dev branch
    git checkout main-dev
    
    # Add and commit project files
    foreach ($file in $mainDevFiles) {
        if (Test-Path $file) {
            git add $file
            Write-Host "✅ Added $file to main-dev branch" -ForegroundColor Green
        } else {
            Write-Host "⚠️  File $file not found, skipping" -ForegroundColor Yellow
        }
    }
    
    # Check if there are changes to commit
    $status = git status --porcelain
    if ($status) {
        git commit -m "🚀 Add project source code to main-dev branch

- Go packages and application code
- Tests and test infrastructure
- Dependencies (go.mod/go.sum)
- Build system (Makefile)
- Project-specific configuration

This branch contains all project source code and development files."
        Write-Host "✅ Committed project files to main-dev branch" -ForegroundColor Green
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
Write-Host "ℹ️  Starting branch organization..." -ForegroundColor Blue

# Check if branches exist
$branches = git branch --list
if ($branches -notcontains "* main") {
    Write-Host "❌ Main branch does not exist" -ForegroundColor Red
    exit 1
}

if ($branches -notcontains "  main-dev") {
    Write-Host "❌ Main-dev branch does not exist" -ForegroundColor Red
    exit 1
}

# Move files to appropriate branches
Move-ToMain
Move-ToMainDev

# Show next steps
Show-BranchStructure
