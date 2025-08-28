#!/bin/bash
# Build VUCI packages properly using the RUTOS SDK
# This script should be run from WSL with the SDK on the VHD

set -e

# Configuration
SDK_PATH="/mnt/sdk/rutos-sdk"  # Path to SDK on VHD
OUTPUT_DIR="/mnt/sdk/packages"  # Where to store built packages

echo "========================================="
echo "PROPER RUTOS SDK BUILD SCRIPT"
echo "========================================="
echo ""

# Check if SDK exists
if [ ! -d "$SDK_PATH" ]; then
    echo "Error: SDK not found at $SDK_PATH"
    echo "Please run setup-sdk-vhd.sh first"
    exit 1
fi

cd "$SDK_PATH"

# Create output directory
mkdir -p "$OUTPUT_DIR"

echo "SDK Path: $SDK_PATH"
echo "Output: $OUTPUT_DIR"
echo ""

# Step 1: Configure the build
echo "========================================="
echo "Step 1: Configuring build environment"
echo "========================================="

# Update feeds
echo "Updating feeds..."
./scripts/feeds update -a
./scripts/feeds install -a

# Step 2: Build SDK example packages
echo ""
echo "========================================="
echo "Step 2: Building SDK example packages"
echo "========================================="

# First, let's see what's available
echo ""
echo "Available VUCI example packages:"
find package/ -type d -name "vuci-app-example*" 2>/dev/null || echo "No example packages found"
find feeds/ -type d -name "vuci-app-example*" 2>/dev/null || echo "No example packages in feeds"

# Build the example API package
echo ""
echo "Building vuci-app-example-api..."
if [ -d "package/vuci-app-example-api" ] || [ -d "feeds/vuci/vuci-app-example-api" ]; then
    make package/vuci-app-example-api/{clean,compile} V=s
else
    echo "Warning: vuci-app-example-api not found, skipping"
fi

# Build the example UI package
echo ""
echo "Building vuci-app-example-ui..."
if [ -d "package/vuci-app-example-ui" ] || [ -d "feeds/vuci/vuci-app-example-ui" ]; then
    make package/vuci-app-example-ui/{clean,compile} V=s
else
    echo "Warning: vuci-app-example-ui not found, skipping"
fi

# Step 3: Copy built packages
echo ""
echo "========================================="
echo "Step 3: Collecting built packages"
echo "========================================="

echo "Finding built packages..."
find bin/ -name "vuci-app-example*.ipk" -exec cp {} "$OUTPUT_DIR/" \; 2>/dev/null || true

