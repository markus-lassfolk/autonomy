#!/bin/bash
# Build VUCI packages using the properly mounted SDK on /mnt/wsl/SDK
# This script uses the correct SDK environment with working toolchain

set -e

echo "========================================="
echo "BUILDING VUCI PACKAGES WITH PROPER SDK"
echo "========================================="
echo ""

# SDK Configuration
SDK_BASE="/mnt/wsl/SDK"
SDK_DIR="$SDK_BASE/src/rutos-ipq40xx-rutx-sdk"

if [ ! -d "$SDK_DIR" ]; then
    echo "Error: SDK not found at $SDK_DIR"
    echo "Available SDKs:"
    ls -la "$SDK_BASE/src/" 2>/dev/null
    exit 1
fi

cd "$SDK_DIR"
echo "Working in SDK: $(pwd)"
echo "Disk usage: $(df -h /mnt/wsl/SDK | tail -1)"
echo ""

# Clean up problematic packages that cause circular dependencies
echo "Cleaning up problematic packages..."
rm -rf package/feeds/packages/ntfs-3g 2>/dev/null || true
rm -rf package/feeds/packages/ntpd 2>/dev/null || true

# Create our VUCI example packages
echo ""
echo "Creating VUCI example packages..."

# Remove old versions
rm -rf package/feeds/vuci/vuci-app-example-api 2>/dev/null || true
rm -rf package/feeds/vuci/vuci-app-example-ui 2>/dev/null || true

# Create directories
mkdir -p package/feeds/vuci/vuci-app-example-api
mkdir -p package/feeds/vuci/vuci-app-example-ui

# ============================================
# Create API Package
# ============================================
echo "Creating API package structure..."

mkdir -p package/feeds/vuci/vuci-app-example-api/files/usr/lib/lua/api/services

cat > package/feeds/vuci/vuci-app-example-api/files/usr/lib/lua/api/services/example.lua << 'EOF'
-- Example API Service for VUCI
local M = {}

function M.get_status()
    return {
        success = true,
        status = "running",
        message = "Example API is working!",
        timestamp = os.time(),
        version = "1.0.8"
    }
end

function M.test()
    return {
        success = true,
        test = "passed",
        data = "Hello from VUCI API"
    }
end

function M.get_config()
    return {
        enabled = true,
        interval = 60,
        debug = false
    }
end

return M
EOF

cat > package/feeds/vuci/vuci-app-example-api/Makefile << 'EOF'
include $(TOPDIR)/rules.mk

PKG_NAME:=vuci-app-example-api
PKG_VERSION:=1.0
PKG_RELEASE:=8
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/vuci-app-example-api
  SECTION:=vuci
  CATEGORY:=VUCI
  SUBMENU:=Applications
  TITLE:=Example VUCI API Service
  DEPENDS:=+lua
endef

define Package/vuci-app-example-api/description
  Example API service for VUCI framework
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Configure
endef

define Build/Compile
endef

define Package/vuci-app-example-api/install
	$(INSTALL_DIR) $(1)/usr/lib/lua/api/services
	$(INSTALL_DATA) ./files/usr/lib/lua/api/services/example.lua $(1)/usr/lib/lua/api/services/
endef

define Package/vuci-app-example-api/postinst
#!/bin/sh
[ -z "$$$${IPKG_INSTROOT}" ] && {
    # Create symlink for proper API access
    mkdir -p /usr/lib/lua/api/services 2>/dev/null || true
    ln -sf /usr/local/usr/lib/lua/api/services/example.lua /usr/lib/lua/api/services/example.lua 2>/dev/null || true
    echo "Example API installed and linked"
}
exit 0
endef

$(eval $(call BuildPackage,vuci-app-example-api))
EOF

# ============================================
# Create UI Package
# ============================================
echo "Creating UI package structure..."

mkdir -p package/feeds/vuci/vuci-app-example-ui/files/usr/share/vuci/menu.d
mkdir -p package/feeds/vuci/vuci-app-example-ui/files/www/example
mkdir -p package/feeds/vuci/vuci-app-example-ui/src/src/views/services

