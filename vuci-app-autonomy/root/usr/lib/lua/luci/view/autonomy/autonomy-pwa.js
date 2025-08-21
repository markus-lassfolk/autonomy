// Autonomy PWA JavaScript
// Provides mobile-optimized functionality and PWA features

class AutonomyPWA {
    constructor() {
        this.isOnline = navigator.onLine;
        this.serviceWorker = null;
        this.pushSubscription = null;
        this.offlineData = {};
        
        this.init();
    }
    
    async init() {
        this.setupEventListeners();
        await this.registerServiceWorker();
        await this.setupPushNotifications();
        this.setupMobileUI();
        this.setupOfflineHandling();
    }
    
    setupEventListeners() {
        // Online/offline status
        window.addEventListener('online', () => {
            this.isOnline = true;
            this.updateOnlineStatus();
            this.syncOfflineData();
        });
        
        window.addEventListener('offline', () => {
            this.isOnline = false;
            this.updateOnlineStatus();
        });
        
        // Before install prompt
        window.addEventListener('beforeinstallprompt', (e) => {
            e.preventDefault();
            this.showInstallPrompt(e);
        });
        
        // App installed
        window.addEventListener('appinstalled', () => {
            this.hideInstallPrompt();
            console.log('Autonomy PWA installed successfully');
        });
        
        // Touch gestures for mobile
        this.setupTouchGestures();
    }
    
    async registerServiceWorker() {
        if ('serviceWorker' in navigator) {
            try {
                this.serviceWorker = await navigator.serviceWorker.register('/cgi-bin/luci/admin/network/autonomy/sw.js');
                console.log('Service Worker registered:', this.serviceWorker);
                
                // Handle service worker updates
                this.serviceWorker.addEventListener('updatefound', () => {
                    const newWorker = this.serviceWorker.installing;
                    newWorker.addEventListener('statechange', () => {
                        if (newWorker.state === 'installed' && navigator.serviceWorker.controller) {
                            this.showUpdatePrompt();
                        }
                    });
                });
            } catch (error) {
                console.error('Service Worker registration failed:', error);
            }
        }
    }
    
    async setupPushNotifications() {
        if ('Notification' in window && 'serviceWorker' in navigator) {
            try {
                const permission = await Notification.requestPermission();
                if (permission === 'granted') {
                    this.pushSubscription = await this.serviceWorker.pushManager.subscribe({
                        userVisibleOnly: true,
                        applicationServerKey: this.urlBase64ToUint8Array(this.getVAPIDPublicKey())
                    });
                    
                    // Send subscription to server
                    await this.sendPushSubscription();
                }
            } catch (error) {
                console.error('Push notification setup failed:', error);
            }
        }
    }
    
    setupMobileUI() {
        // Add mobile-specific CSS classes
        if (this.isMobile()) {
            document.body.classList.add('mobile-device');
            
            // Add mobile navigation
            this.createMobileNavigation();
            
            // Add pull-to-refresh functionality
            this.setupPullToRefresh();
            
            // Add swipe gestures
            this.setupSwipeGestures();
        }
        
        // Add responsive design enhancements
        this.setupResponsiveDesign();
    }
    
    setupOfflineHandling() {
        // Intercept fetch requests for offline handling
        const originalFetch = window.fetch;
        window.fetch = async (...args) => {
            try {
                const response = await originalFetch(...args);
                return response;
            } catch (error) {
                if (!this.isOnline) {
                    return this.handleOfflineRequest(...args);
                }
                throw error;
            }
        };
    }
    
    async handleOfflineRequest(url, options) {
        const requestKey = `${options?.method || 'GET'}_${url}`;
        
        // Check if we have cached data
        if (this.offlineData[requestKey]) {
            return new Response(JSON.stringify(this.offlineData[requestKey]), {
                status: 200,
                headers: { 'Content-Type': 'application/json' }
            });
        }
        
        // Return offline response
        return new Response(JSON.stringify({
            error: 'Offline - No cached data available',
            offline: true,
            timestamp: Date.now()
        }), {
            status: 503,
            headers: { 'Content-Type': 'application/json' }
        });
    }
    
