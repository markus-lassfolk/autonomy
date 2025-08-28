#!/bin/bash
set -e

echo "=== Building RUTOS Example Packages (Testing SDK Foundation) ==="

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="/tmp/example-build-$$"
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
echo "Creating example API package..."
mkdir -p "api-package/usr/lib/lua/api/services"

# Copy and compile API files from example
cp -r "/tmp/example-api-working/files/usr/lib/lua/api/services/"* "api-package/usr/lib/lua/api/services/"

# Compile the Lua files
compile_lua_files "/tmp/example-api-working/files/usr/lib/lua/api/services" "api-package/usr/lib/lua/api/services"

# Create API package control file
cat > "api-package/control" << EOF
Package: vuci-app-example-api
Version: $VERSION
Depends: uci, ubus, api-core
Architecture: $ARCHITECTURE
Installed-Size: 1024
Description: VuCI API Support for Example APP (Testing Foundation)
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
echo "Building example API package..."
cd "api-package"
tar -czf control.tar.gz control postinst
tar -czf data.tar.gz usr/
echo "2.0" > debian-binary
tar -czf "vuci-app-example-api_${VERSION}_${ARCHITECTURE}.ipk" debian-binary control.tar.gz data.tar.gz
cd ..

# Create UI package structure
echo "Creating example UI package..."
mkdir -p "ui-package/usr/share/vuci/menu.d"
mkdir -p "ui-package/www/assets"
mkdir -p "ui-package/www/views/services"

# Copy UI files from example
cp "/tmp/example-ui-working/files/usr/share/vuci/menu.d/example.json" "ui-package/usr/share/vuci/menu.d/"

# Copy Vue components from example
cp -r "/tmp/example-ui-working/src/src/views/services/"* "ui-package/www/views/services/"

# Create simplified JavaScript UI component (compiled from Vue)
cat > "ui-package/www/assets/app.example.app-1.0.js.gz" << 'EOF'
'use strict';
(function () {
    'use strict';
    
    // Vue 3 component for Example
    const { createApp, ref, onMounted } = Vue;
    
    const ExampleApp = {
        setup() {
            const response = ref('-');
            const form = ref({
                name: '',
                custom: ''
            });
            
            const functionExampleCall = async () => {
                try {
                    const response = await fetch('/api/example_f/test');
                    const data = await response.json();
                    response.value = JSON.stringify(data);
                } catch (error) {
                    console.error('Failed to get example:', error);
                }
            };
            
            const executeAction = async () => {
                try {
                    const response = await fetch('/api/example_f/actions/test', {
                        method: 'POST',
                        headers: {
                            'Content-Type': 'application/json'
                        },
                        body: JSON.stringify({
                            data: {
                                name: form.value.name,
                                custom: form.value.custom
                            }
                        })
                    });
                    const data = await response.json();
                    response.value = JSON.stringify(data);
                } catch (error) {
                    console.error('Failed to execute action:', error);
                }
            };
            
            onMounted(() => {
                // Initialize component
            });
            
            return {
                response,
                form,
                functionExampleCall,
                executeAction
            };
        },
        template: `
            <div>
                <div class="card">
                    <div class="card-header">
                        <h5>Example Response</h5>
                    </div>
                    <div class="card-body">
                        <code>{{ response }}</code>
                    </div>
                </div>
                <div class="card">
                    <div class="card-header">
                        <h5>Functions Example</h5>
                    </div>
                    <div class="card-body">
                        <button class="btn btn-primary" @click="functionExampleCall">
                            Get API Example
                        </button>
                    </div>
                </div>
                <div class="card">
                    <div class="card-header">
                        <h5>Functions Example Actions</h5>
                    </div>
                    <div class="card-body">
                        <div class="form-group">
                            <label>Name:</label>
                            <input type="text" class="form-control" v-model="form.name" maxlength="256">
                        </div>
                        <div class="form-group">
                            <label>Custom:</label>
                            <input type="text" class="form-control" v-model="form.custom">
                        </div>
                        <button class="btn btn-success" @click="executeAction">
                            POST API Example
                        </button>
                    </div>
                </div>
            </div>
        `
    };
    
    // Register the component
    if (typeof window !== 'undefined' && window.Vue) {
        window.Vue.createApp(ExampleApp).mount('#example-app');
    }
})();
EOF

# Create HTML template
cat > "ui-package/www/views/services/example.html" << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <title>Example</title>
</head>
<body>
    <div id="example-app"></div>
    <script src="/assets/app.example.app-1.0.js.gz"></script>
</body>
</html>
EOF

# Create UI package control file
cat > "ui-package/control" << EOF
Package: vuci-app-example-ui
Version: $VERSION
Depends: vuci-app-example-api, uci, ubus
Architecture: $ARCHITECTURE
Installed-Size: 2048
Description: VuCI UI Support for Example APP (Testing Foundation)
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
echo "Building example UI package..."
cd "ui-package"
tar -czf control.tar.gz control postinst
tar -czf data.tar.gz usr/ www/
echo "2.0" > debian-binary
tar -czf "vuci-app-example-ui_${VERSION}_${ARCHITECTURE}.ipk" debian-binary control.tar.gz data.tar.gz
cd ..

# Copy packages to project directory
echo "Copying packages to project directory..."
cp "api-package/vuci-app-example-api_${VERSION}_${ARCHITECTURE}.ipk" "$SCRIPT_DIR/"
cp "ui-package/vuci-app-example-ui_${VERSION}_${ARCHITECTURE}.ipk" "$SCRIPT_DIR/"

echo "Build completed successfully!"
echo ""
echo "Packages created:"
echo "- API: $SCRIPT_DIR/vuci-app-example-api_${VERSION}_${ARCHITECTURE}.ipk"
echo "- UI: $SCRIPT_DIR/vuci-app-example-ui_${VERSION}_${ARCHITECTURE}.ipk"
echo ""
echo "To install on device:"
echo "1. Copy packages to device:"
echo "   scp $SCRIPT_DIR/vuci-app-example-*.ipk root@192.168.80.1:/tmp/"
echo ""
echo "2. Install packages:"
echo "   ssh root@192.168.80.1 'opkg install /tmp/vuci-app-example-api_${VERSION}_${ARCHITECTURE}.ipk'"
echo "   ssh root@192.168.80.1 'opkg install /tmp/vuci-app-example-ui_${VERSION}_${ARCHITECTURE}.ipk'"
echo ""
echo "3. Restart services:"
echo "   ssh root@192.168.80.1 '/etc/init.d/rpcd restart && /etc/init.d/uhttpd restart'"

# Cleanup
rm -rf "$BUILD_DIR"





