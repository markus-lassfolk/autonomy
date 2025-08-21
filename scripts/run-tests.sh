#!/bin/bash
set -e

# Test runner script for Autonomy Project
# This script runs all tests and generates reports

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration
TEST_DIR="$PROJECT_ROOT/test"
REPORTS_DIR="$PROJECT_ROOT/test-reports"
COVERAGE_DIR="$REPORTS_DIR/coverage"
BENCHMARK_DIR="$REPORTS_DIR/benchmarks"

# Test configuration
TEST_TIMEOUT="10m"
COVERAGE_THRESHOLD=70
PARALLEL_TESTS=4

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_test() {
    echo -e "${CYAN}[TEST]${NC} $1"
}

# Check if Go is installed
check_go() {
    if ! command -v go &> /dev/null; then
        log_error "Go is not installed or not in PATH"
        exit 1
    fi
    
    GO_VERSION=$(go version | awk '{print $3}')
    log_info "Using Go version: $GO_VERSION"
}

# Create test directories
create_test_dirs() {
    log_info "Creating test directories..."
    mkdir -p "$REPORTS_DIR"
    mkdir -p "$COVERAGE_DIR"
    mkdir -p "$BENCHMARK_DIR"
}

# Run unit tests
run_unit_tests() {
    log_test "Running unit tests..."
    
    cd "$PROJECT_ROOT"
    
    # Run tests with coverage
    go test -v -timeout "$TEST_TIMEOUT" -coverprofile="$COVERAGE_DIR/unit.out" -covermode=atomic ./...
    
    if [ $? -eq 0 ]; then
        log_success "Unit tests completed successfully"
    else
        log_error "Unit tests failed"
        return 1
    fi
}

# Run integration tests
run_integration_tests() {
    log_test "Running integration tests..."
    
    cd "$PROJECT_ROOT"
    
    if [ -d "test/integration" ]; then
        go test -v -timeout "$TEST_TIMEOUT" -coverprofile="$COVERAGE_DIR/integration.out" -covermode=atomic ./test/integration/...
        
        if [ $? -eq 0 ]; then
            log_success "Integration tests completed successfully"
        else
            log_error "Integration tests failed"
            return 1
        fi
    else
        log_warning "Integration test directory not found"
    fi
}

# Run benchmarks
run_benchmarks() {
    log_test "Running benchmarks..."
    
    cd "$PROJECT_ROOT"
    
    # Find all benchmark tests
    BENCHMARK_PACKAGES=$(go list ./... | grep -E "(pkg|cmd)" | head -10)
    
    for pkg in $BENCHMARK_PACKAGES; do
        log_info "Benchmarking package: $pkg"
        
        # Run benchmarks and save results
        go test -bench=. -benchmem -run=^$ "$pkg" > "$BENCHMARK_DIR/$(echo $pkg | tr '/' '_').bench" 2>&1 || true
    done
    
    log_success "Benchmarks completed"
}

# Run race detection tests
run_race_tests() {
    log_test "Running race detection tests..."
    
    cd "$PROJECT_ROOT"
    
    # Run tests with race detection
    go test -race -timeout "$TEST_TIMEOUT" ./...
    
    if [ $? -eq 0 ]; then
        log_success "Race detection tests completed successfully"
    else
        log_error "Race detection tests failed"
        return 1
    fi
}

# Run vet checks
run_vet() {
    log_test "Running go vet..."
    
    cd "$PROJECT_ROOT"
    
    go vet ./...
    
    if [ $? -eq 0 ]; then
        log_success "Go vet completed successfully"
    else
        log_error "Go vet found issues"
        return 1
    fi
}

