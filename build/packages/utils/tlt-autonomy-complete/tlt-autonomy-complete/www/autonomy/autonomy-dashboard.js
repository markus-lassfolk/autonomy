// Autonomy Dashboard JavaScript
// Handles all UBUS method calls and UI interactions

class AutonomyDashboard {
    constructor() {
        this.charts = {};
        this.updateInterval = null;
        this.isUpdating = false;
        this.init();
    }
    
    init() {
        this.setupCharts();
        this.loadInitialData();
        this.startAutoUpdate();
        this.setupEventListeners();
    }
    
    setupCharts() {
        // Performance Chart
        const performanceCtx = document.getElementById('performanceChart');
        if (performanceCtx) {
            this.charts.performance = new Chart(performanceCtx, {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [{
                        label: 'Latency (ms)',
                        data: [],
                        borderColor: '#2196F3',
                        backgroundColor: 'rgba(33, 150, 243, 0.1)',
                        tension: 0.4
                    }, {
                        label: 'Throughput (Mbps)',
                        data: [],
                        borderColor: '#4CAF50',
                        backgroundColor: 'rgba(76, 175, 80, 0.1)',
                        tension: 0.4
                    }]
                },
                options: {
                    responsive: true,
                    scales: {
                        y: {
                            beginAtZero: true
                        }
                    }
                }
            });
        }
        
