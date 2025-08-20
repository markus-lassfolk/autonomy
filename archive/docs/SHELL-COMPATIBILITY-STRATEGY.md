# Shell Compatibility Strategy for RUTOS

<!-- Version: 2.7.0 - Auto-updated documentation -->

## Overview

RUTOS uses ash/dash shell, but our CI/CD workflows expect bash syntax for proper validation.

## Solution: Dual Compatibility Approach

### For Deployment Scripts (RUTOS-specific)

- Use `#!/bin/sh` shebang
- POSIX shell syntax only
- Compatible with ash/dash
- Examples: `deploy-starlink-solution.sh`, `tests/rutos-compatibility-test.sh`

### For Development/CI Scripts (Development environment)

- Keep `#!/bin/bash` shebang
- Can use bash-specific features
- Validated by CI/CD workflows
- Examples: All scripts in `Starlink-RUTOS-Failover/` folder

### Implementation Strategy

#### Option 1: Rename Deployment Script

- `deploy-starlink-solution.sh` → `deploy-starlink-solution-rutos.sh`
- Keep original with bash shebang for CI/CD
- New version optimized for RUTOS

#### Option 2: Conditional Shebang Detection

- Scripts detect their runtime environment
- Use appropriate syntax based on available shell

#### Option 3: CI/CD Workflow Updates

- Update shellcheck to handle both bash and POSIX sh
- Add special handling for RUTOS-specific scripts

## Recommended Approach: Option 1

1. **Keep existing scripts as-is** (bash shebangs for CI/CD)
2. **Create RUTOS-specific variants** for deployment
3. **Update CI/CD to validate both versions**

This maintains CI/CD integrity while providing RUTOS compatibility.
