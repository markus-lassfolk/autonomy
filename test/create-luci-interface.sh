#!/bin/bash

# Create LuCI-style web interface for OpenWrt WSL testing
# This creates a realistic OpenWrt web interface

set -e

echo "=== Creating LuCI-style web interface ==="

# Create web interface directory
WEB_DIR="/var/www/luci"
mkdir -p $WEB_DIR/{css,js,images}

# Create main LuCI-style HTML
cat > $WEB_DIR/index.html << 'EOF'
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>LuCI - OpenWrt WSL Test Environment</title>
    <link rel="stylesheet" href="css/luci.css">
    <link rel="icon" href="images/favicon.ico" type="image/x-icon">
</head>
<body>
    <div class="header">
        <div class="header-content">
            <div class="logo">
                <img src="images/openwrt-logo.png" alt="OpenWrt" class="logo-img">
                <span class="logo-text">LuCI</span>
            </div>
            <div class="header-info">
                <span class="hostname">openwrt-test</span>
                <span class="uptime" id="uptime">Uptime: 0 days, 0 hours, 0 minutes</span>
            </div>
        </div>
    </div>

    <div class="container">
        <div class="sidebar">
            <nav class="nav-menu">
                <div class="nav-section">
                    <h3>System</h3>
                    <ul>
                        <li><a href="#" class="nav-link active" data-page="overview">Overview</a></li>
                        <li><a href="#" class="nav-link" data-page="system">System</a></li>
                        <li><a href="#" class="nav-link" data-page="software">Software</a></li>
                        <li><a href="#" class="nav-link" data-page="startup">Startup</a></li>
                    </ul>
                </div>
                <div class="nav-section">
                    <h3>Network</h3>
                    <ul>
                        <li><a href="#" class="nav-link" data-page="interfaces">Interfaces</a></li>
                        <li><a href="#" class="nav-link" data-page="wireless">Wireless</a></li>
                        <li><a href="#" class="nav-link" data-page="dhcp">DHCP and DNS</a></li>
                        <li><a href="#" class="nav-link" data-page="firewall">Firewall</a></li>
                    </ul>
                </div>
                <div class="nav-section">
                    <h3>Services</h3>
                    <ul>
                        <li><a href="#" class="nav-link" data-page="autonomy">Autonomy System</a></li>
                        <li><a href="#" class="nav-link" data-page="starlink">Starlink</a></li>
                        <li><a href="#" class="nav-link" data-page="cellular">Cellular</a></li>
                        <li><a href="#" class="nav-link" data-page="gps">GPS</a></li>
                    </ul>
                </div>
            </nav>
        </div>

        <div class="main-content">
            <div id="overview" class="page active">
                <h2>System Overview</h2>
                <div class="status-grid">
                    <div class="status-card">
                        <h3>System Status</h3>
                        <div class="status-item">
                            <span class="label">Hostname:</span>
                            <span class="value">openwrt-test</span>
                        </div>
                        <div class="status-item">
                            <span class="label">Model:</span>
                            <span class="value">WSL Test Environment</span>
                        </div>
                        <div class="status-item">
                            <span class="label">Architecture:</span>
                            <span class="value">x86_64</span>
                        </div>
                        <div class="status-item">
                            <span class="label">Kernel:</span>
                            <span class="value">6.6.87.2-microsoft-standard-WSL2</span>
                        </div>
                    </div>

                    <div class="status-card">
                        <h3>Network Interfaces</h3>
                        <div class="status-item">
                            <span class="label">eth0:</span>
                            <span class="value">172.26.83.101/20</span>
                        </div>
                        <div class="status-item">
                            <span class="label">lo:</span>
                            <span class="value">127.0.0.1/8</span>
                        </div>
                        <div class="status-item">
                            <span class="label">Status:</span>
                            <span class="value online">Online</span>
                        </div>
                    </div>

                    <div class="status-card">
                        <h3>Autonomy System</h3>
                        <div class="status-item">
                            <span class="label">Status:</span>
                            <span class="value running">Running</span>
                        </div>
                        <div class="status-item">
                            <span class="label">Version:</span>
                            <span class="value">1.0.0</span>
                        </div>
                        <div class="status-item">
                            <span class="label">Uptime:</span>
                            <span class="value" id="autonomy-uptime">0:00:00</span>
                        </div>
                    </div>

                    <div class="status-card">
                        <h3>Services</h3>
                        <div class="status-item">
                            <span class="label">ubus:</span>
                            <span class="value running">Running</span>
                        </div>
                        <div class="status-item">
                            <span class="label">uci:</span>
                            <span class="value running">Running</span>
                        </div>
                        <div class="status-item">
                            <span class="label">mwan3:</span>
                            <span class="value running">Running</span>
                        </div>
                    </div>
                </div>
            </div>

            <div id="autonomy" class="page">
                <h2>Autonomy System</h2>
                <div class="autonomy-dashboard">
                    <div class="autonomy-card">
                        <h3>System Management</h3>
                        <div class="control-group">
                            <button class="btn btn-primary" onclick="startAutonomy()">Start Service</button>
                            <button class="btn btn-warning" onclick="stopAutonomy()">Stop Service</button>
                            <button class="btn btn-info" onclick="restartAutonomy()">Restart Service</button>
                        </div>
                        <div class="status-display">
                            <pre id="autonomy-status">{"status": "running", "version": "1.0.0"}</pre>
                        </div>
                    </div>

                    <div class="autonomy-card">
                        <h3>Network Interfaces</h3>
                        <div class="interface-list" id="interface-list">
                            <!-- Interfaces will be populated by JavaScript -->
                        </div>
                    </div>

                    <div class="autonomy-card">
                        <h3>Configuration</h3>
                        <div class="config-section">
                            <h4>System Configuration</h4>
                            <pre id="system-config">config system
    option hostname "openwrt-test"
    option timezone "UTC"</pre>
                        </div>
                    </div>
                </div>
            </div>

            <div id="system" class="page">
                <h2>System Configuration</h2>
                <div class="config-form">
                    <div class="form-group">
                        <label for="hostname">Hostname:</label>
                        <input type="text" id="hostname" value="openwrt-test" class="form-control">
                    </div>
                    <div class="form-group">
                        <label for="timezone">Timezone:</label>
                        <select id="timezone" class="form-control">
                            <option value="UTC" selected>UTC</option>
                            <option value="America/New_York">America/New_York</option>
                            <option value="Europe/London">Europe/London</option>
                        </select>
                    </div>
                    <div class="form-group">
                        <button class="btn btn-primary" onclick="saveSystemConfig()">Save Configuration</button>
                    </div>
                </div>
            </div>

            <div id="interfaces" class="page">
                <h2>Network Interfaces</h2>
                <div class="interface-grid">
                    <div class="interface-card">
                        <h3>eth0 (WAN)</h3>
                        <div class="interface-info">
                            <div class="info-item">
                                <span class="label">Status:</span>
                                <span class="value online">Up</span>
                            </div>
                            <div class="info-item">
                                <span class="label">IP Address:</span>
                                <span class="value">172.26.83.101/20</span>
                            </div>
                            <div class="info-item">
                                <span class="label">MAC Address:</span>
                                <span class="value">00:15:5d:53:fa:55</span>
                            </div>
                        </div>
                    </div>

                    <div class="interface-card">
                        <h3>lo (Loopback)</h3>
                        <div class="interface-info">
                            <div class="info-item">
                                <span class="label">Status:</span>
                                <span class="value online">Up</span>
                            </div>
                            <div class="info-item">
                                <span class="label">IP Address:</span>
                                <span class="value">127.0.0.1/8</span>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    </div>

    <script src="js/luci.js"></script>
