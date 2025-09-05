# C Code Verification Tool

A comprehensive tool for verifying C code files to detect errors, missing references, duplicates, and other build-breaking issues.

## Features

### 🔍 **Syntax Error Detection**
- Uses GCC/Clang compiler to detect syntax errors
- Comprehensive error reporting with line numbers
- Support for strict compilation flags

### 📁 **Include File Verification**
- Checks for missing header files
- Detects circular dependencies
- Validates include paths

### 🔄 **Duplicate Detection**
- Finds duplicate function definitions
- Detects duplicate struct/enum definitions
- Identifies conflicting symbol definitions

### 🧹 **Unused Code Detection**
- Finds unused functions and variables
- Identifies unused includes
- Suggests code cleanup opportunities

### 🛡️ **Security & Safety Checks**
- Detects potentially unsafe functions (strcpy, sprintf, etc.)
- Identifies potential buffer overflows
- Checks for uninitialized variables

### 💾 **Memory Management**
- Tracks malloc/free pairs
- Detects potential memory leaks
- Validates memory allocation patterns

### 🔧 **Build Compatibility**
- Checks for platform-specific code
- Validates standard library usage
- Ensures portable code practices

## Installation

### Prerequisites
- **Python 3.6+** - Required for the verification script
- **GCC or Clang** - Required for syntax checking
- **PowerShell 5.1+** (Windows) - For PowerShell wrapper

### Windows Installation

1. **Install Python**:
   ```powershell
   # Using winget
   winget install Python.Python.3.11
   
   # Or download from https://www.python.org/downloads/
   ```

2. **Install a C Compiler**:
   ```powershell
   # Option 1: MinGW-w64 (includes GCC)
   winget install MSYS2.MSYS2
   
   # Option 2: Visual Studio Build Tools (includes Clang)
   winget install Microsoft.VisualStudio.2022.BuildTools
   
   # Option 3: WSL with build-essential
   wsl --install
   wsl -d Ubuntu -e bash -c "sudo apt update && sudo apt install build-essential"
   ```

3. **Verify Installation**:
   ```powershell
   python --version
   gcc --version  # or clang --version
   ```

## Usage

### PowerShell (Recommended for Windows)

```powershell
# Basic verification
.\tools\verify_c_code.ps1

# Verbose output with strict checking
.\tools\verify_c_code.ps1 -Verbose -Strict

# Verify specific directory
.\tools\verify_c_code.ps1 -Directory "src\c\autonomy-daemon"

# Exclude test files
.\tools\verify_c_code.ps1 -Exclude "*.test.c" -Exclude "test_*.c"

# Save report to file
.\tools\verify_c_code.ps1 -Output "verification_report.txt"
```

### Batch File (Windows CMD)

```cmd
REM Basic verification
tools\verify_c_code.bat

REM With options
tools\verify_c_code.bat -d src\c -v -s -o report.txt
```

### Direct Python Usage

```bash
# Basic verification
python tools/verify_c_code.py

# With options
python tools/verify_c_code.py -v -s -o report.txt src/c

# Exclude patterns
python tools/verify_c_code.py -e "*.test.c" -e "test_*.c"
```

## Command Line Options

| Option | Description | Example |
|--------|-------------|---------|
| `-v, --verbose` | Enable verbose output | `-v` |
| `-s, --strict` | Enable strict checking | `-s` |
| `-o, --output FILE` | Save report to file | `-o report.txt` |
| `-e, --exclude PATTERN` | Exclude files matching pattern | `-e "*.test.c"` |
| `-i, --include PATTERN` | Only include files matching pattern | `-i "*.c"` |
| `-h, --help` | Show help message | `-h` |

## Output Format

The tool generates a comprehensive report with the following sections:

### 📊 Summary
- Total number of issues found
- Breakdown by severity (Critical, Error, Warning, Info)

### 🚨 Critical Issues
- Issues that will definitely break the build
- Missing critical dependencies
- Syntax errors

### ❌ Errors
- Issues that will likely cause build failures
- Missing header files
- Duplicate definitions

### ⚠️ Warnings
- Issues that may cause problems
- Unused code
- Potential security issues

### ℹ️ Info
- Suggestions for code improvement
- Best practice recommendations

## Example Output

```
🔍 C Code Verification Report
==================================================

📊 Summary:
  Total issues: 12
  Critical: 2
  Errors: 4
  Warnings: 5
  Info: 1

🚨 CRITICAL (2)
------------------------------
  src/c/autonomy-daemon/gps_manager.h:15
    Missing include file: ubus.h
    💡 Check if ubus.h exists in include paths

❌ ERROR (4)
------------------------------
  src/c/starlink-tracking/core/main.c:45
    Duplicate function definition: signal_handler
    💡 Function defined in multiple files: main.c:45, test_main.c:23

⚠️ WARNING (5)
------------------------------
  src/c/autonomy-daemon/network_controller.h:78
    Unused function: discover_network_interfaces
    💡 Remove unused function or mark as static
```

## Integration with Build Systems

### OpenWrt/RUTOS Integration

The tool is designed to work with OpenWrt build systems. It can be integrated into your Makefile:

```makefile
# Add to your Makefile
define Build/Verify
	$(PYTHON) $(PKG_BUILD_DIR)/tools/verify_c_code.py \
		-v -s -o $(PKG_BUILD_DIR)/verification_report.txt \
		$(PKG_BUILD_DIR)/src/c
endef

# Call before compilation
define Build/Compile
	$(call Build/Verify)
	$(MAKE) -C $(PKG_BUILD_DIR) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)"
endef
```

### CI/CD Integration

Add to your GitHub Actions workflow:

```yaml
- name: Verify C Code
  run: |
    python tools/verify_c_code.py -v -s -o verification_report.txt
    if [ $? -ne 0 ]; then
      echo "C code verification failed"
      cat verification_report.txt
      exit 1
    fi
```

## Troubleshooting

### Common Issues

1. **"Python not found"**
   - Install Python 3.6+ and ensure it's in PATH
   - On Windows, check "Add Python to PATH" during installation

2. **"No C compiler found"**
   - Install GCC (MinGW-w64) or Clang
   - Ensure compiler is in PATH
   - On Windows, you may need to restart your terminal

3. **"Permission denied"**
   - Run PowerShell as Administrator if needed
   - Check file permissions in the project directory

4. **"Module not found"**
   - Ensure you're running from the project root directory
   - Check that all required files are present

### Performance Tips

- Use `-e "*.test.c"` to exclude test files for faster verification
- Use `-i "*.c"` to only check C files (skip headers)
- Run on specific directories instead of the entire codebase

## Contributing

To improve the verification tool:

1. **Add new checks** in the `CCodeVerifier` class
2. **Improve pattern matching** in the parsing methods
3. **Add support for more compilers** in syntax checking
4. **Enhance reporting** with better formatting and suggestions

## License

This tool is part of the Autonomy project and follows the same license terms.

## Support

For issues or questions:
1. Check the troubleshooting section above
2. Review the verbose output (`-v` flag)
3. Create an issue in the project repository
4. Check the generated report file for detailed information
