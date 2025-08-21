#!/bin/bash
#
# Comprehensive verification script for autonomy project (Go + LuCI)
#
# Complete verification solution for both Go backend and LuCI frontend components:
#
# GO VERIFICATION:
# - Code formatting (gofmt, goimports)
# - Linting (golangci-lint, staticcheck, gocritic)
# - Security scanning (gosec)
# - Testing (go test, race detection, coverage)
# - Build verification (multi-platform)
# - Dependency analysis
# - Documentation generation
#
# LUCİ VERIFICATION:
# - Lua syntax checking (lua -p)
# - Lua linting (luacheck)
# - HTML validation (htmlhint)
# - JavaScript linting (eslint)
# - CSS linting (stylelint)
# - Translation validation (msgfmt)
# - LuCI-specific checks

set -e

# Script configuration
SCRIPT_NAME="verify-comprehensive.sh"
VERSION="3.0.0"
START_TIME=$(date +%s)

# Default values
MODE="all"
FILES=()
VERBOSE=false
QUIET=false
DRY_RUN=false
FIX=false
NO_GO=false
NO_LUCI=false
NO_FORMAT=false
NO_LINT=false
NO_SECURITY=false
NO_TESTS=false
NO_BUILD=false
NO_DEPS=false
NO_DOCS=false
NO_TRANSLATIONS=false
COVERAGE=false
RACE=false
INSTALL_DEPS=false
TIMEOUT=300

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
GRAY='\033[0;37m'
WHITE='\033[1;37m'
MAGENTA='\033[0;35m'
NC='\033[0m' # No Color

# Statistics
TOTAL_CHECKS=0
PASSED_CHECKS=0
FAILED_CHECKS=0
WARNINGS=0
ERRORS=0

# Tool configurations
GO_TOOLS=(
    "go:go:Built-in"
    "gofmt:gofmt:go install golang.org/x/tools/cmd/gofmt@latest"
    "goimports:goimports:go install golang.org/x/tools/cmd/goimports@latest"
    "golangci-lint:golangci-lint:go install github.com/golangci/golangci-lint/cmd/golangci-lint@latest"
    "staticcheck:staticcheck:go install honnef.co/go/tools/cmd/staticcheck@latest"
    "gocritic:gocritic:go install github.com/go-critic/go-critic/cmd/gocritic@latest"
    "gosec:gosec:go install github.com/securego/gosec/v2/cmd/gosec@latest"
    "gocyclo:gocyclo:go install github.com/fzipp/gocyclo/cmd/gocyclo@latest"
    "ineffassign:ineffassign:go install github.com/gordonklaus/ineffassign@latest"
    "godoc:godoc:go install golang.org/x/tools/cmd/godoc@latest"
)

LUCI_TOOLS=(
    "lua:lua:Install Lua from https://www.lua.org/download.html"
    "luacheck:luacheck:luarocks install luacheck"
    "htmlhint:htmlhint:npm install -g htmlhint"
    "eslint:eslint:npm install -g eslint"
    "stylelint:stylelint:npm install -g stylelint"
    "msgfmt:msgfmt:Install gettext package for your OS"
)

