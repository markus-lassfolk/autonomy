# Version Management System

This document explains how to manage version numbers across all components of the Autonomy project.

## Overview

The Autonomy project uses a centralized version management system that ensures all components (packages, code, headers) stay synchronized with the same version numbers.

## Files Involved

### 1. `VERSION` (Root Configuration)
The main version configuration file that contains all version information:
```bash
AUTONOMY_VERSION_MAJOR=1
AUTONOMY_VERSION_MINOR=0
AUTONOMY_VERSION_PATCH=0
AUTONOMY_VERSION_BUILD=1

AUTONOMY_VERSION=1.0.0
AUTONOMY_VERSION_FULL=1.0.0-1
```

### 2. `src/c/shared/autonomy_version.h` (C Header)
C header file that defines version macros for use in C code:
```c
#define AUTONOMY_VERSION_MAJOR    1
#define AUTONOMY_VERSION_MINOR    0
#define AUTONOMY_VERSION_PATCH    0
#define AUTONOMY_VERSION_BUILD    1
#define AUTONOMY_VERSION          "1.0.0"
#define AUTONOMY_VERSION_FULL     "1.0.0-1"
```

### 3. Makefiles (Package Configuration)
All package Makefiles include the VERSION file and use the centralized version:
```makefile
include $(CURDIR)/../VERSION
PKG_VERSION:=$(AUTONOMY_VERSION)
PKG_RELEASE:=$(AUTONOMY_VERSION_BUILD)
```

## Version Management Script

The `tools/update_version.py` script provides an easy way to update versions across all components.

### Usage Examples

#### Show Current Version
```bash
python tools/update_version.py --show
```

#### Update Version
```bash
# Update to a new version
python tools/update_version.py 1.1.0

# Update with specific build number
python tools/update_version.py 1.0.1 --build 2

# Update and create git tag
python tools/update_version.py 2.0.0 --tag

# Update, commit, and tag
python tools/update_version.py 1.2.0 --commit --tag --message "Release 1.2.0"
```

### Version Format

The script supports these version formats:
- `1.0.0` - Standard semantic version
- `1.0.0-1` - Version with build number
- `1.0.0.1` - Alternative format with extra number

## Manual Version Updates

If you need to update versions manually:

### 1. Update VERSION file
Edit the `VERSION` file in the project root:
```bash
AUTONOMY_VERSION_MAJOR=1
AUTONOMY_VERSION_MINOR=1
AUTONOMY_VERSION_PATCH=0
AUTONOMY_VERSION_BUILD=1
AUTONOMY_VERSION=1.1.0
AUTONOMY_VERSION_FULL=1.1.0-1
```

### 2. Update C header
Edit `src/c/shared/autonomy_version.h`:
```c
#define AUTONOMY_VERSION_MAJOR    1
#define AUTONOMY_VERSION_MINOR    1
#define AUTONOMY_VERSION_PATCH    0
#define AUTONOMY_VERSION_BUILD    1
#define AUTONOMY_VERSION          "1.1.0"
#define AUTONOMY_VERSION_FULL     "1.1.0-1"
```

## Using Versions in Code

### C Code
Include the version header and use the macros:
```c
#include "autonomy_version.h"

// Use version string
printf("Version: %s\n", AUTONOMY_VERSION);

// Use full version with build
printf("Full Version: %s\n", AUTONOMY_VERSION_FULL);

// Use individual components
printf("Major: %d, Minor: %d, Patch: %d\n", 
       AUTONOMY_VERSION_MAJOR, 
       AUTONOMY_VERSION_MINOR, 
       AUTONOMY_VERSION_PATCH);

// Get runtime version info
const autonomy_version_info_t *version = autonomy_get_version_info();
printf("Version: %s\n", version->version_string);
```

### Makefiles
Use the version variables from the VERSION file:
```makefile
include $(CURDIR)/../VERSION

PKG_NAME:=my-package
PKG_VERSION:=$(AUTONOMY_VERSION)
PKG_RELEASE:=$(AUTONOMY_VERSION_BUILD)
```

## Version Synchronization

The system ensures that these components stay synchronized:

1. **Package Versions** - All Makefiles use `$(AUTONOMY_VERSION)`
2. **C Code Versions** - All C files use `AUTONOMY_VERSION` macros
3. **Build Numbers** - All components use `$(AUTONOMY_VERSION_BUILD)`
4. **Git Tags** - Optional git tags can be created with `--tag`

## Best Practices

1. **Always use the script** - Use `tools/update_version.py` instead of manual updates
2. **Test after updates** - Verify that all components show the same version
3. **Commit changes** - Use `--commit` to commit version changes
4. **Create tags** - Use `--tag` for releases to create git tags
5. **Semantic versioning** - Follow semantic versioning principles (MAJOR.MINOR.PATCH)

## Troubleshooting

### Version Mismatch
If you see different versions in different places:
1. Run `python tools/update_version.py --show` to check current state
2. Use the script to update to the desired version
3. Verify all components show the same version

### Build Issues
If the build system doesn't pick up version changes:
1. Check that Makefiles include the VERSION file correctly
2. Verify the VERSION file syntax is correct
3. Clean and rebuild the packages

### C Code Issues
If C code doesn't see version updates:
1. Check that `autonomy_version.h` is included
2. Verify the header file was updated correctly
3. Rebuild the C code

## Examples

### Release Workflow
```bash
# 1. Update version for release
python tools/update_version.py 1.2.0 --commit --tag --message "Release 1.2.0"

# 2. Build packages
make package/feeds/autonomy/autonomy-daemon/compile

# 3. Verify version in built package
opkg info autonomy-daemon
```

### Development Workflow
```bash
# 1. Update version for development build
python tools/update_version.py 1.2.0 --build 2

# 2. Build and test
make package/feeds/autonomy/autonomy-daemon/compile

# 3. Check version in daemon
ubus call autonomy status
```