</body>
</html>
EOF

# Create CSS styles
cat > $WEB_DIR/css/luci.css << 'EOF'
/* LuCI-style CSS for OpenWrt WSL Test Environment */
* {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
}

body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    background-color: #f5f5f5;
    color: #333;
    line-height: 1.6;
}

.header {
    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
    color: white;
    padding: 1rem 0;
    box-shadow: 0 2px 4px rgba(0,0,0,0.1);
}

.header-content {
    max-width: 1200px;
    margin: 0 auto;
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 0 2rem;
}

.logo {
    display: flex;
    align-items: center;
    gap: 1rem;
}

.logo-img {
    width: 40px;
    height: 40px;
    background: white;
    border-radius: 8px;
    padding: 4px;
}

.logo-text {
    font-size: 1.5rem;
    font-weight: bold;
}

.header-info {
    display: flex;
    flex-direction: column;
    align-items: flex-end;
    gap: 0.5rem;
}

.hostname {
    font-weight: bold;
    font-size: 1.1rem;
}

.uptime {
    font-size: 0.9rem;
    opacity: 0.9;
}

.container {
    max-width: 1200px;
    margin: 0 auto;
    display: flex;
    gap: 2rem;
    padding: 2rem;
}

.sidebar {
    width: 250px;
    background: white;
    border-radius: 8px;
    box-shadow: 0 2px 4px rgba(0,0,0,0.1);
    padding: 1.5rem;
}