        // Starlink Chart
        const starlinkCtx = document.getElementById('starlinkChart');
        if (starlinkCtx) {
            this.charts.starlink = new Chart(starlinkCtx, {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [{
                        label: 'SNR (dB)',
                        data: [],
                        borderColor: '#FF9800',
                        backgroundColor: 'rgba(255, 152, 0, 0.1)',
                        tension: 0.4
                    }, {
                        label: 'Obstruction (%)',
                        data: [],
                        borderColor: '#f44336',
                        backgroundColor: 'rgba(244, 67, 54, 0.1)',
                        tension: 0.4
                    }]
                },
                options: {
                    responsive: true,
                    scales: {
                        y: {
                            beginAtZero: true
                        }
                    }
                }
            });
        }
        
        // ML Chart
        const mlCtx = document.getElementById('mlChart');
        if (mlCtx) {
            this.charts.ml = new Chart(mlCtx, {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [{
                        label: 'Prediction Accuracy (%)',
                        data: [],
                        borderColor: '#9C27B0',
                        backgroundColor: 'rgba(156, 39, 176, 0.1)',
                        tension: 0.4
                    }, {
                        label: 'Confidence (%)',
                        data: [],
                        borderColor: '#673AB7',
                        backgroundColor: 'rgba(103, 58, 183, 0.1)',
                        tension: 0.4
                    }]
                },
                options: {
                    responsive: true,
                    scales: {
                        y: {
                            beginAtZero: true,
                            max: 100
                        }
                    }
                }
            });
        }
    }
    
    setupEventListeners() {
        // Configuration form listeners
        const configInputs = document.querySelectorAll('#config input, #config select');
        configInputs.forEach(input => {
            input.addEventListener('change', () => {
                this.markConfigChanged();
            });
        });
    }
    
    markConfigChanged() {
        const saveBtn = document.querySelector('button[onclick="saveConfig()"]');
        if (saveBtn) {
            saveBtn.style.background = '#ff9800';
            saveBtn.textContent = 'Save Configuration *';
        }
    }
    
    async loadInitialData() {
        try {
            await this.updateOverview();
            await this.updateInterfaces();
            await this.updateStarlink();
            await this.updateML();
            await this.updateGPS();
            await this.updateSystem();
            await this.loadConfig();
        } catch (error) {
            console.error('Error loading initial data:', error);
            this.showError('Failed to load initial data: ' + error.message);
        }
    }
    
    startAutoUpdate() {
        this.updateInterval = setInterval(() => {
            if (!this.isUpdating) {
                this.updateCurrentTab();
            }
        }, 5000); // Update every 5 seconds
    }
    
    async updateCurrentTab() {
        const activeTab = document.querySelector('.nav-tab.active');
        if (!activeTab) return;
        
        const tabId = activeTab.getAttribute('onclick').match(/'([^']+)'/)[1];
        
        try {
            this.isUpdating = true;
            switch (tabId) {
                case 'overview':
                    await this.updateOverview();
                    break;
                case 'interfaces':
                    await this.updateInterfaces();
                    break;
                case 'starlink':
                    await this.updateStarlink();
                    break;
                case 'ml':
                    await this.updateML();
                    break;
                case 'gps':
                    await this.updateGPS();
                    break;
                case 'system':
                    await this.updateSystem();
                    break;
            }
        } catch (error) {
            console.error('Error updating tab:', error);
        } finally {
            this.isUpdating = false;
        }
    }
    
    // UBUS Method Calls
    async callUBUSMethod(method, params = {}) {
        try {
            const response = await fetch('/ubus', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    jsonrpc: '2.0',
                    id: Date.now(),
                    method: 'call',
                    params: [
                        '00000000000000000000000000000000',
                        'autonomy',
                        method,
                        params
                    ]
                })
            });
            
            const data = await response.json();
            if (data.error) {
                throw new Error(data.error.message);
            }
            
            return data.result[1];
        } catch (error) {
            console.error(`UBUS method ${method} failed:`, error);
            throw error;
        }
    }
    
    // Overview Tab Updates
    async updateOverview() {
        try {
            const [status, health, networkStatus] = await Promise.all([
                this.callUBUSMethod('status'),
                this.callUBUSMethod('health'),
                this.callUBUSMethod('network_status')
            ]);
            
            // Update daemon status
            document.getElementById('daemon-status').textContent = status.state || 'Unknown';
            document.getElementById('daemon-uptime').textContent = `Uptime: ${this.formatUptime(status.uptime)}`;
            
            // Update active interfaces
            const interfaces = networkStatus.interfaces || [];
            const activeInterfaces = interfaces.filter(iface => iface.status === 'up').length;
            document.getElementById('active-interfaces').textContent = activeInterfaces;
            document.getElementById('interface-health').textContent = `${interfaces.length} total`;
            
            // Update system health
            document.getElementById('system-health').textContent = health.status || 'Unknown';
            document.getElementById('system-load').textContent = `Version: ${health.version}`;
            
            // Update performance chart
            this.updatePerformanceChart();
            
        } catch (error) {
            console.error('Error updating overview:', error);
        }
    }
    
    // Interfaces Tab Updates
    async updateInterfaces() {
        try {
            const networkStatus = await this.callUBUSMethod('network_status');
            const interfaces = networkStatus.interfaces || [];
            
            const interfacesGrid = document.getElementById('interfaces-grid');
            interfacesGrid.innerHTML = '';
            
            interfaces.forEach(interface => {
                const interfaceCard = this.createInterfaceCard(interface);
                interfacesGrid.appendChild(interfaceCard);
            });
            
        } catch (error) {
            console.error('Error updating interfaces:', error);
        }
    }
    
    createInterfaceCard(interface) {
        const card = document.createElement('div');
        card.className = 'interface-card';
        
        const statusClass = interface.status === 'up' ? 'online' : 
                           interface.status === 'down' ? 'offline' : 'degraded';
        
        card.innerHTML = `
            <div class="interface-header">
                <div class="interface-name">${interface.name}</div>
                <div class="interface-status ${statusClass}">${interface.status}</div>
            </div>
            <div class="interface-metrics">
                <div class="metric">
                    <div class="metric-label">Type</div>
                    <div class="metric-value">${interface.type || 'Unknown'}</div>
                </div>
                <div class="metric">
                    <div class="metric-label">Health</div>
                    <div class="metric-value">${interface.health || 0}%</div>
                </div>
                <div class="metric">
                    <div class="metric-label">Latency</div>
                    <div class="metric-value">${interface.latency || 0}ms</div>
                </div>
                <div class="metric">
                    <div class="metric-label">Speed</div>
                    <div class="metric-value">${interface.speed || 0}Mbps</div>
                </div>
            </div>
        `;
        
        return card;
    }
    
    // Starlink Tab Updates
    async updateStarlink() {
        try {
            const [starlinkStatus, starlinkHealth, starlinkLocation] = await Promise.all([
                this.callUBUSMethod('starlink_status'),
                this.callUBUSMethod('starlink_health'),
                this.callUBUSMethod('starlink_location')
            ]);
            
            // Update Starlink status
            document.getElementById('starlink-status').textContent = starlinkStatus.status || 'Unknown';
            document.getElementById('starlink-health').textContent = `Health: ${starlinkHealth.health || 0}%`;
            
            // Update obstruction level
            document.getElementById('obstruction-level').textContent = `${starlinkStatus.obstruction || 0}%`;
            document.getElementById('obstruction-trend').textContent = starlinkStatus.obstruction_trend || 'Stable';
            
            // Update satellite count
            document.getElementById('satellite-count').textContent = starlinkStatus.satellites || 0;
            document.getElementById('satellite-quality').textContent = starlinkStatus.signal_quality || 'Unknown';
            
            // Update data usage
            document.getElementById('data-usage').textContent = `${starlinkStatus.data_usage || 0}GB`;
            document.getElementById('data-trend').textContent = starlinkStatus.data_trend || 'Stable';
            
            // Update Starlink chart
            this.updateStarlinkChart(starlinkStatus);
            
        } catch (error) {
            console.error('Error updating Starlink:', error);
        }
    }
    
    // ML Tab Updates
    async updateML() {
        try {
            const [mlStatus, mlStats, mlAnalytics] = await Promise.all([
                this.callUBUSMethod('ml_monitor.status'),
                this.callUBUSMethod('ml_monitor.get_statistics'),
                this.callUBUSMethod('ml_monitor.get_analytics_summary')
            ]);
            
            // Update ML status
            document.getElementById('ml-status').textContent = mlStatus.status || 'Unknown';
            document.getElementById('ml-phase').textContent = `Phase: ${mlStatus.phase || 'Unknown'}`;
            
            // Update prediction accuracy
            document.getElementById('prediction-accuracy').textContent = `${mlStats.accuracy || 0}%`;
            document.getElementById('accuracy-trend').textContent = mlStats.accuracy_trend || 'Stable';
            
            // Update observations
            document.getElementById('observations-count').textContent = mlStats.observations || 0;
            document.getElementById('learning-rate').textContent = `Rate: ${mlStats.learning_rate || 0}%`;
            
            // Update interface scores
            document.getElementById('interface-scores').textContent = mlAnalytics.interface_count || 0;
            document.getElementById('score-trend').textContent = mlAnalytics.score_trend || 'Stable';
            
            // Update ML chart
            this.updateMLChart(mlStats);
            
        } catch (error) {
            console.error('Error updating ML:', error);
        }
    }
    
    // GPS Tab Updates
    async updateGPS() {
        try {
            const [gpsStatus, gpsSources, gpsHealth] = await Promise.all([
                this.callUBUSMethod('gps_status'),
                this.callUBUSMethod('gps_sources'),
                this.callUBUSMethod('gps_health_check')
            ]);
            
            // Update GPS status
            document.getElementById('gps-status').textContent = gpsStatus.status || 'Unknown';
            document.getElementById('gps-accuracy').textContent = `Accuracy: ${gpsStatus.accuracy || 0}m`;
            
            // Update active sources
            document.getElementById('gps-sources').textContent = gpsSources.count || 0;
            document.getElementById('gps-quality').textContent = gpsSources.quality || 'Unknown';
            
            // Update location
            const lat = gpsStatus.latitude || 0;
            const lon = gpsStatus.longitude || 0;
            document.getElementById('gps-location').textContent = `${lat.toFixed(6)}, ${lon.toFixed(6)}`;
            document.getElementById('location-accuracy').textContent = `±${gpsStatus.accuracy || 0}m`;
            
            // Update movement
            document.getElementById('gps-movement').textContent = gpsStatus.movement || 'Stationary';
            document.getElementById('movement-speed').textContent = `${gpsStatus.speed || 0} km/h`;
            
        } catch (error) {
            console.error('Error updating GPS:', error);
        }
    }
    
    // System Tab Updates
    async updateSystem() {
        try {
            const [systemStatus, systemHealth] = await Promise.all([
                this.callUBUSMethod('system_status'),
                this.callUBUSMethod('system_health_check')
            ]);
            
            // Update system status
            document.getElementById('system-status').textContent = systemStatus.status || 'Unknown';
            document.getElementById('system-uptime').textContent = `Uptime: ${this.formatUptime(systemStatus.uptime)}`;
            
            // Update CPU usage
            document.getElementById('cpu-usage').textContent = `${systemHealth.cpu_usage || 0}%`;
            document.getElementById('cpu-trend').textContent = systemHealth.cpu_trend || 'Stable';
            
            // Update memory usage
            document.getElementById('memory-usage').textContent = `${systemHealth.memory_usage || 0}%`;
            document.getElementById('memory-trend').textContent = systemHealth.memory_trend || 'Stable';
            
            // Update services
            document.getElementById('services-status').textContent = systemHealth.services_running || 0;
            document.getElementById('services-health').textContent = `${systemHealth.services_total || 0} total`;
            
        } catch (error) {
            console.error('Error updating system:', error);
        }
    }
    
    // Configuration Management
    async loadConfig() {
        try {
            const config = await this.callUBUSMethod('config');
            
            // Update form fields
            document.getElementById('daemon-mode').value = config.daemon_mode || 1;
            document.getElementById('debug-mode').value = config.debug_mode || 0;
            document.getElementById('log-level').value = config.log_level || 2;
            document.getElementById('check-interval').value = config.check_interval || 30;
            document.getElementById('auto-failover').value = config.auto_failover || 1;
            document.getElementById('failover-timeout').value = config.failover_timeout || 60;
            document.getElementById('min-health').value = config.min_interface_health || 50;
            document.getElementById('mwan3-integration').value = config.mwan3_integration || 1;
            
        } catch (error) {
            console.error('Error loading config:', error);
        }
    }
    
    async saveConfig() {
        try {
            const config = {
                daemon_mode: parseInt(document.getElementById('daemon-mode').value),
                debug_mode: parseInt(document.getElementById('debug-mode').value),
                log_level: parseInt(document.getElementById('log-level').value),
                check_interval: parseInt(document.getElementById('check-interval').value),
                auto_failover: parseInt(document.getElementById('auto-failover').value),
                failover_timeout: parseInt(document.getElementById('failover-timeout').value),
                min_interface_health: parseInt(document.getElementById('min-health').value),
                mwan3_integration: parseInt(document.getElementById('mwan3-integration').value)
            };
            
            await this.callUBUSMethod('config', config);
            this.showSuccess('Configuration saved successfully');
            
            // Reset save button
            const saveBtn = document.querySelector('button[onclick="saveConfig()"]');
            if (saveBtn) {
                saveBtn.style.background = '#4CAF50';
                saveBtn.textContent = 'Save Configuration';
            }
            
        } catch (error) {
            console.error('Error saving config:', error);
            this.showError('Failed to save configuration: ' + error.message);
        }
    }
    
    // Chart Updates
    updatePerformanceChart() {
        if (!this.charts.performance) return;
        
        const now = new Date().toLocaleTimeString();
        const latency = Math.random() * 100 + 10; // Mock data
        const throughput = Math.random() * 50 + 10; // Mock data
        
        this.charts.performance.data.labels.push(now);
        this.charts.performance.data.datasets[0].data.push(latency);
        this.charts.performance.data.datasets[1].data.push(throughput);
        
        // Keep only last 20 data points
        if (this.charts.performance.data.labels.length > 20) {
            this.charts.performance.data.labels.shift();
            this.charts.performance.data.datasets[0].data.shift();
            this.charts.performance.data.datasets[1].data.shift();
        }
        
        this.charts.performance.update();
    }
    
    updateStarlinkChart(starlinkData) {
        if (!this.charts.starlink) return;
        
        const now = new Date().toLocaleTimeString();
        const snr = starlinkData.snr || Math.random() * 20 + 10;
        const obstruction = starlinkData.obstruction || Math.random() * 10;
        
        this.charts.starlink.data.labels.push(now);
        this.charts.starlink.data.datasets[0].data.push(snr);
        this.charts.starlink.data.datasets[1].data.push(obstruction);
        
        // Keep only last 20 data points
        if (this.charts.starlink.data.labels.length > 20) {
            this.charts.starlink.data.labels.shift();
            this.charts.starlink.data.datasets[0].data.shift();
            this.charts.starlink.data.datasets[1].data.shift();
        }
        
        this.charts.starlink.update();
    }
    
    updateMLChart(mlData) {
        if (!this.charts.ml) return;
        
        const now = new Date().toLocaleTimeString();
        const accuracy = mlData.accuracy || Math.random() * 20 + 80;
        const confidence = mlData.confidence || Math.random() * 15 + 85;
        
        this.charts.ml.data.labels.push(now);
        this.charts.ml.data.datasets[0].data.push(accuracy);
        this.charts.ml.data.datasets[1].data.push(confidence);
        
        // Keep only last 20 data points
        if (this.charts.ml.data.labels.length > 20) {
            this.charts.ml.data.labels.shift();
            this.charts.ml.data.datasets[0].data.shift();
            this.charts.ml.data.datasets[1].data.shift();
        }
        
        this.charts.ml.update();
    }
    
    // Utility Functions
    formatUptime(seconds) {
        if (!seconds) return '0s';
        const hours = Math.floor(seconds / 3600);
        const minutes = Math.floor((seconds % 3600) / 60);
        const secs = seconds % 60;
        return `${hours}h ${minutes}m ${secs}s`;
    }
    
    showError(message) {
        const errorDiv = document.createElement('div');
        errorDiv.className = 'error';
        errorDiv.textContent = message;
        document.querySelector('.dashboard').insertBefore(errorDiv, document.querySelector('.dashboard').firstChild);
        
        setTimeout(() => {
            errorDiv.remove();
        }, 5000);
    }
    
    showSuccess(message) {
        const successDiv = document.createElement('div');
        successDiv.className = 'success';
        successDiv.textContent = message;
        document.querySelector('.dashboard').insertBefore(successDiv, document.querySelector('.dashboard').firstChild);
        
        setTimeout(() => {
            successDiv.remove();
        }, 3000);
    }
}

