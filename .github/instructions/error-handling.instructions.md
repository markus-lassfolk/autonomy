---
applyTo: "scripts/**/*.sh"
description: "Error handling and debugging patterns for RUTOS"
---

# Error Handling for RUTOS

Implement comprehensive error handling with library support.

## Standard Pattern
```bash
set -e  # Exit on error

# Use safe_execute for all commands
safe_execute "command here" "Description of operation"

# Error handling with context
if ! some_operation; then
    log_error "Operation failed with context information"
    return 1
fi
```

## Debug Support
```bash
# Always support debug modes
DRY_RUN="${DRY_RUN:-0}"
DEBUG="${DEBUG:-0}"
RUTOS_TEST_MODE="${RUTOS_TEST_MODE:-0}"
```

## Function Patterns
```bash
# Clean output functions (no logging)
get_data() {
    # Return pure data only
    find . -name "*.sh"
}

# Calling functions can log
log_step "Collecting data"
data=$(get_data)
```
