# RUTOS Development Workflow

This document outlines the development workflow, tools, and best practices for the RUTOS Starlink Failover project.

## Development Environment Setup

### Required Tools

- **VS Code** with extensions: ShellCheck, Bash IDE, GitLens, Error Lens
- **WSL/Git Bash** for shell script development (not PowerShell)
- **ShellCheck** for POSIX compliance validation
- **shfmt** for code formatting validation

### Terminal Configuration

- **Default**: PowerShell (Windows environment)
- **Recommended for Shell Development**: WSL or Git Bash
- **Best Practice**: Switch to WSL/Git Bash for shell script development and testing

```bash
# Switch to WSL for shell development
wsl

# Make scripts executable in WSL/Git Bash
chmod +x scripts/*.sh
```

## Quality Assurance Pipeline

### Pre-Commit Validation (MANDATORY)

```bash
# Run comprehensive validation before every commit
./scripts/pre-commit-validation.sh

# For staged files only (pre-commit hook usage)
./scripts/pre-commit-validation.sh --staged

# With debug output for troubleshooting
DEBUG=1 ./scripts/pre-commit-validation.sh
```

### Validation System Features

- **ShellCheck compliance** (POSIX sh only)
- **Bash-specific syntax detection**
- **RUTOS compatibility patterns**
- **Code formatting with shfmt**
- **Critical whitespace issues**
- **Template cleanliness validation**

### Quality Checklist

- [ ] Run pre-commit validation and fix all issues
- [ ] All shell scripts pass ShellCheck with no errors
- [ ] No bash-specific syntax (arrays, [[]], function() syntax)
- [ ] All functions have proper closing braces
- [ ] Version information is consistent across scripts
- [ ] Debug mode support is implemented
- [ ] Error handling is comprehensive
- [ ] Templates are clean (no ShellCheck comments)
- [ ] Code formatting passes shfmt validation

## Modern Development Workflow

1. **Switch to WSL/Bash** - Better shell script development environment
2. **Edit Scripts** - Use VS Code with ShellCheck extension
3. **Run Pre-Commit Validation** - `./scripts/pre-commit-validation.sh`
4. **Fix All Issues** - Address errors and warnings before commit
5. **Test Locally** - Validate syntax and basic functionality
6. **Commit** - Only after all quality checks pass

## Testing Strategies

### Remote Installation Testing

```bash
# Test with debug mode (shows system info, disk space, command details)
curl -fsSL https://raw.githubusercontent.com/markus-lassfolk/autonomy/main/scripts/install-rutos.sh | DEBUG=1 sh

# Test with trace logging (enhanced debugging)
curl -fsSL https://raw.githubusercontent.com/markus-lassfolk/autonomy/main/scripts/install-rutos.sh | DEBUG=1 RUTOS_TEST_MODE=1 sh

# Test disk space management with dry run
curl -fsSL https://raw.githubusercontent.com/markus-lassfolk/autonomy/main/scripts/install-rutos.sh | DEBUG=1 DRY_RUN=1 sh
```

### Local Testing Patterns

```bash
# Individual ShellCheck validation
shellcheck scripts/*.sh Starlink-RUTOS-Failover/*.sh

# Check for bash-specific patterns
grep -r "\[\[" scripts/ Starlink-RUTOS-Failover/  # Should return nothing
grep -r "local " scripts/ Starlink-RUTOS-Failover/  # Should return nothing
grep -r "echo -e" scripts/ Starlink-RUTOS-Failover/  # Should return nothing
```

## Git Workflow

### Branch Strategy

- **Main branch**: `main`
- **Development**: Feature branches
- **All changes**: Go through feature branches with PR reviews

### Commit Message Format

```text
Brief description of change

- Bullet point of specific change
- Another specific change
- Reference to issue/feature if applicable
- Quality: All ShellCheck issues resolved
```

## Environment Variables for Development

```bash
# Standard library environment variables
DRY_RUN=1        # Enable dry-run mode (safe execution)
DEBUG=1          # Enable debug logging
RUTOS_TEST_MODE=1 # Enable full trace logging
NO_COLOR=1       # Disable color output
ALLOW_TEST_EXECUTION=1 # Allow execution in test mode
```

## Troubleshooting Development Issues

### Common Issues and Solutions

1. **ShellCheck Errors** - Use pre-commit validation to catch and fix
2. **POSIX Compatibility** - Avoid bash-specific syntax patterns
3. **Color Display Issues** - Use Method 5 printf format for RUTOS
4. **Testing Failures** - Check for logging contamination in output functions
5. **Installation Failures** - Use enhanced debug mode for troubleshooting

### Debug Output Analysis

When analyzing debug output from failed operations:

1. **System Detection** - Look for proper architecture and disk space info
2. **Directory Setup** - Verify adequate space and successful creation
3. **Download Progress** - Check for successful vs. failed downloads
4. **Error Context** - Distinguish between disk/permission vs. network issues

## Tools Integration

- **ShellCheck** - Automated syntax and compatibility validation
- **shfmt** - Code formatting and style validation
- **Pre-commit Hooks** - Automated quality checks before commits
- **Debug Mode** - Enhanced debugging with clean output
- **Version Management** - Automatic version tracking and updates

## Success Metrics

### Code Quality Indicators

- ✅ POSIX sh compatibility (validated by pre-commit system)
- ✅ RUTOS Library System (standardized 4-level logging framework)
- ✅ Comprehensive error handling (enhanced with library functions)
- ✅ Enhanced command tracing (safe_execute with full context)
- ✅ Version information (automatic semantic versioning)

### Development Experience

- ✅ Modern tooling integration (ShellCheck, shfmt)
- ✅ Automated quality checks (pre-commit validation)
- ✅ Enhanced debug output (4-level structured logging)
- ✅ Consistent code formatting (shfmt validation)
