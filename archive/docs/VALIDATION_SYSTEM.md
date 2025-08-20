# RUTOS Busybox Compatibility Validation System

Version: 2.7.1

## Overview

This document describes the comprehensive pre-commit validation system designed to catch busybox compatibility issues
before they reach the RUTX50 router. The system was created after repeatedly encountering issues that only surfaced
during actual deployment.

## Purpose

The validation system addresses the problem of:

- **Late Discovery**: Issues only found during RUTX50 testing
- **Frequent Rework**: Commits that work in development but fail on busybox
- **Manual Checking**: Relying on human memory for compatibility rules
- **Inconsistent Standards**: Different approaches across scripts

## Components

### 1. Pre-commit Validation Script (`scripts/pre-commit-validation.sh`)

- **Comprehensive checks** for busybox compatibility
- **Self-validation** to catch recursive issues
- **Colored output** for easy issue identification
- **Detailed reporting** with line numbers and suggestions

### 2. Git Pre-commit Hook (`.git/hooks/pre-commit`)

- **Automatic execution** on every commit
- **Staged files only** - validates what's actually being committed
- **Blocking behavior** - prevents commits with critical issues
- **Bypass option** - `git commit --no-verify` for emergencies

### 3. Development Environment Setup (`scripts/setup-dev-environment.sh`)

- **One-command setup** for new developers
- **Dependency checking** (ShellCheck, git, etc.)
- **Hook installation** and testing
- **Usage instructions** and examples

## Validation Categories

### Critical Issues (🚨 CRITICAL)

These issues will cause scripts to fail completely on busybox:

1. **Process Substitution**: `< <(command)` → Use pipes instead
2. **Double Brackets**: `[[ ]]` → Use single brackets `[ ]`
3. **Arrays**: `declare -a` or `${array[@]}` → Use space-separated strings
4. **Local Variables**: `local var` → Remove `local` keyword
5. **Bash Strings**: `$'...'` → Use actual newlines or printf
6. **Here Strings**: `<<<` → Use pipes or here documents
7. **Brace Expansion**: `{1..10}` → Use seq or while loops
8. **Trap ERR**: `trap ERR` → Use `trap INT TERM`
9. **Curl -L**: `curl -L` → Use `curl -fsSL` or basic curl
10. **Stat Flags**: `stat -c/-f` → Use wc, ls, or other alternatives

### Major Issues (⚠️ MAJOR)

These issues will likely cause problems or unexpected behavior:

1. **Bash Shebang**: `#!/bin/bash` → Use `#!/bin/sh`
2. **Function Syntax**: `function name()` → Use `name()`
3. **Source Command**: `source file` → Use `. file`
4. **Echo -e**: `echo -e` → Use printf
5. **Printf Security**: Variables in format strings → Use `printf '%s' "$var"`
6. **Readlink -f**: May not be available → Use alternatives
7. **BC without Fallback**: `bc` → Include fallback with `2>/dev/null`

### Minor Issues (📝 MINOR)

These issues are best practices or potential portability concerns:

1. **== Comparison**: `[ "$a" == "$b" ]` → Use `[ "$a" = "$b" ]`
2. **Find -maxdepth**: May not be supported → Use alternatives
3. **Mktemp**: Without template → Include template parameter
4. **Missing Version**: No `SCRIPT_VERSION` variable
5. **Missing Error Handling**: No `set -e`
6. **Dirname/Basename**: Unquoted variables → Quote properly

## Usage

### Pre-commit Hook (Automatic)

```bash
# Normal commit - validation runs automatically
git commit -m "Your commit message"

# Bypass validation (NOT RECOMMENDED)
git commit --no-verify -m "Emergency commit"
```

### Manual Validation

```bash
# Validate all shell files
./scripts/pre-commit-validation.sh

# Validate only staged files
./scripts/pre-commit-validation.sh --staged

# Debug mode for troubleshooting
DEBUG=1 ./scripts/pre-commit-validation.sh
```

### Setup for New Developers

```bash
# One-time setup
./scripts/setup-dev-environment.sh

# Verify setup
./scripts/pre-commit-validation.sh --staged
```

## Validation Rules

### Shebang Validation

- ✅ `#!/bin/sh` - POSIX shell, busybox compatible
- ❌ `#!/bin/bash` - Bash-specific, not available on RUTOS
- ❌ Missing shebang - Unpredictable behavior

### Syntax Validation

The system checks for 50+ specific patterns that cause issues:

```bash
# WRONG - Bash-specific
if [[ "$var" =~ pattern ]]; then
    local result=$(command)
    array[0]="value"
fi

# CORRECT - POSIX/busybox compatible
if echo "$var" | grep -q "pattern"; then
    result=$(command)
    # Use space-separated values instead of arrays
fi
```

### RUTOS-Specific Checks

- **Curl flags**: Checks for unsupported flags
- **Stat commands**: Ensures compatible file operations
- **Find options**: Validates supported flags
- **Trap signals**: Ensures busybox-compatible signals

