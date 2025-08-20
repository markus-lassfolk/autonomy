# VS Code Copilot Instructions for RUTOS Starlink Failover Project

## Project Context

This is a shell-based failover solution for Starlink connectivity on RUTX50 routers running RUTOS (busybox shell
environment). The project provides automated monitoring, configuration management, and failover capabilities with Azure
integration.

## Key Project Characteristics

- **Target Environment**: RUTX50 router with RUTOS RUT5_R_00.07.09.7 (armv7l architecture)
- **Shell Requirements**: POSIX-compliant for busybox sh (not bash)
- **Deployment**: Remote installation via curl from GitHub
- **Version Management**: Automatic semantic versioning with git integration
- **Configuration**: Template-based with automatic migration system

## Shell Scripting Guidelines

### CRITICAL Shell Compatibility Rules

1. **NO bash-specific syntax** - Use POSIX sh only
2. **NO arrays** - Use space-separated strings or multiple variables
3. **NO [[]]** - Use [ ] for all conditions
4. **NO function() syntax** - Use `function_name() {` format
5. **NO local variables** - All variables are global in busybox
6. **NO echo -e** - Use printf instead
7. **NO source command** - Use . (dot) for sourcing
8. **NO $'\n'** - Use actual newlines or printf

### CRITICAL ShellCheck Compliance Rules

1. **SC2034 - Unused variables**: Only define colors you actually use, or use `# shellcheck disable=SC2034` for
   intentionally unused variables
2. **SC2181 - Exit code checking**: Use direct command testing instead of `$?`
3. **SC3045 - export -f**: Never use `export -f` - not supported in POSIX sh
4. **SC1090/SC1091 - Source files**: Add `# shellcheck source=/dev/null` or `# shellcheck disable=SC1091` for dynamic
   sources

### Function Best Practices

```bash
# Correct function definition
function_name() {
    # Always validate inputs
    if [ $# -lt 1 ]; then
        printf "Error: function_name requires at least 1 argument\n"
        return 1
    fi

    # Use explicit closing brace
    # ... function body ...
}  # Always close functions properly
```

### Variable Handling

```bash
# Correct variable assignment and usage
VARIABLE="value"
if [ -n "$VARIABLE" ]; then
    printf "Variable is set: %s\n" "$VARIABLE"
fi

# For configuration variables, always provide defaults
CONFIG_VALUE="${CONFIG_VALUE:-default_value}"
```

## Project Structure Awareness

### Core Scripts and Their Purposes

- `scripts/install-rutos.sh` - Remote installation with comprehensive logging
- `scripts/validate-config.sh` - Configuration validation with debug mode
- `scripts/update-version.sh` - Automatic version management
- `scripts/upgrade-to-advanced.sh` - Configuration upgrades
- `scripts/update-config.sh` - Configuration updates
- `scripts/pre-commit-validation.sh` - Automated RUTOS compatibility validation
- `scripts/self-update.sh` - Self-update functionality
- `scripts/uci-optimizer.sh` - UCI configuration optimizer
- `Starlink-RUTOS-Failover/starlink_monitor.sh` - Main monitoring script
- `config/config.template.sh` - Base configuration template
- `tests/` - All test scripts organized in dedicated directory

### Version Management System

- Format: `MAJOR.MINOR.PATCH+GIT_COUNT.GIT_COMMIT[-dirty]`
- All scripts must include `SCRIPT_VERSION` variable
- Use `scripts/update-version.sh` for version increments
- VERSION and VERSION_INFO files are auto-generated

## RUTOS Library System (CRITICAL)

### Overview

**ALWAYS USE THE RUTOS LIBRARY SYSTEM** for new scripts and when modifying existing scripts. The library provides standardized, reusable components that ensure consistency, maintainability, and proper POSIX sh compatibility.

### Library Architecture

The RUTOS Library System consists of 4 core modules in `scripts/lib/`:

1. **`rutos-lib.sh`** - Main entry point, loads all modules
2. **`rutos-colors.sh`** - Standardized color definitions (Method 5 printf support)
3. **`rutos-logging.sh`** - 4-level logging framework
4. **`rutos-common.sh`** - Common utilities and helper functions

### Mandatory Library Usage Pattern

**EVERY new script MUST follow this pattern:**

```bash
#!/bin/sh
# Script: script-name-rutos.sh
# Version: 1.0.0

set -e

# Version information (auto-updated by update-version.sh)
SCRIPT_VERSION="1.0.0"

# CRITICAL: Load RUTOS library system (REQUIRED)
. "$(dirname "$0")/lib/rutos-lib.sh"

# CRITICAL: Initialize script with library features (REQUIRED)
rutos_init "script-name-rutos.sh" "$SCRIPT_VERSION"

# Now use standardized library functions
log_info "Script started with library system"
safe_execute "echo 'Hello World'" "Print greeting"
```

### 4-Level Logging Framework

The library provides a comprehensive logging system with 4 levels:

1. **NORMAL** (default): Standard operation info
2. **DRY_RUN** (`DRY_RUN=1`): Shows what would be done without executing
3. **DEBUG** (`DEBUG=1`): Detailed debugging with context
4. **RUTOS_TEST_MODE** (`RUTOS_TEST_MODE=1`): Full execution trace

```bash
# Standard logging functions (ALWAYS use these instead of printf)
log_info "General information message"
log_success "Operation completed successfully"
log_warning "Warning about potential issue"
log_error "Error message"
log_step "Progress step indicator"
log_debug "Debug information (only shown when DEBUG=1)"
log_trace "Trace information (only shown when RUTOS_TEST_MODE=1)"

# Enhanced command execution (ALWAYS use instead of direct commands)
safe_execute "systemctl restart service" "Restart system service"
```

### Library Initialization Options

```bash
# Full initialization (recommended for most scripts)
rutos_init "script-name" "1.0.0"

# Simple initialization (minimal setup)
rutos_init_simple "script-name"

# Portable initialization (skip RUTOS environment validation)
rutos_init_portable "script-name" "1.0.0"
```

### CRITICAL: Do NOT Duplicate Library Functions

**NEVER define these functions in scripts** - they are provided by the library:

- ❌ `log_info()`, `log_error()`, `log_debug()`, etc.
- ❌ Color variables (`RED`, `GREEN`, `BLUE`, etc.)
- ❌ `safe_execute()` or similar command execution functions
- ❌ `get_timestamp()` or timestamp functions
- ❌ Environment validation functions

### Legacy Script Migration

When updating existing scripts, replace old patterns:

```bash
# OLD PATTERN (remove this)
RED='\033[0;31m'
GREEN='\033[0;32m'
# ... more duplicate color definitions

log_info() {
    printf "${GREEN}[INFO]${NC} %s\n" "$1"
}
# ... more duplicate function definitions

# NEW PATTERN (use this)
. "$(dirname "$0")/lib/rutos-lib.sh"
rutos_init "script-name" "$SCRIPT_VERSION"
# All functions and colors now available
```

### Environment Variables for Library

The library system recognizes these standard variables:

- `DRY_RUN=1` - Enable dry-run mode (safe execution)
- `DEBUG=1` - Enable debug logging
- `RUTOS_TEST_MODE=1` - Enable full trace logging
- `TEST_MODE=1` - Backward compatibility for RUTOS_TEST_MODE
- `NO_COLOR=1` - Disable color output
- `ALLOW_TEST_EXECUTION=1` - Allow execution in test mode
- `DEMO_TRACING=1` - Enable demonstration mode

### Advanced Library Features

```bash
# Function entry/exit tracing (for DEBUG mode)
log_function_entry "function_name" "param1, param2"
log_function_exit "function_name" "exit_code"

# Variable change tracking (for RUTOS_TEST_MODE)
log_variable_change "VAR_NAME" "old_value" "new_value"

# Enhanced error reporting with context
log_error_with_context "Error message" "script.sh" "42" "function_name"

# Command execution logging (automatic with safe_execute)
safe_execute "curl -s http://example.com" "Fetch data from API"
```

### Library Benefits

1. **Consistency** - All scripts use identical logging format
2. **Maintainability** - Update behavior once, affects all scripts
3. **RUTOS Compatibility** - Tested with busybox sh and Method 5 printf
4. **Enhanced Debugging** - 4-level logging with command tracing
5. **Safety** - DRY_RUN mode prevents accidental changes
6. **Code Reduction** - Eliminates duplicate functions across scripts

