# BugBot Repository-Wide Scan Script

This script automates the process of creating a Pull Request to trigger BugBot scanning of your entire repository.

## Overview

Since BugBot only works with Pull Requests, this script creates a PR containing all current files in your repository, allowing BugBot to perform a comprehensive analysis.

## Prerequisites

1. **GitHub CLI (gh)** - Install from [cli.github.com](https://cli.github.com/)
2. **GitHub Authentication** - Run `gh auth login` to authenticate
3. **Repository Access** - You need write access to the repository

## Installation

The script is already executable. If you need to make it executable again:

```bash
chmod +x scripts/bugbot-scan.sh
```

## Usage

### Basic Usage
```bash
./scripts/bugbot-scan.sh
```

### Help
```bash
./scripts/bugbot-scan.sh --help
```

## What the Script Does

1. **Pre-flight Checks**
   - Verifies you're in a git repository
   - Checks for uncommitted changes (includes them in scan)
   - Validates GitHub CLI installation and authentication
   - Determines repository owner/name from remote URL

2. **Branch Creation**
   - Creates a new branch with timestamp: `bugbot-scan-YYYYMMDD-HHMMSS`
   - Adds all current files (including uncommitted changes)
   - Commits everything with a descriptive message

3. **Pull Request Creation**
   - Pushes the branch to GitHub
   - Creates a PR with descriptive title and body
   - Provides the PR URL for monitoring

4. **BugBot Integration**
   - BugBot automatically detects the new PR
   - Begins analysis of all included files
   - You can also manually trigger with `bugbot run` comment

## Example Output

```
[INFO] Starting BugBot repository-wide scan process...
[INFO] Timestamp: Mon Jan 15 14:30:25 PST 2024

[INFO] Git repository detected
[WARNING] You have uncommitted changes. They will be included in the scan.
[INFO] Uncommitted files:
  M BUILD_ERRORS_ANALYSIS.md
  M VERSION
  A src/c/autonomy-daemon/starlink/starlink_grpc_comprehensive_client.c
  ...

[SUCCESS] GitHub CLI is installed and authenticated
[INFO] Repository: markus-lassfolk/autonomy

[INFO] Proceeding with scan setup...
[INFO] Branch name: bugbot-scan-20240115-143025
[INFO] PR title: BugBot Repository-Wide Scan - 2024-01-15 14:30

[INFO] Creating new branch: bugbot-scan-20240115-143025
[INFO] Adding all files to the branch...
[SUCCESS] Branch created and committed: bugbot-scan-20240115-143025
[INFO] Pushing branch to remote...
[SUCCESS] Branch pushed to remote
[INFO] Creating pull request...
[SUCCESS] Pull request created successfully!
[INFO] PR URL: https://github.com/markus-lassfolk/autonomy/pull/123
[INFO] BugBot should automatically start analyzing this PR
[INFO] You can also manually trigger BugBot by commenting 'bugbot run' in the PR

[SUCCESS] BugBot scan setup completed successfully!
[INFO] Next steps:
[INFO] 1. Wait for BugBot to analyze the PR (usually takes a few minutes)
[INFO] 2. Review BugBot's findings in the PR comments
[INFO] 3. Close the PR when analysis is complete
[INFO] 4. The scan branch will be automatically cleaned up when the PR is closed
```

## After Running the Script

1. **Wait for BugBot** - Usually takes a few minutes to start analysis
2. **Monitor the PR** - Check the PR page for BugBot comments
3. **Review Findings** - BugBot will highlight potential issues
4. **Close PR** - When analysis is complete, close the PR
5. **Cleanup** - The branch will be automatically cleaned up

## Troubleshooting

### GitHub CLI Not Installed
```bash
# Ubuntu/Debian
curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg | sudo dd of=/usr/share/keyrings/githubcli-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" | sudo tee /etc/apt/sources.list.d/github-cli.list > /dev/null
sudo apt update && sudo apt install gh
```

### Not Authenticated
```bash
gh auth login
```

### Repository Access Issues
- Ensure you have write access to the repository
- Check that the remote URL is correct: `git remote -v`

## Safety Features

- **Uncommitted Changes Warning** - Script warns about uncommitted changes
- **User Confirmation** - Asks for confirmation before proceeding
- **Error Handling** - Comprehensive error checking and cleanup
- **Cleanup on Failure** - Automatically cleans up if script fails

## Notes

- The script includes ALL files in your repository, including uncommitted changes
- Each run creates a unique branch with timestamp
- PRs are created against the `main` branch
- BugBot analysis is automatic but can be manually triggered with comments
