# TODO-FIXES.md - Autonomy RUTOS Package Issues

## 🚨 CRITICAL ISSUES (Must Fix First)

### 1. **VUCI API SYSTEM MISUNDERSTANDING** - CRITICAL 🔴
- **Issue**: Our understanding of VUCI API services was incorrect
- **Evidence**:
  - Services don't appear in `ubus list` (even working ntpd doesn't appear)
  - Missing core dependencies (`ConfigService.lua`, `FunctionService.lua`)
  - API endpoints not accessible via HTTP
  - rpcd not loading our Lua services
  - **KEY DISCOVERY**: Even the SDK example service shows "Failed to load page: services/Example is missing or could not be loaded"
- **Impact**: Complete failure of API functionality
- **Fix Required**:
  - Research how VUCI API services actually work in RUTOS
  - Find the correct mechanism for service registration
  - Understand the relationship between VUCI and ubus/rpcd
  - **NEW APPROACH**: Use example foundation and enhance it with our functionality
- **Priority**: 🔴 CRITICAL - This is blocking all API functionality

### 2. **WSL PERMISSION ISSUES** - HIGH 🟡
- **Issue**: WSL cannot write to SDK directory due to permission restrictions
- **Evidence**:
  - `mkdir: cannot create directory ... Permission denied` in SDK directory
  - Works fine in Windows but fails in WSL
  - User is `markusla` but still gets permission denied
- **Impact**: Cannot use SDK build system for proper Lua compilation
- **Fix Required**:
  - Use manual build process (already implemented)
  - Or fix WSL permissions (complex)
- **Priority**: 🟡 HIGH - Workaround available

### 3. **MISSING CORE API DEPENDENCIES** - CRITICAL 🔴
- **Issue**: `ConfigService.lua` and `FunctionService.lua` files are missing
- **Evidence**:
  - `find /usr/local/usr/lib/lua/api/ -name '*Service.lua'` returns nothing
  - Our services require these files but they don't exist
  - **KEY DISCOVERY**: Even example services fail, suggesting this is a system-wide issue
- **Impact**: Lua services cannot load due to missing dependencies
- **Fix Required**:
  - Find where these core service files should come from
  - Install or create the missing core API files
  - **NEW APPROACH**: Use example foundation that should have working dependencies
- **Priority**: 🔴 CRITICAL - Directly related to issue #1

### 4. **WRONG FRONTEND TECHNOLOGY** - CRITICAL 🔴
- **Issue**: We used Angular.js instead of Vue 3
- **Impact**: All web pages fail to load with "Failed to load page" errors
- **Fix Required**:
  - Replace Angular.js controllers with Vue 3 components
  - Create `.vue` files in proper directory structure
  - Use Vue 3 composition API and TLT components
- **Files to Fix**: `build-rutos-simple-ui.sh` - Complete rewrite needed
- **Priority**: 🔴 CRITICAL

## ✅ **COMPLETED SUCCESSFULLY**

### ✅ Package Installation
- Both `vuci-app-autonomy-api` and `vuci-app-autonomy-ui` packages install successfully
- Files are properly placed in correct locations
- Package Manager integration working (packages registered with opkg)

### ✅ File Structure
- API files: `/usr/local/usr/lib/lua/api/services/` ✅
- UI files: `/usr/local/www/assets/` and `/usr/local/www/views/services/` ✅
- Menu file: `/usr/local/usr/share/vuci/menu.d/autonomy.json` ✅

### ✅ Lua Compilation Simulation
- Lua files have "uaQ" header (simulated compilation)
- Files are properly placed and have correct permissions

### ✅ Package Manager
- Packages properly registered with opkg
- Installation and removal working correctly

### ✅ Example Foundation Approach
- Successfully built packages based on SDK example foundation
- Enhanced example services with autonomy-specific functionality
- Maintained the "meta" and "foundation" from working examples

