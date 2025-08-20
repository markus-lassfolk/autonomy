# RUTOS Starlink Failover Project Instructions

## Critical Project Requirements

- **Target**: RUTX50 router with RUTOS RUT5_R_00.07.09.7 (armv7l busybox shell)
- **Shell**: POSIX sh only (NO bash syntax)
- **Library**: Always use RUTOS Library System for all scripts
- **Colors**: Method 5 printf format for RUTOS compatibility
- **Deployment**: Remote installation via curl from GitHub

## RUTOS Library Pattern (MANDATORY)

Every new script MUST follow this pattern:

```bash
#!/bin/sh
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

## Critical Shell Compatibility Rules

1. **NO bash-specific syntax** - Use POSIX sh only
2. **NO arrays** - Use space-separated strings or multiple variables
3. **NO [[]]** - Use [ ] for all conditions
4. **NO function() syntax** - Use `function_name() {` format
5. **NO local variables** - All variables are global in busybox
6. **NO echo -e** - Use printf instead
7. **NO source command** - Use . (dot) for sourcing
8. **NO $'\n'** - Use actual newlines or printf

## NEVER Define These (Library Provides)

❌ **Never manually define these functions/variables:**
- `log_info()`, `log_error()`, `log_debug()`, `log_trace()`, etc.
- Color variables (`RED`, `GREEN`, `BLUE`, etc.)
- `safe_execute()` or similar command execution functions
- `get_timestamp()` or timestamp functions
- Environment validation functions

✅ **Library provides all of this automatically!**

## Method 5 Printf Format (RUTOS Compatible)

```bash
# CORRECT for RUTOS (Method 5) - Shows actual colors
printf "${RED}Error: %s${NC}\n" "$message"
printf "${GREEN}[INFO]${NC} [%s] %s\n" "$timestamp" "$message"

# WRONG - Shows escape codes in RUTOS
printf "%sError: %s%s\n" "$RED" "$message" "$NC"
```

## Standard Functions Always Available

After `rutos_init`, use these library functions:

```bash
# Logging (4-level framework)
log_info "General information"
log_success "Operation completed"
log_warning "Warning message"
log_error "Error message"
log_step "Progress step"
log_debug "Debug info (DEBUG=1)"
log_trace "Trace info (RUTOS_TEST_MODE=1)"

# Safe command execution
safe_execute "command here" "Description of command"

# Environment modes
DRY_RUN=1        # Safe mode - no actual changes
DEBUG=1          # Enable debug logging
RUTOS_TEST_MODE=1 # Enable trace logging
```

## File Naming Conventions

- Scripts: `script-name-rutos.sh`
- Configs: `config.template.sh`
- Documentation: `UPPERCASE.md`

## Quality Requirements

- Run `./scripts/pre-commit-validation.sh` before commits
- All scripts must pass ShellCheck with no errors
- Use WSL/Git Bash for shell script development (not PowerShell)
- Version information must be consistent across scripts