### Remote Installation Support

The library system works in both local development and remote installation:

```bash
# Local development (scripts/lib/ available)
. "$(dirname "$0")/lib/rutos-lib.sh"

# Remote installation (library downloaded automatically)
# install-rutos.sh downloads library to temporary location
```

## Code Generation Rules

### Always Include These Elements

**CRITICAL: Use RUTOS Library System** - All new scripts MUST use the library system:

1. **Library Import**: `. "$(dirname "$0")/lib/rutos-lib.sh"`
2. **Library Initialization**: `rutos_init "script-name" "$SCRIPT_VERSION"`
3. **Version Header**: SCRIPT_VERSION variable (auto-updated by update-version.sh)
4. **Standardized Logging**: Use library functions (log_info, log_error, etc.)
5. **Safe Execution**: Use `safe_execute()` for all system commands
6. **Error Handling**: Use library error functions with context
7. **Debug Support**: Automatic DEBUG=1 and RUTOS_TEST_MODE=1 support

**DO NOT manually define these (provided by library):**

- ❌ Color variables (RED, GREEN, BLUE, etc.)
- ❌ Logging functions (log_info, log_error, etc.)
- ❌ Timestamp functions
- ❌ Command execution functions
- ❌ Environment validation

**Library provides all of this automatically!**

### Standard Color Scheme

```bash
# Color definitions (busybox compatible) - ALWAYS include ALL colors
RED='\033[0;31m'      # Errors, critical issues
GREEN='\033[0;32m'    # Success, info, completed actions
YELLOW='\033[1;33m'   # Warnings, important notices
BLUE='\033[1;35m'     # Steps, progress indicators (bright magenta for better readability)
PURPLE='\033[0;35m'   # Special status, headers
CYAN='\033[0;36m'     # Debug messages, technical info
NC='\033[0m'          # No Color (reset)

# CRITICAL: Use RUTOS-compatible color detection
if [ -t 1 ] && [ "${TERM:-}" != "dumb" ] && [ "${NO_COLOR:-}" != "1" ]; then
    # Colors enabled
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    BLUE='\033[1;35m'
    CYAN='\033[0;36m'
    NC='\033[0m'
else
    # Colors disabled
    RED=""
    GREEN=""
    YELLOW=""
    BLUE=""
    CYAN=""
    NC=""
fi

# CRITICAL: Use proper printf format strings to avoid SC2059 errors
# WRONG: printf "${RED}Error: %s${NC}\n" "$message"
# RIGHT: printf "%sError: %s%s\n" "$RED" "$message" "$NC"

# Standard logging functions
log_info()    # Green [INFO] for general information
log_warning() # Yellow [WARNING] for warnings
log_error()   # Red [ERROR] for errors (to stderr)
log_debug()   # Cyan [DEBUG] for debug info (if DEBUG=1)
log_success() # Green [SUCCESS] for successful completion
log_step()    # Blue [STEP] for progress steps
```

### CRITICAL Printf Format Rules for RUTOS

**BREAKTHROUGH**: After comprehensive testing on RUTOS, we discovered that Method 5 format is the ONLY one that works correctly!

1. **For RUTOS scripts (-rutos.sh): Use Method 5 format** - Embed variables in printf format string
2. **Method 5 shows actual colors in RUTOS** - Other methods show literal escape codes
3. **Use embedded variables format** for RUTOS compatibility

```bash
# METHOD 5 (WORKS in RUTOS) ✅ - Shows actual colors
printf "${RED}Error: %s${NC}\n" "$message"
printf "${GREEN}[INFO]${NC} [%s] %s\n" "$timestamp" "$message"
printf "${GREEN}✅ HEALTHY${NC}   | %-25s | %s\n" "$component" "$details"

# BROKEN FORMAT (Shows escape codes in RUTOS) ❌
printf "%sError: %s%s\n" "$RED" "$message" "$NC"
printf "%s[INFO]%s [%s] %s\n" "$GREEN" "$NC" "$timestamp" "$message"
printf "%s✅ HEALTHY%s   | %-25s | %s\n" "$GREEN" "$NC" "$component" "$details"

# INSTALL SCRIPT FORMAT (Also works) ✅
printf "%bError: %s%b\n" "$RED" "$message" "$NC"
```

**Key Discovery**: RUTOS busybox printf only processes color variables correctly when embedded in the format
string, not when passed as separate arguments.

### CRITICAL Color Detection Rules

1. **NEVER use `command -v tput` pattern** - Not RUTOS compatible
2. **ALWAYS use simplified pattern** shown above
3. **ALWAYS define ALL colors** (RED, GREEN, YELLOW, BLUE, CYAN, NC)
4. **NEVER use `tput colors`** - Not available in busybox

### Color Usage Examples

```bash
# Progress tracking
log_step "Installing dependencies"
log_info "Downloading configuration template"
log_success "Installation completed successfully"

# Error handling
if ! command_exists curl; then
    log_error "curl is required but not installed"
    exit 1
fi

# Warnings
if [ "$USER" != "root" ]; then
    log_warning "Running as non-root user, some features may not work"
fi

# Debug information
log_debug "Configuration file path: $CONFIG_FILE"
log_debug "Current working directory: $(pwd)"
```

### Standard Script Template

```bash
#!/bin/sh
# Script: script-name-rutos.sh
# Version: 1.0.0
# Description: Brief description of script purpose

set -e  # Exit on error

# Version information (auto-updated by update-version.sh)
SCRIPT_VERSION="1.0.0"

# CRITICAL: Load RUTOS library system (REQUIRED)
. "$(dirname "$0")/lib/rutos-lib.sh"

# CRITICAL: Initialize script with library features (REQUIRED)
rutos_init "script-name-rutos.sh" "$SCRIPT_VERSION"

# Now all library functions are available automatically:
# - log_info(), log_error(), log_debug(), log_trace(), etc.
# - Color variables (RED, GREEN, BLUE, etc.)
# - safe_execute() for command execution
# - Environment validation and cleanup handlers

# Main function
main() {
    log_info "Starting script-name-rutos.sh v$SCRIPT_VERSION"

    # Validate environment
    if [ ! -f "/etc/openwrt_release" ]; then
        log_error "This script is designed for OpenWrt/RUTOS systems"
        exit 1
    fi

    log_step "Validating environment"
    # Environment validation logic here

    log_step "Main script logic"
    # Use safe_execute for all system commands
    safe_execute "echo 'Hello World'" "Print greeting"

    # Example of enhanced error handling
    if ! safe_execute "curl -s http://example.com" "Test network connectivity"; then
        log_error_with_context "Network test failed" "$0" "$LINENO" "main"
        exit 1
    fi

    log_success "Script completed successfully"
}

# Execute main function
main "$@"
```

## Configuration Management Guidelines

### Template System Rules

1. **Clean Templates**: No ShellCheck comments in template files
2. **Preserve User Values**: Always maintain user configurations during migration
3. **Backup Strategy**: Create backups before any modifications
4. **Validation**: Separate structure validation from content validation

### Configuration Variables Pattern

```bash
# Standard configuration variable format
VARIABLE_NAME="${VARIABLE_NAME:-default_value}"  # Description of what this controls

# For boolean values
ENABLE_FEATURE="${ENABLE_FEATURE:-true}"  # Enable/disable feature (true/false)

# For numeric values with validation
TIMEOUT_SECONDS="${TIMEOUT_SECONDS:-30}"  # Timeout in seconds (1-300)
```

## Error Handling and Debugging

### Standard Error Patterns

```bash
# Function with error handling and colored output
safe_operation() {
    operation="$1"  # No 'local' keyword - busybox doesn't support it
    log_debug "Attempting: $operation"

    if ! command_here; then
        log_error "Failed to $operation"
        return 1
    fi

    log_debug "Success: $operation"
    return 0
}

# Configuration validation with colors
validate_config() {
    config_file="$1"

    log_step "Validating configuration file"

    if [ ! -f "$config_file" ]; then
        log_error "Configuration file not found: $config_file"
        return 1
    fi

    if ! grep -q "REQUIRED_VARIABLE" "$config_file"; then
        log_warning "Missing required variable in configuration"
        return 1
    fi

    log_success "Configuration validation passed"
    return 0
}

# Network operation with retry and colored feedback
download_with_retry() {
    url="$1"
    output_file="$2"
    max_attempts=3
    attempt=1

    while [ $attempt -le $max_attempts ]; do
        log_step "Download attempt $attempt of $max_attempts"

        if curl -fsSL "$url" -o "$output_file"; then
            log_success "Download completed successfully"
            return 0
        else
            log_warning "Download attempt $attempt failed"
            attempt=$((attempt + 1))
            sleep 2
        fi
    done

    log_error "All download attempts failed"
    return 1
}
```

