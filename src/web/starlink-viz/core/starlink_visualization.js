// 🛰️ Starlink Sky Tracker - Real-time Visualization
// Based on Gemini's research: 123x123 polar projection with proper coordinate transformations

class StarlinkVisualizer {
    constructor() {
        this.currentView = 'ground';
        this.skyView = document.getElementById('skyView');
        this.obstructionCanvas = document.getElementById('obstructionCanvas');
        this.pathsSvg = document.getElementById('pathsSvg');
        this.satellitesContainer = document.getElementById('satellitesContainer');
        this.tooltip = document.getElementById('tooltip');
        
        // Data
        this.satellites = [];
        this.obstructionMap = null;
        this.predictions = [];
        this.activeSatellite = null;
        this.dishLocation = null;
        
        // Animation and updates
        this.updateInterval = null;
        this.currentTimeOffset = 0; // Minutes from now
        this.isRealTime = true;
        
        // Performance tracking
        this.lastUpdateTime = 0;
        this.updateCount = 0;
        this.propagationTimes = [];
        
        this.initializeVisualization();
        this.setupEventHandlers();
        this.startDataUpdates();
    }
    
    initializeVisualization() {
        this.setupElevationRings();
        this.setupCanvas();
        this.setupSVG();
        
        // Initial loading state
        document.getElementById('loadingIndicator').style.display = 'block';
    }
    
    setupElevationRings() {
        const ringsContainer = document.getElementById('elevationRings');
        
        // Create elevation rings for 30°, 45°, 60°, 75°
        const elevations = [30, 45, 60, 75];
        
        elevations.forEach(elevation => {
            const ring = document.createElement('div');
            ring.className = 'elevation-ring';
            
            // Calculate ring size based on elevation
            // 90° (zenith) = 0% radius, 25° (horizon) = 100% radius
            const radiusPercent = ((90 - elevation) / (90 - 25)) * 100;
            const size = `${radiusPercent}%`;
            
            ring.style.width = size;
            ring.style.height = size;
            ring.title = `${elevation}° elevation`;
            
            ringsContainer.appendChild(ring);
        });
    }
    
    setupCanvas() {
        const canvas = this.obstructionCanvas;
        const rect = this.skyView.getBoundingClientRect();
        
        // Set canvas size to match sky view
        canvas.width = rect.width;
        canvas.height = rect.height;
        
        // Update canvas size on window resize
        window.addEventListener('resize', () => {
            const newRect = this.skyView.getBoundingClientRect();
            canvas.width = newRect.width;
            canvas.height = newRect.height;
            this.drawObstructionMap();
        });
    }
    
    setupSVG() {
        const svg = this.pathsSvg;
        const rect = this.skyView.getBoundingClientRect();
        
        svg.setAttribute('viewBox', `0 0 ${rect.width} ${rect.height}`);
    }
    
    setupEventHandlers() {
        // Time slider
        const timeSlider = document.getElementById('timeControl');
        timeSlider.addEventListener('input', (e) => {
            this.currentTimeOffset = parseInt(e.target.value);
            this.isRealTime = (this.currentTimeOffset === 0);
            this.updateTimeDisplay();
            this.updateVisualization();
        });
        
        // Control sliders
        document.getElementById('updateFreq').addEventListener('input', (e) => {
            const freq = parseInt(e.target.value);
            document.getElementById('updateFreqValue').textContent = `${freq} seconds`;
            this.setUpdateFrequency(freq);
        });
        
        document.getElementById('predictionHorizon').addEventListener('input', (e) => {
            const hours = parseInt(e.target.value);
            document.getElementById('horizonValue').textContent = `${hours} hours`;
        });
        
        // Toggle switches
        document.getElementById('showPaths').addEventListener('change', (e) => {
            this.pathsSvg.style.display = e.target.checked ? 'block' : 'none';
        });
        
        document.getElementById('showObstructions').addEventListener('change', (e) => {
            this.obstructionCanvas.style.display = e.target.checked ? 'block' : 'none';
        });
        
        document.getElementById('realTimeUpdates').addEventListener('change', (e) => {
            if (e.target.checked) {
                this.startDataUpdates();
            } else {
                this.stopDataUpdates();
            }
        });
        
        // Mouse events for satellite tooltips
        this.skyView.addEventListener('mousemove', (e) => this.handleMouseMove(e));
        this.skyView.addEventListener('mouseleave', () => this.hideTooltip());
    }
    
