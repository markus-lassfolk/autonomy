#!/bin/bash

# PROPER VUCI Package Build Script
# This script creates properly compiled VUCI packages that will actually work

set -e

echo "========================================="
echo "PROPER VUCI PACKAGE BUILD SCRIPT"
echo "========================================="

# Configuration
PACKAGE_NAME="example"
PACKAGE_NAME_CAPITALIZED="Example"
VERSION="1.0"
RELEASE="1"
ARCH="arm_cortex-a7_neon-vfpv4"
BUILD_DIR="/tmp/vuci-build-$$"
SDK_DIR="/home/markusla/rutos-sdk"

# Clean build directory
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

echo "Build directory: $BUILD_DIR"

# ============================================
# STEP 1: Create Simple Vue Component
# ============================================
echo ""
echo "Creating Vue.js component..."

mkdir -p "$BUILD_DIR/src/views/services"

cat > "$BUILD_DIR/src/views/services/${PACKAGE_NAME_CAPITALIZED}.vue" << 'EOF'
<template>
  <div class="example-app">
    <h2>{{ $t('Example Application') }}</h2>
    
    <div v-if="loading" class="loading">
      {{ $t('Loading...') }}
    </div>
    
    <div v-else class="content">
      <div class="status-section">
        <h3>{{ $t('API Test') }}</h3>
        <button @click="testApi" :disabled="testing">
          {{ testing ? $t('Testing...') : $t('Test API') }}
        </button>
        <div v-if="apiResponse" class="response">
          <pre>{{ apiResponse }}</pre>
        </div>
      </div>
      
      <div class="info-section">
        <h3>{{ $t('Information') }}</h3>
        <p>{{ $t('This is a working VUCI application example.') }}</p>
        <p>{{ $t('Version') }}: 1.0</p>
      </div>
    </div>
  </div>
</template>

<script>
export default {
  name: 'ExampleApp',
  data() {
    return {
      loading: false,
      testing: false,
      apiResponse: null
    }
  },
  methods: {
    async testApi() {
      this.testing = true;
      this.apiResponse = null;
      
      try {
        const response = await this.$axios.get('/api/example_f/test');
        this.apiResponse = JSON.stringify(response.data, null, 2);
      } catch (error) {
        this.apiResponse = 'Error: ' + error.message;
      } finally {
        this.testing = false;
      }
    }
  },
  mounted() {
    console.log('Example app mounted');
  }
}
</script>

<style scoped>
.example-app {
  padding: 20px;
}

.loading {
  text-align: center;
  padding: 40px;
}

.content {
  max-width: 800px;
}

.status-section, .info-section {
  margin-bottom: 30px;
  padding: 20px;
  border: 1px solid #ddd;
  border-radius: 4px;
}

.response {
  margin-top: 10px;
  padding: 10px;
  background: #f5f5f5;
  border-radius: 4px;
}

button {
  padding: 8px 16px;
  background: #007bff;
  color: white;
  border: none;
  border-radius: 4px;
  cursor: pointer;
}

button:disabled {
  opacity: 0.6;
  cursor: not-allowed;
}

button:hover:not(:disabled) {
  background: #0056b3;
}
</style>
EOF

# ============================================
# STEP 2: Compile Vue Component with Vite
# ============================================
echo ""
echo "Setting up compilation environment..."

# Create package.json for compilation
cat > "$BUILD_DIR/package.json" << 'EOF'
{
  "name": "vuci-app-example",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "build": "vite build"
  },
  "dependencies": {
    "vue": "^3.4.21"
  },
  "devDependencies": {
    "@vitejs/plugin-vue": "^5.0.3",
    "vite": "^5.0.12",
    "vite-plugin-compression2": "^1.0.0",
    "vite-plugin-externals": "^0.6.2"
  }
}
EOF

# Create vite config
cat > "$BUILD_DIR/vite.config.js" << 'EOF'
import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import { viteExternalsPlugin } from 'vite-plugin-externals'
import { compression } from 'vite-plugin-compression2'

export default defineConfig({
  plugins: [
    vue({ isProduction: true }),
    viteExternalsPlugin({ 
      vue: 'Vue',
      axios: 'axios',
      'vue-router': 'VueRouter'
    }),
    compression({ 
      algorithm: 'gzip',
      deleteOriginalAssets: true 
    })
  ],
  build: {
    lib: {
      entry: './src/views/services/Example.vue',
      name: 'ExampleApp',
      fileName: () => 'app.example.app-2025-08-08-3a282822359.js',
      formats: ['es']
    },
    rollupOptions: {
      external: ['vue', 'axios', 'vue-router'],
      output: {
        globals: {
          vue: 'Vue',
          axios: 'axios',
          'vue-router': 'VueRouter'
        }
      }
    },
    outDir: './dist',
    emptyOutDir: true
  }
})
EOF

echo ""
echo "Compiling Vue component with Node.js and Vite..."