.nav-section {
    margin-bottom: 2rem;
}

.nav-section h3 {
    color: #667eea;
    font-size: 0.9rem;
    text-transform: uppercase;
    letter-spacing: 0.5px;
    margin-bottom: 0.5rem;
    border-bottom: 1px solid #eee;
    padding-bottom: 0.5rem;
}

.nav-menu ul {
    list-style: none;
}

.nav-menu li {
    margin-bottom: 0.25rem;
}

.nav-link {
    display: block;
    padding: 0.5rem 0.75rem;
    color: #666;
    text-decoration: none;
    border-radius: 4px;
    transition: all 0.2s;
}

.nav-link:hover {
    background-color: #f8f9fa;
    color: #667eea;
}

.nav-link.active {
    background-color: #667eea;
    color: white;
}

.main-content {
    flex: 1;
    background: white;
    border-radius: 8px;
    box-shadow: 0 2px 4px rgba(0,0,0,0.1);
    padding: 2rem;
}

.page {
    display: none;
}

.page.active {
    display: block;
}

.page h2 {
    color: #333;
    margin-bottom: 1.5rem;
    padding-bottom: 0.5rem;
    border-bottom: 2px solid #667eea;
}

.status-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
    gap: 1.5rem;
}

.status-card {
    background: #f8f9fa;
    border-radius: 8px;
    padding: 1.5rem;
    border-left: 4px solid #667eea;
}

.status-card h3 {
    color: #667eea;
    margin-bottom: 1rem;
    font-size: 1.1rem;
}

.status-item {
    display: flex;
    justify-content: space-between;
    margin-bottom: 0.5rem;
    padding: 0.5rem 0;
    border-bottom: 1px solid #eee;
}

.status-item:last-child {
    border-bottom: none;
}

.label {
    font-weight: 500;
    color: #666;
}

.value {
    font-weight: 600;
    color: #333;
}

.value.online, .value.running {
    color: #28a745;
}

.value.offline, .value.stopped {
    color: #dc3545;
}

.autonomy-dashboard {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(350px, 1fr));
    gap: 1.5rem;
}

.autonomy-card {
    background: #f8f9fa;
    border-radius: 8px;
    padding: 1.5rem;
    border-left: 4px solid #28a745;
}

.autonomy-card h3 {
    color: #28a745;
    margin-bottom: 1rem;
}

.control-group {
    display: flex;
    gap: 0.5rem;
    margin-bottom: 1rem;
    flex-wrap: wrap;
}

.btn {
    padding: 0.5rem 1rem;
    border: none;
    border-radius: 4px;
    cursor: pointer;
    font-weight: 500;
    transition: all 0.2s;
    text-decoration: none;
    display: inline-block;
}

.btn-primary {
    background-color: #667eea;
    color: white;
}

.btn-primary:hover {
    background-color: #5a6fd8;
}

.btn-warning {
    background-color: #ffc107;
    color: #212529;
}

.btn-warning:hover {
    background-color: #e0a800;
}

.btn-info {
    background-color: #17a2b8;
    color: white;
}

.btn-info:hover {
    background-color: #138496;
}

.status-display {
    background: #2d3748;
    color: #e2e8f0;
    padding: 1rem;
    border-radius: 4px;
    font-family: 'Courier New', monospace;
    font-size: 0.9rem;
    overflow-x: auto;
}

.config-form {
    max-width: 600px;
}

.form-group {
    margin-bottom: 1.5rem;
}

.form-group label {
    display: block;
    margin-bottom: 0.5rem;
    font-weight: 500;
    color: #666;
}

.form-control {
    width: 100%;
    padding: 0.75rem;
    border: 1px solid #ddd;
    border-radius: 4px;
    font-size: 1rem;
    transition: border-color 0.2s;
}

.form-control:focus {
    outline: none;
    border-color: #667eea;
    box-shadow: 0 0 0 2px rgba(102, 126, 234, 0.2);
}

.interface-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
    gap: 1.5rem;
}

.interface-card {
    background: #f8f9fa;
    border-radius: 8px;
    padding: 1.5rem;
    border-left: 4px solid #17a2b8;
}

.interface-card h3 {
    color: #17a2b8;
    margin-bottom: 1rem;
}

