# RUTOS SDK Quick Reference

## 🚨 CRITICAL UPDATE (December 27, 2024) 🚨
**ROOT CAUSE IDENTIFIED**: VUCI requires webpack/vite compiled JavaScript bundles. Manual package creation without proper compilation will fail with "Failed to load page" errors.

## 🎯 Key Technologies & Architecture

### Core Technologies
- **Frontend**: Vue 3 JavaScript framework (compiled with webpack/vite)
- **Backend**: Lua 5.1 programming language
- **Web Interface**: VUCI (Vue-based, replacing LuCI)
- **Package System**: OpenWrt with VUCI extensions
- **Build System**: SDK with api.mk/app.mk for automatic compilation

### VUCI Module Loading System
- **Dynamic Imports**: Main index.js dynamically imports compiled modules
- **Naming Convention**: `app.<name>.app-<hash>.js.gz` in `/www/assets/`
- **Case Sensitive**: View paths must match exactly (e.g., `services/Example`)
- **Compilation Required**: Vue source must be compiled to optimized bundles

### Package Structure
- **API Package**: `vuci-app-{name}-api` - Backend Lua services
- **UI Package**: `vuci-app-{name}-ui` - Frontend Vue components (requires compilation)
- **Both packages must match names** (e.g., `vuci-app-autonomy-api` and `vuci-app-autonomy-ui`)

## 📁 File Structure Requirements

### ⚠️ CRITICAL: Installation Path Reality
- **opkg installs to**: `/usr/local/` prefix (configured in `/etc/opkg.conf`)
- **Actual paths after installation**:
  - Menu: `/usr/local/usr/share/vuci/menu.d/`
  - API: `/usr/local/usr/lib/lua/api/services/`
  - HTML: `/usr/local/www/` (this works directly)
- **VUCI expects files in**:
  - Menu: `/usr/share/vuci/menu.d/`
  - API: `/usr/lib/lua/api/services/`
- **Solution**: Create symlinks in overlay filesystem or modify package postinst scripts

### API Package Structure
```
vuci-app-{name}-api/
├── Makefile
└── files/
    ├── etc/
    └── usr/
        └── lib/
            └── lua/
                └── api/
                    └── services/
                        ├── config_{name}.lua    # UCI configuration service
                        └── function_{name}.lua  # Function/action service
```

### UI Package Structure
```
vuci-app-{name}-ui/
├── Makefile
├── files/
│   └── usr/
│       └── share/
│           └── vuci/
│               └── menu.d/
│                   └── {name}.json              # Menu configuration
└── src/
    └── src/
        └── views/
            └── services/
                ├── {Name}.vue                   # Main view (capitalized)
                └── {Name}Edit.vue               # Edit form (optional)
```

## 🔧 Build System Integration

### SDK Build Process
1. **Copy packages to SDK**: `cp "vuci-app-{name}-api" "vuci-app-{name}-ui" "RUTX_R_GPL_00.07.17.1/package/feeds/vuci/"`
2. **Configure with menuconfig**: Navigate to **VuCI → Applications/UI**
3. **Build packages**: `make package/vuci-app-{name}-ui/{clean,compile}`
4. **Package Manager integration**: Edit `ipk_packages.json`

### Package Manager Integration
```json
{
  "vuci-app-{name}-ui": {
    "name": "{name}_app",
    "vuci_dep": "vuci-app-{name}-api"
  }
}
```

## 🌐 Web Interface Components

### Menu Configuration (JSON)
```json
{
  "services/{name}": {
    "title": "{Title}",
    "index": 50,
    "view": "services/{Name}",
    "acls": ["services/{name}"]
  }
}
```

### Vue Component Requirements
- **File naming**: Must match menu "view" field (e.g., "services/Autonomy" → "Autonomy.vue")
- **Component structure**: Use Vue 3 composition API
- **UI components**: Use `tlt-*` and `vuci-*` components
- **API calls**: Use `this.$axios` for HTTP requests

### API Endpoints
- **Config API**: `/api/{name}_c/config` (UCI configuration)
- **Function API**: `/api/{name}_f/{function}` (Custom functions)
- **Actions**: `/api/{name}_f/actions/{action}` (POST actions)

## 🔌 Service Integration

### Lua API Services
- **ConfigService**: For UCI configuration management
- **FunctionService**: For custom functions and actions
- **Validation**: Built-in validation with custom hooks
- **Error handling**: Use `self:add_error()` and `self:add_critical_error()`

