#!/bin/bash

# BugBot Repository-Wide Scan Script
# This script creates a PR with all current files to trigger BugBot scanning

set -e  # Exit on any error

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"
BRANCH_PREFIX="bugbot-scan"
TIMESTAMP=$(date +"%Y%m%d-%H%M%S")
BRANCH_NAME="${BRANCH_PREFIX}-${TIMESTAMP}"
PR_TITLE="BugBot Repository-Wide Scan - $(date +"%Y-%m-%d %H:%M")"
PR_BODY="This PR was automatically created to trigger BugBot scanning of the entire repository.

## Purpose
- Comprehensive code analysis using BugBot
- Review all files in the repository for potential issues
- Automated scan trigger

## Files Included
This PR includes all current files in the repository for BugBot analysis.

## Next Steps
1. BugBot will automatically analyze this PR
2. Review BugBot's findings
3. Close this PR after analysis is complete

---
*This PR was created by the bugbot-scan.sh script*"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if we're in a git repository
check_git_repo() {
    if ! git rev-parse --git-dir > /dev/null 2>&1; then
        log_error "Not in a git repository!"
        exit 1
    fi
    log_info "Git repository detected"
}

# Check if we have uncommitted changes
check_working_directory() {
    if ! git diff-index --quiet HEAD --; then
        log_warning "You have uncommitted changes. They will be included in the scan."
        log_info "Uncommitted files:"
        git status --porcelain | sed 's/^/  /'
        echo
        read -p "Continue anyway? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            log_info "Aborted by user"
            exit 0
        fi
    fi
}

# Check if GitHub CLI is installed
check_gh_cli() {
    if ! command -v gh &> /dev/null; then
        log_error "GitHub CLI (gh) is not installed!"
        log_info "Please install it from: https://cli.github.com/"
        log_info "Or use: curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg | sudo dd of=/usr/share/keyrings/githubcli-archive-keyring.gpg"
        log_info "Then: echo \"deb [arch=\$(dpkg --print-architecture) signed-by=/usr/share/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main\" | sudo tee /etc/apt/sources.list.d/github-cli.list > /dev/null"
        log_info "Finally: sudo apt update && sudo apt install gh"
        exit 1
    fi
    
    # Check if user is authenticated
    if ! gh auth status &> /dev/null; then
        log_error "GitHub CLI is not authenticated!"
        log_info "Please run: gh auth login"
        exit 1
    fi
    
    log_success "GitHub CLI is installed and authenticated"
}

# Get repository information
get_repo_info() {
    REPO_URL=$(git remote get-url origin)
    if [[ $REPO_URL == *"github.com"* ]]; then
        REPO_OWNER=$(echo $REPO_URL | sed -n 's/.*github\.com[:/]\([^/]*\)\/\([^/]*\)\.git.*/\1/p')
        REPO_NAME=$(echo $REPO_URL | sed -n 's/.*github\.com[:/]\([^/]*\)\/\([^/]*\)\.git.*/\2/p')
        if [[ -z "$REPO_OWNER" || -z "$REPO_NAME" ]]; then
            # Try without .git extension
            REPO_OWNER=$(echo $REPO_URL | sed -n 's/.*github\.com[:/]\([^/]*\)\/\([^/]*\).*/\1/p')
            REPO_NAME=$(echo $REPO_URL | sed -n 's/.*github\.com[:/]\([^/]*\)\/\([^/]*\).*/\2/p')
        fi
    else
        log_error "This doesn't appear to be a GitHub repository!"
        log_info "Repository URL: $REPO_URL"
        exit 1
    fi
    
    if [[ -z "$REPO_OWNER" || -z "$REPO_NAME" ]]; then
        log_error "Could not determine repository owner and name from URL: $REPO_URL"
        exit 1
    fi
    
    log_info "Repository: $REPO_OWNER/$REPO_NAME"
}

