#!/bin/bash
set -e

echo "=== Building RUTOS Autonomy Simple UI Package ==="

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="/tmp/autonomy-simple-ui-$$"
VERSION="1.0"
ARCHITECTURE="arm_cortex-a7_neon-vfpv4"

echo "Project root: $SCRIPT_DIR"
echo "Build directory: $BUILD_DIR"

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Create UI package structure
echo "Creating UI package..."
mkdir -p "ui-package/usr/share/vuci/menu.d"
mkdir -p "ui-package/www/assets"

# Create simplified menu JSON (single page)
cat > "ui-package/usr/share/vuci/menu.d/autonomy.json" << 'EOF'
{
  "services/autonomy": {
    "title": "Autonomy",
    "index": 50,
    "view": "services/Autonomy",
    "acls": ["services/autonomy"]
  }
}
EOF

# Create simplified JavaScript UI component
cat > "ui-package/www/assets/app.autonomy.app-1.0.js.gz" << 'EOF'
'use strict';
(function () {
    'use strict';
    
    var app = angular.module('app.autonomy', ['app.core']);
    
    app.controller('AutonomyController', ['$scope', '$http', '$interval', function ($scope, $http, $interval) {
        $scope.status = {
            running: false,
            uptime: '',
            last_check: '',
            issues: []
        };
        
        $scope.config = {
            enabled: false,
            check_interval: 30,
            starlink_enabled: true,
            cellular_enabled: true,
            gps_enabled: true
        };
        
        $scope.loadStatus = function() {
            $http.get('/api/autonomy_f/status').then(function(response) {
                $scope.status = response.data;
            }).catch(function(error) {
                console.error('Failed to load status:', error);
                $scope.status = {
                    running: false,
                    uptime: '',
                    last_check: '',
                    issues: ['Failed to load status']
                };
            });
        };
        
        $scope.loadConfig = function() {
            $http.get('/api/autonomy_c/config').then(function(response) {
                if (response.data && response.data.autonomy) {
                    $scope.config = response.data.autonomy;
                }
            }).catch(function(error) {
                console.error('Failed to load config:', error);
            });
        };
        
        $scope.saveConfig = function() {
            $http.post('/api/autonomy_c/config', {
                autonomy: $scope.config
            }).then(function(response) {
                console.log('Config saved successfully');
            }).catch(function(error) {
                console.error('Failed to save config:', error);
            });
        };
        
        $scope.restartService = function() {
            $http.post('/api/autonomy_f/actions/restart').then(function(response) {
                console.log('Service restarted successfully');
                $scope.loadStatus();
            }).catch(function(error) {
                console.error('Failed to restart service:', error);
            });
        };
        
        // Load initial data
        $scope.loadStatus();
        $scope.loadConfig();
        
        // Auto-refresh status every 30 seconds
        $interval(function() {
            $scope.loadStatus();
        }, 30000);
    }]);
    
    // Register the view
    app.config(['$stateProvider', function($stateProvider) {
        $stateProvider.state('services.Autonomy', {
            url: '/services/autonomy',
            templateUrl: 'views/services/autonomy.html',
            controller: 'AutonomyController'
        });
    }]);
})();
EOF

# Create HTML template
mkdir -p "ui-package/www/views/services"
cat > "ui-package/www/views/services/autonomy.html" << 'EOF'
<div class="autonomy-page">
    <div class="row">
        <div class="col-md-6">
            <div class="card">
                <div class="card-header">
                    <h5>Autonomy Status</h5>
                </div>
                <div class="card-body">
                    <div class="row">
                        <div class="col-6">
                            <strong>Status:</strong> 
                            <span ng-class="{'text-success': status.running, 'text-danger': !status.running}">
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
                    <div class="row mt-2" ng-if="status.issues && status.issues.length > 0">
                        <div class="col-12">
                            <strong>Issues:</strong>
                            <ul class="list-unstyled">
                                <li ng-repeat="issue in status.issues" class="text-danger">{{ issue }}</li>
                            </ul>
                        </div>
                    </div>
                    <div class="row mt-3">
                        <div class="col-12">
                            <button class="btn btn-primary" ng-click="loadStatus()">Refresh</button>
                            <button class="btn btn-warning" ng-click="restartService()">Restart Service</button>
                        </div>
                    </div>
                </div>
            </div>
        </div>
        
        <div class="col-md-6">
            <div class="card">
                <div class="card-header">
                    <h5>Configuration</h5>
                </div>
                <div class="card-body">
                    <div class="form-group">
                        <label>
                            <input type="checkbox" ng-model="config.enabled"> Enable Autonomy
                        </label>
                    </div>
                    <div class="form-group">
                        <label>Check Interval (seconds):</label>
                        <input type="number" class="form-control" ng-model="config.check_interval" min="10" max="300">
                    </div>
                    <div class="form-group">
                        <label>
                            <input type="checkbox" ng-model="config.starlink_enabled"> Enable Starlink
                        </label>
                    </div>
                    <div class="form-group">
                        <label>
                            <input type="checkbox" ng-model="config.cellular_enabled"> Enable Cellular
                        </label>
                    </div>
                    <div class="form-group">
                        <label>
                            <input type="checkbox" ng-model="config.gps_enabled"> Enable GPS
                        </label>
                    </div>
                    <div class="form-group">
                        <button class="btn btn-success" ng-click="saveConfig()">Save Configuration</button>
                    </div>
                </div>
            </div>
        </div>
    </div>
</div>
EOF

# Create UI package control file
cat > "ui-package/control" << EOF
Package: vuci-app-autonomy-ui
Version: $VERSION
Depends: vuci-app-autonomy-api, uci, ubus
Architecture: $ARCHITECTURE
Installed-Size: 2048
Description: VuCI UI Support for Autonomy APP (Simplified)
EOF

# Build UI package using gzipped tar format
echo "Building UI package..."
cd "ui-package"
tar -czf control.tar.gz control
tar -czf data.tar.gz usr/ www/
echo "2.0" > debian-binary
tar -czf "vuci-app-autonomy-ui_${VERSION}_${ARCHITECTURE}.ipk" debian-binary control.tar.gz data.tar.gz
cd ..

# Copy package to project directory
echo "Copying package to project directory..."
cp "ui-package/vuci-app-autonomy-ui_${VERSION}_${ARCHITECTURE}.ipk" "$SCRIPT_DIR/"

echo "Build completed successfully!"
echo ""
echo "Package created: $SCRIPT_DIR/vuci-app-autonomy-ui_${VERSION}_${ARCHITECTURE}.ipk"
echo ""
echo "To install on device:"
echo "1. Copy package to device:"
echo "   scp $SCRIPT_DIR/vuci-app-autonomy-ui_${VERSION}_${ARCHITECTURE}.ipk root@192.168.80.1:/tmp/"
echo ""
echo "2. Install package:"
echo "   ssh root@192.168.80.1 'opkg install --force-downgrade /tmp/vuci-app-autonomy-ui_${VERSION}_${ARCHITECTURE}.ipk'"
echo ""
echo "3. Restart services:"
echo "   ssh root@192.168.80.1 '/etc/init.d/rpcd restart && /etc/init.d/uhttpd restart'"

# Cleanup
rm -rf "$BUILD_DIR"





