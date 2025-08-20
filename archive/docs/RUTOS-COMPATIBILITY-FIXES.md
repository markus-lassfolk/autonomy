# RUTOS Compatibility Fixes

**Version:** 2.6.0 | **Updated:** 2025-07-24

Version: 2.7.1

This document outlines the specific changes made to ensure compatibility with RUTOS embedded Linux environment.

## Background

RUTOS is a limited embedded Linux OS that may not have all standard command-line utilities or may have different
command-line options compared to full Linux distributions. Based on past issues with binary dependencies, the following
fixes were implemented.

## Fixed Issues

### 1. Binary Calculator (bc) Dependencies

**Problem**: `bc` calculator may not be available in RUTOS **Solution**:

- Made `bc` installation optional in `install_packages()`
- Replaced all `bc` mathematical operations with shell arithmetic and `awk`
- Added fallback calculations for floating-point operations

**Changes**:

- Line 317: Added comment about bc being optional
- Lines 330-339: Made bc installation non-failing
- Line 698-703: Replaced bc comparisons with shell arithmetic
- Line 827-828: Replaced bc divisions with awk calculations
- Line 1058-1059: Replaced bc divisions with awk calculations

### 2. Timeout Command Dependencies

**Problem**: `timeout` command may not exist in RUTOS **Solution**: Removed all `timeout` prefixes from grpcurl commands

**Changes**:

- Line 672: Removed `timeout 10` from grpcurl command
- Line 676: Removed `timeout 10` from grpcurl command
- Line 790: Removed `timeout 10` from grpcurl command
- Line 794: Removed `timeout 10` from grpcurl command
- Line 876: Removed `timeout 10` from grpcurl command
- Line 1040: Removed `timeout 10` from grpcurl command
- Line 1304: Removed `timeout 10` from grpcurl command

### 3. Stat Command Options

**Problem**: `stat` command flags may differ between systems (-c vs -f) **Solution**: Replaced all `stat` usage with
`wc -c` for file size detection

**Changes**:

- Line 985: Replaced `stat -f%z/-c%s` with `wc -c`
- Line 1067: Replaced `stat -f%z/-c%s` with `wc -c`
- Line 1354: Replaced `stat -f%z/-c%s` with `wc -c`

### 4. Curl Flag Compatibility

**Problem**: Some curl flags like `-fL` may not be supported **Solution**: Added progressive fallback for curl download
options

**Changes**:

- `install_binaries()`: Added curl flag detection and fallback
- First tries curl with `-L` flag, then falls back to basic curl

### 5. Architecture Detection

**Problem**: `uname -m` may behave differently or fail **Solution**: Added error handling and broader architecture
support

**Changes**:

- Line 146: Added error handling for uname command
- Added case statement for better architecture matching
- Supports "armv7l", "aarch64", and "arm" architectures

## RUTOS-Specific Optimizations

### Mathematical Operations

- All floating-point comparisons now use integer arithmetic (multiply by factors to avoid decimals)
- Throughput calculations use `awk` instead of `bc`
- Packet loss and latency comparisons use shell arithmetic

### File Operations

- File size detection uses `wc -c` (universal compatibility)
- Removed dependency on stat command variations

### Network Operations

- grpcurl timeouts rely on internal `max-time` parameter
- Simplified curl download logic with fallbacks

## Testing Recommendations

1. **Verify Binary Availability**:

   ```bash
   which bc timeout stat curl awk wc
   ```

2. **Test Mathematical Operations**:

   ```bash
   # Should work without bc
   echo "123.45" | awk '{printf "%.2f", $1 / 1000000}'
   ```

3. **Test File Size Detection**:

   ```bash
   # Should work universally
   wc -c < /etc/hosts
   ```

4. **Test Curl Downloads**:

   ```bash
   # Should fallback gracefully
   curl --help | grep -q "\-L" && echo "L flag supported" || echo "Basic curl only"
   ```

## Compatibility Status

✅ **Fixed**: bc mathematical operations ✅ **Fixed**: timeout command dependencies  
✅ **Fixed**: stat command variations ✅ **Fixed**: curl flag compatibility ✅ **Fixed**: uname architecture detection
✅ **Verified**: All changes maintain functionality while adding RUTOS compatibility

## Notes

- The only remaining `timeout` reference is in UCI configuration (`mwan3.@condition[1].timeout='1'`) which is a
  configuration value, not a command
- All mathematical operations now use portable shell arithmetic or awk
- File operations use the most basic and universal commands available
- Network operations rely on built-in timeouts rather than external timeout command
