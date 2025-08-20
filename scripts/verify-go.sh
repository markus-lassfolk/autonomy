#!/bin/bash
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# Function to print colored output
print_color() {
    local color=$1
    local message=$2
    echo -e "${color}${message}${NC}"
}

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to run a command and handle errors
run_command() {
    local name=$1
    local cmd=$2
    shift 2
    local args=("$@")

    print_color "$BLUE" "\n${BOLD}[$name]${NC} Running $cmd ${args[*]}"

    if eval "$cmd" "${args[@]}"; then
        print_color "$GREEN" "✓ $name completed successfully"
        return 0
    else
        print_color "$RED" "✗ $name failed with exit code $?"
        return 1
    fi
}

# Function to get Go files to verify
get_go_files() {
    local mode=$1
    shift
    local files=("$@")

    case $mode in
        "all")
            print_color "$BLUE" "Finding all Go files..."
            find . -name "*.go" -type f
            ;;
        "files")
            if [ ${#files[@]} -eq 0 ]; then
                print_color "$RED" "Error: No files specified for verification"
                exit 1
            fi
            print_color "$BLUE" "Using specified files: ${files[*]}"
            printf '%s\n' "${files[@]}"
            ;;
        "staged")
            print_color "$BLUE" "Finding staged Go files..."
            local staged_files
            staged_files=$(git diff --cached --name-only --diff-filter=ACM | grep '\.go$' || true)
            if [ -z "$staged_files" ]; then
                print_color "$YELLOW" "No staged Go files found"
                return
            fi
            print_color "$BLUE" "Staged Go files: $staged_files"
            echo "$staged_files"
            ;;
        *)
            print_color "$RED" "Invalid mode: $mode"
            exit 1
            ;;
    esac
}