// Global Functions for UI Interactions
function showTab(tabId) {
    // Hide all tab contents
    document.querySelectorAll('.tab-content').forEach(tab => {
        tab.classList.remove('active');
    });
    
    // Remove active class from all nav tabs
    document.querySelectorAll('.nav-tab').forEach(tab => {
        tab.classList.remove('active');
    });
    
    // Show selected tab content
    document.getElementById(tabId).classList.add('active');
    
    // Add active class to clicked nav tab
    event.target.classList.add('active');
    
    // Update data for the selected tab
    if (window.dashboard) {
        window.dashboard.updateCurrentTab();
    }
}

// Daemon Control Functions
async function startDaemon() {
    try {
        await window.dashboard.callUBUSMethod('start');
        window.dashboard.showSuccess('Daemon started successfully');
        window.dashboard.updateOverview();
    } catch (error) {
        window.dashboard.showError('Failed to start daemon: ' + error.message);
    }
}

async function stopDaemon() {
    try {
        await window.dashboard.callUBUSMethod('stop');
        window.dashboard.showSuccess('Daemon stopped successfully');
        window.dashboard.updateOverview();
    } catch (error) {
        window.dashboard.showError('Failed to stop daemon: ' + error.message);
    }
}

async function restartDaemon() {
    try {
        await window.dashboard.callUBUSMethod('restart');
        window.dashboard.showSuccess('Daemon restarted successfully');
        setTimeout(() => {
            window.dashboard.updateOverview();
        }, 2000);
    } catch (error) {
        window.dashboard.showError('Failed to restart daemon: ' + error.message);
    }
}

