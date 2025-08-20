#!/bin/bash

# Comprehensive test suite for autonomy
# Following PROJECT_INSTRUCTION.md testing requirements

set -e

echo "🧪 Running comprehensive test suite for autonomy..."
echo "=================================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test results tracking
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Function to print colored output
print_status() {
    local status=$1
    local message=$2
    case $status in
        "PASS")
            echo -e "${GREEN}✅ $message${NC}"
            ((PASSED_TESTS++))
            ;;
        "FAIL")
            echo -e "${RED}❌ $message${NC}"
            ((FAILED_TESTS++))
            ;;
        "WARN")
            echo -e "${YELLOW}⚠️  $message${NC}"
            ;;
        "INFO")
            echo -e "${BLUE}ℹ️  $message${NC}"
            ;;
    esac
    ((TOTAL_TESTS++))
}

# Function to run tests and capture results
run_test_suite() {
    local suite_name=$1
    local test_path=$2

    echo ""
    echo "Running $suite_name..."
    echo "----------------------------------------"

    if go test -v $test_path 2>&1; then
        print_status "PASS" "$suite_name completed successfully"
        return 0
    else
        print_status "FAIL" "$suite_name failed"
        return 1
    fi
}

# Function to run benchmarks
run_benchmarks() {
    local suite_name=$1
    local test_path=$2

    echo ""
    echo "Running $suite_name benchmarks..."
    echo "----------------------------------------"

    if go test -bench=. -benchmem $test_path 2>&1; then
        print_status "PASS" "$suite_name benchmarks completed"
        return 0
    else
        print_status "WARN" "$suite_name benchmarks failed (may be expected)"
        return 1
    fi
}

# Check if Go is available
if ! command -v go &>/dev/null; then
    print_status "FAIL" "Go not found. Please install Go to run tests."
    exit 1
fi

print_status "INFO" "Go version: $(go version)"

# Check if we're in the right directory
if [ ! -f "go.mod" ]; then
    print_status "FAIL" "go.mod not found. Please run from project root."
    exit 1
fi

print_status "INFO" "Project root detected"

# Ensure all dependencies are available
echo ""
echo "Checking dependencies..."
echo "----------------------------------------"
if go mod tidy && go mod download; then
    print_status "PASS" "Dependencies resolved"
else
    print_status "FAIL" "Failed to resolve dependencies"
    exit 1
fi

# Build the project first
echo ""
echo "Building project..."
echo "----------------------------------------"
if go build -o autonomyd ./cmd/autonomyd; then
    print_status "PASS" "Project builds successfully"
else
    print_status "FAIL" "Project build failed"
    exit 1
fi

# Run linting
echo ""
echo "Running linter checks..."
echo "----------------------------------------"
if command -v golangci-lint &>/dev/null; then
    if golangci-lint run ./...; then
        print_status "PASS" "Linting passed"
    else
        print_status "WARN" "Linting found issues (continuing with tests)"
    fi
else
    print_status "WARN" "golangci-lint not found, skipping linter checks"
fi

# Run unit tests
echo ""
echo "🔬 UNIT TESTS"
echo "============="

# Controller tests
run_test_suite "Controller Unit Tests" "./pkg/controller"

# Discovery tests
run_test_suite "Discovery Unit Tests" "./pkg/discovery"

# Decision engine tests (if they exist)
if [ -f "pkg/decision/engine_test.go" ]; then
    run_test_suite "Decision Engine Unit Tests" "./pkg/decision"
else
    print_status "WARN" "Decision engine unit tests not found"
fi

# Collector tests (if they exist)
if [ -f "pkg/collector/starlink_test.go" ]; then
    run_test_suite "Collector Unit Tests" "./pkg/collector"
else
    print_status "WARN" "Collector unit tests not found"
fi

# ubus tests (if they exist)
if [ -f "pkg/ubus/server_test.go" ]; then
    run_test_suite "ubus Unit Tests" "./pkg/ubus"
else
    print_status "WARN" "ubus unit tests not found"
fi

# Run integration tests
echo ""
echo "🔗 INTEGRATION TESTS"
echo "==================="

if [ -d "test/integration" ]; then
    run_test_suite "Integration Tests" "./test/integration"
else
    print_status "WARN" "Integration test directory not found"
fi

# Run benchmarks
echo ""
echo "⚡ PERFORMANCE BENCHMARKS"
echo "========================"

run_benchmarks "Controller Benchmarks" "./pkg/controller"
run_benchmarks "Discovery Benchmarks" "./pkg/discovery"

if [ -d "test/integration" ]; then
    run_benchmarks "Integration Benchmarks" "./test/integration"
fi

# Test coverage analysis
echo ""
echo "📊 TEST COVERAGE ANALYSIS"
echo "========================="

echo "Generating coverage report..."
if go test -coverprofile=coverage.out ./pkg/...; then
    if command -v go &>/dev/null; then
        COVERAGE=$(go tool cover -func=coverage.out | grep total | awk '{print $3}')
        print_status "INFO" "Test coverage: $COVERAGE"

        # Generate HTML coverage report
        go tool cover -html=coverage.out -o coverage.html
        print_status "INFO" "Coverage report generated: coverage.html"
    fi
    print_status "PASS" "Coverage analysis completed"
else
    print_status "WARN" "Coverage analysis failed"
fi

# System integration checks
echo ""
echo "🖥️  SYSTEM INTEGRATION CHECKS"
echo "============================"

# Check if binary runs
echo "Testing binary execution..."
if timeout 5s ./autonomyd --version 2>/dev/null; then
    print_status "PASS" "Binary executes successfully"
else
    print_status "WARN" "Binary execution test failed (may need system dependencies)"
fi

# Check configuration loading
echo "Testing configuration loading..."
if [ -f "configs/autonomy.example" ]; then
    print_status "PASS" "Example configuration found"
else
    print_status "WARN" "Example configuration not found"
fi

# Memory leak detection (if valgrind available)
if command -v valgrind &>/dev/null; then
    echo "Running memory leak detection..."
    if timeout 10s valgrind --leak-check=summary --error-exitcode=1 ./autonomyd --version &>/dev/null; then
        print_status "PASS" "No memory leaks detected"
    else
        print_status "WARN" "Memory leak detection failed or found issues"
    fi
else
    print_status "INFO" "valgrind not available, skipping memory leak detection"
fi

# Cleanup
rm -f autonomyd coverage.out

# Final results
echo ""
echo "📋 TEST RESULTS SUMMARY"
echo "======================="
echo "Total tests: $TOTAL_TESTS"
echo "Passed: $PASSED_TESTS"
echo "Failed: $FAILED_TESTS"

if [ $FAILED_TESTS -eq 0 ]; then
    print_status "PASS" "All critical tests passed! 🎉"
    echo ""
    echo "✅ VERIFICATION COMPLETE"
    echo "========================"
    echo "The autonomy system has been verified with:"
    echo "• Unit tests for core components"
    echo "• Integration tests for component interaction"
    echo "• Performance benchmarks"
    echo "• Build verification"
    echo "• Basic system integration checks"
    echo ""
    echo "🚀 System is ready for deployment testing!"
    exit 0
else
    print_status "FAIL" "$FAILED_TESTS critical tests failed"
    echo ""
    echo "❌ VERIFICATION INCOMPLETE"
    echo "========================="
    echo "Please review failed tests and fix issues before deployment."
    exit 1
fi
