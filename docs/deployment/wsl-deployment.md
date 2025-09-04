# Dynamic WSL Deployment Guide

This guide explains how to implement dynamic path detection for WSL deployment scripts, allowing you to work with different drive letters (D: at work, J: at home, etc.) without hardcoding paths.

## Problem Statement

When working with WSL deployment scripts, you often need to copy packages from Windows to WSL. However, the paths are hardcoded, making it difficult to work across different environments:

- **Work Environment**: D: drive with project at `D:\GitCursor\autonomy`
- **Home Environment**: J: drive with project at `J:\GithubCursor\autonomy`

## Solution: Dynamic Path Detection

### Working Implementation: Bash Script

The primary solution is a bash script that runs in WSL but can be called from PowerShell:

**`scripts/deploy-wsl-dynamic.sh`** - A working bash script that:
- Automatically detects your current drive (D: at work, J: at home, etc.)
- Builds dynamic paths for WSL mounting
- Supports both **OpenWrt** and **RUTOS** packages
- Copies packages from Windows to WSL using the correct `/mnt/[drive]` paths
- Installs packages via opkg in WSL
- Tests and starts the autonomy service

### PowerShell Wrapper (Optional)

Since you prefer PowerShell, you can create a simple PowerShell wrapper:

```powershell
# deploy-wsl-wrapper.ps1
param(
    [string]$Action = "menu",
    [string]$WSLName = "rutos-openwrt-test",
    [string]$Platform = "both"
)

# Function to print colored output
function Write-Status {
    param([string]$Message)
    Write-Host "[INFO] $Message" -ForegroundColor Cyan
}

function Write-Success {
    param([string]$Message)
    Write-Host "[SUCCESS] $Message" -ForegroundColor Green
}

# Get script directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$BashScript = Join-Path $ScriptDir "deploy-wsl-dynamic.sh"

# Check if bash script exists
if (!(Test-Path $BashScript)) {
    Write-Error "Bash script not found: $BashScript"
    exit 1
}

# Set environment variables for the bash script
$env:ACTION = $Action
$env:WSL_NAME = $WSLName

Write-Status "Running dynamic WSL deployment..."
Write-Status "Action: $Action"
Write-Status "WSL Name: $WSLName"

# Run the bash script
if ($Action -eq "menu") {
    # Interactive mode
    Write-Status "Starting interactive deployment menu..."
    bash $BashScript
} else {
    # Direct action
    Write-Status "Executing action: $Action"
    bash $BashScript -Action $Action -WSLName $WSLName
}

Write-Success "Deployment completed!"
```

## Usage Instructions

### 1. Direct Bash Usage (Recommended)

```bash
# Basic usage (interactive menu)
./scripts/deploy-wsl-dynamic.sh

# Direct deployment (both platforms)
./scripts/deploy-wsl-dynamic.sh -Action "1" -Platform "both"

# OpenWrt only
./scripts/deploy-wsl-dynamic.sh -Action "1" -Platform "openwrt"

# RUTOS only
./scripts/deploy-wsl-dynamic.sh -Action "1" -Platform "rutos"

# Just copy packages (both platforms)
./scripts/deploy-wsl-dynamic.sh -Action "2" -Platform "both"

# Show detected paths
./scripts/deploy-wsl-dynamic.sh -Action "6" -Platform "both"
```

### 2. PowerShell Wrapper Usage

```powershell
# Interactive menu (both platforms)
.\scripts\deploy-wsl-wrapper.ps1

# Direct deployment (both platforms)
.\scripts\deploy-wsl-wrapper.ps1 -Action "1" -WSLName "rutos-openwrt-test" -Platform "both"

# OpenWrt only
.\scripts\deploy-wsl-wrapper.ps1 -Action "1" -WSLName "rutos-openwrt-test" -Platform "openwrt"

# RUTOS only
.\scripts\deploy-wsl-wrapper.ps1 -Action "1" -WSLName "rutos-openwrt-test" -Platform "rutos"

# Just copy packages (both platforms)
.\scripts\deploy-wsl-wrapper.ps1 -Action "2" -Platform "both"
```

### 3. Environment Variables

You can also set environment variables:

```powershell
# Set environment variables
$env:ACTION = "1"
$env:WSL_NAME = "rutos-openwrt-test"
$env:PLATFORM = "both"

# Run the script
bash ./scripts/deploy-wsl-dynamic.sh
```

## How It Works

### 1. Dynamic Path Detection

The script automatically detects your current drive and builds appropriate paths for both platforms:

```bash
# Get current drive letter
CURRENT_DRIVE=$(pwd | sed 's/^\([A-Za-z]\):.*/\1/')
WSL_MOUNT_PATH="/mnt/${CURRENT_DRIVE,,}"

# Build paths for both platforms
PROJECT_ROOT="$WSL_MOUNT_PATH/GitCursor/autonomy"
BUILD_DIR_OPENWRT="$PROJECT_ROOT/build-openwrt"
PACKAGE_DIR_OPENWRT="$BUILD_DIR_OPENWRT/packages"
BUILD_DIR_RUTOS="$PROJECT_ROOT/build"
PACKAGE_DIR_RUTOS="$BUILD_DIR_RUTOS"
```

### 2. Path Conversion

Converts Windows paths to WSL paths:

```bash
# Convert Windows path to WSL path
convert_to_wsl_path() {
    local windows_path="$1"
    local current_drive="$2"
    local wsl_mount_path="$3"
    
    # Remove drive letter and convert backslashes
    local path_without_drive=$(echo "$windows_path" | sed "s/^${current_drive}://")
    local wsl_path=$(echo "$path_without_drive" | sed 's/\\/\//g')
    
    echo "$wsl_mount_path$wsl_path"
}
```