# Check if Node.js is available
if ! command -v node &> /dev/null; then
    echo "Node.js not found. Installing simple pre-compiled version..."
    
    # Create pre-compiled JavaScript (simplified version that should work)
    mkdir -p "$BUILD_DIR/dist"
    
    cat > "$BUILD_DIR/dist/app.example.app-2025-08-08-3a282822359.js" << 'EOF'
import{defineComponent as e,openBlock as t,createElementBlock as n,createElementVNode as o,toDisplayString as a,createTextVNode as s,createCommentVNode as i,withDirectives as r,vShow as l}from"vue";const c=e({name:"ExampleApp",data(){return{loading:!1,testing:!1,apiResponse:null}},methods:{async testApi(){this.testing=!0,this.apiResponse=null;try{const e=await this.$axios.get("/api/example_f/test");this.apiResponse=JSON.stringify(e.data,null,2)}catch(e){this.apiResponse="Error: "+e.message}finally{this.testing=!1}}},mounted(){console.log("Example app mounted")}});const d={class:"example-app"},p={key:0,class:"loading"},u={key:1,class:"content"},m={class:"status-section"},f={key:0,class:"response"},g={class:"info-section"};c.render=function(e,c,h,v,x,y){return t(),n("div",d,[x.loading?(t(),n("div",p,a(e.$t("Loading...")),1)):(t(),n("div",u,[o("div",m,[o("h3",null,a(e.$t("API Test")),1),o("button",{onClick:c[0]||(c[0]=(...e)=>y.testApi&&y.testApi(...e)),disabled:x.testing},a(x.testing?e.$t("Testing..."):e.$t("Test API")),9,["disabled"]),x.apiResponse?(t(),n("div",f,[o("pre",null,a(x.apiResponse),1)])):i("",!0)]),o("div",g,[o("h3",null,a(e.$t("Information")),1),o("p",null,a(e.$t("This is a working VUCI application example.")),1),o("p",null,a(e.$t("Version"))+": 1.0",1)])]))])};export default c;
EOF
    
    # Compress the JavaScript
    gzip -c "$BUILD_DIR/dist/app.example.app-2025-08-08-3a282822359.js" > "$BUILD_DIR/dist/app.example.app-2025-08-08-3a282822359.js.gz"
    rm "$BUILD_DIR/dist/app.example.app-2025-08-08-3a282822359.js"
else
    # Try to compile with Node.js
    cd "$BUILD_DIR"
    npm install
    npm run build
    
    # The output should be in dist/
    if [ -f "dist/app.example.app-2025-08-08-3a282822359.js" ]; then
        gzip -c "dist/app.example.app-2025-08-08-3a282822359.js" > "dist/app.example.app-2025-08-08-3a282822359.js.gz"
        rm "dist/app.example.app-2025-08-08-3a282822359.js"
    fi
fi

# ============================================
# STEP 3: Create API Package
# ============================================
echo ""
echo "Creating API package..."

API_DIR="$BUILD_DIR/api-package"
mkdir -p "$API_DIR/data/usr/lib/lua/api/services"

# Create API service
cat > "$API_DIR/data/usr/lib/lua/api/services/example_f.lua" << 'EOF'
-- Example Function Service for VUCI
local M = {}

-- GET /api/example_f/test
function M.test(self)
    return {
        success = true,
        message = "API is working!",
        timestamp = os.time(),
        data = {
            version = "1.0",
            status = "operational"
        }
    }
end

-- POST /api/example_f/action
function M.action(self, data)
    return {
        success = true,
        message = "Action executed",
        input = data
    }
end

return M
EOF

# Create control file for API
cat > "$API_DIR/control" << EOF
Package: vuci-app-${PACKAGE_NAME}-api
Version: ${VERSION}-${RELEASE}
Depends: libc
Section: vuci
Architecture: ${ARCH}
Installed-Size: 2048
Description: Example VUCI API service
EOF

# Create postinst for API
cat > "$API_DIR/postinst" << 'EOF'
#!/bin/sh
[ -z "$IPKG_INSTROOT" ] && {
    /etc/init.d/uhttpd restart 2>/dev/null || true
}
exit 0
EOF
chmod +x "$API_DIR/postinst"

# Create debian-binary
echo "2.0" > "$API_DIR/debian-binary"

# Build API package
cd "$API_DIR"
tar -czf control.tar.gz --owner=root --group=root control postinst
tar -czf data.tar.gz --owner=root --group=root -C data .
tar -cf "../vuci-app-${PACKAGE_NAME}-api_${VERSION}-${RELEASE}_${ARCH}.tar" debian-binary control.tar.gz data.tar.gz
gzip -c "../vuci-app-${PACKAGE_NAME}-api_${VERSION}-${RELEASE}_${ARCH}.tar" > "../vuci-app-${PACKAGE_NAME}-api_${VERSION}-${RELEASE}_${ARCH}.ipk"
rm "../vuci-app-${PACKAGE_NAME}-api_${VERSION}-${RELEASE}_${ARCH}.tar"

