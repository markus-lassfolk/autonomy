# Go Verification Scripts

This directory contains comprehensive Go code verification scripts for the autonomy project.

## Scripts

### PowerShell Script (`verify-go.ps1`)
- **Platform**: Windows (PowerShell 7+)
- **Usage**: `powershell -ExecutionPolicy Bypass -File scripts/verify-go.ps1 [OPTIONS] MODE`

### Bash Script (`verify-go.sh`)
- **Platform**: Linux/macOS/Unix
- **Usage**: `bash scripts/verify-go.sh [OPTIONS] MODE`

## Modes

### 1. All Files (`all`)
Verifies all Go files in the project.

```bash
# PowerShell
.\scripts\verify-go.ps1 -Mode all

# Bash
./scripts/verify-go.sh all
```

### 2. Specific Files (`files`)
Verifies only the specified files.

```bash
# PowerShell
.\scripts\verify-go.ps1 -Mode files -Files "cmd/autonomyd/main.go,pkg/logx/logger.go"

# Bash
./scripts/verify-go.sh files -f "cmd/autonomyd/main.go,pkg/logx/logger.go"
```

### 3. Staged Files (`staged`)
Verifies only files staged for commit (perfect for pre-commit hooks).

```bash
# PowerShell
.\scripts\verify-go.ps1 -Mode staged

# Bash
./scripts/verify-go.sh staged
```

## Options

### PowerShell Options
- `-SkipTests`: Skip running tests (useful for quick checks)
- `-Verbose`: Enable verbose output

### Bash Options
- `-s, --skip-tests`: Skip running tests
- `-v, --verbose`: Enable verbose output
- `-h, --help`: Show help message

## Verification Steps

The scripts perform the following checks in order:

1. **Formatting** (`gofmt`)
   - Checks if code follows Go formatting standards
   - Reports files that need formatting

2. **Import Organization** (`goimports`)
   - Checks if imports are properly organized
   - Reports files with import issues

3. **Go Vet** (`go vet`)
   - Runs Go's built-in static analysis
   - Checks for common programming errors

4. **Static Analysis** (`staticcheck`)
   - Advanced static analysis
   - Finds bugs, performance issues, and style problems

5. **Security Check** (`gosec`)
   - Security-focused static analysis
   - Identifies potential security vulnerabilities

6. **Linting** (`golangci-lint`)
   - Comprehensive linting with multiple linters
   - Enforces coding standards and best practices

7. **Testing** (`go test`)
   - Runs all tests with race detection
   - Can be skipped with `-SkipTests` or `-s` option

## Required Tools

The scripts check for the following tools and skip steps if they're not available:

- `go` - Go compiler (required)
- `gofmt` - Go formatter (included with Go)
- `goimports` - Import organizer
- `golangci-lint` - Linter
- `staticcheck` - Static analyzer
- `gosec` - Security checker

### Installing Missing Tools

Use the Makefile target to install all required tools:

```bash
make install-tools
```

Or install individually:

```bash
# Import organizer
go install golang.org/x/tools/cmd/goimports@latest

# Static analyzer
go install honnef.co/go/tools/cmd/staticcheck@latest

# Security checker
go install github.com/securecodewarrior/gosec/v2/cmd/gosec@latest

# Linter
go install github.com/golangci/golangci-lint/cmd/golangci-lint@latest
```

## Using with Makefile

The project includes a Makefile that provides convenient targets:

```bash
# Verify staged files (pre-commit)
make verify

# Verify all files
make verify-all

# Verify specific files
make verify-files FILES=cmd/autonomyd/main.go,pkg/logx/logger.go

# Quick verification without tests
make verify-quick
```

## Pre-commit Integration

To use these scripts as a pre-commit hook:

1. Create `.git/hooks/pre-commit`:

```bash
#!/bin/bash
# For Unix/Linux/macOS
make verify
```

Or for Windows:

```powershell
# For Windows
make verify
```

2. Make the hook executable (Unix/Linux/macOS):

```bash
chmod +x .git/hooks/pre-commit
```

## CI/CD Integration

For continuous integration, use the `ci` target:

```bash
make ci
```

This runs:
- Full verification on all files
- All tests
- Build process

## Exit Codes

- `0`: All checks passed
- `1`: One or more checks failed

## Output

The scripts provide colored output with clear status indicators:

- ✅ **Green**: Passed checks
- ❌ **Red**: Failed checks
- ⚠️ **Yellow**: Warnings (missing tools, etc.)
- 🔵 **Blue**: Information and progress

## Examples

### Quick Development Check
```bash
# Skip tests for faster feedback
make verify-quick
```

### Pre-commit Verification
```bash
# Only check staged files
make verify
```

### Full Project Verification
```bash
# Check everything including tests
make verify-all
```

### Specific File Check
```bash
# Check only modified files
make verify-files FILES=pkg/sysmgmt/manager.go,pkg/logx/logger.go
```

## Troubleshooting

### PowerShell Execution Policy
If you get execution policy errors on Windows:

```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

### Missing Tools
If tools are missing, the scripts will warn you and skip those checks. Install missing tools using:

```bash
make install-tools
```

### Git Not Found
For staged file verification, ensure you're in a git repository and git is available in your PATH.

### Permission Denied (Unix/Linux/macOS)
Make the bash script executable:

```bash
chmod +x scripts/verify-go.sh
```