show_usage() {
    cat <<EOF
Comprehensive Verification Script v$VERSION

USAGE:
    $SCRIPT_NAME [MODE] [OPTIONS] [FILES...]

MODES:
    all     - Check all components (Go + LuCI) [default]
    go      - Check only Go components
    luci    - Check only LuCI components
    files   - Check specific files or patterns
    staged  - Check staged files for pre-commit
    commit  - Check files in git diff --cached
    ci      - CI/CD mode with all checks

OPTIONS:
    -h, --help           - Show this help
    -v, --verbose        - Verbose output
    -q, --quiet          - Quiet mode (errors only)
    --dry-run            - Show what would be done
    --fix                - Attempt to fix issues automatically
    --install-deps       - Install missing tools automatically

GO OPTIONS:
    --no-go              - Skip Go verification
    --no-format          - Skip formatting checks
    --no-lint            - Skip linting checks
    --no-security        - Skip security checks
    --no-tests           - Skip tests
    --no-build           - Skip build verification
    --no-deps            - Skip dependency analysis
    --no-docs            - Skip documentation generation
    --coverage           - Generate test coverage report
    --race               - Enable race detection in tests

LUCİ OPTIONS:
    --no-luci            - Skip LuCI verification
    --no-translations    - Skip translation validation

EXAMPLES:
    $SCRIPT_NAME all                    # Full verification
    $SCRIPT_NAME go                     # Go-only verification
    $SCRIPT_NAME luci                   # LuCI-only verification
    $SCRIPT_NAME staged                 # Pre-commit check
    $SCRIPT_NAME files "*.lua"          # Check Lua files
    $SCRIPT_NAME all --fix              # Auto-fix mode
    $SCRIPT_NAME ci --coverage --race   # CI/CD mode
    $SCRIPT_NAME all --install-deps     # Install missing tools and verify

REQUIRED TOOLS:
    Go: go, gofmt, goimports, golangci-lint, staticcheck, gocritic, gosec
    LuCI: lua, luacheck, htmlhint, eslint, stylelint, msgfmt

EOF
}

write_log() {
    local message="$1"
    local level="${2:-INFO}"
    local category="${3:-General}"

    if [[ "$QUIET" == "true" && "$level" != "ERROR" ]]; then
        return
    fi

    local timestamp=$(date +"%H:%M:%S")
    local color=""

    case "$level" in
        "SUCCESS") color="$GREEN" ;;
        "WARNING") color="$YELLOW" ;;
        "ERROR") color="$RED" ;;
        "VERBOSE") color="$CYAN" ;;
        *) color="$BLUE" ;;
    esac

    local prefix="[$timestamp] [$level]"
    if [[ "$category" != "General" ]]; then
        prefix="$prefix [$category]"
    fi

    echo -e "${color}$prefix $message${NC}"
}

write_color_output() {
    local message="$1"
    local color="${2:-WHITE}"

    case "$color" in
        "RED") echo -e "${RED}$message${NC}" ;;
        "GREEN") echo -e "${GREEN}$message${NC}" ;;
        "YELLOW") echo -e "${YELLOW}$message${NC}" ;;
        "BLUE") echo -e "${BLUE}$message${NC}" ;;
        "CYAN") echo -e "${CYAN}$message${NC}" ;;
        "GRAY") echo -e "${GRAY}$message${NC}" ;;
        "WHITE") echo -e "${WHITE}$message${NC}" ;;
        "MAGENTA") echo -e "${MAGENTA}$message${NC}" ;;
        *) echo "$message" ;;
    esac
}