# Create menu configuration
cat > package/feeds/vuci/vuci-app-example-ui/files/usr/share/vuci/menu.d/example.json << 'EOF'
{
  "services/example": {
    "title": "Example",
    "index": 500,
    "view": "services/Example"
  }
}
EOF

# Create HTML fallback
cat > package/feeds/vuci/vuci-app-example-ui/files/www/example/index.html << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <title>Example VUCI App</title>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #f5f5f5; padding: 20px; }
        .container { max-width: 1200px; margin: 0 auto; }
        .header { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; padding: 30px; border-radius: 10px; margin-bottom: 20px; }
        .card { background: white; border-radius: 10px; padding: 25px; margin-bottom: 20px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        .status { display: flex; justify-content: space-between; align-items: center; padding: 15px; background: #f8f9fa; border-radius: 8px; margin: 10px 0; }
        .status.success { border-left: 4px solid #28a745; }
        .status.info { border-left: 4px solid #17a2b8; }
        button { background: #667eea; color: white; border: none; padding: 12px 24px; border-radius: 6px; cursor: pointer; font-size: 16px; margin: 5px; transition: all 0.3s; }
        button:hover { background: #5a67d8; transform: translateY(-2px); box-shadow: 0 4px 12px rgba(102, 126, 234, 0.4); }
        button:disabled { background: #ccc; cursor: not-allowed; transform: none; }
        pre { background: #2d3748; color: #48bb78; padding: 15px; border-radius: 6px; overflow-x: auto; font-family: 'Courier New', monospace; }
        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; }
        .metric { text-align: center; padding: 20px; }
        .metric-value { font-size: 2.5em; font-weight: bold; color: #667eea; }
        .metric-label { color: #718096; margin-top: 5px; }
        .spinner { display: inline-block; width: 20px; height: 20px; border: 3px solid rgba(255,255,255,.3); border-radius: 50%; border-top-color: white; animation: spin 1s ease-in-out infinite; }
        @keyframes spin { to { transform: rotate(360deg); } }
        .success-msg { color: #28a745; }
        .error-msg { color: #dc3545; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🚀 Example VUCI Application</h1>
            <p>SDK-Built Package v1.0.8</p>
        </div>
        
        <div class="card">
            <h2>📊 System Status</h2>
            <div class="status success">
                <span>Package Status</span>
                <span class="success-msg">✓ Installed Successfully</span>
            </div>
            <div class="status info">
                <span>SDK Build</span>
                <span>/mnt/wsl/SDK</span>
            </div>
            <div class="status info">
                <span>Installation Path</span>
                <span>/usr/local/www/example/</span>
            </div>
        </div>
        
        <div class="card">
            <h2>🧪 API Testing</h2>
            <div class="grid">
                <div>
                    <button onclick="testAPI('status')">Get Status</button>
                    <button onclick="testAPI('test')">Run Test</button>
                    <button onclick="testAPI('config')">Get Config</button>
                </div>
                <div>
                    <button onclick="checkRoutes()">Check All Routes</button>
                    <button onclick="clearLog()">Clear Log</button>
                </div>
            </div>
            <div id="result" style="margin-top: 20px;"></div>
        </div>
        
        <div class="card">
            <h2>📈 Metrics</h2>
            <div class="grid">
                <div class="metric">
                    <div class="metric-value" id="uptime">0</div>
                    <div class="metric-label">Uptime (seconds)</div>
                </div>
                <div class="metric">
                    <div class="metric-value" id="requests">0</div>
                    <div class="metric-label">API Requests</div>
                </div>
                <div class="metric">
                    <div class="metric-value" id="success-rate">0%</div>
                    <div class="metric-label">Success Rate</div>
                </div>
            </div>
        </div>
        
        <div class="card">
            <h2>📝 API Response Log</h2>
            <pre id="log">Ready for API calls...</pre>
        </div>
    </div>
    
    <script>
        let requestCount = 0;
        let successCount = 0;
        let startTime = Date.now();
        
        // Update metrics
        setInterval(() => {
            document.getElementById('uptime').textContent = Math.floor((Date.now() - startTime) / 1000);
            document.getElementById('requests').textContent = requestCount;
            const rate = requestCount > 0 ? Math.round((successCount / requestCount) * 100) : 0;
            document.getElementById('success-rate').textContent = rate + '%';
        }, 1000);
        
        async function testAPI(endpoint) {
            requestCount++;
            const resultDiv = document.getElementById('result');
            const logDiv = document.getElementById('log');
            
            resultDiv.innerHTML = '<div class="spinner"></div> Calling API...';
            
            try {
                // Try different API paths
                const paths = [
                    `/api/example/${endpoint}`,
                    `/cgi-bin/luci/api/example/${endpoint}`,
                    `/ubus/example.${endpoint}`
                ];
                
                let success = false;
                let lastError = null;
                
                for (const path of paths) {
                    try {
                        const response = await fetch(path);
                        if (response.ok) {
                            const data = await response.json();
                            successCount++;
                            success = true;
                            
                            resultDiv.innerHTML = '<p class="success-msg">✓ API call successful!</p>';
                            logDiv.textContent = JSON.stringify({
                                path: path,
                                status: response.status,
                                data: data
                            }, null, 2);
                            break;
                        }
                    } catch (e) {
                        lastError = e;
                    }
                }
                
                if (!success) {
                    throw lastError || new Error('All API paths failed');
                }
                
            } catch (error) {
                resultDiv.innerHTML = '<p class="error-msg">✗ ' + error.message + '</p>';
                logDiv.textContent = 'Error: ' + error.message + '\n\nTried paths:\n' + 
                    '- /api/example/' + endpoint + '\n' +
                    '- /cgi-bin/luci/api/example/' + endpoint + '\n' +
                    '- /ubus/example.' + endpoint;
            }
        }
        
        async function checkRoutes() {
            const resultDiv = document.getElementById('result');
            const logDiv = document.getElementById('log');
            
            resultDiv.innerHTML = '<div class="spinner"></div> Checking routes...';
            
            const routes = [
                '/api/example/status',
                '/api/example/test',
                '/api/example/config',
                '/www/example/',
                '/usr/local/www/example/'
            ];
            
            let results = [];
            for (const route of routes) {
                try {
                    const response = await fetch(route);
                    results.push(`${route}: ${response.status} ${response.statusText}`);
                } catch (e) {
                    results.push(`${route}: Failed - ${e.message}`);
                }
            }
            
            resultDiv.innerHTML = '<p class="success-msg">Route check complete</p>';
            logDiv.textContent = 'Route Check Results:\n\n' + results.join('\n');
        }
        
        function clearLog() {
            document.getElementById('log').textContent = 'Log cleared. Ready for new API calls...';
            document.getElementById('result').innerHTML = '';
        }
    </script>
</body>
</html>
EOF

# Create Vue component (for reference, needs compilation)
cat > package/feeds/vuci/vuci-app-example-ui/src/src/views/services/Example.vue << 'EOF'
<template>
  <div class="example-app">
    <Card>
      <template #header>
        <h3>Example VUCI Application</h3>
      </template>
      <template #content>
        <div class="p-fluid">
          <div class="field">
            <label>Status</label>
            <Tag :severity="status.success ? 'success' : 'danger'">
              {{ status.message || 'Loading...' }}
            </Tag>
          </div>
          
          <div class="field">
            <Button label="Get Status" @click="getStatus" :loading="loading" />
          </div>
          
          <div class="field" v-if="status.data">
            <pre>{{ JSON.stringify(status.data, null, 2) }}</pre>
          </div>
        </div>
      </template>
    </Card>
  </div>
</template>

<script>
import { ref, onMounted } from 'vue'

export default {
  name: 'Example',
  setup() {
    const status = ref({})
    const loading = ref(false)
    
    const getStatus = async () => {
      loading.value = true
      try {
        const response = await fetch('/api/example/status')
        status.value = await response.json()
      } catch (error) {
        status.value = { success: false, message: error.message }
      } finally {
        loading.value = false
      }
    }
    
    onMounted(() => {
      getStatus()
    })
    
    return {
      status,
      loading,
      getStatus
    }
  }
}
</script>
EOF

cat > package/feeds/vuci/vuci-app-example-ui/Makefile << 'EOF'
include $(TOPDIR)/rules.mk

PKG_NAME:=vuci-app-example-ui
PKG_VERSION:=1.0
PKG_RELEASE:=8
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/vuci-app-example-ui
  SECTION:=vuci
  CATEGORY:=VUCI
  SUBMENU:=Applications
  TITLE:=Example VUCI Web Interface
  DEPENDS:=+vuci-app-example-api
endef

define Package/vuci-app-example-ui/description
  Web interface for Example VUCI application
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Configure
endef

define Build/Compile
	@echo "Note: Vue compilation requires SDK's webpack build system"
	@echo "HTML fallback will be available at /www/example/"
endef

define Package/vuci-app-example-ui/install
	$(INSTALL_DIR) $(1)/usr/share/vuci/menu.d
	$(INSTALL_DATA) ./files/usr/share/vuci/menu.d/example.json $(1)/usr/share/vuci/menu.d/
	$(INSTALL_DIR) $(1)/www/example
	$(INSTALL_DATA) ./files/www/example/index.html $(1)/www/example/
endef

define Package/vuci-app-example-ui/postinst
#!/bin/sh
[ -z "$$$${IPKG_INSTROOT}" ] && {
    # Create symlinks for proper access
    mkdir -p /usr/share/vuci/menu.d 2>/dev/null || true
    ln -sf /usr/local/usr/share/vuci/menu.d/example.json /usr/share/vuci/menu.d/example.json 2>/dev/null || true
    
    # Restart services
    /etc/init.d/uhttpd restart 2>/dev/null || true
    /etc/init.d/rpcd restart 2>/dev/null || true
    
    echo ""
    echo "========================================="
    echo "Example VUCI App Installed!"
    echo "========================================="
    echo "Access at: http://router-ip/example/"
    echo "Menu: Services -> Example (requires Vue compilation)"
    echo ""
}
exit 0
endef

$(eval $(call BuildPackage,vuci-app-example-ui))
EOF

# ============================================
# Configure and Build
# ============================================
echo ""
echo "Configuring build environment..."

# Create minimal config
cat > .config << 'EOF'
CONFIG_TARGET_ipq40xx=y
CONFIG_TARGET_ipq40xx_generic=y
CONFIG_TARGET_ipq40xx_generic_DEVICE_teltonika_rutx=y
CONFIG_DEVEL=y
CONFIG_BUILD_LOG=y
CONFIG_PACKAGE_vuci-app-example-api=m
CONFIG_PACKAGE_vuci-app-example-ui=m
EOF

# Expand config
echo "Running defconfig..."
make defconfig

# Build packages
echo ""
echo "Building packages with $(nproc) threads..."
echo "This may take a few minutes on first run..."

make -j$(nproc) V=sc package/feeds/vuci/vuci-app-example-api/{clean,compile}
make -j$(nproc) V=sc package/feeds/vuci/vuci-app-example-ui/{clean,compile}

# ============================================
# Results
# ============================================
echo ""
echo "========================================="
echo "BUILD COMPLETE!"
echo "========================================="
echo ""

# Find built packages
echo "Looking for built packages..."
PACKAGES=$(find bin/packages -name "vuci-app-example*.ipk" 2>/dev/null)

if [ -n "$PACKAGES" ]; then
    echo "Successfully built packages:"
    for pkg in $PACKAGES; do
        echo "  - $(basename $pkg)"
        ls -lh "$pkg"
    done
    
    # Copy to work directory
    mkdir -p "$SDK_BASE/work/packages"
    cp $PACKAGES "$SDK_BASE/work/packages/"
    echo ""
    echo "Packages copied to: $SDK_BASE/work/packages/"
else
    echo "No packages found. Build may have failed."
    echo "Check build logs above for errors."
fi

echo ""
echo "To deploy to router:"
echo "  1. scp packages to router:/tmp/"
echo "  2. opkg install /tmp/vuci-app-example*.ipk"
echo "  3. Access at http://router-ip/example/"
echo ""