    // Convert azimuth/elevation to screen coordinates (Gemini's polar projection)
    azElToScreen(azimuth, elevation) {
        const rect = this.skyView.getBoundingClientRect();
        const centerX = rect.width / 2;
        const centerY = rect.height / 2;
        const maxRadius = Math.min(centerX, centerY) * 0.9; // 90% of available radius
        
        if (this.currentView === 'satellite') {
            // Satellite view: Looking down at Earth
            // Flip the perspective - higher elevation satellites appear closer to center
            if (elevation < 25 || elevation > 90) {
                return { x: -1, y: -1, valid: false };
            }
            
            // For satellite view, radius represents distance from nadir
            // Higher elevation (closer to zenith from ground) = smaller radius from satellite
            const radiusNormalized = (elevation - 25) / (90 - 25); // Inverted
            const radius = (1.0 - radiusNormalized) * maxRadius; // Flip radius
            
            // Azimuth remains the same but coordinates are flipped
            const azRad = (azimuth + 90) * Math.PI / 180; // Add 90° instead of subtract
            
            const x = centerX + radius * Math.cos(azRad);
            const y = centerY + radius * Math.sin(azRad);
            
            return { x, y, valid: true };
            
        } else {
            // Ground view: Looking up at sky (original implementation)
            if (elevation < 25 || elevation > 90) {
                return { x: -1, y: -1, valid: false };
            }
            
            // Normalize elevation to radius (0 at zenith, 1 at horizon)
            const radiusNormalized = (90 - elevation) / (90 - 25);
            const radius = radiusNormalized * maxRadius;
            
            // Convert azimuth to radians (North is up, so subtract 90°)
            const azRad = (azimuth - 90) * Math.PI / 180;
            
            // Calculate screen coordinates
            const x = centerX + radius * Math.cos(azRad);
            const y = centerY + radius * Math.sin(azRad);
            
            return { x, y, valid: true };
        }
    }
    
    // Convert screen coordinates back to azimuth/elevation
    screenToAzEl(x, y) {
        const rect = this.skyView.getBoundingClientRect();
        const centerX = rect.width / 2;
        const centerY = rect.height / 2;
        const maxRadius = Math.min(centerX, centerY) * 0.9;
        
        const dx = x - centerX;
        const dy = y - centerY;
        const radius = Math.sqrt(dx * dx + dy * dy);
        
        if (radius > maxRadius) {
            return { azimuth: 0, elevation: 0, valid: false };
        }
        
        // Calculate azimuth
        let azimuth = Math.atan2(dy, dx) * 180 / Math.PI + 90;
        if (azimuth < 0) azimuth += 360;
        if (azimuth >= 360) azimuth -= 360;
        
        // Calculate elevation
        const radiusNormalized = radius / maxRadius;
        const elevation = 90 - (radiusNormalized * (90 - 25));
        
        return { azimuth, elevation, valid: true };
    }
    