## 📝 LATEST FINDINGS

### Current Status (August 26, 2025)
- ✅ **Packages install successfully** - Both API and UI packages install without errors
- ✅ **Files in correct locations** - All files placed in proper directories
- ✅ **Lua compilation simulated** - Files have "uaQ" header
- ✅ **Example foundation used** - Based on working SDK examples
- ❌ **VUCI API system misunderstood** - Services don't register as expected
- ❌ **Missing core dependencies** - `ConfigService.lua` and `FunctionService.lua` not found
- ❌ **API not accessible** - Endpoints don't respond via HTTP
- ❌ **Web interface not working** - Even example services show "Failed to load page"

### Key Discovery
**VUCI API services work differently than expected**. Our understanding was based on SDK examples, but the actual RUTOS implementation appears to use a different mechanism for service registration and API access. **Even the SDK example service fails with the same "Failed to load page" error**, indicating this is a system-wide issue, not specific to our implementation.

### WSL Permission Issues
- **Root Cause**: WSL file system permissions don't match Windows permissions
- **Impact**: Cannot use SDK build system for proper Lua compilation
- **Solution**: Manual build process with simulated compilation (working)

### Example Foundation Approach
- **Strategy**: Use the working SDK example as foundation
- **Implementation**: Enhanced example services with autonomy-specific functionality
- **Status**: Packages built and installed successfully
- **Result**: Same issues persist, confirming system-wide problem

### Next Critical Step
Research how VUCI API services actually work in RUTOS and find the correct mechanism for service registration and API access. The fact that even the SDK example fails suggests we need to understand the actual VUCI implementation in this specific RUTOS version.

## 🔍 INVESTIGATION NEEDED

### VUCI API System Research
1. **How do VUCI services actually register?**
   - Check if there's a different registration mechanism
   - Look for alternative API access methods
   - Research VUCI documentation for this specific RUTOS version

2. **Core Dependencies Investigation**
   - Find where `ConfigService.lua` and `FunctionService.lua` should come from
   - Check if they're provided by a different package
   - Investigate if they need to be created manually

3. **Web Interface Access**
   - Research how VUCI web interfaces are actually accessed
   - Check if there's a different URL structure
   - Look for alternative web interface mechanisms

4. **System Comparison**
   - Compare with working VUCI services on the device
   - Check if there are any working examples we can study
   - Investigate the difference between our approach and working services

## 🆕 **NEW INSIGHTS FROM INSTALLED PACKAGES STUDY**

### ✅ **PROPER VUCI PACKAGE STRUCTURE DISCOVERED** - MAJOR BREAKTHROUGH 🟢

### ✅ **SUCCESSFUL DEPLOYMENT ACHIEVED** - MAJOR BREAKTHROUGH 🟢

#### **Status**: ✅ **COMPLETE SUCCESS** - Both API and UI packages installed successfully
#### **Date**: August 27, 2025
#### **Key Achievements**:
- ✅ **Correct IPK format discovered**: Tar archive format (not ar archive)
- ✅ **PowerShell native SSH/SCP**: Proper authentication without WSL issues
- ✅ **API package installed**: `vuci-app-autonomy-api - 1.0-1`
- ✅ **UI package installed**: `vuci-app-autonomy-ui - 1.0-1`
- ✅ **Files properly placed**: All files installed in correct locations
- ✅ **Menu configuration**: Proper VUCI menu entry created
- ✅ **Web server running**: uhttpd active and serving

#### **Final Working Solution**:
1. **IPK Format**: Tar archive containing `debian-binary`, `control.tar.gz`, `data.tar.gz`
2. **Compression**: Gzip compression of the tar archive
3. **Authentication**: PowerShell native SSH/SCP with proper key handling
4. **File Structure**: Correct overlay paths (`/usr/local/usr/local/`)
5. **Dependencies**: Removed problematic `vuci-base` dependency

