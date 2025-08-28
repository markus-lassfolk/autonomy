#!/bin/bash
set -e

echo "=== Building RUTOS Autonomy VUCI App Package ==="

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
BUILD_DIR="/tmp/autonomy-vuci-app"
PACKAGE_NAME="vuci-app-autonomy-ui"
VERSION="2025-08-26-1"
ARCHITECTURE="arm_cortex-a7_neon-vfpv4"

echo "Building in: $BUILD_DIR"
echo "Architecture: $ARCHITECTURE"
echo "Version: $VERSION"

# Create build directory
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Create package structure
echo "Creating package structure..."
mkdir -p "$BUILD_DIR/usr/share/vuci/menu.d"
mkdir -p "$BUILD_DIR/www/assets"

# Create menu JSON file
echo "Creating menu JSON file..."
cat > "$BUILD_DIR/usr/share/vuci/menu.d/autonomy.json" << 'EOF'
{
  "services/autonomy": {
    "title": "Autonomy",
    "index": 50,
    "view": "services/Autonomy",
    "acls": ["services/autonomy"]
  },
  "services/autonomy/overview": {
    "title": "Overview",
    "index": 1,
    "view": "services/AutonomyOverview",
    "acls": ["services/autonomy"]
  },
  "services/autonomy/configuration": {
    "title": "Configuration",
    "index": 2,
    "view": "services/AutonomyConfig",
    "acls": ["services/autonomy"]
  },
  "services/autonomy/status": {
    "title": "Status",
    "index": 3,
    "view": "services/AutonomyStatus",
    "acls": ["services/autonomy"]
  },
  "services/autonomy/logs": {
    "title": "Logs",
    "index": 4,
    "view": "services/AutonomyLogs",
    "acls": ["services/autonomy"]
  }
}
EOF

# Create JavaScript UI file
echo "Creating JavaScript UI file..."
cat > "$BUILD_DIR/www/assets/app.autonomy.app-${VERSION}.js" << 'EOF'
'use strict';

(function () {
    'use strict';

    var app = angular.module('app.autonomy', ['app.core']);

    app.controller('AutonomyController', ['$scope', '$http', function ($scope, $http) {
        $scope.autonomy = {
            status: {},
            config: {},
            logs: []
        };

        $scope.loadStatus = function() {
            $http.get('/cgi-bin/autonomy/status').then(function(response) {
                $scope.autonomy.status = response.data;
            }).catch(function(error) {
                console.error('Failed to load autonomy status:', error);
            });
        };

        $scope.loadConfig = function() {
            $http.get('/cgi-bin/autonomy/config').then(function(response) {
                $scope.autonomy.config = response.data;
            }).catch(function(error) {
                console.error('Failed to load autonomy config:', error);
            });
        };

        $scope.loadLogs = function() {
            $http.get('/cgi-bin/autonomy/logs').then(function(response) {
                $scope.autonomy.logs = response.data;
            }).catch(function(error) {
                console.error('Failed to load autonomy logs:', error);
            });
        };

        $scope.restartService = function() {
            $http.post('/cgi-bin/autonomy/restart').then(function(response) {
                $scope.loadStatus();
            }).catch(function(error) {
                console.error('Failed to restart autonomy service:', error);
            });
        };

        // Load initial data
        $scope.loadStatus();
    }]);

    app.controller('AutonomyOverviewController', ['$scope', '$http', '$interval', function ($scope, $http, $interval) {
        $scope.overview = {
            running: false,
            uptime: '',
            lastCheck: '',
            issues: 0
        };

        $scope.loadOverview = function() {
            $http.get('/cgi-bin/autonomy/overview').then(function(response) {
                $scope.overview = response.data;
            }).catch(function(error) {
                console.error('Failed to load overview:', error);
            });
        };

        // Auto-refresh every 30 seconds
        var refreshInterval = $interval($scope.loadOverview, 30000);
        
        $scope.$on('$destroy', function() {
            if (refreshInterval) {
                $interval.cancel(refreshInterval);
            }
        });

        $scope.loadOverview();
    }]);

    app.controller('AutonomyConfigController', ['$scope', '$http', function ($scope, $http) {
        $scope.config = {};
        $scope.saving = false;

        $scope.loadConfig = function() {
            $http.get('/cgi-bin/autonomy/config').then(function(response) {
                $scope.config = response.data;
            }).catch(function(error) {
                console.error('Failed to load config:', error);
            });
        };

        $scope.saveConfig = function() {
            $scope.saving = true;
            $http.post('/cgi-bin/autonomy/config', $scope.config).then(function(response) {
                $scope.saving = false;
                // Show success message
            }).catch(function(error) {
                $scope.saving = false;
                console.error('Failed to save config:', error);
            });
        };

        $scope.loadConfig();
    }]);

    app.controller('AutonomyStatusController', ['$scope', '$http', '$interval', function ($scope, $http, $interval) {
        $scope.status = {
            daemon: false,
            starlink: {},
            cellular: {},
            gps: {},
            system: {}
        };

        $scope.loadStatus = function() {
            $http.get('/cgi-bin/autonomy/status').then(function(response) {
                $scope.status = response.data;
            }).catch(function(error) {
                console.error('Failed to load status:', error);
            });
        };

        // Auto-refresh every 10 seconds
        var refreshInterval = $interval($scope.loadStatus, 10000);
        
        $scope.$on('$destroy', function() {
            if (refreshInterval) {
                $interval.cancel(refreshInterval);
            }
        });

        $scope.loadStatus();
    }]);

    app.controller('AutonomyLogsController', ['$scope', '$http', '$interval', function ($scope, $http, $interval) {
        $scope.logs = [];
        $scope.autoRefresh = true;

        $scope.loadLogs = function() {
            $http.get('/cgi-bin/autonomy/logs').then(function(response) {
                $scope.logs = response.data;
            }).catch(function(error) {
                console.error('Failed to load logs:', error);
            });
        };

        // Auto-refresh every 5 seconds if enabled
        var refreshInterval = $interval(function() {
            if ($scope.autoRefresh) {
                $scope.loadLogs();
            }
        }, 5000);
        
        $scope.$on('$destroy', function() {
            if (refreshInterval) {
                $interval.cancel(refreshInterval);
            }
        });

        $scope.loadLogs();
    }]);

})();
EOF