# Create new branch and commit all changes
create_scan_branch() {
    log_info "Creating new branch: $BRANCH_NAME"
    
    # Create and checkout new branch
    git checkout -b "$BRANCH_NAME"
    
    # Add all files (including untracked)
    log_info "Adding all files to the branch..."
    git add .
    
    # Check if there are any changes to commit
    if git diff --cached --quiet; then
        log_warning "No changes to commit. Repository is already up to date."
        git checkout main
        git branch -D "$BRANCH_NAME"
        log_info "No PR needed - repository is clean"
        exit 0
    fi
    
    # Commit all changes
    git commit -m "Add all files for BugBot repository-wide scan

- Includes all current files in the repository
- Triggered by bugbot-scan.sh script
- Timestamp: $(date)"
    
    log_success "Branch created and committed: $BRANCH_NAME"
}

# Push branch to remote
push_branch() {
    log_info "Pushing branch to remote..."
    git push -u origin "$BRANCH_NAME"
    log_success "Branch pushed to remote"
}

# Create pull request
create_pull_request() {
    log_info "Creating pull request..."
    
    # Create PR using GitHub CLI
    PR_URL=$(gh pr create \
        --title "$PR_TITLE" \
        --body "$PR_BODY" \
        --base main \
        --head "$BRANCH_NAME" \
        --repo "$REPO_OWNER/$REPO_NAME")
    
    if [[ $? -eq 0 ]]; then
        log_success "Pull request created successfully!"
        log_info "PR URL: $PR_URL"
        log_info "BugBot should automatically start analyzing this PR"
        log_info "You can also manually trigger BugBot by commenting 'bugbot run' in the PR"
    else
        log_error "Failed to create pull request"
        exit 1
    fi
}

# Cleanup function
cleanup() {
    if [[ $? -ne 0 ]]; then
        log_warning "Script failed. Cleaning up..."
        # Switch back to main branch if we're on the scan branch
        current_branch=$(git branch --show-current)
        if [[ "$current_branch" == "$BRANCH_NAME" ]]; then
            git checkout main
            git branch -D "$BRANCH_NAME" 2>/dev/null || true
        fi
    fi
}

# Main execution
main() {
    log_info "Starting BugBot repository-wide scan process..."
    log_info "Timestamp: $(date)"
    echo
    
    # Set up cleanup trap
    trap cleanup EXIT
    
    # Pre-flight checks
    check_git_repo
    check_working_directory
    check_gh_cli
    get_repo_info
    
    echo
    log_info "Proceeding with scan setup..."
    log_info "Branch name: $BRANCH_NAME"
    log_info "PR title: $PR_TITLE"
    echo
    
    # Create the scan
    create_scan_branch
    push_branch
    create_pull_request
    
    echo
    log_success "BugBot scan setup completed successfully!"
    log_info "Next steps:"
    log_info "1. Wait for BugBot to analyze the PR (usually takes a few minutes)"
    log_info "2. Review BugBot's findings in the PR comments"
    log_info "3. Close the PR when analysis is complete"
    log_info "4. The scan branch will be automatically cleaned up when the PR is closed"
}

# Show help if requested
if [[ "$1" == "--help" || "$1" == "-h" ]]; then
    echo "BugBot Repository-Wide Scan Script"
    echo
    echo "This script creates a PR with all current files to trigger BugBot scanning."
    echo
    echo "Usage: $0 [options]"
    echo
    echo "Options:"
    echo "  -h, --help    Show this help message"
    echo
    echo "Requirements:"
    echo "  - Git repository with GitHub remote"
    echo "  - GitHub CLI (gh) installed and authenticated"
    echo "  - Write access to the repository"
    echo
    echo "The script will:"
    echo "  1. Create a new branch with timestamp"
    echo "  2. Add all current files (including uncommitted changes)"
    echo "  3. Push the branch to GitHub"
    echo "  4. Create a pull request"
    echo "  5. BugBot will automatically analyze the PR"
    echo
    exit 0
fi

# Run main function
main "$@"