.interface-info {
    display: flex;
    flex-direction: column;
    gap: 0.5rem;
}

.info-item {
    display: flex;
    justify-content: space-between;
    padding: 0.5rem 0;
    border-bottom: 1px solid #eee;
}

.info-item:last-child {
    border-bottom: none;
}

@media (max-width: 768px) {
    .container {
        flex-direction: column;
        padding: 1rem;
    }

    .sidebar {
        width: 100%;
    }

    .status-grid {
        grid-template-columns: 1fr;
    }

    .autonomy-dashboard {
        grid-template-columns: 1fr;
    }
}
EOF

# Create JavaScript functionality
cat > $WEB_DIR/js/luci.js << 'EOF'
// LuCI-style JavaScript for OpenWrt WSL Test Environment

document.addEventListener('DOMContentLoaded', function() {
    // Navigation
    const navLinks = document.querySelectorAll('.nav-link');
    const pages = document.querySelectorAll('.page');

    navLinks.forEach(link => {
        link.addEventListener('click', function(e) {
            e.preventDefault();

            // Remove active class from all links and pages
            navLinks.forEach(l => l.classList.remove('active'));
            pages.forEach(p => p.classList.remove('active'));

            // Add active class to clicked link
            this.classList.add('active');

            // Show corresponding page
            const targetPage = this.getAttribute('data-page');
            document.getElementById(targetPage).classList.add('active');
        });
    });

    // Update uptime
    function updateUptime() {
        const startTime = new Date();
        const now = new Date();
        const diff = now - startTime;

        const days = Math.floor(diff / (1000 * 60 * 60 * 24));
        const hours = Math.floor((diff % (1000 * 60 * 60 * 24)) / (1000 * 60 * 60));
        const minutes = Math.floor((diff % (1000 * 60 * 60)) / (1000 * 60));

        document.getElementById('uptime').textContent =
            `Uptime: ${days} days, ${hours} hours, ${minutes} minutes`;

        document.getElementById('autonomy-uptime').textContent =
            `${hours}:${minutes.toString().padStart(2, '0')}:${Math.floor((diff % (1000 * 60)) / 1000).toString().padStart(2, '0')}`;
    }

    // Update uptime every second
    setInterval(updateUptime, 1000);
    updateUptime();

    // Autonomy system controls
    window.startAutonomy = function() {
        fetch('/api/autonomy/start', { method: 'POST' })
            .then(response => response.json())
            .then(data => {
                document.getElementById('autonomy-status').textContent = JSON.stringify(data, null, 2);
            })
            .catch(error => {
                console.error('Error:', error);
                document.getElementById('autonomy-status').textContent = '{"error": "Failed to start service"}';
            });
    };

    window.stopAutonomy = function() {
        fetch('/api/autonomy/stop', { method: 'POST' })
            .then(response => response.json())
            .then(data => {
                document.getElementById('autonomy-status').textContent = JSON.stringify(data, null, 2);
            })
            .catch(error => {
                console.error('Error:', error);
                document.getElementById('autonomy-status').textContent = '{"error": "Failed to stop service"}';
            });
    };

    window.restartAutonomy = function() {
        fetch('/api/autonomy/restart', { method: 'POST' })
            .then(response => response.json())
            .then(data => {
                document.getElementById('autonomy-status').textContent = JSON.stringify(data, null, 2);
            })
            .catch(error => {
                console.error('Error:', error);
                document.getElementById('autonomy-status').textContent = '{"error": "Failed to restart service"}';
            });
    };

    window.saveSystemConfig = function() {
        const hostname = document.getElementById('hostname').value;
        const timezone = document.getElementById('timezone').value;

        const config = {
            hostname: hostname,
            timezone: timezone
        };

        fetch('/api/system/config', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(config)
        })
        .then(response => response.json())
        .then(data => {
            alert('Configuration saved successfully!');
        })
        .catch(error => {
            console.error('Error:', error);
            alert('Failed to save configuration');
        });
    };

    // Load initial data
    loadAutonomyStatus();
    loadSystemConfig();
});

function loadAutonomyStatus() {
    // Simulate API call
    setTimeout(() => {
        const status = {
            status: "running",
            version: "1.0.0",
            uptime: "0:00:00",
            interfaces: [
                { name: "eth0", status: "up", ip: "172.26.83.101/20" },
                { name: "lo", status: "up", ip: "127.0.0.1/8" }
            ]
        };
        document.getElementById('autonomy-status').textContent = JSON.stringify(status, null, 2);
    }, 1000);
}