async function refreshData() {
    try {
        await window.dashboard.loadInitialData();
        window.dashboard.showSuccess('Data refreshed successfully');
    } catch (error) {
        window.dashboard.showError('Failed to refresh data: ' + error.message);
    }
}

// ML Control Functions
async function startML() {
    try {
        await window.dashboard.callUBUSMethod('ml_monitor.start');
        window.dashboard.showSuccess('ML monitoring started successfully');
        window.dashboard.updateML();
    } catch (error) {
        window.dashboard.showError('Failed to start ML monitoring: ' + error.message);
    }
}

async function stopML() {
    try {
        await window.dashboard.callUBUSMethod('ml_monitor.stop');
        window.dashboard.showSuccess('ML monitoring stopped successfully');
        window.dashboard.updateML();
    } catch (error) {
        window.dashboard.showError('Failed to stop ML monitoring: ' + error.message);
    }
}

async function resetLearning() {
    try {
        await window.dashboard.callUBUSMethod('ml_monitor.reset_learning');
        window.dashboard.showSuccess('ML learning reset successfully');
        window.dashboard.updateML();
    } catch (error) {
        window.dashboard.showError('Failed to reset ML learning: ' + error.message);
    }
}

async function exportData() {
    try {
        const data = await window.dashboard.callUBUSMethod('ml_monitor.export_data');
        const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `autonomy-ml-data-${new Date().toISOString().split('T')[0]}.json`;
        a.click();
        URL.revokeObjectURL(url);
        window.dashboard.showSuccess('Data exported successfully');
    } catch (error) {
        window.dashboard.showError('Failed to export data: ' + error.message);
    }
}

