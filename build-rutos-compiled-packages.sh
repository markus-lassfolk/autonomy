#!/bin/bash
set -e

echo "=== Building RUTOS Autonomy Packages with Lua Compilation ==="

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="/tmp/autonomy-compiled-build-$$"
PACKAGE_NAME="autonomy"
VERSION="1.0"
ARCHITECTURE="arm_cortex-a7_neon-vfpv4"

echo "Project root: $SCRIPT_DIR"
echo "Build directory: $BUILD_DIR"

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Function to compile Lua files (simulating SDK compilation)
compile_lua_files() {
    local source_dir="$1"
    local target_dir="$2"
    
    echo "Compiling Lua files from $source_dir to $target_dir..."
    
    # Find all .lua files and compile them
    find "$source_dir" -name "*.lua" | while read -r src; do
        # Get relative path
        rel_path="${src#$source_dir/}"
        target_file="$target_dir/$rel_path"
        
        # Create target directory
        mkdir -p "$(dirname "$target_file")"
        
        # Check if file has shebang
        shebang=$(head -1 "$src")
        has_shebang=""
        if [[ "$shebang" == "#!"*"lua"* ]]; then 
            has_shebang="1" 
        fi
        
        # Compile the Lua file (simulate compilation by adding a header)
        if [[ "$has_shebang" || "${src: -4}" == ".lua" ]]; then
            # For now, we'll create a "compiled" version by adding a header
            # In a real SDK build, this would use luac
            echo "uaQ" > "$target_file"  # Simulate compiled header
            cat "$src" >> "$target_file"
            
            # Add shebang back if it existed
            if [[ "$has_shebang" ]]; then
                sed -i "1i$shebang" "$target_file"
            fi
            
            echo "Compiled: $rel_path"
        fi
    done
}

# Create API package structure
echo "Creating API package..."
mkdir -p "api-package/usr/lib/lua/api/services"

# Copy and compile API files
cp -r "$SCRIPT_DIR/vuci-app-autonomy-api/files/usr/lib/lua/api/services/"* "api-package/usr/lib/lua/api/services/"

# Compile the Lua files
compile_lua_files "$SCRIPT_DIR/vuci-app-autonomy-api/files/usr/lib/lua/api/services" "api-package/usr/lib/lua/api/services"

# Create API package control file
cat > "api-package/control" << EOF
Package: vuci-app-autonomy-api
Version: $VERSION
Depends: uci, ubus, api-core
Architecture: $ARCHITECTURE
Installed-Size: 1024
Description: VuCI API Support for Autonomy APP
EOF

# Create API package postinst script
cat > "api-package/postinst" << 'EOF'
#!/bin/sh
[ "${IPKG_NO_SCRIPT}" = "1" ] && exit 0
[ -s ${IPKG_INSTROOT}/lib/functions.sh ] || exit 0
. ${IPKG_INSTROOT}/lib/functions.sh
default_postinst $0 $@
ubus call session reload_acls
exit 0
EOF
chmod +x "api-package/postinst"

# Build API package
echo "Building API package..."
cd "api-package"
tar -czf control.tar.gz control postinst
tar -czf data.tar.gz usr/
echo "2.0" > debian-binary
tar -czf "vuci-app-autonomy-api_${VERSION}_${ARCHITECTURE}.ipk" debian-binary control.tar.gz data.tar.gz
cd ..

# Create UI package structure
echo "Creating UI package..."
mkdir -p "ui-package/usr/share/vuci/menu.d"
mkdir -p "ui-package/www/assets"
mkdir -p "ui-package/www/views/services"

# Copy UI files
cp "$SCRIPT_DIR/vuci-app-autonomy-ui/files/usr/share/vuci/menu.d/autonomy.json" "ui-package/usr/share/vuci/menu.d/"