### Enhanced Remote Installation Debugging

For remote installation scripts, implement comprehensive debugging that shows system context, exact commands, and file operations:

```bash
# Enhanced debug output for remote installation troubleshooting
enhanced_debug_info() {
    if [ "${DEBUG:-0}" = "1" ]; then
        debug_msg "=== ENHANCED SYSTEM INFORMATION ==="
        debug_msg "Architecture: $(uname -m)"
        debug_msg "Kernel: $(uname -r)"
        debug_msg "Available memory: $(free -h | awk '/^Mem:/ {print $7}' 2>/dev/null || echo 'unknown')"
        debug_msg "Current user: $(id)"
        debug_msg "Working directory: $(pwd)"
        debug_msg "PATH: $PATH"

        debug_msg "=== DISK SPACE ANALYSIS ==="
        debug_msg "Root filesystem:"
        df -h / 2>/dev/null || debug_msg "  Cannot read root filesystem info"
        debug_msg "Temporary directories:"
        for dir in /tmp /var/tmp /root/tmp; do
            if [ -d "$dir" ]; then
                available=$(df -h "$dir" | awk 'NR==2 {print $4}' 2>/dev/null || echo 'unknown')
                debug_msg "  $dir: ${available} available"
            else
                debug_msg "  $dir: directory does not exist"
            fi
        done
    fi
}

# Disk space validation before downloads
validate_disk_space() {
    target_dir="$1"
    min_kb="${2:-50}"

    if [ "${DEBUG:-0}" = "1" ]; then
        debug_msg "=== DISK SPACE VALIDATION ==="
        debug_msg "Target directory: $target_dir"
        debug_msg "Minimum required: ${min_kb}KB"
    fi

    if [ ! -d "$target_dir" ]; then
        debug_msg "✗ Directory does not exist: $target_dir"
        return 1
    fi

    available_kb=$(df "$target_dir" | awk 'NR==2 {print $4}' 2>/dev/null || echo "0")
    available_kb=${available_kb:-0}

    debug_msg "Available space: ${available_kb}KB"

    if [ "$available_kb" -ge "$min_kb" ]; then
        debug_msg "✓ Sufficient disk space available"
        return 0
    else
        debug_msg "✗ Insufficient disk space (need ${min_kb}KB, have ${available_kb}KB)"
        return 1
    fi
}

# Enhanced command execution with detailed logging
debug_execute() {
    command="$1"
    description="$2"

    if [ "${DEBUG:-0}" = "1" ]; then
        debug_msg "=== COMMAND EXECUTION ==="
        debug_msg "Description: $description"
        debug_msg "Command: $command"
        debug_msg "Working directory: $(pwd)"
        debug_msg "User: $(id -un 2>/dev/null || echo 'unknown')"
    fi

    if eval "$command"; then
        debug_msg "✓ Command succeeded: $description"
        return 0
    else
        exit_code=$?
        debug_msg "✗ Command failed with exit code $exit_code: $description"
        debug_msg "Failed command: $command"
        return $exit_code
    fi
}
```

### Debug Mode Implementation

```bash
# Debug mode with colored output
DEBUG="${DEBUG:-0}"
if [ "$DEBUG" = "1" ]; then
    log_debug "==================== DEBUG MODE ENABLED ===================="
    set -x  # Enable command tracing
fi

# Debug logging throughout the script
log_debug "Current environment: $(uname -a)"
log_debug "Script version: $SCRIPT_VERSION"
log_debug "Working directory: $(pwd)"

# Conditional debug information
if [ "$DEBUG" = "1" ]; then
    log_debug "Full environment variables:"
    env | grep -E "(STARLINK|CONFIG|DEBUG)" | while read -r line; do
        log_debug "  $line"
    done
fi
```

## Remote Installation Best Practices

### Comprehensive Debug Output for Installation Scripts

When creating installation scripts that will be run remotely via curl, implement comprehensive debugging that provides sufficient information to troubleshoot issues like "curl error 23" or disk space problems:

```bash
# System information display for troubleshooting
show_system_info() {
    if [ "${DEBUG:-0}" = "1" ]; then
        debug_msg "=== SYSTEM INFORMATION ==="
        debug_msg "Architecture: $(uname -m)"
        debug_msg "Kernel: $(uname -r)"
        debug_msg "Free memory: $(free -h | awk '/^Mem:/ {print $7}' 2>/dev/null || echo 'unknown')"
        debug_msg "User: $(id -un 2>/dev/null || echo 'unknown')"
        debug_msg "Working directory: $(pwd)"
        debug_msg "Available disk space:"
        for dir in /tmp /var/tmp /root/tmp .; do
            if [ -d "$dir" ]; then
                available=$(df -h "$dir" | awk 'NR==2 {print $4}' 2>/dev/null || echo 'unknown')
                debug_msg "  $dir: ${available} available"
            fi
        done
    fi
}

# Smart temporary directory selection with space checking
select_temp_directory() {
    min_kb="${1:-50}"
    temp_candidates="/tmp /var/tmp /root/tmp ."

    for candidate in $temp_candidates; do
        if [ -d "$candidate" ]; then
            available_kb=$(df "$candidate" | awk 'NR==2 {print $4}' 2>/dev/null || echo "0")
            available_kb=${available_kb:-0}

            if [ "$available_kb" -ge "$min_kb" ]; then
                debug_msg "Selected temporary directory: $candidate (${available_kb}KB available)"
                echo "$candidate"
                return 0
            else
                debug_msg "Skipping $candidate: only ${available_kb}KB available (need ${min_kb}KB)"
            fi
        else
            debug_msg "Skipping $candidate: directory does not exist"
        fi
    done

    debug_msg "✗ No suitable temporary directory found with ${min_kb}KB free space"
    return 1
}

# Enhanced download function with detailed error reporting
enhanced_download() {
    url="$1"
    output_file="$2"
    description="${3:-file}"

    if [ "${DEBUG:-0}" = "1" ]; then
        debug_msg "=== DOWNLOAD OPERATION ==="
        debug_msg "URL: $url"
        debug_msg "Output: $output_file"
        debug_msg "Description: $description"
        debug_msg "Output directory: $(dirname "$output_file")"
        debug_msg "Directory exists: $([ -d "$(dirname "$output_file")" ] && echo 'yes' || echo 'no')"
        debug_msg "Directory permissions: $(ls -ld "$(dirname "$output_file")" 2>/dev/null || echo 'unknown')"
    fi

    # Try curl first
    if command -v curl >/dev/null 2>&1; then
        debug_msg "Using curl for download"
        if curl -fsSL --connect-timeout 10 --max-time 60 "$url" -o "$output_file"; then
            file_size=$(wc -c <"$output_file" 2>/dev/null || echo "unknown")
            debug_msg "✓ Download completed: $description (${file_size} bytes)"
            return 0
        else
            curl_exit_code=$?
            debug_msg "✗ curl failed with exit code $curl_exit_code"
            debug_msg "  Common curl error codes:"
            debug_msg "    23: Write error (disk full, permissions, etc.)"
            debug_msg "    28: Timeout reached"
            debug_msg "    6: Couldn't resolve host"
        fi
    fi

    # Fallback to wget
    if command -v wget >/dev/null 2>&1; then
        debug_msg "Fallback: Using wget for download"
        if wget -q -O "$output_file" "$url"; then
            file_size=$(wc -c <"$output_file" 2>/dev/null || echo "unknown")
            debug_msg "✓ Download completed with wget: $description (${file_size} bytes)"
            return 0
        else
            wget_exit_code=$?
            debug_msg "✗ wget failed with exit code $wget_exit_code"
        fi
    fi

    debug_msg "✗ All download methods failed for: $description"
    return 1
}

# Comprehensive cleanup with multiple directory support
enhanced_cleanup() {
    session_id="$1"
    cleanup_pattern="${2:-*${session_id}*}"

    debug_msg "=== CLEANUP OPERATION ==="
    debug_msg "Session ID: $session_id"
    debug_msg "Cleanup pattern: $cleanup_pattern"

    for location in /tmp /var/tmp /root/tmp .; do
        if [ -d "$location" ]; then
            find "$location" -name "$cleanup_pattern" -type f 2>/dev/null | while IFS= read -r file; do
                if [ -f "$file" ]; then
                    debug_msg "Cleaning up: $file"
                    rm -f "$file" 2>/dev/null || debug_msg "Warning: Could not remove $file"
                fi
            done
        fi
    done
}
```