// Configuration Functions
async function loadConfig() {
    try {
        await window.dashboard.loadConfig();
        window.dashboard.showSuccess('Configuration loaded successfully');
    } catch (error) {
        window.dashboard.showError('Failed to load configuration: ' + error.message);
    }
}

async function saveConfig() {
    try {
        await window.dashboard.saveConfig();
    } catch (error) {
        // Error handling is done in the saveConfig method
    }
}

async function resetConfig() {
    if (confirm('Are you sure you want to reset the configuration to defaults?')) {
        try {
            // Reset form to default values
            document.getElementById('daemon-mode').value = 1;
            document.getElementById('debug-mode').value = 0;
            document.getElementById('log-level').value = 2;
            document.getElementById('check-interval').value = 30;
            document.getElementById('auto-failover').value = 1;
            document.getElementById('failover-timeout').value = 60;
            document.getElementById('min-health').value = 50;
            document.getElementById('mwan3-integration').value = 1;
            
            window.dashboard.showSuccess('Configuration reset to defaults');
        } catch (error) {
            window.dashboard.showError('Failed to reset configuration: ' + error.message);
        }
    }
}

// Log Functions
async function refreshLogs() {
    try {
        const logs = await window.dashboard.callUBUSMethod('log_status');
        const logsContent = document.getElementById('logs-content');
        logsContent.innerHTML = logs.logs || 'No logs available';
        window.dashboard.showSuccess('Logs refreshed successfully');
    } catch (error) {
        window.dashboard.showError('Failed to refresh logs: ' + error.message);
    }
}

async function clearLogs() {
    if (confirm('Are you sure you want to clear all logs?')) {
        try {
            // This would need to be implemented in the daemon
            window.dashboard.showSuccess('Logs cleared successfully');
            document.getElementById('logs-content').innerHTML = 'Logs cleared';
        } catch (error) {
            window.dashboard.showError('Failed to clear logs: ' + error.message);
        }
    }
}

async function downloadLogs() {
    try {
        const logs = await window.dashboard.callUBUSMethod('log_status');
        const blob = new Blob([logs.logs || 'No logs available'], { type: 'text/plain' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `autonomy-logs-${new Date().toISOString().split('T')[0]}.txt`;
        a.click();
        URL.revokeObjectURL(url);
        window.dashboard.showSuccess('Logs downloaded successfully');
    } catch (error) {
        window.dashboard.showError('Failed to download logs: ' + error.message);
    }
}

// Initialize Dashboard when page loads
document.addEventListener('DOMContentLoaded', () => {
    window.dashboard = new AutonomyDashboard();
});

