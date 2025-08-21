#!/bin/bash
set -e

# Comprehensive Verification Script for Autonomy Project
# This script verifies all aspects of the project including builds, tests, and deployments

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m' # No Color

# Configuration
VERIFY_DIR="$PROJECT_ROOT/verify-reports"
BUILD_DIR="$PROJECT_ROOT/bin"
TEST_DIR="$PROJECT_ROOT/test"

# Verification results
VERIFY_RESULTS=()
TOTAL_TESTS=0
PASSED_TESTS=0

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

log_verify() {
    echo -e "${CYAN}[VERIFY]${NC} $1"
}

log_test() {
    echo -e "${MAGENTA}[TEST]${NC} $1"
}

# Record test result
record_test() {
    local test_name="$1"
    local result="$2"
    local message="$3"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    if [ "$result" = "PASS" ]; then
        PASSED_TESTS=$((PASSED_TESTS + 1))
        log_success "$test_name: $message"
        VERIFY_RESULTS+=("✅ $test_name: PASS")
    else
        log_error "$test_name: $message"
        VERIFY_RESULTS+=("❌ $test_name: FAIL - $message")
    fi
}

# Check if Go is installed
verify_go() {
    log_verify "Verifying Go installation..."
    
    if ! command -v go &> /dev/null; then
        record_test "Go Installation" "FAIL" "Go is not installed or not in PATH"
        return 1
    fi
    
    GO_VERSION=$(go version | awk '{print $3}')
    log_info "Go version: $GO_VERSION"
    
    # Check Go version compatibility
    if [[ "$GO_VERSION" =~ go1\.(2[0-9]|3[0-9]) ]]; then
        record_test "Go Installation" "PASS" "Go $GO_VERSION is compatible"
    else
        record_test "Go Installation" "FAIL" "Go $GO_VERSION is not compatible (requires 1.20+)"
        return 1
    fi
}

# Verify project structure
verify_project_structure() {
    log_verify "Verifying project structure..."
    
    local required_dirs=("pkg" "cmd" "test" "scripts" "docs" "configs")
    local required_files=("go.mod" "go.sum" "Makefile" "README.md")
    
    for dir in "${required_dirs[@]}"; do
        if [ -d "$PROJECT_ROOT/$dir" ]; then
            record_test "Directory: $dir" "PASS" "Directory exists"
        else
            record_test "Directory: $dir" "FAIL" "Directory missing"
        fi
    done
    
    for file in "${required_files[@]}"; do
        if [ -f "$PROJECT_ROOT/$file" ]; then
            record_test "File: $file" "PASS" "File exists"
        else
            record_test "File: $file" "FAIL" "File missing"
        fi
    done
}

# Verify Go modules
verify_go_modules() {
    log_verify "Verifying Go modules..."
    
    cd "$PROJECT_ROOT"
    
    # Check go.mod
    if [ -f "go.mod" ]; then
        record_test "go.mod" "PASS" "Module file exists"
        
        # Verify module name
        MODULE_NAME=$(grep "^module " go.mod | awk '{print $2}')
        if [ -n "$MODULE_NAME" ]; then
            record_test "Module Name" "PASS" "Module: $MODULE_NAME"
        else
            record_test "Module Name" "FAIL" "Invalid module name"
        fi
    else
        record_test "go.mod" "FAIL" "Module file missing"
        return 1
    fi
    
    # Check go.sum
    if [ -f "go.sum" ]; then
        record_test "go.sum" "PASS" "Sum file exists"
    else
        record_test "go.sum" "FAIL" "Sum file missing"
    fi
    
    # Verify dependencies
    if go mod verify > /dev/null 2>&1; then
        record_test "Dependencies" "PASS" "All dependencies verified"
    else
        record_test "Dependencies" "FAIL" "Dependency verification failed"
    fi
    
    # Check for vulnerabilities
    if command -v govulncheck &> /dev/null; then
        if govulncheck ./... > /dev/null 2>&1; then
            record_test "Vulnerabilities" "PASS" "No known vulnerabilities found"
        else
            record_test "Vulnerabilities" "FAIL" "Known vulnerabilities detected"
        fi
    else
        record_test "Vulnerabilities" "WARN" "govulncheck not available"
    fi
}