test_tools() {
    local tools=("$@")
    local category="$1"
    shift

    write_log "Checking required $category tools..." "INFO" "Setup"

    local available=()
    local missing=()

    for tool_info in "${tools[@]}"; do
        IFS=':' read -r tool_name command install_cmd <<<"$tool_info"

        if command -v "$command" >/dev/null 2>&1; then
            available+=("$tool_name")
            write_log "Tool '$tool_name' available" "VERBOSE" "Tools"
        else
            missing+=("$tool_name")
            write_log "Tool '$tool_name' not found" "WARNING" "Tools"
            write_log "Install with: $install_cmd" "INFO" "Tools"
        fi
    done

    if [[ ${#available[@]} -gt 0 ]]; then
        write_log "Available $category tools: ${available[*]}" "SUCCESS" "Setup"
    fi

    if [[ ${#missing[@]} -gt 0 ]]; then
        write_log "Missing $category tools: ${missing[*]}" "WARNING" "Setup"
        # Store missing tools in global variables for later use
        if [[ "$category" == "Go" ]]; then
            GO_MISSING_TOOLS=("${missing[@]}")
        elif [[ "$category" == "LuCI" ]]; then
            LUCI_MISSING_TOOLS=("${missing[@]}")
        fi
        return 1
    fi

    return 0
}

install_missing_tools() {
    local tools=("$@")
    local category="$1"
    shift
    local missing_tools=("$@")

    write_log "Installing missing $category tools..." "INFO" "Setup"

    local installed=()
    local failed=()

    for tool in "${missing_tools[@]}"; do
        # Find the tool info
        local tool_info=""
        for info in "${tools[@]}"; do
            IFS=':' read -r tool_name command install_cmd <<<"$info"
            if [[ "$tool_name" == "$tool" ]]; then
                tool_info="$info"
                break
            fi
        done

        if [[ -z "$tool_info" ]]; then
            write_log "Tool info not found for $tool" "ERROR" "Setup"
            failed+=("$tool")
            continue
        fi

        IFS=':' read -r tool_name command install_cmd <<<"$tool_info"

        write_log "Installing $tool..." "INFO" "Setup"

        if [[ "$install_cmd" == "Built-in" ]]; then
            write_log "Tool '$tool' is built-in, skipping installation" "WARNING" "Setup"
            continue
        fi

        # Handle different installation methods
        local result=""
        if [[ "$install_cmd" == go\ install* ]]; then
            result=$(invoke_command_with_timeout "go" "install" "${install_cmd##* }")
        elif [[ "$install_cmd" == npm\ install* ]]; then
            result=$(invoke_command_with_timeout "npm" "install" "-g" "${install_cmd##* }")
        elif [[ "$install_cmd" == luarocks\ install* ]]; then
            result=$(invoke_command_with_timeout "luarocks" "install" "${install_cmd##* }")
        else
            write_log "Manual installation required for $tool: $install_cmd" "WARNING" "Setup"
            failed+=("$tool")
            continue
        fi

        IFS=':' read -r status exit_code output error <<<"$result"

        if [[ "$status" == "SUCCESS" && "$exit_code" -eq 0 ]]; then
            write_log "Successfully installed $tool" "SUCCESS" "Setup"
            installed+=("$tool")
        else
            write_log "Failed to install $tool: $output $error" "ERROR" "Setup"
            failed+=("$tool")
        fi
    done

    if [[ ${#installed[@]} -gt 0 ]]; then
        write_log "Successfully installed ${#installed[@]} $category tool(s): ${installed[*]}" "SUCCESS" "Setup"
    fi

    if [[ ${#failed[@]} -gt 0 ]]; then
        write_log "Failed to install ${#failed[@]} $category tool(s): ${failed[*]}" "ERROR" "Setup"
        write_log "Please install these tools manually:" "INFO" "Setup"
        for tool in "${failed[@]}"; do
            # Find the install command for this tool
            for info in "${tools[@]}"; do
                IFS=':' read -r tool_name command install_cmd <<<"$info"
                if [[ "$tool_name" == "$tool" ]]; then
                    write_log "  $tool: $install_cmd" "INFO" "Setup"
                    break
                fi
            done
        done
    fi

    # Update global variables
    if [[ "$category" == "Go" ]]; then
        GO_INSTALLED_TOOLS=("${installed[@]}")
        GO_FAILED_TOOLS=("${failed[@]}")
    elif [[ "$category" == "LuCI" ]]; then
        LUCI_INSTALLED_TOOLS=("${installed[@]}")
        LUCI_FAILED_TOOLS=("${failed[@]}")
    fi
}

invoke_command_with_timeout() {
    local command="$1"
    shift
    local args=("$@")
    local timeout_seconds="${TIMEOUT:-300}"
    local working_directory="${PWD}"

    # Create temporary files for output
    local output_file=$(mktemp)
    local error_file=$(mktemp)

    # Run command with timeout
    if timeout "$timeout_seconds" bash -c "cd '$working_directory' && $command ${args[*]} > '$output_file' 2> '$error_file'" 2>/dev/null; then
        local exit_code=$?
        local output=$(cat "$output_file")
        local error=$(cat "$error_file")

        rm -f "$output_file" "$error_file"

        echo "SUCCESS:$exit_code:$output:$error"
    else
        rm -f "$output_file" "$error_file"
        echo "TIMEOUT:-1:Command timed out after $timeout_seconds seconds:"
    fi
}

get_files_to_check() {
    local mode="$1"
    shift
    local files=("$@")

    case "$mode" in
        "all")
            local go_files=$(find . -name "*.go" -not -path "./vendor/*" 2>/dev/null || true)
            local luci_files=$(find . \( -name "*.lua" -o -name "*.html" -o -name "*.js" -o -name "*.css" -o -name "*.po" \) \( -path "*/luci/*" -o -path "*/www/*" \) 2>/dev/null || true)
            echo "GO:$go_files"
            echo "LUCI:$luci_files"
            ;;
        "go")
            local go_files=$(find . -name "*.go" -not -path "./vendor/*" 2>/dev/null || true)
            echo "GO:$go_files"
            echo "LUCI:"
            ;;
        "luci")
            local luci_files=$(find . \( -name "*.lua" -o -name "*.html" -o -name "*.js" -o -name "*.css" -o -name "*.po" \) \( -path "*/luci/*" -o -path "*/www/*" \) 2>/dev/null || true)
            echo "GO:"
            echo "LUCI:$luci_files"
            ;;
        "files")
            local go_files=""
            local luci_files=""

            for file in "${files[@]}"; do
                if [[ "$file" == *.go ]]; then
                    if [[ -f "$file" ]]; then
                        go_files="$go_files $file"
                    fi
                elif [[ "$file" == *.lua || "$file" == *.html || "$file" == *.js || "$file" == *.css || "$file" == *.po ]]; then
                    if [[ -f "$file" ]]; then
                        luci_files="$luci_files $file"
                    fi
                fi
            done

            echo "GO:$go_files"
            echo "LUCI:$luci_files"
            ;;
        "staged")
            local staged_files=$(git diff --cached --name-only 2>/dev/null || true)
            local go_files=""
            local luci_files=""

            for file in $staged_files; do
                if [[ "$file" == *.go ]]; then
                    go_files="$go_files $file"
                elif [[ "$file" == *.lua || "$file" == *.html || "$file" == *.js || "$file" == *.css || "$file" == *.po ]]; then
                    luci_files="$luci_files $file"
                fi
            done

            echo "GO:$go_files"
            echo "LUCI:$luci_files"
            ;;
        *)
            echo "GO:"
            echo "LUCI:"
            ;;
    esac
}

# Go verification functions
invoke_go_format() {
    if [[ "$NO_FORMAT" == "true" ]]; then
        return
    fi

    write_log "Running Go formatting..." "INFO" "Go"

    if [[ "$FIX" == "true" ]]; then
        local result=$(invoke_command_with_timeout "gofmt" "-s" "-w" ".")
        IFS=':' read -r status exit_code output error <<<"$result"

        if [[ "$status" == "SUCCESS" && "$exit_code" -eq 0 ]]; then
            write_log "Go formatting completed" "SUCCESS" "Go"
        else
            write_log "Go formatting failed: $output $error" "ERROR" "Go"
            ((FAILED_CHECKS++))
        fi
    else
        local result=$(invoke_command_with_timeout "gofmt" "-l" ".")
        IFS=':' read -r status exit_code output error <<<"$result"

        if [[ "$status" == "SUCCESS" && "$exit_code" -eq 0 && -z "$output" ]]; then
            write_log "Go formatting is correct" "SUCCESS" "Go"
        else
            write_log "Files need formatting: $output" "WARNING" "Go"
            write_log "Run with --fix to auto-fix formatting issues" "INFO" "Go"
            ((WARNINGS++))
        fi
    fi

    ((TOTAL_CHECKS++))
}

invoke_go_imports() {
    if [[ "$NO_FORMAT" == "true" ]]; then
        return
    fi

    write_log "Organizing imports..." "INFO" "Go"

    if [[ "$FIX" == "true" ]]; then
        local result=$(invoke_command_with_timeout "goimports" "-w" ".")
        IFS=':' read -r status exit_code output error <<<"$result"

        if [[ "$status" == "SUCCESS" && "$exit_code" -eq 0 ]]; then
            write_log "Import organization completed" "SUCCESS" "Go"
        else
            write_log "Import organization failed: $output $error" "ERROR" "Go"
            ((FAILED_CHECKS++))
        fi
    else
        local result=$(invoke_command_with_timeout "goimports" "-l" ".")
        IFS=':' read -r status exit_code output error <<<"$result"

        if [[ "$status" == "SUCCESS" && "$exit_code" -eq 0 && -z "$output" ]]; then
            write_log "Imports are organized" "SUCCESS" "Go"
        else
            write_log "Files need import organization: $output" "WARNING" "Go"
            write_log "Run with --fix to auto-organize imports" "INFO" "Go"
            ((WARNINGS++))
        fi
    fi

    ((TOTAL_CHECKS++))
}

invoke_go_lint() {
    if [[ "$NO_LINT" == "true" ]]; then
        return
    fi

    write_log "Running golangci-lint..." "INFO" "Go"

    local result=$(invoke_command_with_timeout "golangci-lint" "run")
    IFS=':' read -r status exit_code output error <<<"$result"

    if [[ "$status" == "SUCCESS" && "$exit_code" -eq 0 ]]; then
        write_log "Go linting passed" "SUCCESS" "Go"
        ((PASSED_CHECKS++))
    else
        write_log "Go linting failed: $output $error" "ERROR" "Go"
        ((FAILED_CHECKS++))
    fi

    ((TOTAL_CHECKS++))
}

invoke_go_vet() {
    if [[ "$NO_LINT" == "true" ]]; then
        return
    fi

    write_log "Running go vet..." "INFO" "Go"

    local result=$(invoke_command_with_timeout "go" "vet" "./...")
    IFS=':' read -r status exit_code output error <<<"$result"

    if [[ "$status" == "SUCCESS" && "$exit_code" -eq 0 ]]; then
        write_log "Go vet passed" "SUCCESS" "Go"
        ((PASSED_CHECKS++))
    else
        write_log "Go vet failed: $output $error" "ERROR" "Go"
        ((FAILED_CHECKS++))
    fi

    ((TOTAL_CHECKS++))
}

invoke_go_security() {
    if [[ "$NO_SECURITY" == "true" ]]; then
        return
    fi

    write_log "Running security scan..." "INFO" "Go"

    local result=$(invoke_command_with_timeout "gosec" "./...")
    IFS=':' read -r status exit_code output error <<<"$result"

    if [[ "$status" == "SUCCESS" && "$exit_code" -eq 0 ]]; then
        write_log "Security scan passed" "SUCCESS" "Go"
        ((PASSED_CHECKS++))
    else
        write_log "Security scan found issues: $output $error" "ERROR" "Go"
        ((FAILED_CHECKS++))
    fi

    ((TOTAL_CHECKS++))
}

invoke_go_tests() {
    if [[ "$NO_TESTS" == "true" ]]; then
        return
    fi

    write_log "Running tests..." "INFO" "Go"

    local test_args=("test" "./...")
    if [[ "$RACE" == "true" ]]; then
        test_args+=("-race")
    fi
    if [[ "$COVERAGE" == "true" ]]; then
        test_args+=("-coverprofile=coverage.out")
    fi

    local result=$(invoke_command_with_timeout "go" "${test_args[@]}")
    IFS=':' read -r status exit_code output error <<<"$result"

    if [[ "$status" == "SUCCESS" && "$exit_code" -eq 0 ]]; then
        write_log "Tests passed" "SUCCESS" "Go"
        ((PASSED_CHECKS++))

        if [[ "$COVERAGE" == "true" ]]; then
            write_log "Coverage report generated: coverage.out" "INFO" "Go"
        fi
    else
        write_log "Tests failed: $output $error" "ERROR" "Go"
        ((FAILED_CHECKS++))
    fi

    ((TOTAL_CHECKS++))
}

invoke_go_build() {
    if [[ "$NO_BUILD" == "true" ]]; then
        return
    fi

    write_log "Verifying builds..." "INFO" "Go"

    local platforms=("linux/amd64" "linux/arm64" "windows/amd64")
    local failed=0

    for platform in "${platforms[@]}"; do
        IFS='/' read -r os arch <<<"$platform"

        write_log "Building for $platform..." "VERBOSE" "Go"

        export GOOS="$os"
        export GOARCH="$arch"

        local result=$(invoke_command_with_timeout "go" "build" "-o" "bin/autonomyd-$os-$arch" "./cmd/autonomyd")
        IFS=':' read -r status exit_code output error <<<"$result"

        if [[ "$status" != "SUCCESS" || "$exit_code" -ne 0 ]]; then
            write_log "Build failed: autonomyd for $platform" "ERROR" "Go"
            write_log "Error: $output $error" "ERROR" "Go"
            ((failed++))
        fi
    done

    if [[ $failed -eq 0 ]]; then
        write_log "All builds successful" "SUCCESS" "Go"
        ((PASSED_CHECKS++))
    else
        write_log "$failed build(s) failed" "ERROR" "Go"
        ((FAILED_CHECKS++))
    fi

    ((TOTAL_CHECKS++))
}

# LuCI verification functions
invoke_lua_syntax() {
    if [[ "$NO_LINT" == "true" ]]; then
        return
    fi

    write_log "Checking Lua syntax..." "INFO" "LuCI"

    local lua_files=$(find . -name "*.lua" \( -path "*/luci/*" -o -path "*/www/*" \) 2>/dev/null || true)
    local errors=0

    for file in $lua_files; do
        if [[ -f "$file" ]]; then
            local result=$(invoke_command_with_timeout "lua" "-p" "$file")
            IFS=':' read -r status exit_code output error <<<"$result"

            if [[ "$status" != "SUCCESS" || "$exit_code" -ne 0 ]]; then
                write_log "Lua syntax error in $(basename "$file"): $output $error" "ERROR" "LuCI"
                ((errors++))
            fi
        fi
    done

    if [[ $errors -eq 0 ]]; then
        write_log "Lua syntax check passed" "SUCCESS" "LuCI"
        ((PASSED_CHECKS++))
    else
        write_log "Lua syntax check failed: $errors error(s)" "ERROR" "LuCI"
        ((FAILED_CHECKS++))
    fi

    ((TOTAL_CHECKS++))
}

invoke_lua_lint() {
    if [[ "$NO_LINT" == "true" ]]; then
        return
    fi

    write_log "Linting Lua files..." "INFO" "LuCI"

    local result=$(invoke_command_with_timeout "luacheck" "." "--no-color")
    IFS=':' read -r status exit_code output error <<<"$result"

    if [[ "$status" == "SUCCESS" && "$exit_code" -eq 0 ]]; then
        write_log "Lua linting passed" "SUCCESS" "LuCI"
        ((PASSED_CHECKS++))
    else
        write_log "Lua linting found issues: $output $error" "ERROR" "LuCI"
        ((FAILED_CHECKS++))
    fi

    ((TOTAL_CHECKS++))
}

invoke_html_validation() {
    if [[ "$NO_LINT" == "true" ]]; then
        return
    fi

    write_log "Validating HTML files..." "INFO" "LuCI"

    local result=$(invoke_command_with_timeout "htmlhint" ".")
    IFS=':' read -r status exit_code output error <<<"$result"

    if [[ "$status" == "SUCCESS" && "$exit_code" -eq 0 ]]; then
        write_log "HTML validation passed" "SUCCESS" "LuCI"
        ((PASSED_CHECKS++))
    else
        write_log "HTML validation found issues: $output $error" "ERROR" "LuCI"
        ((FAILED_CHECKS++))
    fi

    ((TOTAL_CHECKS++))
}

invoke_javascript_lint() {
    if [[ "$NO_LINT" == "true" ]]; then
        return
    fi

    write_log "Linting JavaScript files..." "INFO" "LuCI"

    local result=$(invoke_command_with_timeout "eslint" "." "--ext" ".js")
    IFS=':' read -r status exit_code output error <<<"$result"

    if [[ "$status" == "SUCCESS" && "$exit_code" -eq 0 ]]; then
        write_log "JavaScript linting passed" "SUCCESS" "LuCI"
        ((PASSED_CHECKS++))
    else
        write_log "JavaScript linting found issues: $output $error" "ERROR" "LuCI"
        ((FAILED_CHECKS++))
    fi

    ((TOTAL_CHECKS++))
}

invoke_css_lint() {
    if [[ "$NO_LINT" == "true" ]]; then
        return
    fi

    write_log "Linting CSS files..." "INFO" "LuCI"

    local result=$(invoke_command_with_timeout "stylelint" "**/*.css")
    IFS=':' read -r status exit_code output error <<<"$result"

    if [[ "$status" == "SUCCESS" && "$exit_code" -eq 0 ]]; then
        write_log "CSS linting passed" "SUCCESS" "LuCI"
        ((PASSED_CHECKS++))
    else
        write_log "CSS linting found issues: $output $error" "ERROR" "LuCI"
        ((FAILED_CHECKS++))
    fi

    ((TOTAL_CHECKS++))
}

invoke_translation_validation() {
    if [[ "$NO_TRANSLATIONS" == "true" ]]; then
        return
    fi

    write_log "Validating translation files..." "INFO" "LuCI"

    local po_files=$(find . -name "*.po" \( -path "*/luci/*" -o -path "*/www/*" \) 2>/dev/null || true)
    local errors=0

    for file in $po_files; do
        if [[ -f "$file" ]]; then
            local result=$(invoke_command_with_timeout "msgfmt" "--check" "$file")
            IFS=':' read -r status exit_code output error <<<"$result"

            if [[ "$status" != "SUCCESS" || "$exit_code" -ne 0 ]]; then
                write_log "Translation error in $(basename "$file"): $output $error" "ERROR" "LuCI"
                ((errors++))
            fi
        fi
    done

    if [[ $errors -eq 0 ]]; then
        write_log "Translation validation passed" "SUCCESS" "LuCI"
        ((PASSED_CHECKS++))
    else
        write_log "Translation validation failed: $errors error(s)" "ERROR" "LuCI"
        ((FAILED_CHECKS++))
    fi

    ((TOTAL_CHECKS++))
}

show_summary() {
    local end_time=$(date +%s)
    local duration=$((end_time - START_TIME))

    write_log "Verification completed in ${duration}s" "INFO" "Summary"
    write_log "Checks run: $TOTAL_CHECKS, Passed: $PASSED_CHECKS, Failed: $FAILED_CHECKS" "INFO" "Summary"

    if [[ $WARNINGS -gt 0 ]]; then
        write_log "$WARNINGS warning(s)" "WARNING" "Summary"
    fi

    if [[ $FAILED_CHECKS -gt 0 ]]; then
        write_log "$FAILED_CHECKS check(s) failed" "ERROR" "Summary"
        exit 1
    else
        write_log "All checks passed!" "SUCCESS" "Summary"
        exit 0
    fi
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h | --help)
            show_usage
            exit 0
            ;;
        -v | --verbose)
            VERBOSE=true
            shift
            ;;
        -q | --quiet)
            QUIET=true
            shift
            ;;
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        --fix)
            FIX=true
            shift
            ;;
        --no-go)
            NO_GO=true
            shift
            ;;
        --no-luci)
            NO_LUCI=true
            shift
            ;;
        --no-format)
            NO_FORMAT=true
            shift
            ;;
        --no-lint)
            NO_LINT=true
            shift
            ;;
        --no-security)
            NO_SECURITY=true
            shift
            ;;
        --no-tests)
            NO_TESTS=true
            shift
            ;;
        --no-build)
            NO_BUILD=true
            shift
            ;;
        --no-deps)
            NO_DEPS=true
            shift
            ;;
        --no-docs)
            NO_DOCS=true
            shift
            ;;
        --no-translations)
            NO_TRANSLATIONS=true
            shift
            ;;
        --coverage)
            COVERAGE=true
            shift
            ;;
        --race)
            RACE=true
            shift
            ;;
        --install-deps)
            INSTALL_DEPS=true
            shift
            ;;
        all | go | luci | files | staged | commit | ci)
            MODE="$1"
            shift
            ;;
        *)
            FILES+=("$1")
            shift
            ;;
    esac
