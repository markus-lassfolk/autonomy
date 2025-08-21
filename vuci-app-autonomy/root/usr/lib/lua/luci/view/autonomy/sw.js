// Autonomy Service Worker
// Provides offline functionality and caching for the PWA

const CACHE_NAME = 'autonomy-v1';
const STATIC_CACHE = 'autonomy-static-v1';
const DYNAMIC_CACHE = 'autonomy-dynamic-v1';

// Files to cache for offline functionality
const STATIC_FILES = [
  '/cgi-bin/luci/admin/network/autonomy/status',
  '/cgi-bin/luci/admin/network/autonomy/monitoring',
  '/cgi-bin/luci/admin/network/autonomy/config',
  '/cgi-bin/luci/admin/network/autonomy/interfaces',
  '/cgi-bin/luci/admin/network/autonomy/telemetry',
  '/cgi-bin/luci/admin/network/autonomy/logs',
  '/luci-static/resources/cbi.js',
  '/luci-static/resources/autonomy.js',
  'https://cdn.jsdelivr.net/npm/chart.js'
];

// Install event - cache static files
self.addEventListener('install', event => {
  console.log('Autonomy Service Worker installing...');
  
  event.waitUntil(
    caches.open(STATIC_CACHE)
      .then(cache => {
        console.log('Caching static files');
        return cache.addAll(STATIC_FILES);
      })
      .then(() => {
        console.log('Static files cached successfully');
        return self.skipWaiting();
      })
      .catch(error => {
        console.error('Failed to cache static files:', error);
      })
  );
});

// Activate event - clean up old caches
self.addEventListener('activate', event => {
  console.log('Autonomy Service Worker activating...');
  
  event.waitUntil(
    caches.keys()
      .then(cacheNames => {
        return Promise.all(
          cacheNames.map(cacheName => {
            if (cacheName !== STATIC_CACHE && cacheName !== DYNAMIC_CACHE) {
              console.log('Deleting old cache:', cacheName);
              return caches.delete(cacheName);
            }
          })
        );
      })
      .then(() => {
        console.log('Service Worker activated');
        return self.clients.claim();
      })
  );
});

// Fetch event - serve from cache or network
self.addEventListener('fetch', event => {
  const request = event.request;
  const url = new URL(request.url);
  
  // Only handle requests to our domain
  if (url.origin !== location.origin && !url.href.includes('cdn.jsdelivr.net')) {
    return;
  }
  
  // Handle API requests differently
  if (url.pathname.includes('/api/')) {
    event.respondWith(handleApiRequest(request));
    return;
  }
  
  // Handle static file requests
  if (request.method === 'GET') {
    event.respondWith(handleStaticRequest(request));
  }
});

// Handle API requests with network-first strategy
async function handleApiRequest(request) {
  try {
    // Try network first
    const networkResponse = await fetch(request);
    
    // Cache successful responses
    if (networkResponse.ok) {
      const cache = await caches.open(DYNAMIC_CACHE);
      cache.put(request, networkResponse.clone());
    }
    
    return networkResponse;
  } catch (error) {
    console.log('Network failed for API request, trying cache:', request.url);
    
    // Fallback to cache
    const cachedResponse = await caches.match(request);
    if (cachedResponse) {
      return cachedResponse;
    }
    
    // Return offline response for API requests
    return new Response(JSON.stringify({
      error: 'Offline - No cached data available',
      offline: true
    }), {
      status: 503,
      headers: { 'Content-Type': 'application/json' }
    });
  }
}

// Handle static file requests with cache-first strategy
async function handleStaticRequest(request) {
  // Try cache first
  const cachedResponse = await caches.match(request);
  if (cachedResponse) {
    return cachedResponse;
  }
  
  try {
    // Fallback to network
    const networkResponse = await fetch(request);
    
    // Cache successful responses
    if (networkResponse.ok) {
      const cache = await caches.open(DYNAMIC_CACHE);
      cache.put(request, networkResponse.clone());
    }
    
    return networkResponse;
  } catch (error) {
    console.log('Network failed for static request:', request.url);
    
    // Return offline page for HTML requests
    if (request.headers.get('accept').includes('text/html')) {
      return caches.match('/cgi-bin/luci/admin/network/autonomy/status');
    }
    
    // Return error for other requests
    return new Response('Offline', {
      status: 503,
      statusText: 'Service Unavailable'
    });
  }
}

// Background sync for offline actions
self.addEventListener('sync', event => {
  if (event.tag === 'autonomy-sync') {
    console.log('Background sync triggered');
    event.waitUntil(syncAutonomyData());
  }
});

// Sync autonomy data when back online
async function syncAutonomyData() {
  try {
    // Sync any pending actions or data
    const response = await fetch('/cgi-bin/luci/admin/network/autonomy/api/status');
    if (response.ok) {
      console.log('Autonomy data synced successfully');
    }
  } catch (error) {
    console.error('Failed to sync autonomy data:', error);
  }
}

// Push notification handling
self.addEventListener('push', event => {
  console.log('Push notification received');
  
  const options = {
    body: event.data ? event.data.text() : 'Autonomy alert',
    icon: '/luci-static/resources/icons/autonomy-192.png',
    badge: '/luci-static/resources/icons/autonomy-192.png',
    vibrate: [100, 50, 100],
    data: {
      dateOfArrival: Date.now(),
      primaryKey: 1
    },
    actions: [
      {
        action: 'explore',
        title: 'View Details',
        icon: '/luci-static/resources/icons/autonomy-192.png'
      },
      {
        action: 'close',
        title: 'Close',
        icon: '/luci-static/resources/icons/autonomy-192.png'
      }
    ]
  };
  
  event.waitUntil(
    self.registration.showNotification('Autonomy Alert', options)
  );
});

// Notification click handling
self.addEventListener('notificationclick', event => {
  console.log('Notification clicked');
  
  event.notification.close();
  
  if (event.action === 'explore') {
    event.waitUntil(
      clients.openWindow('/cgi-bin/luci/admin/network/autonomy/status')
    );
  }
});

// Message handling for communication with main thread
self.addEventListener('message', event => {
  if (event.data && event.data.type === 'SKIP_WAITING') {
    self.skipWaiting();
  }
});
