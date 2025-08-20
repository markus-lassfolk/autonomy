# Branch Testing Guide

**Version:** 2.7.1 | **Updated:** 2025-07-27

**Version:** 2.6.0 | **Updated:** 2025-07-24

**Version:** 2.5.0 | **Updated:** 2025-07-24

**Version:** 2.4.12 | **Updated:** 2025-07-21

**Version:** 2.4.11 | **Updated:** 2025-07-21

**Version:** 2.4.10 | **Updated:** 2025-07-21

**Version:** 2.4.9 | **Updated:** 2025-07-21

**Version:** 2.4.8 | **Updated:** 2025-07-21

**Version:** 2.4.7 | **Updated:** 2025-07-21

## Problem: Branch Download Issue

When working in a development branch (like `feature/testing-improvements`), the install script needs to download support
files from the **same branch**, not from main.

## Solution: Dynamic Branch Support

The install script now supports dynamic branch configuration via environment variables.

### For Main Branch (Production)

```bash
# Standard installation - downloads from main branch
curl -fL https://raw.githubusercontent.com/markus-lassfolk/autonomy/main/scripts/install.sh | sh
```

### For Development Branch Testing

```bash
# Test installation from feature branch
GITHUB_BRANCH="feature/testing-improvements" \
curl -fL https://raw.githubusercontent.com/markus-lassfolk/autonomy/feature/testing-improvements/scripts/install.sh | \
sh -s --
```

Or with DEBUG mode:

```bash
# Test with debug output
DEBUG=1 GITHUB_BRANCH="feature/testing-improvements" \
curl -fL https://raw.githubusercontent.com/markus-lassfolk/autonomy/feature/testing-improvements/scripts/install.sh | \
sh -s --
```

## How It Works

The install script now uses these environment variables:

- `GITHUB_BRANCH` - Which branch to download from (default: "main")
- `GITHUB_REPO` - Which repository to use (default: "markus-lassfolk/autonomy")
- `DEBUG` - Enable debug output (default: "0")

### Internal Logic

```bash
# Dynamic URL construction
GITHUB_BRANCH="${GITHUB_BRANCH:-main}"
GITHUB_REPO="${GITHUB_REPO:-markus-lassfolk/autonomy}"
BASE_URL="https://raw.githubusercontent.com/${GITHUB_REPO}/${GITHUB_BRANCH}"

# All downloads use the dynamic URL
download_file "$BASE_URL/scripts/validate-config.sh" "$INSTALL_DIR/scripts/validate-config.sh"
download_file "$BASE_URL/scripts/update-config.sh" "$INSTALL_DIR/scripts/update-config.sh"
download_file "$BASE_URL/config/config.template.sh" "$INSTALL_DIR/config/config.template.sh"
```

## Verification

After installation, the script shows which branch was used:

```text
Installation directory: /root/starlink-monitor
Configuration file: /root/starlink-monitor/config/config.sh
Uninstall script: /root/starlink-monitor/uninstall.sh
Scripts downloaded from: https://raw.githubusercontent.com/markus-lassfolk/autonomy/feature/testing-improvements

⚠ Development Mode: Using branch 'feature/testing-improvements'
  This is a testing/development installation
```

## Testing Workflow

1. **Development**: Work in branch, test with branch-specific install command
2. **Validation**: Ensure all new features work correctly
3. **Merge**: Create pull request and merge to main
4. **Production**: Standard install command now includes your changes

This ensures you're testing the actual code changes you've made, not the old main branch versions.