### Vue Frontend Integration
- **UCI forms**: Use `vuci-form` and `vuci-typed-section`
- **API calls**: Use `this.$axios.get()` and `this.$axios.post()`
- **Error handling**: Use `this.$message.error()` and `this.$spin()`

## 📦 Package Installation

### Manual Installation
```bash
opkg install vuci-app-{name}-api_1_ipq40xx.ipk
opkg install vuci-app-{name}-ui_1_ipq40xx.ipk
```

### Package Manager Integration
- Build with `make pm`
- Packages available in `bin/packages/<arch_name>/zipped_packages`
- Install via Package Manager web interface

## 🔧 **MANUAL IPK CREATION** (When SDK Build System Fails)

### ✅ **CRITICAL DISCOVERY**: IPK Packages Must Be Gzip Compressed

#### **Problem**:
- Manually created IPK packages fail with "Malformed package file" errors
- opkg expects gzip-compressed IPK files, not raw ar archives

#### **Correct IPK Creation Process**:
```bash
# 1. Create control file
cat > control << EOF
Package: package-name
Version: 1.0-1
Depends: dependencies
Section: vuci
Architecture: arm_cortex-a7_neon-vfpv4
Installed-Size: 1024
Description: Package description
EOF

# 2. Create postinst script
cat > postinst << 'EOF'
#!/bin/sh
# Post-installation script
exit 0
EOF
chmod +x postinst

# 3. Create debian-binary
echo "2.0" > debian-binary

# 4. Create data directory structure
mkdir -p data/usr/local/usr/lib/lua/api/services/
# ... add your files ...

# 5. Create tar archives
tar -czf control.tar.gz --owner=root --group=root control postinst
tar -czf data.tar.gz --owner=root --group=root -C data .

# 6. Create ar archive
ar cr "package_1.0-1_arm_cortex-a7_neon-vfpv4.ipk" debian-binary control.tar.gz data.tar.gz

# 7. CRITICAL: Compress with gzip
gzip -c "package_1.0-1_arm_cortex-a7_neon-vfpv4.ipk" > "package_1.0-1_arm_cortex-a7_neon-vfpv4.ipk.gz"

# 8. Rename to .ipk extension for opkg
mv "package_1.0-1_arm_cortex-a7_neon-vfpv4.ipk.gz" "package_1.0-1_arm_cortex-a7_neon-vfpv4.ipk"
```

#### **Verification**:
```bash
# Check file type (should show "gzip compressed data")
file package_1.0-1_arm_cortex-a7_neon-vfpv4.ipk

# Verify ar structure (after gunzip)
gunzip -c package_1.0-1_arm_cortex-a7_neon-vfpv4.ipk > temp.ipk
ar t temp.ipk
# Should show: debian-binary, control.tar.gz, data.tar.gz
```

#### **Key Points**:
- **Working packages** are gzip compressed: `file ntpd_*.ipk` shows "gzip compressed data"
- **Our packages** were raw ar archives: `file test-simple_*.ipk` showed "Debian binary package"
- **opkg expects** gzip-compressed IPK files
- **Solution**: Always compress with gzip after creating ar archive

## 🚨 Critical Issues with Our Implementation

### 1. Wrong Frontend Technology
- **We used**: Angular.js
- **Should use**: Vue 3
- **Impact**: Pages fail to load because VUCI expects Vue components

### 2. Missing API Package
- **We created**: Only UI package
- **Missing**: API package with Lua services
- **Impact**: No backend functionality for web interface

### 3. Wrong File Structure
- **We used**: Angular.js controllers in JavaScript
- **Should use**: Vue 3 components in `.vue` files
- **Impact**: VUCI can't find the view components

### 4. Missing Build System Integration
- **We used**: Manual IPK creation
- **Should use**: SDK build system
- **Impact**: No proper integration with Package Manager

### 5. Wrong Menu Configuration
- **We used**: Generic view names
- **Should use**: Specific Vue component names
- **Impact**: VUCI can't resolve view references

## ✅ Correct Implementation Path

1. **Create API package** with Lua services
2. **Create UI package** with Vue 3 components
3. **Use SDK build system** for proper integration
4. **Follow naming conventions** exactly
5. **Test with SDK examples** first

## 🆕 **NEW INSIGHTS FROM WORKING PACKAGES STUDY**

### ✅ **PROPER VUCI PACKAGE STRUCTURE DISCOVERED** - MAJOR BREAKTHROUGH 🟢

#### **Working Package Examples Analyzed**:
- **NTPD**: `vuci-app-ntpd-api` and `vuci-app-ntpd-ui` ✅
- **UPNP**: `vuci-app-upnp-api` and `vuci-app-upnp-ui` ✅
- **Python3**: Standard OpenWrt packages ✅