### Installation Script Error Handling

Always implement robust error handling for remote installation scenarios:

```bash
# Pre-installation validation
pre_installation_checks() {
    # Check available space
    if ! temp_dir=$(select_temp_directory 100); then
        print_status "$RED" "✗ Insufficient disk space for installation"
        print_status "$YELLOW" "  Please free up space in /tmp, /var/tmp, or /root/tmp"
        exit 1
    fi

    # Check network connectivity
    if ! curl -s --connect-timeout 5 --max-time 10 -o /dev/null "https://github.com" 2>/dev/null; then
        if ! wget -q --timeout=10 -O /dev/null "https://github.com" 2>/dev/null; then
            print_status "$RED" "✗ Network connectivity test failed"
            print_status "$YELLOW" "  Please check internet connection and DNS"
            exit 1
        fi
    fi

    # Show system information if debug enabled
    show_system_info
}

# Post-installation verification
post_installation_verification() {
    installation_dir="$1"

    if [ ! -d "$installation_dir" ]; then
        print_status "$RED" "✗ Installation directory not created: $installation_dir"
        return 1
    fi

    script_count=$(find "$installation_dir" -name "*-rutos.sh" -type f | wc -l)
    debug_msg "Installed scripts: $script_count"

    if [ "$script_count" -lt 5 ]; then
        print_status "$YELLOW" "⚠ Warning: Only $script_count scripts installed (expected more)"
    else
        print_status "$GREEN" "✓ Installation verification passed: $script_count scripts installed"
    fi
}
```

## Testing and Validation

### Testing Environment Context

- **Router**: RUTX50 with RUTOS RUT5_R_00.07.09.7
- **Architecture**: armv7l
- **Shell**: busybox sh (limited POSIX compliance)
- **Network**: Starlink primary, cellular backup
- **Installation**: Remote via curl

### Common Testing Scenarios

1. **Fresh Installation**: New router setup
2. **Configuration Migration**: Upgrading from old template
3. **Debug Mode**: Troubleshooting with DEBUG=1
4. **Version Updates**: Using update-version.sh
5. **Remote Downloads**: GitHub raw content access
6. **Installation Troubleshooting**: Diagnosing curl errors and disk space issues
7. **Fallback Directory Testing**: Verifying /root/tmp and alternative temporary locations
8. **Resource-Constrained Installation**: Testing on systems with limited disk space

## VS Code Development Environment

### Terminal Configuration

- **Default Shell**: PowerShell (Windows environment)
- **Recommended for Shell Development**: Switch to WSL/bash when working on shell scripts
- **Available Options**: PowerShell, WSL, Git Bash
- **Best Practice**: Use WSL or Git Bash for shell script development and testing

### Terminal Commands for Development

```bash
# Switch to WSL for shell development
wsl

# Or use Git Bash for better POSIX compatibility
# Terminal -> Select Default Profile -> Git Bash

# Make scripts executable in WSL/Git Bash (not needed in PowerShell)
chmod +x scripts/*.sh
```

### Why Use WSL/Git Bash for Shell Development?

- **Better POSIX Compliance**: Closer to RUTOS environment
- **ShellCheck Support**: Native shell script linting
- **File Permissions**: Can set executable permissions
- **Command Compatibility**: Tools like `find`, `grep`, `awk` work as expected
- **Testing**: More accurate testing environment for shell scripts

### Code Quality and Pre-Commit Checks

#### CRITICAL: Always Run Before Commits

1. **Pre-Commit Validation**: Run `./scripts/pre-commit-validation.sh` for comprehensive checks
2. **ShellCheck**: Validate all shell scripts for POSIX compliance
3. **shfmt Integration**: Automatic formatting validation using industry standard
4. **Version Consistency**: Ensure all scripts have matching version info
5. **Template Validation**: Verify configuration templates are clean

#### Automated Pre-Commit Validation System

```bash
# Run comprehensive validation (REQUIRED before commits)
./scripts/pre-commit-validation.sh

# For staged files only (pre-commit hook)
./scripts/pre-commit-validation.sh --staged

# With debug output for troubleshooting
DEBUG=1 ./scripts/pre-commit-validation.sh

# The validation system checks:
# - ShellCheck compliance (POSIX sh only)
# - Bash-specific syntax detection
# - RUTOS compatibility patterns
# - Code formatting with shfmt
# - Critical whitespace issues
# - Template cleanliness
```

#### Remote Installation Debugging

Test the enhanced installation script with comprehensive debug output:

```bash
# Test with debug mode (shows system info, disk space, command details)
curl -fsSL https://raw.githubusercontent.com/markus-lassfolk/autonomy/main/scripts/install-rutos.sh | DEBUG=1 sh

# Test with both debug and test mode (maximum verbosity)
# NOTE: RUTOS_TEST_MODE=1 enables trace logging - script should run normally
curl -fsSL https://raw.githubusercontent.com/markus-lassfolk/autonomy/main/scripts/install-rutos.sh | DEBUG=1 RUTOS_TEST_MODE=1 sh

# Test disk space management with dry run (prevents actual changes)
curl -fsSL https://raw.githubusercontent.com/markus-lassfolk/autonomy/main/scripts/install-rutos.sh | DEBUG=1 DRY_RUN=1 sh

# Alternative test method for local testing:
curl -fsSL https://raw.githubusercontent.com/markus-lassfolk/autonomy/main/scripts/install-rutos.sh > install-test.sh
DEBUG=1 RUTOS_TEST_MODE=1 sh install-test.sh
```

The enhanced debug output includes:

- **System Information**: Architecture, kernel, memory, disk space analysis for all temp directories
- **Command Execution**: Exact commands with working directory and user context
- **File Operations**: Download URLs, output paths, file sizes, permissions, directory validation
- **Disk Space Management**: Pre-download space checking with fallback directory selection (/tmp → /var/tmp → /root/tmp → current dir)
- **Error Context**: Detailed curl/wget error codes with troubleshooting suggestions (e.g., error 23 = disk space/permissions)
- **Cleanup Tracing**: Comprehensive temporary file cleanup across all potential locations

#### Interpreting Debug Output for Troubleshooting

When analyzing debug output from failed installations:

1. **Successful System Detection**: Look for `===== SYSTEM INFORMATION =====` section showing proper architecture, disk space, and permissions
2. **Temporary Directory Setup**: Verify `===== TEMPORARY DIRECTORY SETUP =====` shows adequate space and successful creation
3. **Download Progress**: Check `===== DOWNLOADING FILE =====` sections for successful downloads vs. failures
4. **Error Context**: For curl error 23, check if previous downloads succeeded - this indicates network/availability issues rather than disk/permission problems
5. **RUTOS_TEST_MODE Expected Behavior**: When using `RUTOS_TEST_MODE=1`, the script should run normally with enhanced trace logging - it should NOT exit early

**Example Analysis**:

```text
✅ System Info: armv7l, 121MB available in /tmp, proper permissions
✅ Library Downloads: 4 files successfully downloaded (26KB total)
✅ Script Execution: RUTOS_TEST_MODE enabled - enhanced trace logging active
✅ Installation Process: Continues normally with trace logging enabled
→ DIAGNOSIS: Script should complete normally with enhanced debugging output
→ RESULT: RUTOS_TEST_MODE working correctly for enhanced debugging
```

#### ShellCheck Integration

```bash
# Install ShellCheck (if not already installed)
# On WSL/Ubuntu:
sudo apt-get install shellcheck

# On Windows with Chocolatey:
choco install shellcheck

# Install shfmt for formatting validation
# On WSL/Ubuntu:
sudo apt-get install shfmt
# Or: go install mvdan.cc/sh/v3/cmd/shfmt@latest

# The pre-commit validation script handles all checks automatically
# Manual checks are no longer needed if using the validation system
```

#### Pre-Commit Quality Checklist