    createMobileNavigation() {
        const nav = document.createElement('nav');
        nav.className = 'mobile-nav';
        nav.innerHTML = `
            <div class="mobile-nav-header">
                <button class="mobile-menu-toggle" id="mobile-menu-toggle">
                    <span></span>
                    <span></span>
                    <span></span>
                </button>
                <h1>Autonomy</h1>
                <div class="mobile-status-indicator" id="mobile-status-indicator"></div>
            </div>
            <div class="mobile-nav-menu" id="mobile-nav-menu">
                <a href="/cgi-bin/luci/admin/network/autonomy/status" class="nav-item">
                    <i class="icon-status"></i>
                    <span>Status</span>
                </a>
                <a href="/cgi-bin/luci/admin/network/autonomy/monitoring" class="nav-item">
                    <i class="icon-monitoring"></i>
                    <span>Monitoring</span>
                </a>
                <a href="/cgi-bin/luci/admin/network/autonomy/config" class="nav-item">
                    <i class="icon-config"></i>
                    <span>Config</span>
                </a>
                <a href="/cgi-bin/luci/admin/network/autonomy/interfaces" class="nav-item">
                    <i class="icon-interfaces"></i>
                    <span>Interfaces</span>
                </a>
                <a href="/cgi-bin/luci/admin/network/autonomy/telemetry" class="nav-item">
                    <i class="icon-telemetry"></i>
                    <span>Telemetry</span>
                </a>
                <a href="/cgi-bin/luci/admin/network/autonomy/logs" class="nav-item">
                    <i class="icon-logs"></i>
                    <span>Logs</span>
                </a>
            </div>
        `;
        
        document.body.insertBefore(nav, document.body.firstChild);
        
        // Setup mobile menu toggle
        const menuToggle = document.getElementById('mobile-menu-toggle');
        const navMenu = document.getElementById('mobile-nav-menu');
        
        menuToggle.addEventListener('click', () => {
            navMenu.classList.toggle('active');
            menuToggle.classList.toggle('active');
        });
    }
    
    setupPullToRefresh() {
        let startY = 0;
        let currentY = 0;
        let pullDistance = 0;
        const threshold = 100;
        
        document.addEventListener('touchstart', (e) => {
            if (window.scrollY === 0) {
                startY = e.touches[0].clientY;
            }
        });
        
        document.addEventListener('touchmove', (e) => {
            if (window.scrollY === 0 && startY > 0) {
                currentY = e.touches[0].clientY;
                pullDistance = currentY - startY;
                
                if (pullDistance > 0) {
                    e.preventDefault();
                    this.showPullToRefreshIndicator(pullDistance);
                }
            }
        });
        
        document.addEventListener('touchend', () => {
            if (pullDistance > threshold) {
                this.refreshData();
            }
            this.hidePullToRefreshIndicator();
            startY = 0;
            pullDistance = 0;
        });
    }
    
    setupSwipeGestures() {
        let startX = 0;
        let startY = 0;
        let endX = 0;
        let endY = 0;
        
        document.addEventListener('touchstart', (e) => {
            startX = e.touches[0].clientX;
            startY = e.touches[0].clientY;
        });
        
        document.addEventListener('touchend', (e) => {
            endX = e.changedTouches[0].clientX;
            endY = e.changedTouches[0].clientY;
            
            const deltaX = endX - startX;
            const deltaY = endY - startY;
            
            // Horizontal swipe
            if (Math.abs(deltaX) > Math.abs(deltaY) && Math.abs(deltaX) > 50) {
                if (deltaX > 0) {
                    this.handleSwipeRight();
                } else {
                    this.handleSwipeLeft();
                }
            }
        });
    }
    
    setupTouchGestures() {
        // Double tap to refresh
        let lastTap = 0;
        document.addEventListener('touchend', (e) => {
            const currentTime = new Date().getTime();
            const tapLength = currentTime - lastTap;
            
            if (tapLength < 500 && tapLength > 0) {
                this.refreshData();
                e.preventDefault();
            }
            lastTap = currentTime;
        });
    }
    
