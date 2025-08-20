# Comprehensive Code Quality System

**Version:** 2.6.0 | **Updated:** 2025-07-24

This document describes the comprehensive code quality system for the RUTOS Starlink Failover project.

## Overview

We've implemented a multi-language code quality system that validates:

- **Shell scripts** (.sh) - ShellCheck + shfmt
- **Python files** (.py) - black, flake8, pylint, mypy, isort, bandit
- **PowerShell files** (.ps1) - PSScriptAnalyzer
- **Markdown files** (.md) - markdownlint + prettier
- **JSON/YAML files** - jq, yamllint, prettier
- **Azure Bicep files** (.bicep) - bicep lint

## Quick Start

### 1. Install All Tools

```bash
# One-time setup - installs all code quality tools
./scripts/setup-code-quality-tools.sh
```

### 2. Run Comprehensive Validation

```bash
# Validate all files in the repository
./scripts/comprehensive-validation.sh --all

# Or use the convenient alias (after setup)
validate-code --all
```

### 3. Language-Specific Validation

```bash
# Validate only shell scripts
./scripts/comprehensive-validation.sh --shell-only

# Validate only Python files
./scripts/comprehensive-validation.sh --python-only

# Validate only Markdown files
./scripts/comprehensive-validation.sh --md-only
```

## Tool Categories

### Shell Script Quality (ShellCheck + shfmt)

- **ShellCheck**: POSIX compliance, bug detection, best practices
- **shfmt**: Consistent formatting and style
- **Focus**: RUTOS/busybox compatibility

### Python Quality (6 tools)

- **black**: Uncompromising code formatting
- **isort**: Import statement sorting
- **flake8**: Style guide enforcement (PEP 8)
- **pylint**: Comprehensive code analysis
- **mypy**: Static type checking
- **bandit**: Security vulnerability scanning

### PowerShell Quality (PSScriptAnalyzer)

- **PSScriptAnalyzer**: PowerShell best practices and style

### Markdown Quality (markdownlint + prettier)

- **markdownlint**: Markdown structure and style
- **prettier**: Consistent formatting

### Configuration Quality (jq, yamllint, prettier)

- **jq**: JSON syntax validation
- **yamllint**: YAML structure validation
- **prettier**: Consistent formatting

### Azure Infrastructure Quality (Bicep)

- **bicep lint**: Azure resource validation

## Configuration Files

### Python Configuration (`pyproject.toml`)

Modern Python project configuration with settings for:

- black (line length: 88)
- isort (compatible with black)
- pylint (custom rules)
- mypy (strict type checking)
- bandit (security rules)

### Flake8 Configuration (`setup.cfg`)

Since flake8 doesn't support pyproject.toml yet:

- Line length: 88 (matches black)
- Ignores conflicts with black
- Per-file ignores for **init**.py

### Markdown Configuration (`.markdownlint.json`)

- Line length: 120 characters
- Allows HTML elements for documentation
- Consistent heading styles

### Prettier Configuration (`.prettierrc.json`)

- Print width: 100 characters
- Language-specific overrides
- Consistent formatting across file types

## Installation Options

### Automatic Installation

```bash
# Install all tools
./scripts/setup-code-quality-tools.sh

# Install specific categories
./scripts/setup-code-quality-tools.sh --python
./scripts/setup-code-quality-tools.sh --nodejs
./scripts/setup-code-quality-tools.sh --system
```

### Manual Installation Commands

#### Ubuntu/Debian

```bash
# System tools
sudo apt-get update
sudo apt-get install -y shellcheck shfmt jq nodejs npm

# Python tools
pip3 install --user black flake8 pylint mypy isort bandit yamllint

# Node.js tools
npm install -g markdownlint-cli prettier

# PowerShell (optional)
wget -q https://packages.microsoft.com/config/ubuntu/20.04/packages-microsoft-prod.deb
sudo dpkg -i packages-microsoft-prod.deb
sudo apt-get update
sudo apt-get install -y powershell
pwsh -Command "Install-Module -Name PSScriptAnalyzer -Force"

# Azure CLI and Bicep (optional)
curl -sL https://aka.ms/InstallAzureCLIDeb | sudo bash
az bicep install
```

#### macOS

```bash
# System tools
brew install shellcheck shfmt jq node

# Python tools
pip3 install --user black flake8 pylint mypy isort bandit yamllint

# Node.js tools
npm install -g markdownlint-cli prettier

# PowerShell (optional)
brew install --cask powershell

# Azure CLI and Bicep (optional)
brew install azure-cli
az bicep install
```

#### Windows (PowerShell)

```powershell
# Install Chocolatey first
Set-ExecutionPolicy Bypass -Scope Process -Force
[System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072
iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))

# Install tools
choco install shellcheck jq nodejs python3 azure-cli
pip install black flake8 pylint mypy isort bandit yamllint
npm install -g markdownlint-cli prettier
az bicep install
```

## Usage Examples

### Comprehensive Validation

```bash
# Check all files in the repository
./scripts/comprehensive-validation.sh --all

# Check specific files
./scripts/comprehensive-validation.sh script.sh analysis.py README.md

# Check only specific file types
./scripts/comprehensive-validation.sh --python-only
./scripts/comprehensive-validation.sh --shell-only
./scripts/comprehensive-validation.sh --md-only
```

### Individual Tool Usage