- [ ] Run `./scripts/pre-commit-validation.sh` and fix all issues
- [ ] All shell scripts pass ShellCheck with no errors
- [ ] No bash-specific syntax (arrays, [[]], function() syntax)
- [ ] All functions have proper closing braces
- [ ] Version information is consistent across scripts
- [ ] Debug mode support is implemented
- [ ] Error handling is comprehensive
- [ ] Templates are clean (no ShellCheck comments)
- [ ] Code formatting passes shfmt validation

#### Automated Quality Check Script

```bash
# Run comprehensive quality checks (use in WSL/Git Bash)
./scripts/pre-commit-validation.sh

# On Windows PowerShell, switch to WSL first:
wsl
./scripts/pre-commit-validation.sh

# The validation system provides:
# - Color-coded output (RED for critical, YELLOW for major, BLUE for minor)
# - Detailed issue reporting with line numbers
# - Comprehensive summary with pass/fail statistics
# - Debug mode for troubleshooting validation issues
```

#### Manual Quality Checks (if automated script fails)

```bash
# Individual checks you can run manually
shellcheck scripts/*.sh Starlink-RUTOS-Failover/*.sh
grep -r "\[\[" scripts/ Starlink-RUTOS-Failover/  # Should return nothing
grep -r "local " scripts/ Starlink-RUTOS-Failover/  # Should return nothing
grep -r "echo -e" scripts/ Starlink-RUTOS-Failover/  # Should return nothing
```

#### VS Code Extensions for Quality

- **ShellCheck**: Provides real-time shell script linting
- **Bash IDE**: Enhanced shell script editing
- **GitLens**: Better git integration for version tracking
- **Error Lens**: Inline error display

### Modern Development Workflow

1. **Switch to WSL/Bash**: Better shell script development environment
2. **Edit Scripts**: Use VS Code with ShellCheck extension
3. **Run Pre-Commit Validation**: `./scripts/pre-commit-validation.sh`
4. **Fix All Issues**: Address errors and warnings before commit
5. **Test Locally**: Validate syntax and basic functionality
6. **Commit**: Only after all quality checks pass

### Development Tools Integration

- **ShellCheck**: Automated syntax and compatibility validation
- **shfmt**: Code formatting and style validation
- **Pre-commit Hooks**: Automated quality checks before commits
- **Debug Mode**: Enhanced debugging with clean output (`DEBUG=1`)
- **Version Management**: Automatic version tracking and updates

### Quality Assurance Pipeline

```bash
# Step 1: Run pre-commit validation
./scripts/pre-commit-validation.sh

# Step 2: Fix any issues found
# Critical issues must be fixed
# Major issues should be fixed
# Minor issues can be addressed later

# Step 3: Verify fixes
./scripts/pre-commit-validation.sh --staged

# Step 4: Commit when all checks pass
git commit -m "Description of changes"
```

### Development Environment Setup

```bash
# Install required tools (WSL/Linux)
sudo apt-get update
sudo apt-get install shellcheck shfmt

# Or on Windows with Chocolatey
choco install shellcheck

# Set up pre-commit hooks (optional)
cp scripts/pre-commit-validation.sh .git/hooks/pre-commit
chmod +x .git/hooks/pre-commit
```

## Azure Integration Considerations

### When Working with Azure Components

- Follow Azure development best practices
- Use Azure tools when available
- Implement proper authentication patterns
- Consider network connectivity requirements for RUTOS environment

### Azure Logging Integration

- Located in `Starlink-RUTOS-Failover/AzureLogging/`
- Python-based analysis tools with requirements.txt
- Bicep templates for infrastructure
- PowerShell and shell setup scripts

## File Naming and Organization

### Naming Conventions

- Scripts: `kebab-case.sh` (e.g., `validate-config.sh`)
- Configs: `config.unified.template.sh` (unified template with all features)
- Documentation: `UPPERCASE.md` (e.g., `TESTING.md`)
- Logs: `lowercase.log` (e.g., `installation.log`)

### Directory Structure

```text
scripts/           # All utility scripts
config/           # Configuration templates
Starlink-RUTOS-Failover/  # Main monitoring scripts
AzureLogging/     # Azure integration components
tests/            # Test scripts and validation tools
docs/             # Documentation
```

### Test Directory Organization

```text
tests/
├── README.md                      # Test documentation
├── test-suite.sh                  # Main test runner
├── test-core-logic.sh             # Core functionality tests
├── test-comprehensive-scenarios.sh # Comprehensive testing
├── test-deployment-functions.sh   # Deployment testing
├── test-final-verification.sh     # Final verification
├── test-validation-features.sh    # Validation system tests
├── test-validation-fix.sh         # Validation fixes
├── audit-rutos-compatibility.sh   # RUTOS compatibility audit
├── rutos-compatibility-test.sh    # RUTOS compatibility testing
├── verify-deployment.sh           # Deployment verification
└── verify-deployment-script.sh    # Alternative deployment verification
```

## Common Pitfalls to Avoid

1. **Bash-specific syntax** in a busybox environment
2. **Missing function closing braces** causing nested definitions
3. **ShellCheck comments** in template files
4. **Unsafe crontab modifications** (comment instead of delete)
5. **Missing error handling** in remote operations
6. **Hardcoded paths** without environment validation
7. **Version mismatches** between scripts
8. **NOT using RUTOS Library System** - All new scripts must use the library
9. **Duplicating library functions** - Never define log_info(), colors, etc. when using library
10. **Ignoring safe_execute()** - Always use safe_execute() instead of direct commands
11. **Missing library initialization** - Every script must call rutos_init() after loading library

## Git and Version Control

### Branch Strategy

- Main branch: `main`
- Development: `feature/testing-improvements`
- All changes go through feature branches

### Pre-Commit Quality Gates

```bash
# MANDATORY: Run before every commit
./scripts/pre-commit-validation.sh

# For staged files only (pre-commit hook usage)
./scripts/pre-commit-validation.sh --staged

# With debug output for troubleshooting
DEBUG=1 ./scripts/pre-commit-validation.sh

# The validation system automatically handles:
# - ShellCheck validation
# - RUTOS compatibility checks
# - Code formatting validation
# - Version consistency checks
# - Template validation
```

### Commit Message Format

```text
Brief description of change

- Bullet point of specific change
- Another specific change
- Reference to issue/feature if applicable
- Quality: All ShellCheck issues resolved
```

### GitHub Actions/Workflows

- **Shell Script Validation**: Automated ShellCheck on all PRs
- **POSIX Compliance**: Verify busybox compatibility
- **Version Consistency**: Check all scripts have matching versions
- **Template Validation**: Ensure configuration templates are clean

## Success Metrics

### Code Quality Indicators

- ✅ POSIX sh compatibility (validated by pre-commit system)
- ✅ RUTOS Library System (standardized 4-level logging framework)
- ✅ Comprehensive error handling (enhanced with library functions)
- ✅ 4-level debugging (NORMAL/DRY_RUN/DEBUG/RUTOS_TEST_MODE)
- ✅ Enhanced command tracing (safe_execute with full context)
- ✅ Timestamped logging (consistent color-coded format)
- ✅ Version information (automatic semantic versioning)
- ✅ Safe operation patterns (validated by 50+ compatibility checks)

### Testing Validation

- ✅ Works on RUTX50 with RUTOS (tested through 23 rounds)
- ✅ Remote installation via curl (production ready with library system)
- ✅ Configuration migration (automatic template system)
- ✅ Enhanced debug functionality (4-level logging system)
- ✅ Library system operation (self-contained with remote download)
- ✅ Version system operation (git-integrated versioning)
- ✅ Quality assurance (automated pre-commit validation)

### Development Experience

- ✅ RUTOS Library System (eliminates code duplication)
- ✅ Modern tooling integration (ShellCheck, shfmt)
- ✅ Automated quality checks (pre-commit validation)
- ✅ Enhanced debug output (4-level structured logging)
- ✅ Repository organization (dedicated test directory)
- ✅ Comprehensive documentation (library system docs)
- ✅ Consistent code formatting (shfmt validation)

### Production Readiness

- ✅ RUTOS compatibility (busybox shell support with library)
- ✅ Enhanced error handling (library-based comprehensive safety)
- ✅ Configuration management (template migration)
- ✅ Remote deployment (curl installation with library download)
- ✅ Version tracking (semantic versioning)
- ✅ Library system reliability (tested across all scripts)