    // Switch between ground view and satellite view
    switchView(view) {
        this.currentView = view;
        
        // Update button states
        document.getElementById('groundViewBtn').classList.toggle('active', view === 'ground');
        document.getElementById('satelliteViewBtn').classList.toggle('active', view === 'satellite');
        
        // Update sky view appearance based on perspective
        if (view === 'satellite') {
            // Satellite view: Looking down at Earth
            this.skyView.style.background = 'radial-gradient(circle at center, #001a33 0%, #003366 50%, #004080 100%)';
            
            // Update compass directions (flipped for satellite view)
            document.querySelectorAll('.compass').forEach(compass => {
                if (compass.textContent === 'N') compass.textContent = 'S';
                else if (compass.textContent === 'S') compass.textContent = 'N';
                else if (compass.textContent === 'E') compass.textContent = 'W';
                else if (compass.textContent === 'W') compass.textContent = 'E';
            });
            
            // Update elevation ring labels (now distance rings from nadir)
            document.querySelectorAll('.elevation-ring').forEach((ring, index) => {
                const distances = [1000, 2000, 3000, 4000]; // km from nadir
                ring.title = `${distances[index]} km from nadir`;
            });
            
        } else {
            // Ground view: Looking up at sky
            this.skyView.style.background = 'radial-gradient(circle at center, #001122 0%, #000000 70%, #333333 100%)';
            
            // Restore normal compass directions
            document.querySelectorAll('.compass').forEach(compass => {
                const position = compass.classList.contains('south') ? 'S' : 
                               compass.classList.contains('east') ? 'E' :
                               compass.classList.contains('west') ? 'W' : 'N';
                compass.textContent = position;
            });
            
            // Restore elevation ring labels
            document.querySelectorAll('.elevation-ring').forEach((ring, index) => {
                const elevations = [30, 45, 60, 75];
                ring.title = `${elevations[index]}° elevation`;
            });
        }
        
        // Update visualization with new perspective
        this.updateVisualization();
    }
    
    // Draw obstruction map on canvas
    drawObstructionMap() {
        if (!this.obstructionMap) return;
        
        const canvas = this.obstructionCanvas;
        const ctx = canvas.getContext('2d');
        const rect = this.skyView.getBoundingClientRect();
        
        canvas.width = rect.width;
        canvas.height = rect.height;
        
        const centerX = canvas.width / 2;
        const centerY = canvas.height / 2;
        const maxRadius = Math.min(centerX, centerY) * 0.9;
        
        // Create image data for the obstruction map
        const imageData = ctx.createImageData(canvas.width, canvas.height);
        
        // Draw 123x123 polar projection
        for (let screenY = 0; screenY < canvas.height; screenY++) {
            for (let screenX = 0; screenX < canvas.width; screenX++) {
                const coords = this.screenToAzEl(screenX, screenY);
                
                if (coords.valid) {
                    // Get SNR value from obstruction map
                    const snr = this.getObstructionSNR(coords.azimuth, coords.elevation);
                    
                    if (snr !== null) {
                        // Convert SNR to color
                        const color = this.snrToColor(snr);
                        const pixelIndex = (screenY * canvas.width + screenX) * 4;
                        
                        imageData.data[pixelIndex] = color.r;     // Red
                        imageData.data[pixelIndex + 1] = color.g; // Green
                        imageData.data[pixelIndex + 2] = color.b; // Blue
                        imageData.data[pixelIndex + 3] = color.a; // Alpha
                    }
                }
            }
        }
        
        ctx.putImageData(imageData, 0, 0);
    }
    
    // Convert SNR value to color
    snrToColor(snr) {
        // High SNR (good signal) = Green
        // Medium SNR = Yellow
        // Low SNR (obstructed) = Red
        
        if (snr > 0.8) {
            return { r: 0, g: 255, b: 0, a: 100 }; // Green
        } else if (snr > 0.5) {
            const factor = (snr - 0.5) / 0.3;
            return { 
                r: Math.floor(255 * (1 - factor)), 
                g: 255, 
                b: 0, 
                a: 100 
            }; // Yellow to Green
        } else {
            return { r: 255, g: 0, b: 0, a: 150 }; // Red
        }
    }
    
    // Get obstruction SNR value for given coordinates
    getObstructionSNR(azimuth, elevation) {
        if (!this.obstructionMap || !this.obstructionMap.snrData) {
            return null;
        }
        
        // Use Gemini's coordinate conversion algorithm
        const pixel = this.azElToPixel(azimuth, elevation);
        if (!pixel.valid) {
            return null;
        }
        
        const index = pixel.row * 123 + pixel.col;
        if (index < 0 || index >= this.obstructionMap.snrData.length) {
            return null;
        }
        
        return this.obstructionMap.snrData[index];
    }
    