function loadSystemConfig() {
    // Simulate loading system config
    const config = `config system
    option hostname "openwrt-test"
    option timezone "UTC"`;
    document.getElementById('system-config').textContent = config;
}
EOF

# Create simple images (placeholder)
mkdir -p $WEB_DIR/images
echo "Creating placeholder images..."

# Create a simple favicon
cat > $WEB_DIR/images/favicon.ico << 'EOF'
# This is a placeholder for favicon.ico
EOF

# Create a simple OpenWrt logo placeholder
cat > $WEB_DIR/images/openwrt-logo.png << 'EOF'
# This is a placeholder for openwrt-logo.png
EOF

# Configure nginx
sudo tee /etc/nginx/sites-available/luci << 'EOF'
server {
    listen 80;
    server_name _;
    root /var/www/luci;
    index index.html;

    location / {
        try_files $uri $uri/ =404;
    }

    location /api/ {
        # Proxy API calls to our mock backend
        proxy_pass http://127.0.0.1:8081/;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }

    location ~* \.(css|js|png|jpg|jpeg|gif|ico|svg)$ {
        expires 1y;
        add_header Cache-Control "public, immutable";
    }
}
EOF

# Enable the site
sudo ln -sf /etc/nginx/sites-available/luci /etc/nginx/sites-enabled/
sudo rm -f /etc/nginx/sites-enabled/default

# Create a simple API backend
cat > $WEB_DIR/api-server.py << 'EOF'
#!/usr/bin/env python3
import json
import subprocess
import time
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs

class LuCIAPIHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        parsed_path = urlparse(self.path)

        if parsed_path.path == '/api/autonomy/status':
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()

            status = {
                "status": "running",
                "version": "1.0.0",
                "uptime": "0:00:00",
                "interfaces": [
                    {"name": "eth0", "status": "up", "ip": "172.26.83.101/20"},
                    {"name": "lo", "status": "up", "ip": "127.0.0.1/8"}
                ]
            }
            self.wfile.write(json.dumps(status).encode())

        elif parsed_path.path == '/api/system/config':
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()

            config = {
                "hostname": "openwrt-test",
                "timezone": "UTC",
                "config": "config system\n    option hostname \"openwrt-test\"\n    option timezone \"UTC\""
            }
            self.wfile.write(json.dumps(config).encode())

        else:
            self.send_response(404)
            self.end_headers()
            self.wfile.write(b'Not found')

    def do_POST(self):
        parsed_path = urlparse(self.path)

        if parsed_path.path in ['/api/autonomy/start', '/api/autonomy/stop', '/api/autonomy/restart']:
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()

            action = parsed_path.path.split('/')[-1]
            response = {
                "status": "success",
                "message": f"Autonomy service {action}ed successfully",
                "timestamp": time.time()
            }
            self.wfile.write(json.dumps(response).encode())

        elif parsed_path.path == '/api/system/config':
            content_length = int(self.headers['Content-Length'])
            post_data = self.rfile.read(content_length)
            config = json.loads(post_data.decode())

            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()

            response = {
                "status": "success",
                "message": "Configuration saved successfully",
                "config": config
            }
            self.wfile.write(json.dumps(response).encode())

        else:
            self.send_response(404)
            self.end_headers()
            self.wfile.write(b'Not found')

    def log_message(self, format, *args):
        # Suppress access logs
        pass

if __name__ == '__main__':
    server = HTTPServer(('localhost', 8081), LuCIAPIHandler)
    print('Starting LuCI API server on port 8081...')
    server.serve_forever()
EOF

chmod +x $WEB_DIR/api-server.py

# Start services
echo "Starting services..."
sudo systemctl start nginx
sudo systemctl enable nginx

# Start API server in background
cd $WEB_DIR
nohup python3 api-server.py > api-server.log 2>&1 &

echo "=== LuCI-style web interface created successfully! ==="
echo ""
echo "Access your OpenWrt LuCI interface at:"
echo "http://172.26.83.101"
echo ""
echo "Features:"
echo "- LuCI-style navigation and interface"
echo "- System overview with real-time status"
echo "- Autonomy system management"
echo "- Network interface monitoring"
echo "- Configuration management"
echo "- Responsive design for mobile/desktop"
echo ""
echo "The interface includes:"
echo "- Mock API endpoints for testing"
echo "- Real-time uptime display"
echo "- System configuration forms"
echo "- Network interface status"
echo "- Autonomy service controls"