---

**Remember**: This project prioritizes reliability and compatibility over advanced features. Always test changes in the
RUTOS environment and maintain backward compatibility with existing configurations.

## Continuous Learning and Documentation Improvement

### Learning Capture Protocol

**CRITICAL**: Whenever you discover something new, useful, or important during development sessions, immediately add it to this file to build our collective knowledge base.

#### What to Document

1. **New RUTOS/BusyBox Discoveries**
   - Shell compatibility issues and solutions
   - BusyBox command limitations or alternatives
   - POSIX sh patterns that work vs. those that don't
   - Router-specific behaviors or constraints

2. **Debug and Testing Insights**
   - Debugging techniques that work well in RUTOS environment
   - Common error patterns and their solutions
   - Testing strategies that reveal issues early
   - Performance considerations for resource-constrained routers

3. **Development Workflow Improvements**
   - VS Code setup optimizations
   - Using Winows with Powershell in VSCode, all Linux scripts and commands need to be run in WSL or Bash.
   - Tool configurations that enhance productivity
   - Git workflow patterns that work well for this project
   - Quality assurance discoveries

4. **Script Architecture Patterns**
   - Effective function design patterns
   - Error handling strategies that work reliably
   - Configuration management insights
   - Version management learnings

5. **Integration and Deployment Learnings**
   - Remote deployment gotchas and solutions
   - Network connectivity considerations
   - Azure integration best practices
   - Production deployment insights

#### Documentation Guidelines

````bash
# Use this format for new learnings:
### [Category] - [Brief Title] (Date: YYYY-MM-DD)

**Discovery**: What was learned or discovered
**Context**: When/where this applies
**Implementation**: How to apply this learning
**Impact**: Why this matters for the project
**Example**: Code example or specific case (if applicable)

# Example:
### Shell Scripting - Subprocess Output Contamination Fix (Date: 2025-07-23)

**Discovery**: Using pipes with logging functions inside find commands contaminates script lists with log output like "[STEP]", "[DEBUG]" being treated as script filenames
**Context**: When collecting script lists using find with logging inside loops or subshells
**Implementation**: Move all logging AFTER data collection, use temp files instead of pipes for complex processing
**Impact**: Prevents critical parsing failures that can cause divide-by-zero errors and complete test system failure
**Example**:
```bash
# WRONG - logging contaminates output
find . -name "*.sh" | while read script; do
    log_debug "Found: $script"  # This contamination breaks parsing
done

# RIGHT - collect first, log after
temp_file="/tmp/scripts_$$"
find . -name "*.sh" > "$temp_file"
log_step "Finding scripts"  # Safe to log after collection
````

#### Integration Process

1. **During Development**: Add learnings immediately when discovered
2. **Session Completion**: Review and consolidate new insights
3. **Regular Updates**: Periodically review and refine existing sections
4. **Cross-Reference**: Link new learnings to existing sections when relevant

#### Learning Categories

- **RUTOS Compatibility**: Hardware/OS specific discoveries
- **Shell Scripting**: POSIX sh and BusyBox insights
- **Debug and Testing**: Development and validation improvements
- **Architecture**: Code organization and pattern discoveries
- **Integration**: Deployment and system integration learnings
- **Quality Assurance**: Testing and validation methodology improvements
- **Development Workflow**: Tool and process optimizations

### Recent Learning Captures

#### Shell Scripting - Subprocess Output Contamination Fix (Date: 2025-07-23)

**Discovery**: Using pipes with logging functions inside find commands contaminates script lists with log output like "[STEP]", "[DEBUG]" being treated as script filenames
**Context**: When collecting script lists using find with logging inside loops or subshells  
**Implementation**: Move all logging AFTER data collection, use temp files instead of pipes for complex processing
**Impact**: Prevents critical parsing failures that can cause divide-by-zero errors and complete test system failure
**Example**:

```bash
# WRONG - logging contaminates output
find . -name "*.sh" | while read script; do
    log_debug "Found: $script"  # This contamination breaks parsing
done

# RIGHT - collect first, log after
temp_file="/tmp/scripts_$$"
find . -name "*.sh" > "$temp_file"
log_step "Finding scripts"  # Safe to log after collection
```

#### Shell Scripting - Function Output Contamination (Date: 2025-07-23)

**Discovery**: ANY logging calls inside a function that returns output via `$()` command substitution will contaminate the return value, causing log messages to be treated as actual data
**Context**: When functions are meant to return pure data (like script lists) that will be parsed by calling code
**Implementation**: NEVER put logging calls inside functions that return output via stdout. Move all logging to the calling function.
**Impact**: Prevents critical bugs where log output like "[STEP] Finding scripts" gets treated as actual script filenames, causing syntax errors and complete system failure
**Example**:

```bash
# WRONG - logging inside output function contaminates return value
get_script_list() {
    log_step "Finding scripts"  # This becomes part of the returned data!
    find . -name "*.sh"
}
script_list=$(get_script_list)  # Now contains log messages mixed with script names

# RIGHT - logging outside the output function
get_script_list() {
    # NO LOGGING - pure output function
    find . -name "*.sh"
}
log_step "Finding scripts"      # Safe - not captured by $()
script_list=$(get_script_list)  # Clean script list only
```

#### Testing - File-Based Processing Over Pipes (Date: 2025-07-23)

**Discovery**: BusyBox subshell variable persistence issues make pipe-based processing unreliable for counters and state
**Context**: When processing lists of items and tracking results/counters across iterations
**Implementation**: Use temporary files to pass data between processing stages instead of pipes with variable updates
**Impact**: Ensures reliable result tracking and prevents variables being reset to zero after subshell completion
**Example**:

```bash
# WRONG - variables lost in subshell
find . -name "*.sh" | while read script; do
    COUNTER=$((COUNTER + 1))  # Lost when pipe ends
done

# RIGHT - file-based approach
temp_results="/tmp/results_$$"
find . -name "*.sh" > /tmp/scripts_$$
while read script; do
    echo "PASS:$script" >> "$temp_results"
done < /tmp/scripts_$$
COUNTER=$(wc -l < "$temp_results")
```

#### Shell Scripting - BusyBox Command Output Whitespace (Date: 2025-07-23)

**Discovery**: BusyBox `wc` and `grep -c` commands can include unwanted whitespace/newlines in output, causing arithmetic errors and display issues like "0\n0" instead of "0"
**Context**: When capturing command output in variables for arithmetic operations or display
**Implementation**: Always strip whitespace with `tr -d ' \n\r'` when capturing numeric output from BusyBox commands
**Impact**: Prevents "bad number" arithmetic errors and malformed display output in RUTOS environment
**Example**:

```bash
# WRONG - can include newlines/whitespace causing "0\n0" display
COUNT=$(wc -l < file)
MATCHES=$(grep -c "pattern" file)

# RIGHT - strip all whitespace for clean numbers
COUNT=$(wc -l < file | tr -d ' \n\r')
MATCHES=$(grep -c "pattern" file | tr -d ' \n\r')
```

#### Testing - Early Exit Pattern for RUTOS_TEST_MODE (Date: 2025-07-23)

**Discovery**: Scripts with proper dry-run support can still fail testing due to missing dependencies, network access, or environment setup issues that occur after syntax validation
**Context**: When scripts have dry-run support but still timeout or fail during test execution due to attempting real operations
**Implementation**: Add early exit pattern immediately after dry-run variable setup to prevent any execution beyond syntax validation
**Impact**: Eliminates execution timeout failures and dependency errors during testing, allowing pure syntax and compatibility validation
**Example**:

```bash
# Dry-run and test mode support
DRY_RUN="${DRY_RUN:-0}"
RUTOS_TEST_MODE="${RUTOS_TEST_MODE:-0}"

# Debug dry-run status
if [ "${DEBUG:-0}" = "1" ]; then
    printf "[DEBUG] DRY_RUN=%s, RUTOS_TEST_MODE=%s\n" "$DRY_RUN" "$RUTOS_TEST_MODE" >&2
fi

# Early exit in test mode to prevent execution errors
if [ "${RUTOS_TEST_MODE:-0}" = "1" ]; then
    printf "[INFO] RUTOS_TEST_MODE enabled - script syntax OK, exiting without execution\n" >&2
    exit 0
