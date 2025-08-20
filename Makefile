# Autonomy Daemon Makefile
# Comprehensive build, test, and deployment automation

.PHONY: all build test test-unit test-integration test-benchmarks clean lint fmt vet coverage help install-deps cross-compile

# Variables
PROJECT_NAME := autonomyd
MAIN_PACKAGE := ./cmd/autonomyd
BUILD_DIR := build
COVERAGE_DIR := coverage
TEST_TIMEOUT := 60s
INTEGRATION_TIMEOUT := 120s

# Build flags
LDFLAGS := -s -w
BUILD_FLAGS := -ldflags="$(LDFLAGS)"

# Go commands
GO := go
GOFMT := gofmt
GOVET := $(GO) vet
GOLINT := golint
STATICCHECK := staticcheck

# Colors for output
RED := \033[0;31m
GREEN := \033[0;32m
YELLOW := \033[1;33m
BLUE := \033[0;34m
NC := \033[0m # No Color

# Default target
all: clean lint test build

# Help target
help:
	@echo "$(BLUE)Autonomy Daemon Build System$(NC)"
	@echo ""
	@echo "$(YELLOW)Available targets:$(NC)"
	@echo "  $(GREEN)build$(NC)             - Build the daemon binary"
	@echo "  $(GREEN)test$(NC)              - Run all tests (unit + integration)"
	@echo "  $(GREEN)test-unit$(NC)         - Run unit tests only"
	@echo "  $(GREEN)test-integration$(NC)  - Run integration tests only"
	@echo "  $(GREEN)test-benchmarks$(NC)   - Run benchmark tests"
	@echo "  $(GREEN)coverage$(NC)          - Generate test coverage report"
	@echo "  $(GREEN)lint$(NC)              - Run all linters"
	@echo "  $(GREEN)fmt$(NC)               - Format Go code"
	@echo "  $(GREEN)vet$(NC)               - Run go vet"
	@echo "  $(GREEN)clean$(NC)             - Clean build artifacts"
	@echo "  $(GREEN)install-deps$(NC)      - Install development dependencies"
	@echo "  $(GREEN)cross-compile$(NC)     - Build for multiple architectures"
	@echo "  $(GREEN)help$(NC)              - Show this help message"

# Install development dependencies
install-deps:
	@echo "$(YELLOW)Installing development dependencies...$(NC)"
	@$(GO) install honnef.co/go/tools/cmd/staticcheck@latest
	@$(GO) install golang.org/x/tools/cmd/goimports@latest
	@$(GO) mod download
	@$(GO) mod tidy
	@echo "$(GREEN)✓ Dependencies installed$(NC)"

# Build the main binary
build:
	@echo "$(YELLOW)Building $(PROJECT_NAME)...$(NC)"
	@mkdir -p $(BUILD_DIR)
	@$(GO) build $(BUILD_FLAGS) -o $(BUILD_DIR)/$(PROJECT_NAME) $(MAIN_PACKAGE)
	@echo "$(GREEN)✓ Build complete: $(BUILD_DIR)/$(PROJECT_NAME)$(NC)"

# Format Go code
fmt:
	@echo "$(YELLOW)Formatting Go code...$(NC)"
	@$(GOFMT) -s -w .
	@goimports -w .
	@echo "$(GREEN)✓ Code formatted$(NC)"

# Run go vet
vet:
	@echo "$(YELLOW)Running go vet...$(NC)"
	@$(GOVET) ./...
	@echo "$(GREEN)✓ go vet passed$(NC)"

# Run staticcheck
staticcheck:
	@echo "$(YELLOW)Running staticcheck...$(NC)"
	@if command -v $(STATICCHECK) >/dev/null 2>&1; then \
		$(STATICCHECK) ./...; \
		echo "$(GREEN)✓ staticcheck passed$(NC)"; \
	else \
		echo "$(YELLOW)⚠ staticcheck not installed (run 'make install-deps')$(NC)"; \
	fi

# Run all linters
lint: fmt vet staticcheck
	@echo "$(GREEN)✓ All linters passed$(NC)"

# Run unit tests
test-unit:
	@echo "$(YELLOW)Running unit tests...$(NC)"
	@$(GO) test -race -timeout $(TEST_TIMEOUT) -v ./pkg/...
	@echo "$(GREEN)✓ Unit tests passed$(NC)"

# Run integration tests
test-integration:
	@echo "$(YELLOW)Running integration tests...$(NC)"
	@$(GO) test -race -timeout $(INTEGRATION_TIMEOUT) -v ./test/integration/...
	@echo "$(GREEN)✓ Integration tests passed$(NC)"

# Run benchmark tests
test-benchmarks:
	@echo "$(YELLOW)Running benchmark tests...$(NC)"
	@mkdir -p $(COVERAGE_DIR)
	@$(GO) test -bench=. -benchmem -timeout $(INTEGRATION_TIMEOUT) ./... > $(COVERAGE_DIR)/benchmarks.txt
	@echo "$(GREEN)✓ Benchmarks complete (results in $(COVERAGE_DIR)/benchmarks.txt)$(NC)"

# Run all tests
test: test-unit test-integration
	@echo "$(GREEN)✓ All tests passed$(NC)"

# Generate test coverage
coverage:
	@echo "$(YELLOW)Generating test coverage...$(NC)"
	@mkdir -p $(COVERAGE_DIR)
	@$(GO) test -race -coverprofile=$(COVERAGE_DIR)/coverage.out -timeout $(TEST_TIMEOUT) ./pkg/...
	@$(GO) tool cover -html=$(COVERAGE_DIR)/coverage.out -o $(COVERAGE_DIR)/coverage.html
	@$(GO) tool cover -func=$(COVERAGE_DIR)/coverage.out | grep total: | awk '{print "$(GREEN)✓ Test coverage: " $$3 "$(NC)"}'
	@echo "$(BLUE)Coverage report: $(COVERAGE_DIR)/coverage.html$(NC)"

# Cross-compile for multiple architectures
cross-compile:
	@echo "$(YELLOW)Cross-compiling for multiple architectures...$(NC)"
	@mkdir -p $(BUILD_DIR)
	
	@echo "$(BLUE)Building for linux/amd64...$(NC)"
	@CGO_ENABLED=0 GOOS=linux GOARCH=amd64 $(GO) build $(BUILD_FLAGS) -o $(BUILD_DIR)/$(PROJECT_NAME)-linux-amd64 $(MAIN_PACKAGE)
	
	@echo "$(BLUE)Building for linux/arm...$(NC)"
	@CGO_ENABLED=0 GOOS=linux GOARCH=arm $(GO) build $(BUILD_FLAGS) -o $(BUILD_DIR)/$(PROJECT_NAME)-linux-arm $(MAIN_PACKAGE)
	
	@echo "$(BLUE)Building for linux/arm64...$(NC)"
	@CGO_ENABLED=0 GOOS=linux GOARCH=arm64 $(GO) build $(BUILD_FLAGS) -o $(BUILD_DIR)/$(PROJECT_NAME)-linux-arm64 $(MAIN_PACKAGE)
	
	@echo "$(BLUE)Building for linux/mips...$(NC)"
	@CGO_ENABLED=0 GOOS=linux GOARCH=mips $(GO) build $(BUILD_FLAGS) -o $(BUILD_DIR)/$(PROJECT_NAME)-linux-mips $(MAIN_PACKAGE)
	
	@echo "$(BLUE)Building for linux/mipsle...$(NC)"
	@CGO_ENABLED=0 GOOS=linux GOARCH=mipsle $(GO) build $(BUILD_FLAGS) -o $(BUILD_DIR)/$(PROJECT_NAME)-linux-mipsle $(MAIN_PACKAGE)
	
	@echo "$(GREEN)✓ Cross-compilation complete$(NC)"
	@ls -la $(BUILD_DIR)/$(PROJECT_NAME)-*

# Clean build artifacts
clean:
	@echo "$(YELLOW)Cleaning build artifacts...$(NC)"
	@rm -rf $(BUILD_DIR)
	@rm -rf $(COVERAGE_DIR)
	@$(GO) clean -testcache
	@$(GO) clean -modcache || true
	@echo "$(GREEN)✓ Clean complete$(NC)"

# Development workflow targets
dev: lint test build
	@echo "$(GREEN)✓ Development build complete$(NC)"

# CI/CD targets
ci: install-deps lint test coverage cross-compile
	@echo "$(GREEN)✓ CI build complete$(NC)"

# Quick test for development
quick-test:
	@echo "$(YELLOW)Running quick tests...$(NC)"
	@$(GO) test -short -timeout 30s ./pkg/...
	@echo "$(GREEN)✓ Quick tests passed$(NC)"

# Test with race detection disabled (for resource-constrained environments)
test-no-race:
	@echo "$(YELLOW)Running tests without race detection...$(NC)"
	@$(GO) test -timeout $(TEST_TIMEOUT) -v ./pkg/...
	@$(GO) test -timeout $(INTEGRATION_TIMEOUT) -v ./test/integration/...
	@echo "$(GREEN)✓ Tests passed$(NC)"

# Performance profiling
profile:
	@echo "$(YELLOW)Running performance profiling...$(NC)"
	@mkdir -p $(COVERAGE_DIR)
	@$(GO) test -cpuprofile=$(COVERAGE_DIR)/cpu.prof -memprofile=$(COVERAGE_DIR)/mem.prof -bench=. ./pkg/...
	@echo "$(GREEN)✓ Profiling complete$(NC)"
	@echo "$(BLUE)CPU profile: $(COVERAGE_DIR)/cpu.prof$(NC)"
	@echo "$(BLUE)Memory profile: $(COVERAGE_DIR)/mem.prof$(NC)"
	@echo "$(BLUE)View with: go tool pprof $(COVERAGE_DIR)/cpu.prof$(NC)"

# Install binary to system
install: build
	@echo "$(YELLOW)Installing $(PROJECT_NAME)...$(NC)"
	@sudo cp $(BUILD_DIR)/$(PROJECT_NAME) /usr/sbin/$(PROJECT_NAME)
	@sudo chmod +x /usr/sbin/$(PROJECT_NAME)
	@echo "$(GREEN)✓ $(PROJECT_NAME) installed to /usr/sbin/$(NC)"

# Create release package
package: cross-compile
	@echo "$(YELLOW)Creating release package...$(NC)"
	@mkdir -p $(BUILD_DIR)/package
	@cp -r $(BUILD_DIR)/$(PROJECT_NAME)-* $(BUILD_DIR)/package/
	@cp -r scripts/ $(BUILD_DIR)/package/
	@cp -r configs/ $(BUILD_DIR)/package/
	@cp README.md $(BUILD_DIR)/package/ 2>/dev/null || true
	@tar -czf $(BUILD_DIR)/$(PROJECT_NAME)-release.tar.gz -C $(BUILD_DIR)/package .
	@echo "$(GREEN)✓ Release package: $(BUILD_DIR)/$(PROJECT_NAME)-release.tar.gz$(NC)"

# Docker targets (if needed in the future)
docker-build:
	@echo "$(YELLOW)Building Docker image...$(NC)"
	@docker build -t $(PROJECT_NAME):latest .
	@echo "$(GREEN)✓ Docker image built$(NC)"

# Show project statistics
stats:
	@echo "$(BLUE)Project Statistics:$(NC)"
	@echo "Lines of Go code: $$(find . -name '*.go' -not -path './vendor/*' | xargs wc -l | tail -1 | awk '{print $$1}')"
	@echo "Number of packages: $$(find ./pkg -name '*.go' -not -name '*_test.go' | wc -l)"
	@echo "Number of test files: $$(find . -name '*_test.go' | wc -l)"
	@echo "Number of functions: $$(grep -r '^func ' --include='*.go' ./pkg | wc -l)"
	@echo "Binary size (if built): $$(if [ -f $(BUILD_DIR)/$(PROJECT_NAME) ]; then du -h $(BUILD_DIR)/$(PROJECT_NAME) | cut -f1; else echo 'Not built'; fi)"