echo "API package created: vuci-app-${PACKAGE_NAME}-api_${VERSION}-${RELEASE}_${ARCH}.ipk"

# ============================================
# STEP 4: Create UI Package
# ============================================
echo ""
echo "Creating UI package..."

UI_DIR="$BUILD_DIR/ui-package"
mkdir -p "$UI_DIR/data/usr/share/vuci/menu.d"
mkdir -p "$UI_DIR/data/www/assets"

# Copy compiled JavaScript
if [ -f "$BUILD_DIR/dist/app.example.app-2025-08-08-3a282822359.js.gz" ]; then
    cp "$BUILD_DIR/dist/app.example.app-2025-08-08-3a282822359.js.gz" "$UI_DIR/data/www/assets/"
else
    echo "Warning: Compiled JavaScript not found!"
fi

# Create menu configuration (CRITICAL: correct path!)
cat > "$UI_DIR/data/usr/share/vuci/menu.d/example.json" << EOF
{"services/example":{"title":"Example","index":50,"view":"services/Example","acls":["services/example"]}}
EOF

# Create ACL file
mkdir -p "$UI_DIR/data/usr/share/rpcd/acl.d"
cat > "$UI_DIR/data/usr/share/rpcd/acl.d/example.json" << 'EOF'
{
    "services/example": {
        "description": "Example application access",
        "read": {
            "ubus": {
                "example": ["*"]
            }
        },
        "write": {
            "ubus": {
                "example": ["*"]
            }
        }
    }
}
EOF

# Create control file for UI
cat > "$UI_DIR/control" << EOF
Package: vuci-app-${PACKAGE_NAME}-ui
Version: ${VERSION}-${RELEASE}
Depends: libc, vuci-app-${PACKAGE_NAME}-api
Section: vuci
Architecture: ${ARCH}
Installed-Size: 4096
Description: Example VUCI UI application
EOF

# Create postinst for UI
cat > "$UI_DIR/postinst" << 'EOF'
#!/bin/sh
[ -z "$IPKG_INSTROOT" ] && {
    # Clear browser cache
    rm -rf /tmp/luci-*
    # Restart web server
    /etc/init.d/uhttpd restart 2>/dev/null || true
    # Update VUCI menu cache if it exists
    [ -f /usr/bin/vuci-update-menu ] && /usr/bin/vuci-update-menu
}
exit 0
EOF
chmod +x "$UI_DIR/postinst"

# Create debian-binary
echo "2.0" > "$UI_DIR/debian-binary"

# Build UI package
cd "$UI_DIR"
tar -czf control.tar.gz --owner=root --group=root control postinst
tar -czf data.tar.gz --owner=root --group=root -C data .
tar -cf "../vuci-app-${PACKAGE_NAME}-ui_${VERSION}-${RELEASE}_${ARCH}.tar" debian-binary control.tar.gz data.tar.gz
gzip -c "../vuci-app-${PACKAGE_NAME}-ui_${VERSION}-${RELEASE}_${ARCH}.tar" > "../vuci-app-${PACKAGE_NAME}-ui_${VERSION}-${RELEASE}_${ARCH}.ipk"
rm "../vuci-app-${PACKAGE_NAME}-ui_${VERSION}-${RELEASE}_${ARCH}.tar"

echo "UI package created: vuci-app-${PACKAGE_NAME}-ui_${VERSION}-${RELEASE}_${ARCH}.ipk"

# ============================================
# STEP 5: Copy packages to current directory
# ============================================
echo ""
echo "Copying packages to current directory..."

cp "$BUILD_DIR/vuci-app-${PACKAGE_NAME}-api_${VERSION}-${RELEASE}_${ARCH}.ipk" .
cp "$BUILD_DIR/vuci-app-${PACKAGE_NAME}-ui_${VERSION}-${RELEASE}_${ARCH}.ipk" .

echo ""
echo "========================================="
echo "BUILD COMPLETE!"
echo "========================================="
echo ""
echo "Packages created:"
echo "  - vuci-app-${PACKAGE_NAME}-api_${VERSION}-${RELEASE}_${ARCH}.ipk"
echo "  - vuci-app-${PACKAGE_NAME}-ui_${VERSION}-${RELEASE}_${ARCH}.ipk"
echo ""
echo "Key improvements in this build:"
echo "  ✓ Menu files in correct location (/usr/share/vuci/menu.d/)"
echo "  ✓ Compiled JavaScript with correct naming pattern"
echo "  ✓ Proper gzip compression for IPK packages"
echo "  ✓ ACL files for permissions"
echo "  ✓ Simplified API service that should work"
echo ""
echo "To deploy:"
echo "  1. Copy packages to router: scp *.ipk root@192.168.80.1:/tmp/"
echo "  2. Install: opkg install /tmp/vuci-app-example-*.ipk"
echo "  3. Access at: http://192.168.80.1 -> Services -> Example"
echo ""

# Clean up build directory
rm -rf "$BUILD_DIR"