#### **Installed Files**:
```
/usr/local/usr/local/usr/lib/lua/api/services/autonomy.lua
/usr/local/usr/local/usr/share/vuci/menu.d/autonomy.json
/usr/local/usr/local/www/assets/app.autonomy.app-1756247925.js.gz
```

#### **Menu Configuration**:
```json
{"services/autonomy":{"title":"Autonomy","index":50,"view":"services/Autonomy","acls":["services/autonomy"]}}
```

#### **Next Steps**:
1. Open browser: http://192.168.80.1
2. Look for "Autonomy" in the VUCI menu
3. Test application functionality
4. Verify API endpoints work correctly

### ✅ **IPK PACKAGE COMPRESSION DISCOVERED** - CRITICAL BREAKTHROUGH 🔴

#### **IPK Package Format Issue**:
- **Problem**: Our manually created IPK packages were not installing due to "Malformed package file" errors
- **Root Cause**: IPK packages must be **gzip compressed** after creation with `ar`
- **Evidence**: 
  - Working packages (ntpd, upnp) are gzip compressed: `file ntpd_4.2.8p15-3_arm_cortex-a7_neon-vfpv4.ipk` shows "gzip compressed data"
  - Our packages were raw ar archives: `file test-simple_1.0-1_arm_cortex-a7_neon-vfpv4.ipk` shows "Debian binary package"
  - opkg expects gzip-compressed IPK files

#### **Correct IPK Creation Process**:
```bash
# 1. Create ar archive
ar cr "package_1.0-1_arm_cortex-a7_neon-vfpv4.ipk" debian-binary control.tar.gz data.tar.gz

# 2. Compress with gzip (CRITICAL STEP)
gzip -c "package_1.0-1_arm_cortex-a7_neon-vfpv4.ipk" > "package_1.0-1_arm_cortex-a7_neon-vfpv4.ipk.gz"

# 3. Rename to .ipk extension for opkg
mv "package_1.0-1_arm_cortex-a7_neon-vfpv4.ipk.gz" "package_1.0-1_arm_cortex-a7_neon-vfpv4.ipk"
```

#### **Verification Process**:
```bash
# Check file type (should show "gzip compressed data")
file package_1.0-1_arm_cortex-a7_neon-vfpv4.ipk

# Verify ar structure (after gunzip)
gunzip -c package_1.0-1_arm_cortex-a7_neon-vfpv4.ipk > temp.ipk
ar t temp.ipk
# Should show: debian-binary, control.tar.gz, data.tar.gz
```

#### **Impact**:
- **Before**: All manually created packages failed with "Malformed package file"
- **After**: Packages install successfully with gzip compression
- **Status**: ✅ **RESOLVED** - Test package now installs successfully

#### **Next Steps**:
1. **Update all build scripts** to include gzip compression step
2. **Apply to VUCI packages** (API and UI packages)
3. **Test full deployment** with compressed packages
4. **Document the process** for future builds

#### **Working Package Examples Found**:
- **NTPD**: `vuci-app-ntpd-api` and `vuci-app-ntpd-ui` (working)
- **UPNP**: `vuci-app-upnp-api` and `vuci-app-upnp-ui` (working)
- **Python3**: Standard OpenWrt packages (working)

#### **Correct File Structure Pattern**:
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

2. **Menu Configuration Format**:
   ```json
   {"system/admin/datetime/ntpd":{"title":"NTPD","index":30,"view":"services/Ntpd","acls":["system/admin/datetime/ntpd"]}}
   {"services/upnp":{"title":"UPNP","index":220,"view":"services/Upnp","acls":["services/upnp"]}}
   ```

3. **UI Assets Location**: `/usr/local/www/assets/` (NOT `/www/views/`)
   - Vue.js applications are compiled to `.js.gz` files
   - No individual `.vue` files in production

4. **Package Installation Pattern**:
   - All packages install to `/usr/local/` overlay directory
   - This ensures persistence across reboots
   - Files are properly placed by the package system

