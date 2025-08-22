# Comprehensive Code Quality System

**Version:** 3.0.0 | **Updated:** 2025-08-22

This document describes the comprehensive code quality system for the Autonomy networking project, ensuring high code quality, security, and maintainability across all components.

## Overview

We've implemented a multi-language code quality system that validates:

- **Go files** (.go) - gofmt, golint, govet, gosec, staticcheck, race detector
- **Shell scripts** (.sh) - ShellCheck + shfmt
- **Python files** (.py) - black, flake8, pylint, mypy, isort, bandit
- **PowerShell files** (.ps1) - PSScriptAnalyzer
- **Markdown files** (.md) - markdownlint + prettier
- **JSON/YAML files** - jq, yamllint, prettier
- **Configuration files** - UCI validation, ubus schema validation

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
# Validate only Go files
./scripts/comprehensive-validation.sh --go-only

# Validate only shell scripts
./scripts/comprehensive-validation.sh --shell-only

# Validate only Python files
./scripts/comprehensive-validation.sh --python-only

# Validate only Markdown files
./scripts/comprehensive-validation.sh --md-only
```

## Tool Categories

### Go Quality (6 tools)

- **gofmt**: Uncompromising code formatting
- **golint**: Style guide enforcement
- **govet**: Static analysis and bug detection
- **gosec**: Security vulnerability scanning
- **staticcheck**: Advanced static analysis
- **race detector**: Concurrency issue detection

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

### UCI/ubus Quality

- **UCI validation**: Configuration structure and content validation
- **ubus schema**: API schema validation and testing

## Configuration Files

### Go Configuration (`go.mod`, `go.work`)

Modern Go project configuration with:

- Go 1.23+ module system
- Workspace configuration for multi-module projects
- Dependency management with go.mod
- Tool version management with go.work

### Go Tools Configuration

```bash
# .golangci.yml - Comprehensive Go linting
linters:
  enable:
    - gofmt
    - golint
    - govet
    - gosec
    - staticcheck
    - errcheck
    - ineffassign
    - misspell
    - unconvert
    - gosimple
    - structcheck
    - varcheck
    - unused
    - deadcode
    - typecheck
    - gocyclo
    - dupl
    - goconst
    - gocritic
    - godot
    - goheader
    - goimports
    - gomnd
    - gomodguard
    - goprintffuncname
    - gosec
    - gosimple
    - govet
    - ineffassign
    - lll
    - misspell
    - nakedret
    - noctx
    - nolintlint
    - rowserrcheck
    - staticcheck
    - structcheck
    - stylecheck
    - typecheck
    - unconvert
    - unparam
    - unused
    - varcheck
    - whitespace
    - wrapcheck
    - wsl

run:
  timeout: 5m
  tests: true
  build-tags: ["autonomy"]

issues:
  exclude-rules:
    - path: _test\.go
      linters:
        - gomnd
        - gocyclo
```

### Python Configuration (`pyproject.toml`)

Modern Python project configuration with settings for:

- black (line length: 88)
- isort (compatible with black)
- pylint (custom rules)
- mypy (strict type checking)
- bandit (security rules)

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
./scripts/setup-code-quality-tools.sh --go
./scripts/setup-code-quality-tools.sh --python
./scripts/setup-code-quality-tools.sh --nodejs
./scripts/setup-code-quality-tools.sh --system
```

### Manual Installation Commands

#### Go Tools

```bash
# Install Go tools
go install golang.org/x/lint/golint@latest
go install github.com/securecodewarrior/gosec/v2/cmd/gosec@latest
go install honnef.co/go/tools/cmd/staticcheck@latest
go install github.com/golangci/golangci-lint/cmd/golangci-lint@latest

# Install additional Go tools
go install github.com/fzipp/gocyclo/cmd/gocyclo@latest
go install github.com/jgautheron/goconst/cmd/goconst@latest
go install github.com/mibk/dupl/cmd/dupl@latest
go install github.com/ultraware/funlen/cmd/funlen@latest
go install github.com/ultraware/whitespace/cmd/whitespace@latest
```

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
```

## GitHub Actions Integration

### Pre-commit Hooks

```yaml
# .github/workflows/code-quality.yml
name: Code Quality

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]