    // Convert Az/El to 123x123 pixel coordinates (Gemini's algorithm)
    azElToPixel(azDeg, elDeg) {
        const MAP_DIAMETER = 123;
        const CENTER_PIXEL = 61;
        const MAX_RADIUS_PIXELS = 61.5;
        const MIN_ELEVATION = 25.0;
        const MAX_ELEVATION = 90.0;
        
        if (elDeg < MIN_ELEVATION) {
            return { row: -1, col: -1, valid: false };
        }
        
        // Normalize elevation to radius (0 at zenith, 1 at edge)
        const radiusNormalized = (MAX_ELEVATION - elDeg) / (MAX_ELEVATION - MIN_ELEVATION);
        const pixelRadius = radiusNormalized * MAX_RADIUS_PIXELS;
        
        // Convert polar to Cartesian (North is up, so subtract 90°)
        const azRad = (azDeg - 90) * Math.PI / 180;
        const col = Math.floor(CENTER_PIXEL + pixelRadius * Math.cos(azRad));
        const row = Math.floor(CENTER_PIXEL + pixelRadius * Math.sin(azRad));
        
        // Check bounds
        if (row >= 0 && row < MAP_DIAMETER && col >= 0 && col < MAP_DIAMETER) {
            return { row, col, valid: true };
        }
        
        return { row: -1, col: -1, valid: false };
    }
    
    // Draw satellite paths
    drawSatellitePaths() {
        const svg = this.pathsSvg;
        svg.innerHTML = ''; // Clear existing paths
        
        if (!document.getElementById('showPaths').checked) {
            return;
        }
        
        this.satellites.forEach(satellite => {
            if (satellite.trajectory && satellite.trajectory.length > 1) {
                const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
                
                let pathData = '';
                let isFirstPoint = true;
                
                satellite.trajectory.forEach(point => {
                    const screen = this.azElToScreen(point.azimuth, point.elevation);
                    if (screen.valid) {
                        if (isFirstPoint) {
                            pathData += `M ${screen.x} ${screen.y}`;
                            isFirstPoint = false;
                        } else {
                            pathData += ` L ${screen.x} ${screen.y}`;
                        }
                    }
                });
                
                path.setAttribute('d', pathData);
                
                // Set path style based on satellite state
                if (satellite.id === this.activeSatellite?.id) {
                    path.className.baseVal = 'satellite-path path-active';
                } else if (satellite.isObstructed) {
                    path.className.baseVal = 'satellite-path path-obstructed';
                } else {
                    path.className.baseVal = 'satellite-path path-clear';
                }
                
                svg.appendChild(path);
            }
        });
    }
    
    // Update satellite positions
    updateSatellites() {
        this.satellitesContainer.innerHTML = '';
        
        let visibleCount = 0;
        let unobstructedCount = 0;
        
        this.satellites.forEach(satellite => {
            if (satellite.elevation >= 25) { // Above dish minimum
                visibleCount++;
                
                const screen = this.azElToScreen(satellite.azimuth, satellite.elevation);
                if (screen.valid) {
                    const satElement = document.createElement('div');
                    satElement.className = 'satellite';
                    satElement.style.left = `${screen.x}px`;
                    satElement.style.top = `${screen.y}px`;
                    
                    // Determine satellite state
                    if (satellite.id === this.activeSatellite?.id) {
                        satElement.classList.add('active');
                    } else if (satellite.isObstructed) {
                        satElement.classList.add('obstructed');
                    } else {
                        satElement.classList.add('visible');
                        unobstructedCount++;
                    }
                    
                    // Add data attributes for tooltip
                    satElement.dataset.satelliteId = satellite.id;
                    satElement.dataset.azimuth = satellite.azimuth.toFixed(1);
                    satElement.dataset.elevation = satellite.elevation.toFixed(1);
                    satElement.dataset.range = satellite.range ? satellite.range.toFixed(1) : 'Unknown';
                    satElement.dataset.snr = satellite.signalQuality ? satellite.signalQuality.toFixed(3) : 'Unknown';
                    
                    this.satellitesContainer.appendChild(satElement);
                }
            }
        });
        
        // Update metrics
        document.getElementById('visibleSats').textContent = visibleCount;
        document.getElementById('unobstructedSats').textContent = unobstructedCount;
        document.getElementById('satelliteCount').textContent = `${visibleCount} satellites`;
        
        const coverage = visibleCount > 0 ? Math.round((unobstructedCount / visibleCount) * 100) : 0;
        document.getElementById('skyCoverage').textContent = `${coverage}%`;
    }
    
