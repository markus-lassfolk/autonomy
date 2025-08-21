# autonomy Verification Scripts

This directory contains comprehensive verification scripts for the autonomy project, supporting both Go backend and LuCI frontend components.

## 📋 Overview

The verification system provides multiple levels of code quality assurance:

- **Go Verification**: Code formatting, linting, security scanning, testing, and build verification
- **LuCI Verification**: Lua syntax checking, HTML/JS/CSS validation, and translation validation
- **Comprehensive Verification**: Complete end-to-end verification of all components

## 🚀 Quick Start

### Basic Usage

```bash
# Run comprehensive verification (Go + LuCI)
./scripts/verify-comprehensive.sh all

# Run Go-only verification
./scripts/verify-comprehensive.sh go

# Run LuCI-only verification
./scripts/verify-comprehensive.sh luci

# Pre-commit check (staged files only)
./scripts/verify-comprehensive.sh staged
```

### Using Makefile

```bash
# Comprehensive verification
make verify-comprehensive

# Go-only verification
make verify-all

# LuCI-only verification
make verify-luci

# Pre-commit check
make verify
```

## 📁 Script Files

### Core Scripts

| Script | Platform | Description |
|--------|----------|-------------|
| `verify-comprehensive.sh` | Unix/Linux/macOS | Complete verification (Go + LuCI) |
| `verify-comprehensive.ps1` | Windows PowerShell | Complete verification (Go + LuCI) |
| `verify-go-enhanced.ps1` | Windows PowerShell | Enhanced Go-only verification |
| `verify-go.sh` | Unix/Linux/macOS | Basic Go verification |

### Legacy Scripts

| Script | Platform | Description |
|--------|----------|-------------|
| `verify-go.ps1` | Windows PowerShell | Basic Go verification (legacy) |

## 🛠️ Verification Modes

### Modes

- **`all`** - Check all components (Go + LuCI) [default]
- **`go`** - Check only Go components
- **`luci`** - Check only LuCI components
- **`files`** - Check specific files or patterns
- **`staged`** - Check staged files for pre-commit
- **`commit`** - Check files in git diff --cached
- **`ci`** - CI/CD mode with all checks

### Examples

```bash
# Check specific files
./scripts/verify-comprehensive.sh files "*.lua" "pkg/controller/*.go"

# Pre-commit check
./scripts/verify-comprehensive.sh staged

# CI/CD mode with coverage and race detection
./scripts/verify-comprehensive.sh ci --coverage --race

# Auto-fix mode
./scripts/verify-comprehensive.sh all --fix
```

## 🔧 Go Verification

### Tools Used

| Tool | Purpose | Installation |
|------|---------|--------------|
| `gofmt` | Code formatting | `go install golang.org/x/tools/cmd/gofmt@latest` |
| `goimports` | Import organization | `go install golang.org/x/tools/cmd/goimports@latest` |
| `golangci-lint` | Comprehensive linting | `go install github.com/golangci/golangci-lint/cmd/golangci-lint@latest` |
| `staticcheck` | Static analysis | `go install honnef.co/go/tools/cmd/staticcheck@latest` |
| `gocritic` | Code quality checks | `go install github.com/go-critic/go-critic/cmd/gocritic@latest` |
| `gosec` | Security scanning | `go install github.com/securego/gosec/v2/cmd/gosec@latest` |
| `go test` | Unit testing | Built-in |
| `go build` | Build verification | Built-in |

### Go Options

| Option | Description |
|--------|-------------|
| `--no-go` | Skip Go verification |
| `--no-format` | Skip formatting checks |
| `--no-lint` | Skip linting checks |
| `--no-security` | Skip security checks |
| `--no-tests` | Skip tests |
| `--no-build` | Skip build verification |
| `--coverage` | Generate test coverage report |
| `--race` | Enable race detection in tests |

## 🎨 LuCI Verification

### Tools Used

| Tool | Purpose | Installation |
|------|---------|--------------|
| `lua` | Lua syntax checking | Install from https://www.lua.org/download.html |
| `luacheck` | Lua linting | `luarocks install luacheck` |
| `htmlhint` | HTML validation | `npm install -g htmlhint` |
| `eslint` | JavaScript linting | `npm install -g eslint` |
| `stylelint` | CSS linting | `npm install -g stylelint` |
| `msgfmt` | Translation validation | Install gettext package |

### LuCI Options

| Option | Description |
|--------|-------------|
| `--no-luci` | Skip LuCI verification |
| `--no-translations` | Skip translation validation |

## ⚙️ Configuration Options

### General Options

| Option | Description |
|--------|-------------|
| `-h, --help` | Show detailed help |
| `-v, --verbose` | Enable verbose output |
| `-q, --quiet` | Quiet mode (errors only) |
| `--dry-run` | Show what would be done |
| `--fix` | Attempt to fix issues automatically |

### Timeout

- Default timeout: 300 seconds per check
- Configurable via `--timeout` option

## 🔧 Installation

### Prerequisites