#### **Correct File Structure Pattern** (ACTUAL PRODUCTION):

```
/usr/local/usr/lib/lua/api/services/
├── ntpd.lua                    # API service file
├── upnp_settings.lua           # API service file
├── upnp_acls.lua               # API service file
└── upnp_redirects.lua          # API service file

/usr/local/usr/share/vuci/menu.d/
├── ntpd.json                   # Menu configuration
└── upnp.json                   # Menu configuration

/usr/local/www/assets/
├── app.ntpd.app-*.js.gz        # Compiled Vue.js application
└── app.upnp.app-*.js.gz        # Compiled Vue.js application
```

#### **Key Discoveries**:

1. **API Services Location**: `/usr/local/usr/lib/lua/api/services/` (NOT `/usr/lib/lua/api/services/`)
   - All working services are in the `/usr/local/` overlay directory
   - This explains why our files weren't being found

2. **Menu Configuration Format** (ACTUAL WORKING FORMAT):
   ```json
   {"system/admin/datetime/ntpd":{"title":"NTPD","index":30,"view":"services/Ntpd","acls":["system/admin/datetime/ntpd"]}}
   {"services/upnp":{"title":"UPNP","index":220,"view":"services/Upnp","acls":["services/upnp"]}}
   ```

3. **UI Assets Location**: `/usr/local/www/assets/` (NOT `/www/views/`)
   - Vue.js applications are compiled to `.js.gz` files
   - No individual `.vue` files in production
   - Files are minified and compressed

4. **Package Installation Pattern**:
   - All packages install to `/usr/local/` overlay directory
   - This ensures persistence across reboots
   - Files are properly placed by the package system

#### **Correct Implementation Path** (UPDATED):

1. **API Package Structure**:
   ```
   vuci-app-{name}-api/
   ├── Makefile (include ../api.mk)
   └── files/
       └── usr/
           └── lib/
               └── lua/
                   └── api/
                       └── services/
                           └── {name}.lua
   ```

2. **UI Package Structure**:
   ```
   vuci-app-{name}-ui/
   ├── Makefile (include ../app.mk)
   ├── files/
   │   └── usr/
   │       └── share/
   │           └── vuci/
   │               └── menu.d/
   │                   └── {name}.json
   └── src/
       └── src/
           └── views/
               └── services/
                   └── {Name}.vue
   ```

3. **Build System Integration**:
   - Use SDK's `api.mk` and `app.mk` for proper build integration
   - These handle all the complex build logic automatically
   - Files are automatically placed in correct locations

#### **Menu Configuration Examples** (FROM WORKING PACKAGES):

**NTPD Menu**:
```json
{"system/admin/datetime/ntpd":{"title":"NTPD","index":30,"view":"services/Ntpd","acls":["system/admin/datetime/ntpd"]}}
```

**UPNP Menu**:
```json
{"services/upnp":{"title":"UPNP","index":220,"view":"services/Upnp","acls":["services/upnp"]}}
```

#### **API Service Examples** (FROM WORKING PACKAGES):

**NTPD Service** (`/usr/local/usr/lib/lua/api/services/ntpd.lua`):
- Simple Lua service file
- No complex dependencies
- Basic functionality

**UPNP Services**:
- `upnp_settings.lua` - Configuration management
- `upnp_acls.lua` - Access control lists
- `upnp_redirects.lua` - Port forwarding

#### **Next Steps**:
1. **Create proper VUCI packages** following the exact pattern of working packages
2. **Use SDK build system** with `api.mk` and `app.mk`
3. **Place files in correct overlay locations** (`/usr/local/`)
4. **Follow the exact menu configuration format** used by working packages
5. **Use Vue.js compilation** for UI assets (not individual .vue files)

#### **Critical Fix Required**:
- **Replace our manual overlay approach** with proper SDK package system
- **Use the exact file structure** of working packages (ntpd, upnp)
- **Follow the menu configuration format** exactly as used by working packages
- **Leverage the SDK's build system** instead of manual IPK creation
- **✅ ADDED**: **Use gzip compression** for all manually created IPK packages

This discovery provides the **correct foundation** for building working VUCI packages that will actually appear in the UI and function properly.

### ✅ **IPK PACKAGE COMPRESSION ISSUE RESOLVED** - MAJOR BREAKTHROUGH 🟢

#### **Status**: ✅ **RESOLVED** - Test package now installs successfully
#### **Impact**: All manually created packages can now be installed properly
#### **Next Action**: Apply gzip compression to VUCI packages and test full deployment