fi
```

#### Development Workflow - Systematic Error Fixing Strategy (Date: 2025-07-23)

**Discovery**: When fixing multiple script errors, execution errors must be fixed before missing dry-run support to get accurate testing results
**Context**: When running comprehensive testing on large codebases with multiple types of errors (syntax, execution, missing features)
**Implementation**: Fix execution errors first (timeouts, environment issues), then add missing dry-run support, then address compatibility issues
**Impact**: Prevents masking of real issues and provides accurate progress tracking during systematic fixes
**Example**:

```bash
# Priority order for fixing script issues:
# 1. Add early exit patterns for RUTOS_TEST_MODE (execution errors)
# 2. Add missing dry-run support patterns (missing features)
# 3. Fix syntax and compatibility issues (code quality)
# 4. Validate with comprehensive testing system
```

#### Shell Scripting - Duplicate Code Section Detection (Date: 2025-07-23)

**Discovery**: Scripts can accidentally contain duplicate sections (like duplicate dry-run setup) that cause confusing behavior and testing failures
**Context**: When scripts have been edited multiple times or have merge conflicts that weren't properly resolved
**Implementation**: Always scan for duplicate function definitions and variable setups when debugging script issues
**Impact**: Prevents unexpected behavior from conflicting duplicate code sections
**Example**:

```bash
# WRONG - duplicate sections cause issues
DRY_RUN="${DRY_RUN:-0}"
safe_execute() { ... }

# ... later in file ...
DRY_RUN="${DRY_RUN:-0}"  # Duplicate!
safe_execute() { ... }   # Duplicate function definition!

# RIGHT - single clean definitions
DRY_RUN="${DRY_RUN:-0}"
safe_execute() { ... }
```

#### System Administration - Cleanup Script Completeness (Date: 2025-07-23)

**Discovery**: Installation systems often create multiple types of auto-starting components that cleanup scripts must address to be truly complete: cron entries, auto-recovery services, and version-pinned recovery scripts
**Context**: When creating cleanup/uninstall scripts for systems that set up automated monitoring, updates, and recovery
**Implementation**: Cleanup scripts must handle ALL installed components: monitoring crons, system maintenance crons, auto-update crons, auto-recovery init.d services, and version-pinned recovery scripts
**Impact**: Prevents incomplete cleanup that leaves systems in hybrid states with some automation still running after "cleanup"
**Example**:

```bash
# INCOMPLETE - misses system-maintenance and self-update crons
sed 's|^\([^#].*starlink_monitor.*\)|# CLEANUP: \1|g'

# COMPLETE - handles all installed automation
sed 's|^\([^#].*\(starlink_monitor-rutos\.sh\|starlink_logger-rutos\.sh\|check_starlink_api\|system-maintenance-rutos\.sh\|self-update-rutos\.sh\).*\)|# CLEANUP COMMENTED: \1|g'

# Also remove auto-recovery service
/etc/init.d/starlink-restore disable
rm -f /etc/init.d/starlink-restore

# And version-pinned recovery scripts
rm -f /etc/starlink-config/install-pinned-version.sh
```

#### System Administration - Cleanup Script Safety (Date: 2025-07-23)

**Discovery**: Cleanup scripts are extremely dangerous when they default to immediate execution instead of dry-run mode, causing accidental data loss
**Context**: When creating scripts that remove files, modify configurations, or clean up installations
**Implementation**: ALWAYS default to DRY_RUN=1 (safe mode), require explicit --execute or --force flags for real execution, include 5-second warning countdown
**Impact**: Prevents accidental complete system cleanup and data loss during development and testing
**Example**:

```bash
# DANGEROUS - executes immediately by default
DRY_RUN="${DRY_RUN:-0}"

# SAFE - defaults to dry-run, requires explicit execution
DRY_RUN="${DRY_RUN:-1}"
FORCE_CLEANUP="${FORCE_CLEANUP:-0}"

# Require explicit flag parsing
case "$1" in
    --execute|--force) DRY_RUN=0; FORCE_CLEANUP=1 ;;
    --dry-run) DRY_RUN=1 ;;
esac

# Safety warning for real execution
if [ "$DRY_RUN" = "0" ]; then
    print_status "$RED" "⚠️  WARNING: REAL CLEANUP MODE!"
    print_status "$YELLOW" "Press Ctrl+C within 5 seconds to cancel..."
    sleep 5
fi
```

#### Development Experience - Comprehensive Testing Integration Success (Date: 2025-07-23)

**Discovery**: Systematic error fixing combined with comprehensive testing significantly improves codebase quality metrics and provides clear progress tracking
**Context**: When managing large numbers of script errors across multiple categories (execution, compatibility, missing features)
**Implementation**: Use systematic approach: fix execution errors → add missing dry-run support → run comprehensive testing → track improvement metrics
**Impact**: Achieved improvement from 57% to 80% script success rate through systematic fixes and proper testing validation
**Example**:

```bash
# Testing approach that provides clear progress tracking:
# 1. Run comprehensive testing to establish baseline
# 2. Fix execution errors (timeouts, environment issues)
# 3. Add missing dry-run support to remaining scripts
# 4. Re-run comprehensive testing to measure improvement
# 5. Focus on remaining compatibility and syntax issues

# Result: Clear progress metrics and systematic quality improvement
```

#### System Administration - Remote Installation Debug Enhancement (Date: 2025-07-27)

**Discovery**: Remote installation debugging requires comprehensive tracing showing system info, exact commands, file operations, and disk space management to effectively troubleshoot curl errors and installation failures
**Context**: When users report installation failures like "curl error 23" without sufficient information to diagnose the root cause
**Implementation**: Implement multi-level debug output with system information display, exact command
logging, file operation tracing, disk space validation, and fallback directory management
**Impact**: Enables precise troubleshooting of installation issues with detailed context including system
specs, disk space, file permissions, and command execution details
**Example**:

```bash
# Enhanced debug output pattern for installation scripts
if [ "${DEBUG:-0}" = "1" ]; then
    debug_msg "=== SYSTEM INFORMATION ==="
    debug_msg "Architecture: $(uname -m)"
    debug_msg "Kernel: $(uname -r)"
    debug_msg "Free memory: $(free -h | awk '/^Mem:/ {print $7}' 2>/dev/null || echo 'unknown')"
    debug_msg "Available disk space in $temp_dir: $(df -h "$temp_dir" | awk 'NR==2 {print $4}' 2>/dev/null || echo 'unknown')"
fi

# Disk space checking with fallback directories
check_disk_space() {
    dir="$1"
    min_kb="${2:-50}"  # Default 50KB minimum

    if [ ! -d "$dir" ]; then
        return 1
    fi

    available_kb=$(df "$dir" | awk 'NR==2 {print $4}' 2>/dev/null || echo "0")
    available_kb=${available_kb:-0}

    debug_msg "Disk space check: $dir has ${available_kb}KB available (need ${min_kb}KB)"

    if [ "$available_kb" -ge "$min_kb" ]; then
        return 0
    else
        return 1
    fi
}

# Smart temporary directory selection with fallback
temp_candidates="/tmp /var/tmp /root/tmp ."
for candidate in $temp_candidates; do
    if has_enough_space "$candidate" "50"; then
        temp_dir="$candidate"
        debug_msg "Selected temporary directory: $temp_dir (${available_kb}KB available)"
        break
    fi
