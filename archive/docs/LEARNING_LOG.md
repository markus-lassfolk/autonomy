# RUTOS Project Learning Log

This document captures important discoveries, patterns, and lessons learned during the development of the RUTOS Starlink Failover project.

## Learning Capture Protocol

**Purpose**: Build collective knowledge base for RUTOS/BusyBox development challenges and solutions.

### What to Document

1. **RUTOS/BusyBox Discoveries** - Shell compatibility issues and solutions
2. **Debug and Testing Insights** - Debugging techniques and testing strategies  
3. **Development Workflow Improvements** - Tool configurations and productivity enhancements
4. **Script Architecture Patterns** - Effective function design and error handling
5. **Integration and Deployment Learnings** - Remote deployment solutions and gotchas

### Documentation Format

```bash
### [Category] - [Brief Title] (Date: YYYY-MM-DD)

**Discovery**: What was learned or discovered
**Context**: When/where this applies
**Implementation**: How to apply this learning
**Impact**: Why this matters for the project
**Example**: Code example or specific case (if applicable)
```

## Recent Learning Captures

### Shell Scripting - Subprocess Output Contamination Fix (Date: 2025-07-23)

**Discovery**: Using pipes with logging functions inside find commands contaminates script lists with log output like "[STEP]", "[DEBUG]" being treated as script filenames
**Context**: When collecting script lists using find with logging inside loops or subshells  
**Implementation**: Move all logging AFTER data collection, use temp files instead of pipes for complex processing
**Impact**: Prevents critical parsing failures that can cause divide-by-zero errors and complete test system failure
**Example**:

```bash
# WRONG - logging contaminates output
find . -name "*.sh" | while read script; do
    log_debug "Found: $script"  # This contamination breaks parsing
done

# RIGHT - collect first, log after
temp_file="/tmp/scripts_$$"
find . -name "*.sh" > "$temp_file"
log_step "Finding scripts"  # Safe to log after collection
```

### Shell Scripting - Function Output Contamination (Date: 2025-07-23)

**Discovery**: ANY logging calls inside a function that returns output via `$()` command substitution will contaminate the return value, causing log messages to be treated as actual data
**Context**: When functions are meant to return pure data (like script lists) that will be parsed by calling code
**Implementation**: NEVER put logging calls inside functions that return output via stdout. Move all logging to the calling function.
**Impact**: Prevents critical bugs where log output like "[STEP] Finding scripts" gets treated as actual script filenames, causing syntax errors and complete system failure
**Example**:

```bash
# WRONG - logging inside output function contaminates return value
get_script_list() {
    log_step "Finding scripts"  # This becomes part of the returned data!
    find . -name "*.sh"
}
script_list=$(get_script_list)  # Now contains log messages mixed with script names

# RIGHT - logging outside the output function
get_script_list() {
    # NO LOGGING - pure output function
    find . -name "*.sh"
}
log_step "Finding scripts"      # Safe - not captured by $()
script_list=$(get_script_list)  # Clean script list only
```

### Testing - File-Based Processing Over Pipes (Date: 2025-07-23)

**Discovery**: BusyBox subshell variable persistence issues make pipe-based processing unreliable for counters and state
**Context**: When processing lists of items and tracking results/counters across iterations
**Implementation**: Use temporary files to pass data between processing stages instead of pipes with variable updates
**Impact**: Ensures reliable result tracking and prevents variables being reset to zero after subshell completion
**Example**:

```bash
# WRONG - variables lost in subshell
find . -name "*.sh" | while read script; do
    COUNTER=$((COUNTER + 1))  # Lost when pipe ends
done

# RIGHT - file-based approach
temp_results="/tmp/results_$$"
find . -name "*.sh" > /tmp/scripts_$$
while read script; do
    echo "PASS:$script" >> "$temp_results"
done < /tmp/scripts_$$
COUNTER=$(wc -l < "$temp_results")
```

### Shell Scripting - BusyBox Command Output Whitespace (Date: 2025-07-23)

**Discovery**: BusyBox `wc` and `grep -c` commands can include unwanted whitespace/newlines in output, causing arithmetic errors and display issues like "0\n0" instead of "0"
**Context**: When capturing command output in variables for arithmetic operations or display
**Implementation**: Always strip whitespace with `tr -d ' \n\r'` when capturing numeric output from BusyBox commands
**Impact**: Prevents "bad number" arithmetic errors and malformed display output in RUTOS environment
**Example**:

```bash
# WRONG - can include newlines/whitespace causing "0\n0" display
COUNT=$(wc -l < file)
MATCHES=$(grep -c "pattern" file)

# RIGHT - strip all whitespace for clean numbers
COUNT=$(wc -l < file | tr -d ' \n\r')
MATCHES=$(grep -c "pattern" file | tr -d ' \n\r')
```

### System Administration - Remote Installation Debug Enhancement (Date: 2025-07-27)

**Discovery**: Remote installation debugging requires comprehensive tracing showing system info, exact commands, file operations, and disk space management to effectively troubleshoot curl errors and installation failures
**Context**: When users report installation failures like "curl error 23" without sufficient information to diagnose the root cause
**Implementation**: Implement multi-level debug output with system information display, exact command logging, file operation tracing, disk space validation, and fallback directory management
**Impact**: Enables precise troubleshooting of installation issues with detailed context including system specs, disk space, file permissions, and command execution details

### System Administration - Corrected RUTOS_TEST_MODE Behavior (Date: 2025-07-27)

**Discovery**: Previous documentation incorrectly stated that RUTOS_TEST_MODE=1 causes early exit - this was wrong. According to our RUTOS Library System design, RUTOS_TEST_MODE enables trace logging only
**Context**: When testing remote installation scripts with RUTOS_TEST_MODE=1, the script should run normally with enhanced trace logging, not exit early
**Implementation**: RUTOS_TEST_MODE=1 enables trace logging, DRY_RUN=1 prevents actual changes - these are separate functions
**Impact**: Ensures RUTOS_TEST_MODE works as designed for enhanced debugging without preventing script execution

## Integration Notes

- Add new learnings immediately when discovered during development
- Cross-reference with existing patterns and solutions
- Update related instruction files when new patterns emerge
- Review and consolidate insights during major development milestones