# Compress the JavaScript file
echo "Compressing JavaScript file..."
cd "$BUILD_DIR/www/assets"
gzip -c "app.autonomy.app-${VERSION}.js" > "app.autonomy.app-${VERSION}.js.gz"
rm "app.autonomy.app-${VERSION}.js"

cd "$BUILD_DIR"

# Create control file
echo "Creating control file..."
cat > control << EOF
Package: $PACKAGE_NAME
Version: $VERSION
Depends: vuci-app-core-ui
Architecture: $ARCHITECTURE
Installed-Size: 8192
Description: VUCI UI for Autonomy Service
 Provides web interface for the Autonomy networking system.
 Includes overview, configuration, status, and logs pages.
EOF

# Create postinst script
echo "Creating postinst script..."
cat > postinst << 'EOF'
#!/bin/sh
# Post-installation script for autonomy VUCI app

# Restart VUCI services to load new menu
if [ -f /etc/init.d/rpcd ]; then
    /etc/init.d/rpcd restart
fi

if [ -f /etc/init.d/uhttpd ]; then
    /etc/init.d/uhttpd restart
fi

echo "Autonomy VUCI app installed successfully!"
exit 0
EOF

chmod +x postinst

# Create prerm script
echo "Creating prerm script..."
cat > prerm << 'EOF'
#!/bin/sh
# Pre-removal script for autonomy VUCI app

# Restart VUCI services to remove menu
if [ -f /etc/init.d/rpcd ]; then
    /etc/init.d/rpcd restart
fi

if [ -f /etc/init.d/uhttpd ]; then
    /etc/init.d/uhttpd restart
fi

echo "Autonomy VUCI app removed."
exit 0
EOF

chmod +x prerm

# Create control.tar.gz
echo "Creating control.tar.gz..."
tar -czf control.tar.gz control postinst prerm

# Create debian-binary
echo "Creating debian-binary..."
echo "2.0" > debian-binary

# Create signature file
echo "Creating signature file..."
echo "Signature: This is a placeholder signature for autonomy VUCI app" > control+data.sig

# Create data.tar.gz
echo "Creating data.tar.gz..."
tar -czf data.tar.gz usr/ www/

# Create the final IPK
echo "Creating final IPK file..."
tar -czf "${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}.ipk" debian-binary control.tar.gz data.tar.gz control+data.sig

# Move IPK to project root
mv "${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}.ipk" "$PROJECT_ROOT/"

echo "VUCI app package created successfully!"
echo "Package: ${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}.ipk"

# Show package size
PACKAGE_SIZE=$(du -h "${PROJECT_ROOT}/${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}.ipk" | cut -f1)
echo "Package size: $PACKAGE_SIZE"

# Clean up
rm -f data.tar.gz control.tar.gz debian-binary control+data.sig control postinst prerm
rm -rf "$BUILD_DIR"

cd "$PROJECT_ROOT"

echo "Build completed successfully!"
echo ""
echo "This VUCI app will integrate autonomy into the RUTOS web interface!"
echo "Install it using: opkg install ${PACKAGE_NAME}_${VERSION}_${ARCHITECTURE}.ipk"





