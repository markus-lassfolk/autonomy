# Contributing to Autonomy

Thank you for your interest in contributing to the Autonomy project! This document provides guidelines for contributing to the project.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Coding Standards](#coding-standards)
- [Testing](#testing)
- [Pull Request Process](#pull-request-process)
- [Issue Reporting](#issue-reporting)
- [Documentation](#documentation)

## Code of Conduct

This project and its participants are governed by our Code of Conduct. By participating, you are expected to uphold this code.

## Getting Started

### Prerequisites

- Go 1.23 or later
- Git
- Make (optional, for build automation)
- Docker (for testing)

### Development Setup

1. **Fork the repository**
   ```bash
   git clone https://github.com/your-username/autonomy.git
   cd autonomy
   ```

2. **Set up the development environment**
   ```bash
   # Install dependencies
   go mod download
   
   # Run tests
   go test ./...
   ```

3. **Create a feature branch**
   ```bash
   git checkout -b feature/your-feature-name
   ```

## Coding Standards

### Go Code

- Follow [Effective Go](https://golang.org/doc/effective_go.html)
- Use `gofmt` for code formatting
- Run `golint` and `go vet` before committing
- Write meaningful commit messages

### File Organization

- Use snake_case for file names
- Group related functionality in packages
- Follow the existing directory structure

### Error Handling

- Always check and handle errors explicitly
- Use wrapped errors with context: `fmt.Errorf("failed to load config: %w", err)`
- Log errors with appropriate log levels

### Logging

- Use structured JSON logging with `pkg/logx/Logger`
- Include relevant context in log messages
- Use appropriate log levels (debug, info, warn, error)

## Testing

### Unit Tests

- Write tests for all new functionality
- Use table-driven tests where appropriate
- Aim for high test coverage
- Mock external dependencies

### Integration Tests

- Test integration with UCI, ubus, and mwan3
- Test RUTOS/OpenWrt compatibility
- Test failover scenarios

### Performance Tests

- Benchmark critical code paths
- Test memory usage and CPU efficiency
- Ensure performance targets are met

## Pull Request Process

1. **Create a feature branch** from `main-dev`
2. **Make your changes** following coding standards
3. **Write tests** for new functionality
4. **Update documentation** as needed
5. **Run the test suite** locally
6. **Submit a pull request** to `main-dev`

### Pull Request Guidelines

- Provide a clear description of changes
- Reference related issues
- Include test results
- Update relevant documentation
- Ensure all CI checks pass

### Review Process

- All PRs require at least one review
- Address review comments promptly
- Maintainers may request changes
- PRs are merged after approval

## Issue Reporting

### Bug Reports

When reporting bugs, please include:

- Clear description of the issue
- Steps to reproduce
- Expected vs actual behavior
- System information (OS, version, etc.)
- Relevant logs

### Feature Requests

For feature requests, include:

- Clear description of the feature
- Use case and motivation
- Proposed implementation approach
- Impact on existing functionality

## Documentation

### Code Documentation

- Document exported functions and types
- Include usage examples
- Keep comments up to date
- Use godoc format

### User Documentation

- Update README files for user-facing changes
- Add configuration examples
- Document new features
- Update troubleshooting guides

## Development Workflow

### Branch Strategy

- `main`: Production-ready code
- `main-dev`: Development branch
- Feature branches: `feature/description`
- Bug fix branches: `fix/description`

### Commit Messages

Use conventional commit format:

```
type(scope): description

[optional body]

[optional footer]
```

Types:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes
- `refactor`: Code refactoring
- `test`: Test changes
- `chore`: Maintenance tasks

### Release Process

1. Create release branch from `main-dev`
2. Update version numbers
3. Update changelog
4. Run full test suite
5. Create pull request to `main`
6. Tag release after merge

## Getting Help

- Check existing documentation
- Search existing issues
- Join discussions in GitHub
- Contact maintainers for urgent issues

## License

By contributing to this project, you agree that your contributions will be licensed under the same license as the project.

## Thank You

Thank you for contributing to Autonomy! Your contributions help make the project better for everyone.