    setupResponsiveDesign() {
        // Add viewport meta tag if not present
        if (!document.querySelector('meta[name="viewport"]')) {
            const viewport = document.createElement('meta');
            viewport.name = 'viewport';
            viewport.content = 'width=device-width, initial-scale=1.0, user-scalable=no';
            document.head.appendChild(viewport);
        }
        
        // Add mobile-specific styles
        const mobileStyles = document.createElement('style');
        mobileStyles.textContent = `
            .mobile-device .cbi-section {
                margin: 10px;
                padding: 15px;
                border-radius: 10px;
                box-shadow: 0 2px 10px rgba(0,0,0,0.1);
            }
            
            .mobile-device .cbi-value {
                margin-bottom: 15px;
            }
            
            .mobile-device .progress-bar {
                height: 25px;
                border-radius: 12px;
            }
            
            .mobile-device .interface-card {
                margin-bottom: 15px;
                border-radius: 10px;
            }
            
            .mobile-nav {
                position: fixed;
                top: 0;
                left: 0;
                right: 0;
                background: #fff;
                box-shadow: 0 2px 10px rgba(0,0,0,0.1);
                z-index: 1000;
            }
            
            .mobile-nav-header {
                display: flex;
                align-items: center;
                padding: 10px 15px;
                border-bottom: 1px solid #eee;
            }
            
            .mobile-menu-toggle {
                background: none;
                border: none;
                padding: 5px;
                margin-right: 15px;
            }
            
            .mobile-menu-toggle span {
                display: block;
                width: 25px;
                height: 3px;
                background: #333;
                margin: 5px 0;
                transition: 0.3s;
            }
            
            .mobile-nav-menu {
                display: none;
                background: #fff;
                border-top: 1px solid #eee;
            }
            
            .mobile-nav-menu.active {
                display: block;
            }
            
            .nav-item {
                display: flex;
                align-items: center;
                padding: 15px;
                text-decoration: none;
                color: #333;
                border-bottom: 1px solid #eee;
            }
            
            .nav-item:hover {
                background: #f5f5f5;
            }
            
            .mobile-status-indicator {
                width: 10px;
                height: 10px;
                border-radius: 50%;
                background: #28a745;
                margin-left: auto;
            }
            
            .mobile-status-indicator.offline {
                background: #dc3545;
            }
            
            .pull-to-refresh-indicator {
                position: fixed;
                top: 0;
                left: 0;
                right: 0;
                background: #007bff;
                color: white;
                text-align: center;
                padding: 10px;
                transform: translateY(-100%);
                transition: transform 0.3s;
                z-index: 1001;
            }
            
            .pull-to-refresh-indicator.show {
                transform: translateY(0);
            }
        `;
        document.head.appendChild(mobileStyles);
    }
    
    updateOnlineStatus() {
        const indicator = document.getElementById('mobile-status-indicator');
        if (indicator) {
            indicator.classList.toggle('offline', !this.isOnline);
        }
        
        // Show offline banner
        if (!this.isOnline) {
            this.showOfflineBanner();
        } else {
            this.hideOfflineBanner();
        }
    }
    
    showOfflineBanner() {
        if (!document.getElementById('offline-banner')) {
            const banner = document.createElement('div');
            banner.id = 'offline-banner';
            banner.className = 'offline-banner';
            banner.innerHTML = `
                <div class="offline-content">
                    <span>You are currently offline. Some features may be limited.</span>
                    <button onclick="this.parentElement.parentElement.remove()">×</button>
                </div>
            `;
            
            const styles = document.createElement('style');
            styles.textContent = `
                .offline-banner {
                    position: fixed;
                    top: 0;
                    left: 0;
                    right: 0;
                    background: #ffc107;
                    color: #333;
                    padding: 10px;
                    text-align: center;
                    z-index: 1002;
                }
                
                .offline-content {
                    display: flex;
                    justify-content: space-between;
                    align-items: center;
                    max-width: 600px;
                    margin: 0 auto;
                }
                
                .offline-content button {
                    background: none;
                    border: none;
                    font-size: 20px;
                    cursor: pointer;
                    padding: 0 10px;
                }
            `;
            document.head.appendChild(styles);
            document.body.appendChild(banner);
        }
    }
    
    hideOfflineBanner() {
        const banner = document.getElementById('offline-banner');
        if (banner) {
            banner.remove();
        }
    }
    
    showPullToRefreshIndicator(distance) {
        let indicator = document.getElementById('pull-to-refresh-indicator');
        if (!indicator) {
            indicator = document.createElement('div');
            indicator.id = 'pull-to-refresh-indicator';
            indicator.className = 'pull-to-refresh-indicator';
            indicator.textContent = 'Pull to refresh...';
            document.body.appendChild(indicator);
        }
        
        if (distance > 50) {
            indicator.classList.add('show');
        }
    }
    
    hidePullToRefreshIndicator() {
        const indicator = document.getElementById('pull-to-refresh-indicator');
        if (indicator) {
            indicator.classList.remove('show');
        }
    }
    
    async refreshData() {
        // Reload current page data
        if (typeof loadStatus === 'function') {
            loadStatus();
        } else if (typeof loadMonitoringData === 'function') {
            loadMonitoringData();
        } else {
            location.reload();
        }
    }
    
    handleSwipeRight() {
        // Navigate to previous page or open menu
        if (window.history.length > 1) {
            window.history.back();
        }
    }
    
    handleSwipeLeft() {
        // Navigate to next page or close menu
        const navMenu = document.getElementById('mobile-nav-menu');
        if (navMenu && navMenu.classList.contains('active')) {
            navMenu.classList.remove('active');
            document.getElementById('mobile-menu-toggle').classList.remove('active');
        }
    }
    
