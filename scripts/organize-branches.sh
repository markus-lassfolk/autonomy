#!/bin/bash

# Branch Organization Script for Autonomy Project
# This script helps organize files between main (infrastructure) and main-dev (project) branches

set -e

echo "🌿 Organizing Autonomy Project Branches"
echo "======================================"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}✅ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

print_error() {
    echo -e "${RED}❌ $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ️  $1${NC}"
}

# Check if we're in a git repository
if ! git rev-parse --git-dir > /dev/null 2>&1; then
    print_error "Not in a git repository. Please run this script from the project root."
    exit 1
fi

# Check current branch
CURRENT_BRANCH=$(git branch --show-current)
print_info "Current branch: $CURRENT_BRANCH"

# Files that should be on MAIN branch (infrastructure)
MAIN_FILES=(
    ".github/"
    "scripts/"
    "docs/"
    "configs/"
    "README.md"
    "ARCHITECTURE.md"
    "ROADMAP.md"
    "STATUS.md"
    "PROJECT_INSTRUCTION.md"
    "AUTONOMOUS_WORKFLOWS_IMPLEMENTATION.md"
    "AUTONOMOUS_WORKFLOWS_COMPLETE.md"
    "github-todo.md"
    "ENHANCED_MONITORING_PWA_SUMMARY.md"
    "RUTOS_INTEGRATION_SUMMARY.md"
    "IMPLEMENTATION_SUMMARY.md"
    "azure/"
    "luci/"
    "package/"
    "uci-schema/"
    "vuci-app-autonomy/"
    ".gitignore"
    ".cursorinstructions"
    ".cursorrules"
)

# Files that should be on MAIN-DEV branch (project code)
MAIN_DEV_FILES=(
    "pkg/"
    "cmd/"
    "test/"
    "go.mod"
    "go.sum"
    "Makefile"
    "TODO.md"
    "etc/"
)

# Function to move files to main branch
move_to_main() {
    print_info "Moving infrastructure files to main branch..."
    
    # Switch to main branch
    git checkout main
    
    # Add and commit infrastructure files
    for file in "${MAIN_FILES[@]}"; do
        if [ -e "$file" ]; then
            git add "$file"
            print_status "Added $file to main branch"
        else
            print_warning "File $file not found, skipping"
        fi
    done
    
    # Commit changes
    if git diff --cached --quiet; then
        print_info "No changes to commit on main branch"
    else
        git commit -m "🏗️ Add infrastructure files to main branch

- GitHub workflows and CI/CD configuration
- Documentation and setup guides
- Build scripts and deployment tools
- Configuration examples and schemas
- Project architecture and roadmap

This branch contains all infrastructure and documentation files."
        print_status "Committed infrastructure files to main branch"
    fi
}

# Function to move files to main-dev branch
move_to_main_dev() {
    print_info "Moving project files to main-dev branch..."
    
    # Switch to main-dev branch
    git checkout main-dev
    
    # Add and commit project files
    for file in "${MAIN_DEV_FILES[@]}"; do
        if [ -e "$file" ]; then
            git add "$file"
            print_status "Added $file to main-dev branch"
        else
            print_warning "File $file not found, skipping"
        fi
    done
    
    # Commit changes
    if git diff --cached --quiet; then
        print_info "No changes to commit on main-dev branch"
    else
        git commit -m "🚀 Add project source code to main-dev branch

- Go packages and application code
- Tests and test infrastructure
- Dependencies (go.mod/go.sum)
- Build system (Makefile)
- Project-specific configuration

This branch contains all project source code and development files."
        print_status "Committed project files to main-dev branch"
    fi
}

# Function to create branch protection rules
setup_branch_protection() {
    print_info "Setting up branch protection rules..."
    
    # This would typically be done via GitHub API or web interface
    print_warning "Please manually configure branch protection rules:"
    echo "1. Go to Settings → Branches"
    echo "2. Add rule for 'main' branch:"
    echo "   - Require pull request reviews"
    echo "   - Require status checks to pass"
    echo "   - Restrict pushes to matching branches"
    echo "3. Add rule for 'main-dev' branch:"
    echo "   - Require pull request reviews"
    echo "   - Allow force pushes (for development)"
}