#### Go Tools
```bash
# Install all Go tools
make install-tools

# Or install individually
go install golang.org/x/tools/cmd/goimports@latest
go install honnef.co/go/tools/cmd/staticcheck@latest
go install github.com/securego/gosec/v2/cmd/gosec@latest
go install github.com/golangci/golangci-lint/cmd/golangci-lint@latest
```

#### LuCI Tools
```bash
# Install all LuCI tools
make install-luci-tools

# Or install individually
npm install -g htmlhint eslint stylelint
luarocks install luacheck

# Install Lua and gettext manually
# - Lua: https://www.lua.org/download.html
# - gettext: Package manager (apt, yum, brew, etc.)
```

### Platform-Specific Installation

#### Windows
```powershell
# Install Node.js tools
npm install -g htmlhint eslint stylelint

# Install Lua tools (requires LuaRocks)
luarocks install luacheck

# Install gettext (via Chocolatey or manual download)
choco install gettext
```

#### macOS
```bash
# Install via Homebrew
brew install lua gettext

# Install Node.js tools
npm install -g htmlhint eslint stylelint

# Install Lua tools
luarocks install luacheck
```

#### Linux (Ubuntu/Debian)
```bash
# Install system packages
sudo apt update
sudo apt install lua5.3 luarocks gettext

# Install Node.js tools
npm install -g htmlhint eslint stylelint

# Install Lua tools
luarocks install luacheck
```

## 📊 Output and Reporting

### Log Levels

- **INFO** - General information
- **SUCCESS** - Successful operations
- **WARNING** - Non-critical issues
- **ERROR** - Critical issues
- **VERBOSE** - Detailed debugging information

### Statistics

The scripts provide comprehensive statistics:
- Total checks run
- Passed checks
- Failed checks
- Warnings
- Execution time

### Exit Codes

- **0** - All checks passed
- **1** - One or more checks failed

## 🔄 Integration

### Pre-commit Hooks

Add to `.git/hooks/pre-commit`:
```bash
#!/bin/bash
make verify
```

### CI/CD Integration

#### GitHub Actions
```yaml
name: Verification
on: [push, pull_request]
jobs:
  verify:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - uses: actions/setup-go@v4
        with:
          go-version: '1.21'
      - uses: actions/setup-node@v3
        with:
          node-version: '18'
      - run: make install-tools
      - run: make install-luci-tools
      - run: make verify-comprehensive
```

#### GitLab CI
```yaml
verify:
  stage: test
  script:
    - make install-tools
    - make install-luci-tools
    - make verify-comprehensive
```

## 🐛 Troubleshooting

### Common Issues

#### Tool Not Found
```
[WARNING] [Setup] Tool 'luacheck' not found
[INFO] [Setup] Install with: luarocks install luacheck
```

**Solution**: Install the missing tool using the provided command.

#### Permission Denied
```
Permission denied: ./scripts/verify-comprehensive.sh
```

**Solution**: Make the script executable:
```bash
chmod +x scripts/verify-comprehensive.sh
```

#### Timeout Issues
```
Command timed out after 300 seconds
```

**Solution**: Increase timeout or optimize the specific check.

### Debug Mode

Enable verbose output for debugging:
```bash
./scripts/verify-comprehensive.sh all --verbose
```

### Dry Run Mode

Test what would be done without making changes:
```bash
./scripts/verify-comprehensive.sh all --dry-run
```

## 📈 Performance

### Optimization Tips

1. **Use specific modes**: Run only what you need
   ```bash
   # Only Go verification
   ./scripts/verify-comprehensive.sh go
   
   # Only LuCI verification
   ./scripts/verify-comprehensive.sh luci
   ```

2. **Skip unnecessary checks**:
   ```bash
   # Skip tests for quick check
   ./scripts/verify-comprehensive.sh all --no-tests
   
   # Skip build verification
   ./scripts/verify-comprehensive.sh all --no-build
   ```

3. **Use staged mode for pre-commit**:
   ```bash
   ./scripts/verify-comprehensive.sh staged
   ```

### Typical Execution Times

| Mode | Go Files | LuCI Files | Typical Time |
|------|----------|------------|--------------|
| `go` | 30 | 0 | 15-30s |
| `luci` | 0 | 50 | 10-20s |
| `all` | 30 | 50 | 25-50s |
| `staged` | 5 | 2 | 5-15s |

## 🤝 Contributing

### Adding New Tools

1. Add tool configuration to the script
2. Create verification function
3. Add to main execution flow
4. Update documentation

### Script Structure

```
scripts/
├── verify-comprehensive.sh      # Main comprehensive script
├── verify-comprehensive.ps1     # Windows PowerShell version
├── verify-go-enhanced.ps1       # Enhanced Go verification
├── verify-go.sh                 # Basic Go verification
└── VERIFICATION_README.md       # This documentation
```

## 📝 License

This verification system is part of the autonomy project and follows the same license terms.

## 🆘 Support

For issues with the verification scripts:

1. Check the troubleshooting section
2. Enable verbose output for debugging
3. Review the tool installation requirements
4. Check platform-specific requirements

---

**Note**: The verification scripts are designed to be comprehensive and may take some time to complete. Use specific modes and options to optimize for your workflow.