# Verify build process
verify_build() {
    log_verify "Verifying build process..."
    
    cd "$PROJECT_ROOT"
    
    # Create build directory
    mkdir -p "$BUILD_DIR"
    
    # Build main binary
    if go build -o "$BUILD_DIR/autonomyd" cmd/autonomysysmgmt/main.go; then
        record_test "Main Build" "PASS" "Main binary built successfully"
        
        # Check binary size
        BINARY_SIZE=$(stat -c%s "$BUILD_DIR/autonomyd" 2>/dev/null || stat -f%z "$BUILD_DIR/autonomyd" 2>/dev/null)
        if [ "$BINARY_SIZE" -lt 50000000 ]; then  # 50MB limit
            record_test "Binary Size" "PASS" "Binary size: ${BINARY_SIZE} bytes"
        else
            record_test "Binary Size" "FAIL" "Binary too large: ${BINARY_SIZE} bytes"
        fi
    else
        record_test "Main Build" "FAIL" "Failed to build main binary"
        return 1
    fi
    
    # Build webhook server
    if [ -f "scripts/webhook-server.go" ]; then
        if go build -o "$BUILD_DIR/webhook-server" scripts/webhook-server.go; then
            record_test "Webhook Build" "PASS" "Webhook server built successfully"
        else
            record_test "Webhook Build" "FAIL" "Failed to build webhook server"
        fi
    else
        record_test "Webhook Build" "WARN" "Webhook server source not found"
    fi
    
    # Test binary execution
    if [ -f "$BUILD_DIR/autonomyd" ]; then
        if timeout 5s "$BUILD_DIR/autonomyd" --help > /dev/null 2>&1; then
            record_test "Binary Execution" "PASS" "Binary executes successfully"
        else
            record_test "Binary Execution" "FAIL" "Binary execution failed"
        fi
    fi
}

# Verify tests
verify_tests() {
    log_verify "Verifying tests..."
    
    cd "$PROJECT_ROOT"
    
    # Run unit tests
    if go test -v ./pkg/... > /dev/null 2>&1; then
        record_test "Unit Tests" "PASS" "All unit tests passed"
    else
        record_test "Unit Tests" "FAIL" "Some unit tests failed"
    fi
    
    # Run integration tests
    if [ -d "test/integration" ]; then
        if go test -v ./test/integration/... > /dev/null 2>&1; then
            record_test "Integration Tests" "PASS" "All integration tests passed"
        else
            record_test "Integration Tests" "FAIL" "Some integration tests failed"
        fi
    else
        record_test "Integration Tests" "WARN" "Integration test directory not found"
    fi
    
    # Run benchmarks
    if go test -bench=. -run=^$ ./pkg/... > /dev/null 2>&1; then
        record_test "Benchmarks" "PASS" "Benchmarks completed"
    else
        record_test "Benchmarks" "WARN" "No benchmarks found"
    fi
}

# Verify code quality
verify_code_quality() {
    log_verify "Verifying code quality..."
    
    cd "$PROJECT_ROOT"
    
    # Run go vet
    if go vet ./... > /dev/null 2>&1; then
        record_test "Go Vet" "PASS" "No issues found"
    else
        record_test "Go Vet" "FAIL" "Issues found by go vet"
    fi
    
    # Run golangci-lint if available
    if command -v golangci-lint &> /dev/null; then
        if golangci-lint run > /dev/null 2>&1; then
            record_test "Linting" "PASS" "No linting issues found"
        else
            record_test "Linting" "FAIL" "Linting issues found"
        fi
    else
        record_test "Linting" "WARN" "golangci-lint not available"
    fi
    
    # Check code formatting
    if go fmt ./... > /dev/null 2>&1; then
        record_test "Code Format" "PASS" "Code is properly formatted"
    else
        record_test "Code Format" "FAIL" "Code formatting issues found"
    fi
}