    // Handle mouse movement for tooltips
    handleMouseMove(event) {
        const rect = this.skyView.getBoundingClientRect();
        const x = event.clientX - rect.left;
        const y = event.clientY - rect.top;
        
        // Check if mouse is over a satellite
        const elements = document.elementsFromPoint(event.clientX, event.clientY);
        const satellite = elements.find(el => el.classList.contains('satellite'));
        
        if (satellite) {
            this.showTooltip(event.clientX, event.clientY, {
                id: satellite.dataset.satelliteId,
                azimuth: satellite.dataset.azimuth,
                elevation: satellite.dataset.elevation,
                range: satellite.dataset.range,
                snr: satellite.dataset.snr
            });
        } else {
            // Check if mouse is over sky area - show coordinates
            const coords = this.screenToAzEl(x, y);
            if (coords.valid) {
                const snr = this.getObstructionSNR(coords.azimuth, coords.elevation);
                this.showTooltip(event.clientX, event.clientY, {
                    azimuth: coords.azimuth.toFixed(1),
                    elevation: coords.elevation.toFixed(1),
                    snr: snr ? snr.toFixed(3) : 'No data'
                });
            } else {
                this.hideTooltip();
            }
        }
    }
    
    showTooltip(x, y, data) {
        const tooltip = this.tooltip;
        
        if (data.id) {
            // Satellite tooltip
            tooltip.innerHTML = `
                <strong>${data.id}</strong><br>
                Az: ${data.azimuth}°, El: ${data.elevation}°<br>
                Range: ${data.range} km<br>
                SNR: ${data.snr}
            `;
        } else {
            // Sky coordinate tooltip
            tooltip.innerHTML = `
                Az: ${data.azimuth}°, El: ${data.elevation}°<br>
                SNR: ${data.snr}
            `;
        }
        
        tooltip.style.left = `${x + 10}px`;
        tooltip.style.top = `${y - 10}px`;
        tooltip.style.display = 'block';
    }
    
    hideTooltip() {
        this.tooltip.style.display = 'none';
    }
    
    // Update time display
    updateTimeDisplay() {
        const now = new Date();
        const targetTime = new Date(now.getTime() + this.currentTimeOffset * 60000);
        
        if (this.currentTimeOffset === 0) {
            document.getElementById('currentTime').textContent = 'Now';
        } else {
            const hours = Math.floor(this.currentTimeOffset / 60);
            const minutes = this.currentTimeOffset % 60;
            document.getElementById('currentTime').textContent = `+${hours}h ${minutes}m`;
        }
    }
    
    // Start real-time data updates
    startDataUpdates() {
        if (this.updateInterval) {
            clearInterval(this.updateInterval);
        }
        
        const frequency = parseInt(document.getElementById('updateFreq').value) * 1000;
        
        this.updateInterval = setInterval(() => {
            this.fetchData();
        }, frequency);
        
        // Initial fetch
        this.fetchData();
    }
    
    // Stop data updates
    stopDataUpdates() {
        if (this.updateInterval) {
            clearInterval(this.updateInterval);
            this.updateInterval = null;
        }
    }
    
    // Set update frequency
    setUpdateFrequency(seconds) {
        if (document.getElementById('realTimeUpdates').checked) {
            this.startDataUpdates();
        }
    }
    