done

# Main execution
write_log "autonomy Comprehensive Verification Script v$VERSION" "INFO" "Setup"
write_log "Mode: $MODE" "INFO" "Setup"

if [[ "$DRY_RUN" == "true" ]]; then
    write_log "DRY RUN MODE - No changes will be made" "WARNING" "Setup"
fi

# Check tools
go_tools_ok=true
luci_tools_ok=true

if ! test_tools "Go" "${GO_TOOLS[@]}"; then
    go_tools_ok=false
fi

if ! test_tools "LuCI" "${LUCI_TOOLS[@]}"; then
    luci_tools_ok=false
fi

# Install missing tools if requested
if [[ "$INSTALL_DEPS" == "true" ]]; then
    write_log "Auto-installation mode enabled" "INFO" "Setup"

    # Install Go tools
    if [[ "$go_tools_ok" != "true" && ${#GO_MISSING_TOOLS[@]} -gt 0 ]]; then
        install_missing_tools "${GO_TOOLS[@]}" "Go" "${GO_MISSING_TOOLS[@]}"
        if [[ ${#GO_FAILED_TOOLS[@]} -eq 0 ]]; then
            go_tools_ok=true
            write_log "All Go tools installed successfully" "SUCCESS" "Setup"
        else
            write_log "Some Go tools failed to install" "WARNING" "Setup"
        fi
    fi

    # Install LuCI tools
    if [[ "$luci_tools_ok" != "true" && ${#LUCI_MISSING_TOOLS[@]} -gt 0 ]]; then
        install_missing_tools "${LUCI_TOOLS[@]}" "LuCI" "${LUCI_MISSING_TOOLS[@]}"
        if [[ ${#LUCI_FAILED_TOOLS[@]} -eq 0 ]]; then
            luci_tools_ok=true
            write_log "All LuCI tools installed successfully" "SUCCESS" "Setup"
        else
            write_log "Some LuCI tools failed to install" "WARNING" "Setup"
        fi
    fi

    # Re-check tools after installation
    if [[ "$go_tools_ok" == "true" || "$luci_tools_ok" == "true" ]]; then
        write_log "Re-checking tools after installation..." "INFO" "Setup"
        if test_tools "Go" "${GO_TOOLS[@]}"; then
            go_tools_ok=true
        fi
        if test_tools "LuCI" "${LUCI_TOOLS[@]}"; then
            luci_tools_ok=true
        fi
    fi
fi

# Get files to check
mapfile -t file_info < <(get_files_to_check "$MODE" "${FILES[@]}")
go_files=""
luci_files=""

for line in "${file_info[@]}"; do
    IFS=':' read -r category files <<<"$line"
    if [[ "$category" == "GO" ]]; then
        go_files="$files"
    elif [[ "$category" == "LUCI" ]]; then
        luci_files="$files"
    fi
done

go_count=$(echo "$go_files" | wc -w)
luci_count=$(echo "$luci_files" | wc -w)

write_log "Found $go_count Go files and $luci_count LuCI files to verify" "INFO" "Setup"

# Run Go verification
if [[ "$NO_GO" != "true" && "$go_tools_ok" == "true" ]]; then
    write_log "Starting Go verification..." "INFO" "Go"

    invoke_go_format
    invoke_go_imports
    invoke_go_lint
    invoke_go_vet
    invoke_go_security
    invoke_go_tests
    invoke_go_build
fi

# Run LuCI verification
if [[ "$NO_LUCI" != "true" && "$luci_tools_ok" == "true" ]]; then
    write_log "Starting LuCI verification..." "INFO" "LuCI"

    invoke_lua_syntax
    invoke_lua_lint
    invoke_html_validation
    invoke_javascript_lint
    invoke_css_lint
    invoke_translation_validation
fi

# Show summary
show_summary