### 3. Multi-Platform Package Management

Copies and installs packages for both OpenWrt and RUTOS:

```bash
# OpenWrt packages
BUILD_DIR_OPENWRT="$PROJECT_ROOT/build-openwrt"
PACKAGE_DIR_OPENWRT="$BUILD_DIR_OPENWRT/packages"
# Packages: autonomy_*.ipk

# RUTOS packages  
BUILD_DIR_RUTOS="$PROJECT_ROOT/build"
PACKAGE_DIR_RUTOS="$BUILD_DIR_RUTOS"
# Packages: autonomy_*.ipk, luci-app-autonomy_*.ipk

# Copy packages to WSL
wsl -d "$WSL_NAME" -e bash -c "cp '$wsl_source_path' '$wsl_dest_path'"

# Install packages via opkg
wsl -d "$WSL_NAME" -e bash -c "opkg install '$wsl_package_path'"
```

## Service Management

The script ensures services are started during installation:

### 1. Automatic Service Start

When you use "Deploy All" (Option 1), the service is automatically:
- ✅ **Enabled** for auto-start on WSL boot
- ✅ **Started** immediately after installation
- ✅ **Verified** to be running with process check

### 2. Service Commands

```bash
# Enable service to start on boot
/etc/init.d/autonomy enable

# Start the service
/etc/init.d/autonomy start

# Check service status
/etc/init.d/autonomy status

# Check if process is running
pgrep autonomysysmgmt
```

## Testing Dynamic Path Detection

### 1. Test Path Detection

```bash
# Test from different drives
cd D:\GitCursor\autonomy
./scripts/deploy-wsl-dynamic.sh -Action "6" -Platform "both"  # Show paths

cd J:\GithubCursor\autonomy
./scripts/deploy-wsl-dynamic.sh -Action "6" -Platform "both"  # Show paths
```

### 2. Test Package Copy

```bash
# Test package copying (both platforms)
./scripts/deploy-wsl-dynamic.sh -Action "2" -Platform "both"  # Build and copy packages

# Test OpenWrt only
./scripts/deploy-wsl-dynamic.sh -Action "2" -Platform "openwrt"  # OpenWrt packages only

# Test RUTOS only
./scripts/deploy-wsl-dynamic.sh -Action "2" -Platform "rutos"  # RUTOS packages only
```

### 3. Test Full Deployment

```bash
# Test complete deployment (both platforms)
./scripts/deploy-wsl-dynamic.sh -Action "1" -Platform "both"  # Deploy all

# Test OpenWrt only
./scripts/deploy-wsl-dynamic.sh -Action "1" -Platform "openwrt"  # OpenWrt only

# Test RUTOS only
./scripts/deploy-wsl-dynamic.sh -Action "1" -Platform "rutos"  # RUTOS only
```

## Troubleshooting

### Common Issues

1. **Path Conversion Errors**
   - Ensure the drive letter is correctly detected
   - Check that the WSL mount path exists

2. **Package Not Found**
   - Verify packages exist in the build directory
   - Check the package pattern matches your files

3. **WSL Instance Not Found**
   - Ensure the WSL instance exists
   - Create it using the setup script first

### Debug Commands

```bash
# Debug path detection
./scripts/deploy-wsl-dynamic.sh -Action "6" -Platform "both"

# Debug OpenWrt package discovery
ls -la build-openwrt/packages/autonomy_*.ipk

# Debug RUTOS package discovery
ls -la build/autonomy_*.ipk
ls -la build/luci-app-autonomy_*.ipk

# Debug WSL path conversion
echo "Current drive: $(pwd | sed 's/^\([A-Za-z]\):.*/\1/')"
echo "WSL mount path: /mnt/$(pwd | sed 's/^\([A-Za-z]\):.*/\1/' | tr '[:upper:]' '[:lower:]')"
```

## Best Practices

1. **Always use dynamic path detection** instead of hardcoded paths
2. **Test on multiple drives** to ensure compatibility
3. **Use environment variables** for configuration when possible
4. **Provide clear error messages** when paths are not found
5. **Log all path operations** for debugging purposes

## Integration with CI/CD

For automated deployments, you can use environment variables:

```bash
# In CI/CD pipeline
export PROJECT_ROOT="$BUILD_SOURCESDIRECTORY"
export CURRENT_DRIVE=$(echo "$BUILD_SOURCESDIRECTORY" | cut -c1)
export WSL_MOUNT_PATH="/mnt/${CURRENT_DRIVE,,}"
export PLATFORM="both"

./scripts/deploy-wsl-dynamic.sh -Action "1" -Platform "both"
```

## Summary

This approach ensures your deployment scripts work consistently across different environments without requiring manual path updates. The bash script provides the core functionality, while the PowerShell wrapper gives you the Windows-native interface you prefer.

### Key Benefits:

✅ **Dynamic Drive Detection**: Works on D:, J:, or any drive  
✅ **Multi-Platform Support**: Handles both OpenWrt and RUTOS packages  
✅ **Automatic Path Conversion**: Converts Windows paths to WSL paths  
✅ **Service Management**: Starts and enables services automatically  
✅ **Cross-Platform**: Works from Windows PowerShell or Linux  
✅ **Error Handling**: Comprehensive error checking and reporting  
✅ **Flexible Usage**: Interactive menu or command-line options  

This solution eliminates the need for hardcoded paths and works seamlessly across different environments (work vs home) without any manual configuration changes.
