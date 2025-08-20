# Infinite Loop Fix for RUTOS Configuration Processing

## Problem Identified
The infinite loop in `scripts/install-rutos.sh` is caused by sed commands missing the `\1` capture group replacement. This results in empty variable names being processed repeatedly.

**Problematic lines:**
- Line 957: `sed 's/^export \([^=]*\)=.*//'` (missing `\1`)
- Line 959: `sed 's/^\([^=]*\)=.*//'` (missing `\1`)

## Manual Fix Required

Since automated replacement is challenging due to complex escaping, here's the manual fix:

### Fix Line 957:
Change:
```bash
var_name=$(echo "$template_line" | sed 's/^export \([^=]*\)=.*//')
```

To:
```bash
var_name=$(echo "$template_line" | sed 's/^export \([^=]*\)=.*/\1/')
```

### Fix Line 959:
Change:
```bash
var_name=$(echo "$template_line" | sed 's/^\([^=]*\)=.*//')
```

To:
```bash
var_name=$(echo "$template_line" | sed 's/^\([^=]*\)=.*/\1/')
```

### Add Validation (after line 960):
Insert after the `fi` on line 960:
```bash

# Critical fix: Validate variable name to prevent infinite loop
if [ -z "$var_name" ] || ! echo "$var_name" | grep -q "^[A-Za-z_][A-Za-z0-9_]*$"; then
    config_debug "Skipping invalid/empty variable name in line: $template_line"
    continue
fi
```

## Root Cause
The sed patterns capture groups but don't use the `\1` replacement, resulting in:
1. Empty variable names extracted
2. Infinite loop processing empty variables
3. Continuous "Processing template variable: " messages

## Testing
After the fix, run:
```bash
DEBUG=1 CONFIG_FILE=/etc/starlink-config/config.sh ./scripts/install-rutos.sh
```

The script should no longer loop infinitely on empty variable names.