    async syncOfflineData() {
        // Sync any offline changes when back online
        if (Object.keys(this.offlineData).length > 0) {
            console.log('Syncing offline data...');
            // Implement sync logic here
            this.offlineData = {};
        }
    }
    
    showInstallPrompt(event) {
        // Create install prompt
        const prompt = document.createElement('div');
        prompt.className = 'install-prompt';
        prompt.innerHTML = `
            <div class="install-content">
                <h3>Install Autonomy Monitor</h3>
                <p>Add this app to your home screen for quick access</p>
                <div class="install-buttons">
                    <button onclick="autonomyPWA.installApp()">Install</button>
                    <button onclick="autonomyPWA.hideInstallPrompt()">Not Now</button>
                </div>
            </div>
        `;
        
        const styles = document.createElement('style');
        styles.textContent = `
            .install-prompt {
                position: fixed;
                bottom: 20px;
                left: 20px;
                right: 20px;
                background: #fff;
                border-radius: 10px;
                box-shadow: 0 4px 20px rgba(0,0,0,0.3);
                z-index: 1003;
                padding: 20px;
            }
            
            .install-buttons {
                display: flex;
                gap: 10px;
                margin-top: 15px;
            }
            
            .install-buttons button {
                flex: 1;
                padding: 10px;
                border: none;
                border-radius: 5px;
                cursor: pointer;
            }
            
            .install-buttons button:first-child {
                background: #007bff;
                color: white;
            }
            
            .install-buttons button:last-child {
                background: #6c757d;
                color: white;
            }
        `;
        document.head.appendChild(styles);
        document.body.appendChild(prompt);
        
        this.installEvent = event;
    }
    
    hideInstallPrompt() {
        const prompt = document.querySelector('.install-prompt');
        if (prompt) {
            prompt.remove();
        }
    }
    
    async installApp() {
        if (this.installEvent) {
            this.installEvent.prompt();
            const choice = await this.installEvent.userChoice;
            if (choice.outcome === 'accepted') {
                console.log('User accepted the install prompt');
            }
            this.hideInstallPrompt();
        }
    }
    
    showUpdatePrompt() {
        const prompt = document.createElement('div');
        prompt.className = 'update-prompt';
        prompt.innerHTML = `
            <div class="update-content">
                <h3>Update Available</h3>
                <p>A new version of Autonomy Monitor is available</p>
                <button onclick="autonomyPWA.updateApp()">Update Now</button>
            </div>
        `;
        
        const styles = document.createElement('style');
        styles.textContent = `
            .update-prompt {
                position: fixed;
                top: 50%;
                left: 50%;
                transform: translate(-50%, -50%);
                background: #fff;
                border-radius: 10px;
                box-shadow: 0 4px 20px rgba(0,0,0,0.3);
                z-index: 1003;
                padding: 20px;
                text-align: center;
            }
            
            .update-prompt button {
                background: #28a745;
                color: white;
                border: none;
                padding: 10px 20px;
                border-radius: 5px;
                cursor: pointer;
                margin-top: 15px;
            }
        `;
        document.head.appendChild(styles);
        document.body.appendChild(prompt);
    }
    
    updateApp() {
        if (this.serviceWorker) {
            this.serviceWorker.postMessage({ type: 'SKIP_WAITING' });
            location.reload();
        }
    }
    
    async sendPushSubscription() {
        try {
            await fetch('/cgi-bin/luci/admin/network/autonomy/api/push_subscription', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(this.pushSubscription)
            });
        } catch (error) {
            console.error('Failed to send push subscription:', error);
        }
    }
    
    getVAPIDPublicKey() {
        // This should be provided by the server
        return 'YOUR_VAPID_PUBLIC_KEY';
    }
    
    urlBase64ToUint8Array(base64String) {
        const padding = '='.repeat((4 - base64String.length % 4) % 4);
        const base64 = (base64String + padding)
            .replace(/-/g, '+')
            .replace(/_/g, '/');
        
        const rawData = window.atob(base64);
        const outputArray = new Uint8Array(rawData.length);
        
        for (let i = 0; i < rawData.length; ++i) {
            outputArray[i] = rawData.charCodeAt(i);
        }
        return outputArray;
    }
    
    isMobile() {
        return /Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini/i.test(navigator.userAgent) ||
               window.innerWidth <= 768;
    }
}

// Initialize PWA when DOM is loaded
document.addEventListener('DOMContentLoaded', () => {
    window.autonomyPWA = new AutonomyPWA();
});