# Function to create workflow for branch synchronization
create_sync_workflow() {
    print_info "Creating branch synchronization workflow..."
    
    cat > .github/workflows/sync-branches.yml << 'EOF'
name: Sync Branches

on:
  push:
    branches: [main, main-dev]
  workflow_dispatch:

permissions:
  contents: write
  pull-requests: write

jobs:
  sync-infrastructure:
    runs-on: ubuntu-latest
    if: github.ref == 'refs/heads/main-dev'
    
    steps:
    - name: Checkout repository
      uses: actions/checkout@v4
      with:
        fetch-depth: 0
        
    - name: Sync infrastructure files to main
      run: |
        # Check if infrastructure files changed
        INFRA_CHANGED=false
        
        # Check for infrastructure file changes
        for file in .github/ scripts/ docs/ configs/ README.md ARCHITECTURE.md ROADMAP.md STATUS.md PROJECT_INSTRUCTION.md AUTONOMOUS_WORKFLOWS_*.md github-todo.md ENHANCED_MONITORING_PWA_SUMMARY.md RUTOS_INTEGRATION_SUMMARY.md IMPLEMENTATION_SUMMARY.md azure/ luci/ package/ uci-schema/ vuci-app-autonomy/ .gitignore .cursorinstructions .cursorrules; do
          if git diff --name-only HEAD~1 | grep -q "^$file"; then
            INFRA_CHANGED=true
            break
          fi
        done
        
        if [ "$INFRA_CHANGED" = "true" ]; then
          echo "Infrastructure files changed, syncing to main branch..."
          
          # Create PR to main branch
          gh pr create \
            --title "🔄 Sync infrastructure changes from main-dev" \
            --body "This PR syncs infrastructure changes from the main-dev branch to main.
            
            **Changes:**
            - Infrastructure files updated
            - Documentation updates
            - CI/CD configuration changes
            
            **Review required:** Please review and merge these infrastructure changes." \
            --base main \
            --head main-dev
        else
          echo "No infrastructure files changed, skipping sync"
        fi

  sync-project:
    runs-on: ubuntu-latest
    if: github.ref == 'refs/heads/main'
    
    steps:
    - name: Checkout repository
      uses: actions/checkout@v4
      with:
        fetch-depth: 0
        
    - name: Sync project files to main-dev
      run: |
        # Check if project files changed
        PROJECT_CHANGED=false
        
        # Check for project file changes
        for file in pkg/ cmd/ test/ go.mod go.sum Makefile TODO.md etc/; do
          if git diff --name-only HEAD~1 | grep -q "^$file"; then
            PROJECT_CHANGED=true
            break
          fi
        done
        
        if [ "$PROJECT_CHANGED" = "true" ]; then
          echo "Project files changed, syncing to main-dev branch..."
          
          # Create PR to main-dev branch
          gh pr create \
            --title "🔄 Sync project changes from main" \
            --body "This PR syncs project changes from the main branch to main-dev.
            
            **Changes:**
            - Project source code updates
            - Dependencies updated
            - Build system changes
            
            **Review required:** Please review and merge these project changes." \
            --base main-dev \
            --head main
        else
          echo "No project files changed, skipping sync"
        fi
EOF
    
    print_status "Created branch synchronization workflow"
}

# Main execution
main() {
    echo ""
    print_info "Starting branch organization..."
    
    # Check if branches exist
    if ! git show-ref --verify --quiet refs/heads/main; then
        print_error "Main branch does not exist"
        exit 1
    fi
    
    if ! git show-ref --verify --quiet refs/heads/main-dev; then
        print_error "Main-dev branch does not exist"
        exit 1
    fi
    
    # Move files to appropriate branches
    move_to_main
    move_to_main_dev
    
    # Create synchronization workflow
    create_sync_workflow
    
    # Setup instructions
    echo ""
    print_info "Branch organization complete!"
    echo ""
    echo "📋 Next steps:"
    echo "1. Push both branches:"
    echo "   git push origin main"
    echo "   git push origin main-dev"
    echo ""
    echo "2. Set up branch protection rules in GitHub"
    echo "3. Configure default branch settings"
    echo "4. Test the synchronization workflow"
    echo ""
    echo "🌿 Branch structure:"
    echo "   main: Infrastructure, workflows, documentation"
    echo "   main-dev: Project source code, tests, dependencies"
}

# Run main function
main "$@"