jobs:
  go-quality:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v4
    
    - name: Set up Go
      uses: actions/setup-go@v4
      with:
        go-version: '1.23'
    
    - name: Install Go tools
      run: |
        go install golang.org/x/lint/golint@latest
        go install github.com/securecodewarrior/gosec/v2/cmd/gosec@latest
        go install honnef.co/go/tools/cmd/staticcheck@latest
    
    - name: Run Go linters
      run: |
        gofmt -s -w .
        golint ./...
        go vet ./...
        gosec ./...
        staticcheck ./...
    
    - name: Run tests
      run: go test -v -race ./...
    
    - name: Run benchmarks
      run: go test -bench=. ./...

  shell-quality:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v4
    
    - name: Install shellcheck
      run: sudo apt-get install -y shellcheck
    
    - name: Run shellcheck
      run: shellcheck scripts/*.sh

  markdown-quality:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v4
    
    - name: Install markdownlint
      run: npm install -g markdownlint-cli
    
    - name: Run markdownlint
      run: markdownlint docs/ README.md
```

## Security Scanning

### Go Security (gosec)

```bash
# Run security scan
gosec ./...

# Generate security report
gosec -fmt json -out security-report.json ./...
```

### Python Security (bandit)

```bash
# Run security scan
bandit -r ./scripts/

# Generate security report
bandit -r ./scripts/ -f json -o security-report.json
```

### Dependency Scanning

```bash
# Go dependencies
go list -m all | nancy sleuth

# Python dependencies
safety check

# Node.js dependencies
npm audit
```

## Performance Analysis

### Go Performance

```bash
# Run benchmarks
go test -bench=. -benchmem ./...

# Generate CPU profile
go test -cpuprofile=cpu.prof -bench=. ./...

# Generate memory profile
go test -memprofile=mem.prof -bench=. ./...

# Analyze profiles
go tool pprof cpu.prof
go tool pprof mem.prof
```

### Memory Analysis

```bash
# Run with memory profiling
go test -memprofile=mem.prof ./...

# Analyze memory usage
go tool pprof -alloc_space mem.prof
```

## Code Coverage

### Go Coverage

```bash
# Generate coverage report
go test -coverprofile=coverage.out ./...

# View coverage in browser
go tool cover -html=coverage.out -o coverage.html

# Coverage threshold check
go test -coverprofile=coverage.out ./...
go tool cover -func=coverage.out | grep total | awk '{print $3}' | sed 's/%//' | awk '{if($1 < 80) exit 1}'
```

## Continuous Integration

### Pre-commit Validation

```bash
#!/bin/bash
# scripts/pre-commit.sh

set -e

echo "🔍 Running pre-commit validation..."

# Go formatting and linting
echo "📝 Formatting Go code..."
gofmt -s -w .

echo "🔍 Running Go linters..."
golangci-lint run

echo "🧪 Running Go tests..."
go test -v -race ./...

echo "📊 Checking code coverage..."
go test -coverprofile=coverage.out ./...
coverage=$(go tool cover -func=coverage.out | grep total | awk '{print $3}' | sed 's/%//')
echo "Code coverage: ${coverage}%"

if (( $(echo "$coverage < 80" | bc -l) )); then
    echo "❌ Code coverage below 80%"
    exit 1
fi

# Shell script validation
echo "🐚 Validating shell scripts..."
shellcheck scripts/*.sh

# Markdown validation
echo "📖 Validating markdown..."
markdownlint docs/ README.md

echo "✅ Pre-commit validation passed!"
```

## Quality Metrics

### Code Quality Metrics

```go
type QualityMetrics struct {
    CodeCoverage     float64 `json:"code_coverage"`
    CyclomaticComplexity int `json:"cyclomatic_complexity"`
    LinesOfCode      int    `json:"lines_of_code"`
    SecurityIssues   int    `json:"security_issues"`
    LintIssues       int    `json:"lint_issues"`
    TestCount        int    `json:"test_count"`
    BenchmarkCount   int    `json:"benchmark_count"`
}
```

### Performance Metrics

```go
type PerformanceMetrics struct {
    MemoryUsage      int64   `json:"memory_usage"`
    CPUUsage         float64 `json:"cpu_usage"`
    ResponseTime     float64 `json:"response_time"`
    Throughput       float64 `json:"throughput"`
    ErrorRate        float64 `json:"error_rate"`
}
```

## Best Practices

### Go Best Practices

1. **Error Handling**: Always check and handle errors explicitly
2. **Context Usage**: Use context.Context for cancellation and timeouts
3. **Resource Management**: Use defer for cleanup
4. **Concurrency**: Use sync.RWMutex for thread-safe operations
5. **Testing**: Write comprehensive unit and integration tests
6. **Documentation**: Document exported functions and types

### Security Best Practices

1. **Input Validation**: Validate all inputs with comprehensive validation
2. **Secure Communication**: Use HTTPS/TLS for all network communication
3. **Authentication**: Implement proper authentication for API endpoints
4. **Logging**: Never log sensitive information
5. **Dependencies**: Regularly update dependencies and scan for vulnerabilities

### Performance Best Practices

1. **Memory Management**: Use efficient data structures and avoid memory leaks
2. **Concurrency**: Use goroutines appropriately with proper synchronization
3. **Caching**: Implement caching with TTL for expensive operations
4. **Profiling**: Regular performance profiling and optimization
5. **Resource Limits**: Set appropriate resource limits for embedded systems

## Troubleshooting

### Common Issues

1. **Go tool installation**: Ensure Go 1.23+ is installed
2. **Permission issues**: Use `go install` with proper permissions
3. **Network issues**: Configure proxy settings for corporate networks
4. **Memory issues**: Increase memory limits for large codebases

### Debug Mode

```bash
# Enable debug logging
export DEBUG=1
export GOLANGCI_LINT_DEBUG=1

# Run with verbose output
golangci-lint run --verbose
```

## 📞 Support

For issues and support:
- Check logs: `golangci-lint run --verbose`
- Documentation: `/usr/share/doc/autonomy/code-quality.md`
- Configuration: `.golangci.yml`
- GitHub Issues: Report bugs and feature requests