    // Fetch data from UBUS API
    async fetchData() {
        const startTime = performance.now();
        
        try {
            // Fetch tracker status
            const statusResponse = await this.callUBUS('starlink_tracker', 'status');
            this.updateStatus(statusResponse);
            
            // Fetch satellite positions
            const satellitesResponse = await this.callUBUS('starlink_tracker', 'satellites');
            this.updateSatelliteData(satellitesResponse);
            
            // Fetch predictions
            const predictionsResponse = await this.callUBUS('starlink_tracker', 'predictions');
            this.updatePredictions(predictionsResponse);
            
            // Update visualization
            this.updateVisualization();
            
            // Update performance metrics
            const endTime = performance.now();
            this.updatePerformanceMetrics(endTime - startTime);
            
            // Hide loading indicator
            document.getElementById('loadingIndicator').style.display = 'none';
            
        } catch (error) {
            console.error('Failed to fetch data:', error);
            document.getElementById('connectionStatus').textContent = 'Connection Error';
            document.getElementById('connectionStatus').style.color = '#ff4757';
        }
    }
    
    // Call UBUS API
    async callUBUS(object, method, params = {}) {
        // In a real implementation, this would call the actual UBUS API
        // For demo purposes, we'll simulate the data
        return this.simulateUBUSResponse(object, method);
    }
    
    // Simulate UBUS responses for demonstration
    simulateUBUSResponse(object, method) {
        if (object === 'starlink_tracker') {
            switch (method) {
                case 'status':
                    return {
                        status: 'monitoring',
                        initialized: true,
                        monitoring_active: true,
                        visible_satellites: 12,
                        unobstructed_satellites: 8,
                        total_predictions: 15,
                        correct_predictions: 12,
                        accuracy_percentage: 80.0,
                        last_update: Math.floor(Date.now() / 1000),
                        dish_location: {
                            latitude: 40.7128,
                            longitude: -74.0060,
                            altitude: 10.0,
                            boresight_azimuth: 45.0,
                            boresight_elevation: 30.0
                        },
                        obstruction_map: {
                            total_cells: 15129,
                            obstructed_cells: 3782,
                            obstruction_percentage: 25.0,
                            average_snr: 0.75
                        }
                    };
                    
                case 'satellites':
                    return {
                        count: 12,
                        visible_count: 12,
                        unobstructed_count: 8,
                        satellites: this.generateSimulatedSatellites()
                    };
                    
                case 'predictions':
                    return {
                        count: 2,
                        predictions: [
                            {
                                start_time: Math.floor(Date.now() / 1000) + 3600,
                                end_time: Math.floor(Date.now() / 1000) + 4500,
                                duration_seconds: 900,
                                risk_level: 3,
                                description: "Tree obstruction blocks active satellite",
                                predicted_available_sats: 0,
                                confidence_score: 0.85
                            },
                            {
                                start_time: Math.floor(Date.now() / 1000) + 7200,
                                end_time: Math.floor(Date.now() / 1000) + 7800,
                                duration_seconds: 600,
                                risk_level: 2,
                                description: "Marginal signal degradation expected",
                                predicted_available_sats: 2,
                                confidence_score: 0.72
                            }
                        ]
                    };
            }
        }
        
        return {};
    }
    
    // Generate simulated satellite data for demonstration
    generateSimulatedSatellites() {
        const satellites = [];
        
        for (let i = 0; i < 12; i++) {
            const azimuth = Math.random() * 360;
            const elevation = 25 + Math.random() * 65; // 25° to 90°
            const snr = Math.random();
            
            satellites.push({
                satellite_id: `STARLINK-${1000 + i}`,
                azimuth: azimuth,
                elevation: elevation,
                range: 500 + Math.random() * 1000,
                is_visible: true,
                is_obstructed: snr < 0.7,
                signal_quality: snr
            });
        }
        
        return satellites;
    }
    