# Generate coverage report
generate_coverage_report() {
    log_test "Generating coverage report..."
    
    cd "$PROJECT_ROOT"
    
    # Merge coverage files
    if [ -f "$COVERAGE_DIR/unit.out" ] && [ -f "$COVERAGE_DIR/integration.out" ]; then
        go tool cover -func="$COVERAGE_DIR/unit.out" > "$COVERAGE_DIR/coverage.txt"
        echo "" >> "$COVERAGE_DIR/coverage.txt"
        echo "Integration Tests:" >> "$COVERAGE_DIR/coverage.txt"
        go tool cover -func="$COVERAGE_DIR/integration.out" >> "$COVERAGE_DIR/coverage.txt"
    elif [ -f "$COVERAGE_DIR/unit.out" ]; then
        go tool cover -func="$COVERAGE_DIR/unit.out" > "$COVERAGE_DIR/coverage.txt"
    elif [ -f "$COVERAGE_DIR/integration.out" ]; then
        go tool cover -func="$COVERAGE_DIR/integration.out" > "$COVERAGE_DIR/coverage.txt"
    fi
    
    # Generate HTML coverage report
    if [ -f "$COVERAGE_DIR/unit.out" ]; then
        go tool cover -html="$COVERAGE_DIR/unit.out" -o "$COVERAGE_DIR/coverage.html"
    fi
    
    log_success "Coverage report generated"
}

# Check coverage threshold
check_coverage() {
    log_test "Checking coverage threshold..."
    
    if [ -f "$COVERAGE_DIR/coverage.txt" ]; then
        COVERAGE_PERCENT=$(grep "total:" "$COVERAGE_DIR/coverage.txt" | awk '{print $3}' | sed 's/%//')
        
        if [ -n "$COVERAGE_PERCENT" ] && [ "$COVERAGE_PERCENT" -lt "$COVERAGE_THRESHOLD" ]; then
            log_warning "Coverage is below threshold: ${COVERAGE_PERCENT}% < ${COVERAGE_THRESHOLD}%"
            return 1
        else
            log_success "Coverage threshold met: ${COVERAGE_PERCENT}% >= ${COVERAGE_THRESHOLD}%"
        fi
    else
        log_warning "No coverage report found"
    fi
}

# Run security tests
run_security_tests() {
    log_test "Running security tests..."
    
    cd "$PROJECT_ROOT"
    
    # Check for common security issues
    if command -v gosec &> /dev/null; then
        gosec ./... > "$REPORTS_DIR/security-report.txt" 2>&1 || true
        log_success "Security scan completed"
    else
        log_warning "gosec not found, skipping security tests"
    fi
}

# Run performance tests
run_performance_tests() {
    log_test "Running performance tests..."
    
    cd "$PROJECT_ROOT"
    
    # Run performance tests if they exist
    if [ -d "test/performance" ]; then
        go test -v -timeout "$TEST_TIMEOUT" ./test/performance/... > "$REPORTS_DIR/performance-report.txt" 2>&1 || true
        log_success "Performance tests completed"
    else
        log_warning "Performance test directory not found"
    fi
}

# Generate test summary
generate_test_summary() {
    log_test "Generating test summary..."
    
    cd "$PROJECT_ROOT"
    
    cat > "$REPORTS_DIR/test-summary.md" << EOF
# Test Summary Report

Generated: $(date)

## Test Results

### Unit Tests
- Status: $(if [ -f "$COVERAGE_DIR/unit.out" ]; then echo "✅ PASSED"; else echo "❌ FAILED"; fi)

### Integration Tests
- Status: $(if [ -d "test/integration" ] && [ -f "$COVERAGE_DIR/integration.out" ]; then echo "✅ PASSED"; else echo "⚠️  SKIPPED"; fi)

### Race Detection
- Status: $(if [ $? -eq 0 ]; then echo "✅ PASSED"; else echo "❌ FAILED"; fi)

### Go Vet
- Status: $(if [ $? -eq 0 ]; then echo "✅ PASSED"; else echo "❌ FAILED"; fi)

### Coverage
- Threshold: ${COVERAGE_THRESHOLD}%
- Current: $(if [ -f "$COVERAGE_DIR/coverage.txt" ]; then grep "total:" "$COVERAGE_DIR/coverage.txt" | awk '{print $3}'; else echo "N/A"; fi)

## Files Generated
- Coverage Report: $COVERAGE_DIR/coverage.html
- Coverage Summary: $COVERAGE_DIR/coverage.txt
- Benchmark Results: $BENCHMARK_DIR/
- Security Report: $REPORTS_DIR/security-report.txt
- Performance Report: $REPORTS_DIR/performance-report.txt

## Next Steps
1. Review coverage report
2. Address any failing tests
3. Improve test coverage if below threshold
4. Review security and performance reports
EOF

    log_success "Test summary generated: $REPORTS_DIR/test-summary.md"
}

