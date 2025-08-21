# Development Guide

This guide covers development setup, building, testing, and contributing to the autonomy project.

## Development Environment

### Prerequisites

- Go 1.22+ 
- Git
- Make (optional, for build scripts)
- Docker (optional, for containerized builds)

### Repository Setup

```bash
# Clone the repository
git clone https://github.com/your-org/autonomy.git
cd autonomy

# Install dependencies
go mod download

# Verify setup
go version
go env
```

## Building

### Local Development Build

```bash
# Build for your local platform
go build ./cmd/autonomyd

# Build with debug symbols
go build -gcflags="all=-N -l" ./cmd/autonomyd

# Build with race detection
go build -race ./cmd/autonomyd
```

### Cross-Platform Builds

```bash
# Build for ARM (RutOS/OpenWrt)
export CGO_ENABLED=0
GOOS=linux GOARCH=arm GOARM=7 go build -ldflags "-s -w" -o autonomyd ./cmd/autonomyd

# Build for x86_64
GOOS=linux GOARCH=amd64 go build -ldflags "-s -w" -o autonomyd ./cmd/autonomyd

# Build for multiple platforms
make build-all
```

### Production Builds

```bash
# Optimized production build
go build -ldflags "-s -w -X main.Version=$(git describe --tags)" ./cmd/autonomyd

# Build with specific version
go build -ldflags "-s -w -X main.Version=1.0.0" ./cmd/autonomyd
```

## Testing

### Unit Tests

```bash
# Run all tests
go test ./...

# Run tests with verbose output
go test -v ./...

# Run tests with coverage
go test -cover ./...

# Generate coverage report
go test -coverprofile=coverage.out ./...
go tool cover -html=coverage.out -o coverage.html
```

### Integration Tests

```bash
# Run integration tests
go test ./test/integration/...

# Run specific integration test
go test ./test/integration/ -run TestEndToEnd

# Run with timeout
go test -timeout 5m ./test/integration/...
```

### System Tests

```bash
# Run system tests (requires test environment)
make test-system

# Run tests in Docker
docker-compose -f test/docker-compose.yml up --abort-on-container-exit
```

### Test Utilities

```bash
# Run linter
golangci-lint run

# Run security scanner
gosec ./...

# Run static analysis
staticcheck ./...
```

## Development Workflow

### Code Quality

```bash
# Format code
go fmt ./...

# Run linter
make lint

# Run all quality checks
make quality
```

### Git Workflow

```bash
# Create feature branch
git checkout -b feature/new-feature

# Make changes and commit
git add .
git commit -m "feat: add new feature"

# Push and create PR
git push origin feature/new-feature
```

### Pre-commit Hooks

Install pre-commit hooks to ensure code quality:

```bash
# Install pre-commit
pip install pre-commit

# Install hooks
pre-commit install

# Run manually
pre-commit run --all-files
```

## Debugging

### Local Development

```bash
# Run with debug logging
./autonomyd --config test/config/test.conf --log-level debug

# Run with specific debug flags
./autonomyd --debug --trace --config test/config/test.conf

# Attach debugger
dlv debug ./cmd/autonomyd
```

### Remote Debugging

```bash
# Build with debug symbols
go build -gcflags="all=-N -l" ./cmd/autonomyd

# Run with remote debugger
./autonomyd --debug-listen :2345
```

### Profiling

```bash
# CPU profiling
go test -cpuprofile cpu.prof ./...

# Memory profiling
go test -memprofile mem.prof ./...

# Analyze profiles
go tool pprof cpu.prof
go tool pprof mem.prof
```

## Documentation

### Generating Documentation

```bash
# Generate API documentation
go doc -all ./pkg/api

# Generate godoc
godoc -http=:6060

# Generate markdown docs
go run docs/generate.go
```

### Documentation Standards

- All public APIs must have godoc comments
- Examples should be provided for complex functions
- Update README.md for user-facing changes
- Update relevant docs/ files for technical changes

## Contributing

### Before Submitting

1. **Review Guidelines**: Check [CONTRIBUTING.md](../CONTRIBUTING.md)
2. **Run Tests**: Ensure all tests pass
3. **Code Quality**: Run linters and formatters
4. **Documentation**: Update relevant documentation
5. **Commit Message**: Use conventional commit format

### Commit Message Format

```
type(scope): description

[optional body]

[optional footer]
```

Types: `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `chore`

### Pull Request Process

1. Create feature branch from `main`
2. Make changes with clear commits
3. Add tests for new functionality
4. Update documentation
5. Ensure CI passes
6. Request review from maintainers

## CI/CD

### GitHub Actions

The project uses GitHub Actions for automated testing and building:

- **Tests**: Run on every push and PR
- **Builds**: Create artifacts for multiple platforms
- **Quality**: Run linters and security scans
- **Deployment**: Automated releases on tags

### Local CI

```bash
# Run full CI pipeline locally
make ci

# Run specific CI steps
make test
make build
make quality
```

## Performance

### Benchmarks

```bash
# Run benchmarks
go test -bench=. ./...

# Run specific benchmark
go test -bench=BenchmarkDecisionEngine ./pkg/decision/

# Run with memory profiling
go test -bench=. -benchmem ./...
```

### Performance Monitoring

```bash
# Monitor CPU usage
go tool pprof -seconds 30 http://localhost:6060/debug/pprof/profile

# Monitor memory usage
go tool pprof http://localhost:6060/debug/pprof/heap
```

## Troubleshooting

### Common Issues

1. **Build failures**: Check Go version and dependencies
2. **Test failures**: Verify test environment setup
3. **Linter errors**: Run `go fmt` and fix formatting issues
4. **Import errors**: Run `go mod tidy` to clean dependencies

### Getting Help

- Check [Troubleshooting Guide](TROUBLESHOOTING.md)
- Review [Issues](https://github.com/your-org/autonomy/issues)
- Ask questions in [Discussions](https://github.com/your-org/autonomy/discussions)

## Next Steps

- Read [Architecture Guide](../ARCHITECTURE.md) for system design
- Review [API Reference](API_REFERENCE.md) for integration
- Check [Testing Guide](autonomy_TEST_PLAN.md) for test strategies
- Explore [Deployment Guide](DEPLOYMENT.md) for production setup