done
```

#### System Administration - Curl Error 23 Troubleshooting (Date: 2025-07-27)

**Discovery**: Curl error 23 "Failure writing output to destination" can occur even when disk space and
permissions are adequate, often indicating network issues, missing target files, or interrupted connections
during multi-file downloads
**Context**: When remote installation scripts work perfectly for initial downloads but fail on subsequent
files with curl error 23
**Implementation**: Enhanced error handling that distinguishes between disk/permission issues vs.
network/availability issues, with specific retry logic and fallback mechanisms
**Impact**: Provides clear guidance for troubleshooting curl error 23 based on the context - if disk space
is adequate and previous downloads succeeded, focus on network connectivity and target file availability
**Example**:

```bash
# Enhanced curl error 23 analysis and handling
handle_curl_error_23() {
    failed_url="$1"
    target_file="$2"
    available_space_kb="$3"

    debug_msg "=== CURL ERROR 23 ANALYSIS ==="
    debug_msg "Failed URL: $failed_url"
    debug_msg "Target file: $target_file"
    debug_msg "Available disk space: ${available_space_kb}KB"
    debug_msg "Target directory: $(dirname "$target_file")"
    debug_msg "Directory writable: $([ -w "$(dirname "$target_file")" ] && echo 'yes' || echo 'no')"

    # If we have adequate space and permissions, this is likely a network/availability issue
    if [ "$available_space_kb" -gt 100 ] && [ -w "$(dirname "$target_file")" ]; then
        debug_msg "ANALYSIS: Adequate disk space and write permissions available"
        debug_msg "LIKELY CAUSE: Network connectivity issue or target file unavailable"
        debug_msg "RECOMMENDED ACTION: Check network connectivity and verify target URL exists"

        # Test basic connectivity
        if curl -s --connect-timeout 5 --max-time 10 -o /dev/null "https://github.com" 2>/dev/null; then
            debug_msg "Network connectivity: WORKING (GitHub reachable)"
            debug_msg "LIKELY CAUSE: Specific target file unavailable or URL incorrect"
        else
            debug_msg "Network connectivity: FAILED (Cannot reach GitHub)"
            debug_msg "LIKELY CAUSE: Network connectivity issue"
        fi
    else
        debug_msg "ANALYSIS: Insufficient disk space or permission issues"
        debug_msg "LIKELY CAUSE: Disk space (${available_space_kb}KB) or write permissions"
    fi
}

# Example integration in download function
download_with_error_analysis() {
    url="$1"
    output_file="$2"
    description="$3"

    if ! curl -fsSL "$url" -o "$output_file"; then
        curl_exit_code=$?
        if [ "$curl_exit_code" = "23" ]; then
            available_kb=$(df "$(dirname "$output_file")" | awk 'NR==2 {print $4}' 2>/dev/null || echo "0")
            handle_curl_error_23 "$url" "$output_file" "$available_kb"
        fi
        return $curl_exit_code
    fi
}
```

#### System Administration - Corrected RUTOS_TEST_MODE Behavior (Date: 2025-07-27)

**Discovery**: Previous documentation incorrectly stated that RUTOS_TEST_MODE=1 causes early exit - this was wrong. According to our RUTOS Library System design, RUTOS_TEST_MODE enables trace logging only
**Context**: When testing remote installation scripts with RUTOS_TEST_MODE=1, the script should run normally with enhanced trace logging, not exit early
**Implementation**: RUTOS_TEST_MODE=1 enables trace logging, DRY_RUN=1 prevents actual changes - these are separate functions
**Impact**: Ensures RUTOS_TEST_MODE works as designed for enhanced debugging without preventing script execution
**Example**:

```bash
# CORRECT behavior according to RUTOS Library System:
# RUTOS_TEST_MODE=1 - Enables trace logging (log_trace messages)
# DRY_RUN=1 - Prevents actual changes (safe mode)
# DEBUG=1 - Enables debug logging (log_debug messages)

# RUTOS_TEST_MODE should NOT cause early exit
if [ "${RUTOS_TEST_MODE:-0}" = "1" ]; then
    log_trace "RUTOS_TEST_MODE enabled - trace logging active"
    # Script continues normal execution with enhanced logging
fi

# Only DRY_RUN should prevent actual changes
if [ "${DRY_RUN:-0}" = "1" ]; then
    log_info "DRY_RUN mode - no actual changes will be made"
    # Script runs but skips actual file operations
fi
```

        debug_msg "ANALYSIS: Insufficient disk space or permission issues"
        debug_msg "LIKELY CAUSE: Disk space (${available_space_kb}KB) or write permissions"
    fi

}

# Example integration in download function

download_with_error_analysis() {
url="$1"
output_file="$2"
description="$3"

    if ! curl -fsSL "$url" -o "$output_file"; then
        curl_exit_code=$?
        if [ "$curl_exit_code" = "23" ]; then
            available_kb=$(df "$(dirname "$output_file")" | awk 'NR==2 {print $4}' 2>/dev/null || echo "0")
            handle_curl_error_23 "$url" "$output_file" "$available_kb"
        fi
        return $curl_exit_code
    fi

}

````

#### System Administration - Disk Space Management for RUTOS (Date: 2025-07-27)

**Discovery**: RUTOS systems often have limited disk space, requiring intelligent temporary directory selection with multiple fallbacks and proper cleanup including /root/tmp as a viable option
**Context**: When remote installation fails due to insufficient disk space in default /tmp directory, particularly on resource-constrained router systems
**Implementation**: Implement disk space checking before downloads, provide multiple fallback directories (/tmp → /var/tmp → /root/tmp → current dir), ensure cleanup of all temporary locations
**Impact**: Prevents installation failures due to disk space issues and provides reliable cleanup even when using non-standard temporary locations
**Example**:

```bash
# Intelligent temporary directory selection
has_enough_space() {
    target_dir="$1"
    min_kb="${2:-50}"

    if [ ! -d "$target_dir" ]; then
        return 1
    fi

    available_kb=$(df "$target_dir" | awk 'NR==2 {print $4}' 2>/dev/null || echo "0")
    available_kb=${available_kb:-0}

    if [ "$available_kb" -ge "$min_kb" ]; then
        debug_msg "✓ $target_dir has sufficient space: ${available_kb}KB (need ${min_kb}KB)"
        return 0
    else
        debug_msg "✗ $target_dir insufficient space: ${available_kb}KB (need ${min_kb}KB)"
        return 1
    fi
}

# Enhanced cleanup with multiple directory support
cleanup_temp_library() {
    for location in /tmp /var/tmp /root/tmp .; do
        if [ -d "$location" ]; then
            # Clean files with our session ID
            find "$location" -name "*rutos_lib_$$*" -type f 2>/dev/null | while IFS= read -r file; do
                if [ -f "$file" ]; then
                    debug_msg "Cleaning up: $file"
                    rm -f "$file" 2>/dev/null || true
                fi
            done
        fi
    done
}
````

## Current Project Status (As of July 2025)

### Development Milestones Achieved

- **Round 1-23**: Comprehensive testing and validation system development
- **RUTOS Library System**: Complete 4-module framework implemented
- **Production Ready**: Full RUTOS compatibility achieved
- **Automated Quality**: Pre-commit validation system implemented
- **Repository Organized**: Clean structure with dedicated test directory
- **4-Level Logging**: Enhanced debugging with standardized framework
- **Validation Coverage**: 50+ compatibility checks implemented

### Core Features Operational

- ✅ **RUTOS Library System**: Complete 4-module framework with auto-loading
- ✅ **4-Level Logging**: NORMAL, DRY_RUN, DEBUG, RUTOS_TEST_MODE
- ✅ **Remote Installation**: Works via curl on RUTX50 with library download
- ✅ **Enhanced Installation Debugging**: Comprehensive system info, disk space management, and error reporting
- ✅ **Smart Disk Space Management**: Automatic fallback directories (/tmp → /var/tmp → /root/tmp → current dir)
- ✅ **Configuration Management**: Template migration and validation
- ✅ **Enhanced Debugging**: Command tracing with safe_execute()
- ✅ **Version Management**: Automatic semantic versioning
- ✅ **Quality Assurance**: Comprehensive pre-commit validation
- ✅ **RUTOS Compatibility**: Full busybox shell support with library

### Current Focus Areas

1. **Library Integration**: Migrate remaining scripts to use RUTOS library system
2. **Enhanced Monitoring**: Unified scripts with full library features
3. **Documentation**: Complete library system documentation
4. **Azure Integration**: Logging and monitoring components with library support

### Recent Improvements

- **Library System**: Complete RUTOS library implementation with 4 modules
- **Standardized Logging**: 4-level framework with Method 5 printf support
- **Enhanced Debugging**: Command tracing and variable tracking
- **Self-Contained Install**: Remote library download capability
- **Code Reduction**: Eliminated duplicate functions across all scripts
- **Round 23**: Debug output improvements with clean, structured logging
- **Modern Tooling**: Integration of shfmt and enhanced validation
- **Development Experience**: Better debugging and quality assurance
- **Enhanced Installation Debugging**: Comprehensive system info, disk space checking, and multi-level tracing for remote installations
- **Smart Disk Space Management**: Automatic fallback directory selection (/tmp → /var/tmp → /root/tmp → current dir) with proper cleanup
- **Installation Reliability**: Pre-download space validation and enhanced error reporting for curl/wget operations
- **Troubleshooting Support**: Detailed debugging output showing exact commands, file locations, permissions, and system context