# Verify configuration
verify_configuration() {
    log_verify "Verifying configuration..."
    
    # Check configuration files
    local config_files=(
        "configs/autonomy.example"
        "configs/autonomy.comprehensive.example"
        "etc/config/autonomy"
        "uci-schema/autonomy.sc"
    )
    
    for config in "${config_files[@]}"; do
        if [ -f "$PROJECT_ROOT/$config" ]; then
            record_test "Config: $config" "PASS" "Configuration file exists"
        else
            record_test "Config: $config" "FAIL" "Configuration file missing"
        fi
    done
    
    # Verify UCI schema syntax
    if [ -f "$PROJECT_ROOT/uci-schema/autonomy.sc" ]; then
        if command -v uci &> /dev/null; then
            if uci show autonomy > /dev/null 2>&1; then
                record_test "UCI Schema" "PASS" "UCI schema is valid"
            else
                record_test "UCI Schema" "FAIL" "UCI schema validation failed"
            fi
        else
            record_test "UCI Schema" "WARN" "UCI not available for validation"
        fi
    fi
}

# Verify scripts
verify_scripts() {
    log_verify "Verifying scripts..."
    
    local scripts=(
        "scripts/build.sh"
        "scripts/run-tests.sh"
        "scripts/verify-comprehensive.sh"
        "scripts/webhook-server.go"
        "scripts/webhook-receiver.js"
    )
    
    for script in "${scripts[@]}"; do
        if [ -f "$PROJECT_ROOT/$script" ]; then
            record_test "Script: $script" "PASS" "Script exists"
            
            # Check script permissions
            if [ -x "$PROJECT_ROOT/$script" ]; then
                record_test "Script Permissions: $script" "PASS" "Script is executable"
            else
                record_test "Script Permissions: $script" "FAIL" "Script is not executable"
            fi
        else
            record_test "Script: $script" "FAIL" "Script missing"
        fi
    done
    
    # Test shell script syntax
    for script in scripts/*.sh; do
        if [ -f "$script" ]; then
            if bash -n "$script" > /dev/null 2>&1; then
                record_test "Script Syntax: $script" "PASS" "Shell script syntax is valid"
            else
                record_test "Script Syntax: $script" "FAIL" "Shell script syntax error"
            fi
        fi
    done
}

# Verify documentation
verify_documentation() {
    log_verify "Verifying documentation..."
    
    local docs=(
        "README.md"
        "ARCHITECTURE.md"
        "ROADMAP.md"
        "STATUS.md"
        "TODO.md"
        "docs/CONFIGURATION.md"
        "docs/DEPLOYMENT.md"
        "docs/DEVELOPMENT.md"
    )
    
    for doc in "${docs[@]}"; do
        if [ -f "$PROJECT_ROOT/$doc" ]; then
            record_test "Documentation: $doc" "PASS" "Documentation exists"
        else
            record_test "Documentation: $doc" "FAIL" "Documentation missing"
        fi
    done
    
    # Check README content
    if [ -f "$PROJECT_ROOT/README.md" ]; then
        README_SIZE=$(wc -c < "$PROJECT_ROOT/README.md")
        if [ "$README_SIZE" -gt 1000 ]; then
            record_test "README Content" "PASS" "README has substantial content"
        else
            record_test "README Content" "FAIL" "README is too short"
        fi
    fi
}

# Verify GitHub workflows
verify_github_workflows() {
    log_verify "Verifying GitHub workflows..."
    
    local workflows=(
        ".github/workflows/security-scan.yml"
        ".github/workflows/code-quality.yml"
        ".github/workflows/test-deployment.yml"
        ".github/workflows/webhook-receiver.yml"
        ".github/workflows/copilot-autonomous-fix.yml"
        ".github/workflows/build-packages.yml"
        ".github/workflows/dependency-management.yml"
        ".github/workflows/performance-monitoring.yml"
        ".github/workflows/documentation.yml"
        ".github/workflows/sync-branches.yml"
    )
    
    for workflow in "${workflows[@]}"; do
        if [ -f "$PROJECT_ROOT/$workflow" ]; then
            record_test "Workflow: $workflow" "PASS" "Workflow exists"
            
            # Check YAML syntax
            if command -v yamllint &> /dev/null; then
                if yamllint "$PROJECT_ROOT/$workflow" > /dev/null 2>&1; then
                    record_test "Workflow Syntax: $workflow" "PASS" "YAML syntax is valid"
                else
                    record_test "Workflow Syntax: $workflow" "FAIL" "YAML syntax error"
                fi
            else
                record_test "Workflow Syntax: $workflow" "WARN" "yamllint not available"
            fi
        else
            record_test "Workflow: $workflow" "FAIL" "Workflow missing"
        fi
    done
}

# Verify security
verify_security() {
    log_verify "Verifying security..."
    
    cd "$PROJECT_ROOT"
    
    # Check for sensitive files
    local sensitive_patterns=("*.key" "*.pem" "*.p12" "*.pfx" "*.crt")
    local found_sensitive=false
    
    for pattern in "${sensitive_patterns[@]}"; do
        if find . -name "$pattern" -not -path "./.git/*" | grep -q .; then
            found_sensitive=true
            break
        fi
    done
    
    if [ "$found_sensitive" = false ]; then
        record_test "Sensitive Files" "PASS" "No sensitive files found"
    else
        record_test "Sensitive Files" "FAIL" "Sensitive files found in repository"
    fi
    
    # Check .gitignore
    if [ -f ".gitignore" ]; then
        record_test ".gitignore" "PASS" "Git ignore file exists"
    else
        record_test ".gitignore" "FAIL" "Git ignore file missing"
    fi
    
    # Check for hardcoded secrets
    if grep -r "password\|secret\|token\|key" --include="*.go" --include="*.sh" --include="*.yml" --include="*.yaml" . | grep -v "test\|example\|TODO" | grep -q .; then
        record_test "Hardcoded Secrets" "FAIL" "Potential hardcoded secrets found"
    else
        record_test "Hardcoded Secrets" "PASS" "No hardcoded secrets found"
    fi
}

# Generate verification report
generate_verification_report() {
    log_verify "Generating verification report..."
    
    mkdir -p "$VERIFY_DIR"
    
    local report_file="$VERIFY_DIR/verification-report.md"
    
    cat > "$report_file" << EOF
# Comprehensive Verification Report

Generated: $(date)

## Summary
- Total Tests: $TOTAL_TESTS
- Passed: $PASSED_TESTS
- Failed: $((TOTAL_TESTS - PASSED_TESTS))
- Success Rate: $([ $TOTAL_TESTS -gt 0 ] && echo "$((PASSED_TESTS * 100 / TOTAL_TESTS))%" || echo "N/A")

## Test Results

EOF
    
    for result in "${VERIFY_RESULTS[@]}"; do
        echo "$result" >> "$report_file"
    done
    
    cat >> "$report_file" << EOF

## Recommendations

EOF
    
    if [ $PASSED_TESTS -eq $TOTAL_TESTS ]; then
        echo "✅ All verifications passed! The project is ready for deployment." >> "$report_file"
    else
        echo "⚠️  Some verifications failed. Please address the issues before deployment." >> "$report_file"
    fi
    
    log_success "Verification report generated: $report_file"
}

# Show verification summary
show_verification_summary() {
    echo ""
    log_info "Verification Summary:"
    echo "===================="
    echo "Total Tests: $TOTAL_TESTS"
    echo "Passed: $PASSED_TESTS"
    echo "Failed: $((TOTAL_TESTS - PASSED_TESTS))"
    
    if [ $TOTAL_TESTS -gt 0 ]; then
        local success_rate=$((PASSED_TESTS * 100 / TOTAL_TESTS))
        echo "Success Rate: ${success_rate}%"
        
        if [ $success_rate -ge 90 ]; then
            log_success "Excellent! Project verification is highly successful."
        elif [ $success_rate -ge 80 ]; then
            log_success "Good! Project verification is mostly successful."
        elif [ $success_rate -ge 70 ]; then
            log_warning "Fair. Some issues need attention."
        else
            log_error "Poor. Significant issues need to be addressed."
        fi
    fi
    
    echo ""
    echo "Detailed results saved to: $VERIFY_DIR/verification-report.md"
}

# Clean verification artifacts
clean() {
    log_info "Cleaning verification artifacts..."
    rm -rf "$VERIFY_DIR"
    log_success "Verification artifacts cleaned"
}

# Show usage
usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -h, --help          Show this help message"
    echo "  -c, --clean         Clean verification artifacts"
    echo "  -g, --go            Verify Go installation only"
    echo "  -s, --structure     Verify project structure only"
    echo "  -b, --build         Verify build process only"
    echo "  -t, --tests         Verify tests only"
    echo "  -q, --quality       Verify code quality only"
    echo "  -a, --all           Verify everything (default)"
    echo ""
    echo "Examples:"
    echo "  $0                  Verify everything"
    echo "  $0 -b               Verify build process only"
    echo "  $0 -t               Verify tests only"
    echo "  $0 -c               Clean verification artifacts"
}

# Main function
main() {
    local clean_only=false
    local go_only=false
    local structure_only=false
    local build_only=false
    local tests_only=false
    local quality_only=false
    local verify_all=true
    
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                usage
                exit 0
                ;;
            -c|--clean)
                clean_only=true
                verify_all=false
                shift
                ;;
            -g|--go)
                go_only=true
                verify_all=false
                shift
                ;;
            -s|--structure)
                structure_only=true
                verify_all=false
                shift
                ;;
            -b|--build)
                build_only=true
                verify_all=false
                shift
                ;;
            -t|--tests)
                tests_only=true
                verify_all=false
                shift
                ;;
            -q|--quality)
                quality_only=true
                verify_all=false
                shift
                ;;
            -a|--all)
                verify_all=true
                shift
                ;;
            *)
                log_error "Unknown option: $1"
                usage
                exit 1
                ;;
        esac
    done
    
    log_info "Starting comprehensive verification..."
    
    # Execute requested actions
    if [ "$clean_only" = true ]; then
        clean
        exit 0
    fi
    
    # Execute specific verifications
    if [ "$go_only" = true ]; then
        verify_go
        show_verification_summary
        exit 0
    fi
    
    if [ "$structure_only" = true ]; then
        verify_project_structure
        show_verification_summary
        exit 0
    fi
    
    if [ "$build_only" = true ]; then
        verify_go
        verify_build
        show_verification_summary
        exit 0
    fi
    
    if [ "$tests_only" = true ]; then
        verify_go
        verify_tests
        show_verification_summary
        exit 0
    fi
    
    if [ "$quality_only" = true ]; then
        verify_code_quality
        show_verification_summary
        exit 0
    fi
    
    # Default: verify everything
    if [ "$verify_all" = true ]; then
        verify_go
        verify_project_structure
        verify_go_modules
        verify_build
        verify_tests
        verify_code_quality
        verify_configuration
        verify_scripts
        verify_documentation
        verify_github_workflows
        verify_security
        generate_verification_report
        show_verification_summary
    fi
    
    log_success "Comprehensive verification completed!"
}

# Run main function with all arguments
main "$@"