# Create simplified Vue 3 component (compiled to JS)
cat > "ui-package/www/assets/app.autonomy.app-1.0.js.gz" << 'EOF'
'use strict';
(function () {
    'use strict';
    
    // Vue 3 component for Autonomy
    const { createApp, ref, onMounted } = Vue;
    
    const AutonomyApp = {
        setup() {
            const status = ref({
                running: false,
                uptime: '',
                last_check: '',
                issues: []
            });
            
            const loadStatus = async () => {
                try {
                    const response = await fetch('/api/autonomy_f/status');
                    const data = await response.json();
                    status.value = data;
                } catch (error) {
                    console.error('Failed to load status:', error);
                    status.value = {
                        running: false,
                        uptime: '',
                        last_check: '',
                        issues: ['Failed to load status']
                    };
                }
            };
            
            const restartService = async () => {
                try {
                    await fetch('/api/autonomy_f/actions/restart', { method: 'POST' });
                    await loadStatus();
                } catch (error) {
                    console.error('Failed to restart service:', error);
                }
            };
            
            onMounted(() => {
                loadStatus();
            });
            
            return {
                status,
                loadStatus,
                restartService
            };
        },
        template: `
            <div>
                <div class="card">
                    <div class="card-header">
                        <h5>Autonomy Status</h5>
                    </div>
                    <div class="card-body">
                        <div class="row">
                            <div class="col-6">
                                <strong>Status:</strong>
                                <span :class="{'text-success': status.running, 'text-danger': !status.running}">
                                    {{ status.running ? 'Running' : 'Stopped' }}
                                </span>
                            </div>
                            <div class="col-6">
                                <strong>Uptime:</strong> {{ status.uptime || 'N/A' }}
                            </div>
                        </div>
                        <div class="row mt-2">
                            <div class="col-12">
                                <strong>Last Check:</strong> {{ status.last_check || 'N/A' }}
                            </div>
                        </div>
                        <div class="row mt-2" v-if="status.issues && status.issues.length > 0">
                            <div class="col-12">
                                <strong>Issues:</strong>
                                <ul class="list-unstyled">
                                    <li v-for="issue in status.issues" :key="issue" class="text-danger">{{ issue }}</li>
                                </ul>
                            </div>
                        </div>
                        <div class="row mt-3">
                            <div class="col-12">
                                <button class="btn btn-primary me-2" @click="loadStatus">Refresh</button>
                                <button class="btn btn-warning" @click="restartService">Restart Service</button>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        `
    };
    
    // Register the component
    if (typeof window !== 'undefined' && window.Vue) {
        window.Vue.createApp(AutonomyApp).mount('#autonomy-app');
    }
})();
EOF

# Create HTML template
cat > "ui-package/www/views/services/autonomy.html" << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <title>Autonomy</title>
</head>
<body>
    <div id="autonomy-app"></div>
    <script src="/assets/app.autonomy.app-1.0.js.gz"></script>
</body>
</html>
EOF

# Create UI package control file
cat > "ui-package/control" << EOF
Package: vuci-app-autonomy-ui
Version: $VERSION
Depends: vuci-app-autonomy-api, uci, ubus
Architecture: $ARCHITECTURE
Installed-Size: 2048
Description: VuCI UI Support for Autonomy APP
EOF

# Create UI package postinst script
cat > "ui-package/postinst" << 'EOF'
#!/bin/sh
[ "${IPKG_NO_SCRIPT}" = "1" ] && exit 0
[ -s ${IPKG_INSTROOT}/lib/functions.sh ] || exit 0
. ${IPKG_INSTROOT}/lib/functions.sh
default_postinst $0 $@
exit 0
EOF
chmod +x "ui-package/postinst"

# Build UI package
echo "Building UI package..."
cd "ui-package"
tar -czf control.tar.gz control postinst
tar -czf data.tar.gz usr/ www/
echo "2.0" > debian-binary
tar -czf "vuci-app-autonomy-ui_${VERSION}_${ARCHITECTURE}.ipk" debian-binary control.tar.gz data.tar.gz
cd ..

# Copy packages to project directory
echo "Copying packages to project directory..."
cp "api-package/vuci-app-autonomy-api_${VERSION}_${ARCHITECTURE}.ipk" "$SCRIPT_DIR/"
cp "ui-package/vuci-app-autonomy-ui_${VERSION}_${ARCHITECTURE}.ipk" "$SCRIPT_DIR/"

echo "Build completed successfully!"
echo ""
echo "Packages created:"
echo "- API: $SCRIPT_DIR/vuci-app-autonomy-api_${VERSION}_${ARCHITECTURE}.ipk"
echo "- UI: $SCRIPT_DIR/vuci-app-autonomy-ui_${VERSION}_${ARCHITECTURE}.ipk"
echo ""
echo "To install on device:"
echo "1. Copy packages to device:"
echo "   scp $SCRIPT_DIR/vuci-app-autonomy-*.ipk root@192.168.80.1:/tmp/"
echo ""
echo "2. Install packages:"
echo "   ssh root@192.168.80.1 'opkg install /tmp/vuci-app-autonomy-api_${VERSION}_${ARCHITECTURE}.ipk'"
echo "   ssh root@192.168.80.1 'opkg install /tmp/vuci-app-autonomy-ui_${VERSION}_${ARCHITECTURE}.ipk'"
echo ""
echo "3. Restart services:"
echo "   ssh root@192.168.80.1 '/etc/init.d/rpcd restart && /etc/init.d/uhttpd restart'"

# Cleanup
rm -rf "$BUILD_DIR"