```bash
# Python tools
black --check analysis.py
isort --check-only analysis.py
flake8 analysis.py
pylint analysis.py
mypy analysis.py
bandit analysis.py

# Shell tools
shellcheck script.sh
shfmt -i 4 -ci -d script.sh

# Markdown tools
markdownlint README.md
prettier --check README.md

# JSON/YAML tools
jq empty config.json
yamllint config.yaml
prettier --check config.json
```

### Auto-fixing Issues

```bash
# Python auto-fixes
black analysis.py
isort analysis.py

# Shell auto-fixes
shfmt -i 4 -ci -w script.sh

# Markdown auto-fixes
prettier --write README.md

# JSON/YAML auto-fixes
prettier --write config.json
```

## Integration with Development Workflow

### Pre-commit Hook Integration

```bash
# Use the existing pre-commit validation (shell scripts only)
./scripts/pre-commit-validation.sh

# Or use comprehensive validation for all languages
./scripts/comprehensive-validation.sh --all
```

### VS Code Integration

Install these extensions for real-time validation:

- **Python**: Python extension (includes pylint, black integration)
- **Shell**: ShellCheck extension
- **PowerShell**: PowerShell extension
- **Markdown**: markdownlint extension
- **Prettier**: Prettier extension

### Git Hook Setup

```bash
# Install comprehensive validation as pre-commit hook
cp scripts/comprehensive-validation.sh .git/hooks/pre-commit
chmod +x .git/hooks/pre-commit
```

## Validation Output

### Success Output

```console
[SUCCESS] [2025-07-15 14:30:45] ✅ analysis.py passed validation
[SUCCESS] [2025-07-15 14:30:46] ✅ script.sh passed validation
[SUCCESS] [2025-07-15 14:30:47] ✅ README.md passed validation
[INFO] [2025-07-15 14:30:48] 🎉 All files passed comprehensive validation!
```

### Failure Output

```console
[ERROR] [2025-07-15 14:30:45] Black formatting issues in analysis.py
[INFO] [2025-07-15 14:30:45] Run 'black analysis.py' to fix formatting
[ERROR] [2025-07-15 14:30:46] ShellCheck failed for script.sh
[ERROR] [2025-07-15 14:30:47] markdownlint issues in README.md
[ERROR] [2025-07-15 14:30:48] ❌ 3 validation issues found across 3 files
```

## Tool Availability Check

The validation script automatically checks which tools are available:

```bash
# Check what tools are installed
./scripts/comprehensive-validation.sh --install-deps
./scripts/setup-code-quality-tools.sh --verify
```

## Best Practices

### Python Development

1. **Format first**: Run `black` and `isort` before other checks
2. **Fix style**: Address `flake8` warnings
3. **Comprehensive analysis**: Run `pylint` for detailed feedback
4. **Type safety**: Use `mypy` for type checking
5. **Security**: Run `bandit` for security issues

### Shell Development

1. **POSIX compliance**: Use `shellcheck` for compatibility
2. **Consistent formatting**: Use `shfmt` for style
3. **RUTOS focus**: Ensure busybox compatibility

### Documentation

1. **Structure**: Use `markdownlint` for consistent structure
2. **Formatting**: Use `prettier` for consistent style
3. **Readability**: Keep lines under 120 characters

### Configuration File Standards

1. **Syntax**: Validate with `jq` (JSON) and `yamllint` (YAML)
2. **Formatting**: Use `prettier` for consistency
3. **Structure**: Maintain clear, readable configuration

## Troubleshooting

### Common Issues

#### Tool Not Found

```bash
# Check tool availability
./scripts/setup-code-quality-tools.sh --verify

# Install missing tools
./scripts/setup-code-quality-tools.sh --python
```

#### Path Issues

```bash
# Add user bin to PATH
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

#### Permission Issues

```bash
# Install tools with --user flag
pip3 install --user black flake8 pylint mypy isort bandit yamllint
```

### Debug Mode

```bash
# Run with debug output
DEBUG=1 ./scripts/comprehensive-validation.sh --all
```

## Integration with CI/CD

### GitHub Actions Example

```yaml
name: Code Quality

on: [push, pull_request]

jobs:
  quality:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Setup Code Quality Tools
        run: ./scripts/setup-code-quality-tools.sh

      - name: Run Comprehensive Validation
        run: ./scripts/comprehensive-validation.sh --all
```

### Pre-commit Configuration

```yaml
# .pre-commit-config.yaml
repos:
  - repo: local
    hooks:
      - id: comprehensive-validation
        name: Comprehensive Code Quality
        entry: ./scripts/comprehensive-validation.sh
        language: script
        pass_filenames: false
        always_run: true
```

## Performance Considerations

### Selective Validation

```bash
# Validate only changed files
git diff --name-only | xargs ./scripts/comprehensive-validation.sh

# Validate by file type
./scripts/comprehensive-validation.sh --python-only
```

### Parallel Processing

The comprehensive validation script processes files sequentially but validates each file with multiple tools in parallel
where possible.

## Conclusion

This comprehensive code quality system ensures:

- **Consistent style** across all languages
- **High code quality** with multiple validation layers
- **Security scanning** for vulnerabilities
- **Documentation quality** for maintainability
- **Configuration validation** for reliability
- **RUTOS compatibility** for production deployment

The system is designed to be:

- **Easy to install** with automated setup
- **Easy to use** with simple commands
- **Comprehensive** covering all file types
- **Configurable** with sensible defaults
- **Maintainable** with clear documentation

Use `./scripts/comprehensive-validation.sh --help` for detailed usage information.