# Function to check formatting
check_formatting() {
    local files=("$@")
    local unformatted_files=()

    print_color "$BLUE" "\n${BOLD}[FORMATTING]${NC} Checking code formatting..."

    for file in "${files[@]}"; do
        if [ -f "$file" ]; then
            local temp_file
            temp_file=$(mktemp)
            cp "$file" "$temp_file"

            if gofmt -s -w "$temp_file" >/dev/null 2>&1; then
                if ! cmp -s "$file" "$temp_file"; then
                    unformatted_files+=("$file")
                fi
            fi

            rm -f "$temp_file"
        fi
    done

    if [ ${#unformatted_files[@]} -gt 0 ]; then
        print_color "$RED" "✗ Found ${#unformatted_files[@]} unformatted file(s):"
        for file in "${unformatted_files[@]}"; do
            print_color "$RED" "  - $file"
        done
        return 1
    else
        print_color "$GREEN" "✓ All files are properly formatted"
        return 0
    fi
}

# Function to check imports
check_imports() {
    local files=("$@")
    local import_issues=()

    print_color "$BLUE" "\n${BOLD}[IMPORTS]${NC} Checking import organization..."

    for file in "${files[@]}"; do
        if [ -f "$file" ]; then
            local temp_file
            temp_file=$(mktemp)
            cp "$file" "$temp_file"

            if command_exists goimports; then
                if goimports -w "$temp_file" >/dev/null 2>&1; then
                    if ! cmp -s "$file" "$temp_file"; then
                        import_issues+=("$file")
                    fi
                fi
            fi

            rm -f "$temp_file"
        fi
    done

    if [ ${#import_issues[@]} -gt 0 ]; then
        print_color "$RED" "✗ Found ${#import_issues[@]} file(s) with import issues:"
        for file in "${import_issues[@]}"; do
            print_color "$RED" "  - $file"
        done
        return 1
    else
        print_color "$GREEN" "✓ All imports are properly organized"
        return 0
    fi
}

# Main verification function
verify_go_code() {
    local files=("$@")
    local errors=()
    local warnings=()

    if [ ${#files[@]} -eq 0 ]; then
        print_color "$YELLOW" "No Go files to verify"
        return 0
    fi

    print_color "$BLUE" "\n${BOLD}[VERIFICATION STARTED]${NC} Verifying ${#files[@]} Go file(s)"

    # Check required tools
    local required_tools=("go" "gofmt" "goimports" "golangci-lint" "govet" "staticcheck" "gosec")
    local missing_tools=()

    for tool in "${required_tools[@]}"; do
        if ! command_exists "$tool"; then
            missing_tools+=("$tool")
        fi
    done

    if [ ${#missing_tools[@]} -gt 0 ]; then
        print_color "$YELLOW" "\n${BOLD}[WARNING]${NC} Missing tools: ${missing_tools[*]}"
        print_color "$YELLOW" "Some verification steps will be skipped"
        warnings+=("Missing tools: ${missing_tools[*]}")
    fi

    # 1. Formatting check
    if command_exists gofmt; then
        if ! check_formatting "${files[@]}"; then
            errors+=("Code formatting issues found")
        fi
    fi

    # 2. Import organization check
    if command_exists goimports; then
        if ! check_imports "${files[@]}"; then
            errors+=("Import organization issues found")
        fi
    fi

    # 3. Go vet
    print_color "$BLUE" "\n${Bold}[VET]${NC} Running go vet..."
    if ! run_command "go vet" "go" "vet" "./..."; then
        errors+=("Go vet issues found")
    fi

    # 4. Staticcheck
    if command_exists staticcheck; then
        print_color "$BLUE" "\n${BOLD}[STATICCHECK]${NC} Running staticcheck..."
        if ! run_command "staticcheck" "staticcheck" "./..."; then
            errors+=("Staticcheck issues found")
        fi
    fi

    # 5. Security check
    if command_exists gosec; then
        print_color "$BLUE" "\n${BOLD}[SECURITY]${NC} Running gosec..."
        if ! run_command "gosec" "gosec" "./..."; then
            errors+=("Security issues found")
        fi
    fi

    # 6. Linting
    if command_exists golangci-lint; then
        print_color "$BLUE" "\n${BOLD}[LINTING]${NC} Running golangci-lint..."
        if ! run_command "golangci-lint" "golangci-lint" "run"; then
            errors+=("Linting issues found")
        fi
    fi

    # 7. Tests (if not skipped)
    if [ "$SKIP_TESTS" != "true" ]; then
        print_color "$BLUE" "\n${BOLD}[TESTING]${NC} Running tests..."
        if ! run_command "go test" "go" "test" "-race" "-v" "./..."; then
            errors+=("Tests failed")
        fi
    else
        print_color "$YELLOW" "\n${BOLD}[TESTING]${NC} Skipped (set SKIP_TESTS=true to skip)"
    fi

    # Summary
    print_color "$BLUE" "\n${BOLD}[SUMMARY]${NC}"
    if [ ${#errors[@]} -eq 0 ]; then
        print_color "$GREEN" "✓ All verification checks passed!"
        if [ ${#warnings[@]} -gt 0 ]; then
            print_color "$YELLOW" "\nWarnings:"
            for warning in "${warnings[@]}"; do
                print_color "$YELLOW" "  - $warning"
            done
        fi
        return 0
    else
        print_color "$RED" "✗ Verification failed with ${#errors[@]} error(s):"
        for error in "${errors[@]}"; do
            print_color "$RED" "  - $error"
        done
        if [ ${#warnings[@]} -gt 0 ]; then
            print_color "$YELLOW" "\nWarnings:"
            for warning in "${warnings[@]}"; do
                print_color "$YELLOW" "  - $warning"
            done
        fi
        return 1
    fi
}

# Function to show usage
show_usage() {
    cat <<EOF
Usage: $0 [OPTIONS] MODE

Go code verification script for the autonomy project

MODE:
    all       Verify all Go files in the project
    files     Verify specific files (use -f/--files parameter)
    staged    Verify only staged files (for pre-commit)

OPTIONS:
    -f, --files FILES     Specific files to verify (comma-separated)
    -s, --skip-tests      Skip running tests (useful for quick checks)
    -v, --verbose         Enable verbose output
    -h, --help           Show this help message

EXAMPLES:
    # Verify all files
    $0 all

    # Verify specific files
    $0 files -f "cmd/autonomyd/main.go,pkg/logx/logger.go"

    # Verify staged files (pre-commit)
    $0 staged

    # Quick check without tests
    $0 all -s

EOF
}

# Parse command line arguments
MODE=""
FILES=()
SKIP_TESTS=false
VERBOSE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        all | files | staged)
            MODE="$1"
            shift
            ;;
        -f | --files)
            IFS=',' read -ra FILES <<<"$2"
            shift 2
            ;;
        -s | --skip-tests)
            SKIP_TESTS=true
            shift
            ;;
        -v | --verbose)
            VERBOSE=true
            shift
            ;;
        -h | --help)
            show_usage
            exit 0
            ;;
        *)
            print_color "$RED" "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
done

# Validate mode
if [ -z "$MODE" ]; then
    print_color "$RED" "Error: Mode is required"
    show_usage
    exit 1
fi

# Main execution
print_color "$BLUE" "${BOLD}[autonomy GO VERIFICATION]${NC}"
print_color "$BLUE" "Mode: $MODE"
if [ "$VERBOSE" = true ]; then
    print_color "$BLUE" "Verbose: Enabled"
fi

# Get files to verify
mapfile -t go_files < <(get_go_files "$MODE" "${FILES[@]}")

# Run verification
verify_go_code "${go_files[@]}"
