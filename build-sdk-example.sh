#!/bin/bash

# SDK Example Builder
# Builds the VUCI example packages from the SDK

set -e

# Configuration
BUILD_DIR="/home/markusla/example-build"
TIMESTAMP=$(date +%s)

echo "Building SDK VUCI Examples"
echo "Build Directory: $BUILD_DIR"

cd "$BUILD_DIR"

echo "Building API package..."

# Create API package structure
mkdir -p "api-package/data/usr/lib/lua/api/services"
mkdir -p "api-package/data/usr/share/vuci/path.d"
mkdir -p "api-package/data/usr/share/rpcd/acl.d"
mkdir -p "api-package/data/etc/config"
mkdir -p "api-package/control"

# Copy API files from SDK example
cp "vuci-app-example-api/files/usr/lib/lua/api/services/"* "api-package/data/usr/lib/lua/api/services/"
cp "vuci-app-example-api/files/usr/share/vuci/path.d/"* "api-package/data/usr/share/vuci/path.d/"
cp "vuci-app-example-api/files/usr/share/rpcd/acl.d/"* "api-package/data/usr/share/rpcd/acl.d/"
cp "vuci-app-example-api/files/etc/config/"* "api-package/data/etc/config/"

# API Package - Control file
cat > "api-package/control/control" << EOF
Package: vuci-app-example-api
Version: 1.0-1
Depends: rpcd, rpcd-mod-file, rpcd-mod-iwinfo, rpcd-mod-rpcsys
Section: vuci
Architecture: arm_cortex-a7_neon-vfpv4
Installed-Size: 2048
Source: vuci-app-example-api
SourceName: vuci-app-example-api
SourceDateEpoch: ${TIMESTAMP}
Description: VuCI API Support for Example APP
 This package provides the API backend for the Example VUCI application.
EOF

# API Package - Postinst script
cat > "api-package/control/postinst" << 'EOF'
#!/bin/sh
# Post-installation script for vuci-app-example-api
echo "Installing Example API service..."
exit 0
EOF
chmod +x "api-package/control/postinst"

echo "Building UI package..."

# Create UI package structure
mkdir -p "ui-package/data/usr/share/vuci/menu.d"
mkdir -p "ui-package/data/usr/local/www/assets"
mkdir -p "ui-package/control"

# Copy UI files from SDK example
cp "vuci-app-example-ui/files/usr/share/vuci/menu.d/"* "ui-package/data/usr/share/vuci/menu.d/"

# UI Package - Control file
cat > "ui-package/control/control" << EOF
Package: vuci-app-example-ui
Version: 1.0-1
Depends: vuci-app-example-api
Section: vuci
Architecture: arm_cortex-a7_neon-vfpv4
Installed-Size: 4096
Source: vuci-app-example-ui
SourceName: vuci-app-example-ui
SourceDateEpoch: ${TIMESTAMP}
Description: VuCI UI Support for Example APP
 This package provides the web interface for the Example VUCI application.
EOF

# UI Package - Postinst script
cat > "ui-package/control/postinst" << 'EOF'
#!/bin/sh
# Post-installation script for vuci-app-example-ui
echo "Installing Example UI..."
exit 0
EOF
chmod +x "ui-package/control/postinst"

# Create a simple Vue.js file based on the SDK example
cat > "ui-package/data/usr/local/www/assets/app.example.app-${TIMESTAMP}.js" << 'EOF'
// Example Vue.js Application
// Based on SDK example