### Required Patterns

- **Version information**: `SCRIPT_VERSION` variable
- **Error handling**: `set -e` for fail-fast behavior
- **Function structure**: Proper opening/closing braces

## Integration with Development Workflow

### Before Committing

1. **Write code** using standard practices
2. **Stage changes** with `git add`
3. **Commit** - validation runs automatically
4. **Fix issues** if validation fails
5. **Re-commit** after fixes

### During Development

```bash
# Check compatibility while coding
./scripts/pre-commit-validation.sh path/to/script.sh

# Validate entire project
./scripts/pre-commit-validation.sh

# Quick quality check
./scripts/quality-check-enhanced.sh
```

### CI/CD Pipeline

The validation can be integrated into GitHub Actions:

```yaml
- name: Validate Shell Scripts
  run: |
    chmod +x scripts/pre-commit-validation.sh
    ./scripts/pre-commit-validation.sh
```

## Real-World Examples

### Example 1: Process Substitution Fix

```bash
# WRONG - Causes "syntax error: redirection unexpected"
while IFS=: read -r line_num line_content; do
    echo "Line: $line_num"
done < <(grep -n "pattern" file.txt)

# CORRECT - Busybox compatible
grep -n "pattern" file.txt | while IFS=: read -r line_num line_content; do
    echo "Line: $line_num"
done
```

### Example 2: Local Variable Fix

```bash
# WRONG - "local: not found"
function_name() {
    local var1="value1"
    local var2="value2"
}

# CORRECT - Global variables in busybox
function_name() {
    var1="value1"
    var2="value2"
}
```

### Example 3: Trap Signal Fix

```bash
# WRONG - "trap: ERR: bad trap"
trap 'echo "Error occurred"' ERR

# CORRECT - Busybox-supported signals
trap 'echo "Script interrupted"' INT TERM
```

## Performance Impact

- **Validation time**: ~1-2 seconds for typical script
- **Pre-commit overhead**: Minimal - only validates changed files
- **Development speed**: Faster due to early issue detection
- **Deployment reliability**: Significantly improved

## Troubleshooting

### Common Issues

1. **"Syntax error: redirection unexpected"**

   - Usually process substitution `< <(...)`
   - Solution: Use pipes instead

2. **"local: not found"**

   - Using `local` keyword in functions
   - Solution: Remove `local` keyword

3. **"trap: ERR: bad trap"**
   - Using `trap ERR` signal
   - Solution: Use `trap INT TERM`

### Debug Mode

```bash
# Enable debug output
DEBUG=1 ./scripts/pre-commit-validation.sh

# Check specific file
DEBUG=1 ./scripts/pre-commit-validation.sh path/to/script.sh
```

### Self-Validation

The validation script validates itself to catch recursive issues:

```bash
# Check validation script itself
./scripts/pre-commit-validation.sh scripts/pre-commit-validation.sh
```

## Maintenance

### Adding New Checks

1. **Identify the issue** during RUTX50 testing
2. **Add detection pattern** to appropriate function
3. **Test on existing scripts** to verify accuracy
4. **Update documentation** with examples

### Updating Patterns

```bash
# Add new check in check_bash_syntax()
if grep -n "new_pattern" "$file" >/dev/null 2>&1; then
    grep -n "new_pattern" "$file" | while IFS=: read -r line_num line_content; do
        report_issue "CRITICAL" "$file" "$line_num" "Description of issue"
    done
fi
```

### Testing Changes

```bash
# Test on all scripts
./scripts/pre-commit-validation.sh

# Test with known issues
./scripts/pre-commit-validation.sh test-files/

# Verify no false positives
./scripts/pre-commit-validation.sh scripts/install.sh
```

## Benefits

### For Developers

- **Early Detection**: Issues found before deployment
- **Learning Tool**: Understands busybox limitations
- **Consistent Standards**: Enforced coding practices
- **Faster Debugging**: Clear error messages with line numbers

### For Project

- **Reliability**: Fewer deployment failures
- **Quality**: Consistent code across all scripts
- **Maintainability**: Self-documenting compatibility rules
- **Documentation**: Living record of compatibility requirements

### For RUTX50 Deployment

- **Success Rate**: Higher first-time deployment success
- **Fewer Iterations**: Less back-and-forth with fixes
- **Predictable Behavior**: Scripts work as expected
- **Reduced Downtime**: Fewer failed deployments

## Future Enhancements

### Planned Features

1. **Custom Rules**: Project-specific validation rules
2. **Integration**: VS Code extension for real-time validation
3. **Metrics**: Track validation success rates
4. **Auto-fix**: Suggestions for common issues

### Advanced Validation

1. **Semantic Analysis**: Beyond syntax checking
2. **Performance Checks**: Resource usage validation
3. **Security Scanning**: Common security pitfalls
4. **Dependency Validation**: Required tools and versions

---

**The validation system learns from every issue encountered, continuously improving to prevent similar problems in the
future.**