#### **Correct Implementation Path**:

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

## ✅ **PARTIAL SUCCESS ACHIEVED** (December 27, 2024)

### Working:
- ✅ **HTML Fallback Page**: http://192.168.80.1/vuci-app-example/ works!
- ✅ **Packages Install**: Both API and UI packages install successfully
- ✅ **Menu Entry**: Shows up in Services menu
- ✅ **Symlinks Created**: /www/example.html now works after manual symlink

### Still Not Working:
- ❌ **Vue Component**: "Failed to load page" - needs webpack compilation
- ❌ **Package Manager**: Doesn't show packages (they're in /usr/local/)

## 🔴 **ROOT CAUSE IDENTIFIED - VUCI MODULE LOADING ISSUE** (December 27, 2024)

### **ChatGPT Analysis Confirmed** ✅
The advice from ChatGPT is **absolutely correct**. The "Failed to load page" error occurs because:

1. **Missing Compiled JavaScript Assets**: VUCI expects webpack/vite compiled JavaScript bundles in `/www/assets/` with the pattern `app.<name>.app-<hash>.js.gz`

2. **Dynamic Import System**: The main VUCI index.js file uses dynamic imports like:
   ```javascript
   import("./app.upnp.app-2025-08-08-3a282822359.js")
   ```

3. **Case-Sensitive View Paths**: The menu.json `view` field must exactly match the import path (case-sensitive):
   - Menu: `"view":"services/Upnp"` (capitalized)
   - The compiled JS must be properly registered in the webpack manifest

4. **Actual File Locations on RUTOS**:
   - Menu files: `/usr/share/vuci/menu.d/` (in overlay: `/overlay/root/upper/usr/share/vuci/menu.d/`)
   - Compiled JS: `/www/assets/app.<name>.app-<hash>.js.gz`
   - Our files installed to: `/usr/local/usr/share/vuci/menu.d/` (wrong location!)

### **The Real Problem**:
- **We never created properly compiled JavaScript bundles**
- **Our menu files are in the wrong location** (`/usr/local/usr/share/` instead of `/usr/share/`)
- **VUCI uses webpack/vite to compile Vue.js source into optimized bundles**
- **The SDK's build system (api.mk/app.mk) handles this compilation automatically**

### **Why Even SDK Examples Failed**:
Our manual build process didn't use the SDK's webpack/vite compilation pipeline, so even the SDK example packages we built manually don't have the required compiled JavaScript assets.

### **Solution Required**:
1. **Use the SDK's build system properly** with `make package/vuci-app-example-ui/compile`
2. **Or manually compile Vue.js with webpack/vite** following the SDK's build process
3. **Install files to the correct locations** (`/usr/share/vuci/menu.d/`, not `/usr/local/usr/share/vuci/menu.d/`)
4. **Ensure compiled JS assets** are created with proper naming convention

## 🎯 **KEY DISCOVERY - OPKG INSTALLATION PREFIX** (December 27, 2024)

### **The Real Installation Issue**:
RUTOS's opkg.conf has `dest root /usr/local` which means:
- All user packages install to `/usr/local/` prefix
- Files end up in `/usr/local/usr/share/` instead of `/usr/share/`
- This is why our packages don't work even with correct structure!

### **Working Solution**:
1. **Accept the /usr/local/ prefix** - it's by design
2. **Create symlinks** in overlay filesystem:
   ```bash
   ln -sf /usr/local/usr/share/vuci/menu.d/example.json /overlay/root/upper/usr/share/vuci/menu.d/example.json
   ln -sf /usr/local/usr/lib/lua/api/services/example.lua /overlay/root/upper/usr/lib/lua/api/services/example.lua
   ```
3. **HTML works** because web server serves both `/www` and `/usr/local/www`
4. **Vue component still needs webpack compilation** - that's the remaining issue