    // Update status from API response
    updateStatus(status) {
        if (status.status === 'monitoring') {
            document.getElementById('connectionStatus').textContent = 'Connected';
            document.getElementById('connectionStatus').style.color = '#00ff88';
        }
        
        if (status.dish_location) {
            this.dishLocation = status.dish_location;
            document.getElementById('dishStatus').textContent = 
                `${status.dish_location.latitude.toFixed(4)}°, ${status.dish_location.longitude.toFixed(4)}°`;
        }
        
        if (status.obstruction_map) {
            document.getElementById('obstructionLevel').textContent = 
                `${status.obstruction_map.obstruction_percentage.toFixed(1)}%`;
        }
        
        // Update accuracy
        document.getElementById('accuracy').textContent = 
            `${status.accuracy_percentage.toFixed(1)}%`;
    }
    
    // Update satellite data
    updateSatelliteData(data) {
        this.satellites = data.satellites || [];
        
        // Find active satellite (highest signal quality among unobstructed)
        let bestSat = null;
        let bestQuality = 0;
        
        this.satellites.forEach(sat => {
            if (!sat.is_obstructed && sat.signal_quality > bestQuality) {
                bestQuality = sat.signal_quality;
                bestSat = sat;
            }
        });
        
        this.activeSatellite = bestSat;
        document.getElementById('activeSat').textContent = 
            bestSat ? bestSat.satellite_id : 'None';
    }
    
    // Update predictions
    updatePredictions(data) {
        this.predictions = data.predictions || [];
        
        const predictionsList = document.getElementById('predictionsList');
        predictionsList.innerHTML = '';
        
        if (this.predictions.length === 0) {
            predictionsList.innerHTML = `
                <div style="text-align: center; color: #00ff88; padding: 2rem;">
                    ✅ No outages predicted!
                </div>
            `;
        } else {
            this.predictions.forEach(prediction => {
                const item = document.createElement('div');
                const riskClass = ['', 'low', 'medium', 'high'][prediction.risk_level] || 'high';
                item.className = `prediction-item ${riskClass}`;
                
                const startTime = new Date(prediction.start_time * 1000);
                const duration = Math.floor(prediction.duration_seconds / 60);
                
                item.innerHTML = `
                    <div style="font-weight: bold;">Risk Level ${prediction.risk_level}</div>
                    <div class="prediction-time">${startTime.toLocaleTimeString()} - ${duration} minutes</div>
                    <div class="prediction-desc">${prediction.description}</div>
                `;
                
                predictionsList.appendChild(item);
            });
        }
        
        document.getElementById('alertCount').textContent = `${this.predictions.length} alerts`;
    }
    
    // Update performance metrics
    updatePerformanceMetrics(processingTime) {
        this.propagationTimes.push(processingTime);
        if (this.propagationTimes.length > 10) {
            this.propagationTimes.shift();
        }
        
        const avgTime = this.propagationTimes.reduce((a, b) => a + b, 0) / this.propagationTimes.length;
        document.getElementById('propagationTime').textContent = `${avgTime.toFixed(1)} ms`;
        
        this.updateCount++;
        const now = Date.now();
        if (this.lastUpdateTime > 0) {
            const rate = 1000 / (now - this.lastUpdateTime);
            document.getElementById('updateRate').textContent = `${rate.toFixed(1)} Hz`;
        }
        this.lastUpdateTime = now;
        
        document.getElementById('lastUpdate').textContent = new Date().toLocaleTimeString();
    }
    
    // Main visualization update
    updateVisualization() {
        this.drawObstructionMap();
        this.updateSatellites();
        this.drawSatellitePaths();
    }
}

// View switching function (called from HTML)
function switchView(view) {
    if (window.visualizer) {
        window.visualizer.switchView(view);
    }
}

// Initialize visualization when page loads
document.addEventListener('DOMContentLoaded', () => {
    window.visualizer = new StarlinkVisualizer();
});

// Handle window resize
window.addEventListener('resize', () => {
    if (window.visualizer) {
        window.visualizer.updateVisualization();
    }
});