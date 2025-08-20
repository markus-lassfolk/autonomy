---
applyTo: "scripts/**/*-rutos.sh"
description: "RUTOS Library System usage for standardized scripts"
---

# RUTOS Library System Usage

Always load and initialize RUTOS library system.

## Required Pattern
```bash
. "$(dirname "$0")/lib/rutos-lib.sh"
rutos_init "script-name-rutos.sh" "$SCRIPT_VERSION"
```

## Use Library Functions
- Use `log_info()`, `log_error()`, etc. instead of printf
- Use `safe_execute()` for all system commands
- Never define color variables - library provides them

## 4-Level Logging
- NORMAL: Standard operation
- DRY_RUN=1: Safe mode preview
- DEBUG=1: Debug information
- RUTOS_TEST_MODE=1: Full trace logging