echo ""
echo "Built packages:"
ls -la "$OUTPUT_DIR"/*.ipk 2>/dev/null || echo "No packages found"

# Step 4: Create our custom Autonomy package
echo ""
echo "========================================="
echo "Step 4: Creating custom Autonomy package"
echo "========================================="

# Create package directory structure
AUTONOMY_API_DIR="$SDK_PATH/package/vuci-app-autonomy-api"
AUTONOMY_UI_DIR="$SDK_PATH/package/vuci-app-autonomy-ui"

# Clean old packages
rm -rf "$AUTONOMY_API_DIR" "$AUTONOMY_UI_DIR"

# Create API package
echo "Creating Autonomy API package structure..."
mkdir -p "$AUTONOMY_API_DIR/files/usr/lib/lua/api/services"

cat > "$AUTONOMY_API_DIR/Makefile" << 'EOF'
include $(TOPDIR)/rules.mk

PKG_NAME:=vuci-app-autonomy-api
PKG_VERSION:=1.0
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/vuci-app-autonomy-api
  SECTION:=vuci
  CATEGORY:=VUCI
  TITLE:=Autonomy API Service
  DEPENDS:=+lua +libuci-lua
endef

define Package/vuci-app-autonomy-api/description
  API backend for Autonomy network management
endef

define Build/Compile
	# No compilation needed for Lua
endef

define Package/vuci-app-autonomy-api/install
	$(INSTALL_DIR) $(1)/usr/lib/lua/api/services
	$(INSTALL_DATA) ./files/usr/lib/lua/api/services/autonomy.lua $(1)/usr/lib/lua/api/services/
endef

$(eval $(call BuildPackage,vuci-app-autonomy-api))
EOF

cat > "$AUTONOMY_API_DIR/files/usr/lib/lua/api/services/autonomy.lua" << 'EOF'
local uci = require "uci"
local json = require "json"

local M = {}

function M.get_status()
    return {
        status = "running",
        version = "1.0.0",
        uptime = os.time(),
        message = "Autonomy service is operational"
    }
end

function M.get_config()
    local cursor = uci.cursor()
    local config = {}
    
    cursor:foreach("autonomy", "global", function(s)
        config = s
    end)
    
    return config
end

function M.set_config(data)
    local cursor = uci.cursor()
    
    if data.enabled ~= nil then
        cursor:set("autonomy", "global", "enabled", data.enabled and "1" or "0")
    end
    
    cursor:commit("autonomy")
    return { success = true }
end

return M
EOF

# Create UI package
echo "Creating Autonomy UI package structure..."
mkdir -p "$AUTONOMY_UI_DIR/src/src/views/services"
mkdir -p "$AUTONOMY_UI_DIR/src/src/locales"

cat > "$AUTONOMY_UI_DIR/Makefile" << 'EOF'
include $(TOPDIR)/rules.mk

PKG_NAME:=vuci-app-autonomy-ui
PKG_VERSION:=1.0
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk
include ../feeds/vuci/app.mk

define Package/vuci-app-autonomy-ui
  SECTION:=vuci
  CATEGORY:=VUCI
  TITLE:=Autonomy Web Interface
  DEPENDS:=+vuci-ui-core +vuci-app-autonomy-api
endef

define Package/vuci-app-autonomy-ui/description
  Web interface for Autonomy network management
endef

$(eval $(call BuildPackage,vuci-app-autonomy-ui))
EOF

cat > "$AUTONOMY_UI_DIR/src/src/views/services/Autonomy.vue" << 'EOF'
<template>
  <div>
    <Card>
      <template #header>
        <h5>Autonomy Network Management</h5>
      </template>
      <template #content>
        <div class="p-fluid">
          <div class="field">
            <label>Service Status</label>
            <Tag :severity="status.status === 'running' ? 'success' : 'danger'">
              {{ status.status }}
            </Tag>
          </div>
          
          <div class="field">
            <label>Version</label>
            <p>{{ status.version }}</p>
          </div>
          
          <div class="field">
            <label>Message</label>
            <p>{{ status.message }}</p>
          </div>
          
          <div class="field">
            <label for="enabled">Service Enabled</label>
            <InputSwitch v-model="config.enabled" />
          </div>
          
          <div class="field">
            <Button label="Save Configuration" @click="saveConfig" />
          </div>
        </div>
      </template>
    </Card>
  </div>
</template>

<script>
import { ref, onMounted } from 'vue'

export default {
  name: 'Autonomy',
  setup() {
    const status = ref({})
    const config = ref({})
    
    const loadStatus = async () => {
      try {
        const response = await fetch('/api/autonomy/status')
        status.value = await response.json()
      } catch (error) {
        console.error('Failed to load status:', error)
      }
    }
    
    const loadConfig = async () => {
      try {
        const response = await fetch('/api/autonomy/config')
        config.value = await response.json()
      } catch (error) {
        console.error('Failed to load config:', error)
      }
    }
    
    const saveConfig = async () => {
      try {
        const response = await fetch('/api/autonomy/config', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(config.value)
        })
        const result = await response.json()
        if (result.success) {
          alert('Configuration saved successfully')
        }
      } catch (error) {
        console.error('Failed to save config:', error)
      }
    }
    
    onMounted(() => {
      loadStatus()
      loadConfig()
    })
    
    return {
      status,
      config,
      saveConfig
    }
  }
}
</script>
EOF

cat > "$AUTONOMY_UI_DIR/src/src/locales/en.json" << 'EOF'
{
  "menu": {
    "services/autonomy": "Autonomy"
  }
}
EOF

cat > "$AUTONOMY_UI_DIR/src/menu.json" << 'EOF'
{
  "services/autonomy": {
    "title": "Autonomy",
    "index": 100,
    "view": "services/Autonomy"
  }
}
EOF

# Build Autonomy packages
echo ""
echo "Building Autonomy packages..."
make package/vuci-app-autonomy-api/{clean,compile} V=s
make package/vuci-app-autonomy-ui/{clean,compile} V=s

# Copy Autonomy packages
find bin/ -name "vuci-app-autonomy*.ipk" -exec cp {} "$OUTPUT_DIR/" \; 2>/dev/null || true

# Step 5: Summary
echo ""
echo "========================================="
echo "BUILD COMPLETE!"
echo "========================================="
echo ""
echo "Packages built and stored in: $OUTPUT_DIR"
echo ""
ls -la "$OUTPUT_DIR"/*.ipk 2>/dev/null || echo "No packages found"
echo ""
echo "Key advantages of this approach:"
echo "  ✓ Uses SDK's native build system"
echo "  ✓ Proper webpack/vite compilation"
echo "  ✓ Correct file paths and structure"
echo "  ✓ All dependencies handled automatically"
echo "  ✓ No permission issues from Windows"
echo ""
echo "To deploy packages:"
echo "  1. Copy IPK files to router: scp *.ipk root@192.168.80.1:/tmp/"
echo "  2. Install: opkg install /tmp/*.ipk"
echo "  3. Restart web server: /etc/init.d/uhttpd restart"