# Show test results
show_test_results() {
    log_info "Test Results Summary:"
    echo ""
    
    if [ -f "$COVERAGE_DIR/coverage.txt" ]; then
        echo "📊 Coverage Summary:"
        cat "$COVERAGE_DIR/coverage.txt"
        echo ""
    fi
    
    if [ -d "$BENCHMARK_DIR" ] && [ "$(ls -A $BENCHMARK_DIR)" ]; then
        echo "🏃 Benchmark Results:"
        ls -la "$BENCHMARK_DIR"/*.bench 2>/dev/null || echo "  No benchmark results"
        echo ""
    fi
    
    echo "📁 Reports generated in: $REPORTS_DIR"
}

# Clean test artifacts
clean() {
    log_info "Cleaning test artifacts..."
    rm -rf "$REPORTS_DIR"
    log_success "Test artifacts cleaned"
}

# Show usage
usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -h, --help          Show this help message"
    echo "  -c, --clean         Clean test artifacts"
    echo "  -u, --unit          Run unit tests only"
    echo "  -i, --integration   Run integration tests only"
    echo "  -b, --benchmark     Run benchmarks only"
    echo "  -r, --race          Run race detection tests only"
    echo "  -v, --vet           Run go vet only"
    echo "  -s, --security      Run security tests only"
    echo "  -p, --performance   Run performance tests only"
    echo "  -a, --all           Run all tests (default)"
    echo ""
    echo "Examples:"
    echo "  $0                  Run all tests"
    echo "  $0 -u               Run unit tests only"
    echo "  $0 -i               Run integration tests only"
    echo "  $0 -c               Clean test artifacts"
}

# Main function
main() {
    local clean_only=false
    local unit_only=false
    local integration_only=false
    local benchmark_only=false
    local race_only=false
    local vet_only=false
    local security_only=false
    local performance_only=false
    local run_all=true
    
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                usage
                exit 0
                ;;
            -c|--clean)
                clean_only=true
                run_all=false
                shift
                ;;
            -u|--unit)
                unit_only=true
                run_all=false
                shift
                ;;
            -i|--integration)
                integration_only=true
                run_all=false
                shift
                ;;
            -b|--benchmark)
                benchmark_only=true
                run_all=false
                shift
                ;;
            -r|--race)
                race_only=true
                run_all=false
                shift
                ;;
            -v|--vet)
                vet_only=true
                run_all=false
                shift
                ;;
            -s|--security)
                security_only=true
                run_all=false
                shift
                ;;
            -p|--performance)
                performance_only=true
                run_all=false
                shift
                ;;
            -a|--all)
                run_all=true
                shift
                ;;
            *)
                log_error "Unknown option: $1"
                usage
                exit 1
                ;;
        esac
    done
    
    log_info "Starting Autonomy test suite..."
    
    # Check prerequisites
    check_go
    
    # Execute requested actions
    if [ "$clean_only" = true ]; then
        clean
        exit 0
    fi
    
    # Create test directories
    create_test_dirs
    
    # Execute specific test types
    if [ "$unit_only" = true ]; then
        run_unit_tests
        generate_coverage_report
        show_test_results
        exit 0
    fi
    
    if [ "$integration_only" = true ]; then
        run_integration_tests
        generate_coverage_report
        show_test_results
        exit 0
    fi
    
    if [ "$benchmark_only" = true ]; then
        run_benchmarks
        show_test_results
        exit 0
    fi
    
    if [ "$race_only" = true ]; then
        run_race_tests
        exit 0
    fi
    
    if [ "$vet_only" = true ]; then
        run_vet
        exit 0
    fi
    
    if [ "$security_only" = true ]; then
        run_security_tests
        exit 0
    fi
    
    if [ "$performance_only" = true ]; then
        run_performance_tests
        exit 0
    fi
    
    # Default: run all tests
    if [ "$run_all" = true ]; then
        run_vet
        run_unit_tests
        run_integration_tests
        run_race_tests
        run_benchmarks
        run_security_tests
        run_performance_tests
        generate_coverage_report
        check_coverage
        generate_test_summary
        show_test_results
    fi
    
    log_success "Test suite completed successfully!"
}

# Run main function with all arguments
main "$@"