(function() {
    'use strict';
    
    // Example Vue 3 component
    const ExampleApp = {
        name: 'ExampleApp',
        data() {
            return {
                response: "-",
                form: {
                    name: "",
                    custom: ""
                }
            }
        },
        methods: {
            async functionExampleCall() {
                try {
                    const response = await fetch('/api/example_f/test');
                    if (response.ok) {
                        const data = await response.json();
                        this.response = JSON.stringify(data);
                    }
                } catch (error) {
                    console.error('Failed to get example:', error);
                }
            },
            async executeAction() {
                try {
                    const response = await fetch('/api/example_f/actions/test', {
                        method: 'POST',
                        headers: {
                            'Content-Type': 'application/json'
                        },
                        body: JSON.stringify({
                            data: {
                                name: this.form.name,
                                custom: this.form.custom
                            }
                        })
                    });
                    if (response.ok) {
                        const data = await response.json();
                        this.response = JSON.stringify(data);
                    }
                } catch (error) {
                    console.error('Failed to execute action:', error);
                }
            }
        },
        template: `
            <div class="example-app">
                <h2>Example Application</h2>
                <div class="response-section">
                    <h3>Response</h3>
                    <code>{{ response }}</code>
                </div>
                <div class="functions-section">
                    <h3>Functions Example</h3>
                    <button @click="functionExampleCall">Get API Example</button>
                </div>
                <div class="form-section">
                    <h3>Functions Example Actions</h3>
                    <div>
                        <label>Name:</label>
                        <input v-model="form.name" maxlength="256" required />
                    </div>
                    <div>
                        <label>Custom:</label>
                        <input v-model="form.custom" />
                    </div>
                    <button @click="executeAction">POST API Example</button>
                </div>
            </div>
        `
    };
    
    // Register component globally
    if (typeof window !== 'undefined' && window.Vue) {
        window.Vue.component('example-app', ExampleApp);
    }
    
    // Export for module systems
    if (typeof module !== 'undefined' && module.exports) {
        module.exports = ExampleApp;
    }
})();
EOF

# Compress the Vue.js file to .js.gz
gzip -c "ui-package/data/usr/local/www/assets/app.example.app-${TIMESTAMP}.js" > "ui-package/data/usr/local/www/assets/app.example.app-${TIMESTAMP}.js.gz"
rm "ui-package/data/usr/local/www/assets/app.example.app-${TIMESTAMP}.js"

echo "Creating IPK packages (Tar Format)..."

# Function to create IPK package in tar format
create_ipk_tar() {
    local package_dir="$1"
    local package_name="$2"
    
    echo "Creating $package_name..."
    
    # Create debian-binary
    echo "2.0" > "$BUILD_DIR/$package_dir/debian-binary"
    
    # Create tar archives
    cd "$BUILD_DIR/$package_dir/control"
    tar -czf "../control.tar.gz" --owner=root --group=root control postinst
    cd "$BUILD_DIR/$package_dir"
    
    cd "$BUILD_DIR/$package_dir/data"
    tar -czf "../data.tar.gz" --owner=root --group=root .
    cd "$BUILD_DIR/$package_dir"
    
    # Create tar archive containing IPK components
    tar -cf "${package_name}_1.0-1_arm_cortex-a7_neon-vfpv4.ipk" debian-binary control.tar.gz data.tar.gz
    
    # Compress the tar archive with gzip
    gzip -c "${package_name}_1.0-1_arm_cortex-a7_neon-vfpv4.ipk" > "${package_name}_1.0-1_arm_cortex-a7_neon-vfpv4.ipk.gz"
    
    # Rename to .ipk extension for opkg
    mv "${package_name}_1.0-1_arm_cortex-a7_neon-vfpv4.ipk.gz" "${package_name}_1.0-1_arm_cortex-a7_neon-vfpv4.ipk"
    
    # Verify package
    echo "Verifying $package_name..."
    file "${package_name}_1.0-1_arm_cortex-a7_neon-vfpv4.ipk"
    
    # Move to build directory
    mv "${package_name}_1.0-1_arm_cortex-a7_neon-vfpv4.ipk" "$BUILD_DIR/"
    
    cd "$BUILD_DIR"
}

# Create API package
create_ipk_tar "api-package" "vuci-app-example-api"

# Create UI package  
create_ipk_tar "ui-package" "vuci-app-example-ui"

echo "Package creation complete!"
echo ""
echo "Created packages:"
ls -la *.ipk
echo ""
echo "Package verification:"
for pkg in *.ipk; do
    echo "$pkg: $(file "$pkg")"
done

echo ""
echo "Ready for deployment!"


