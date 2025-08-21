#!/usr/bin/env node

/**
 * Webhook Receiver for Autonomy Project
 * This script receives and processes webhook requests for testing
 */

const http = require('http');
const https = require('https');
const crypto = require('crypto');
const url = require('url');

// Configuration
const config = {
    port: process.env.WEBHOOK_PORT || 3000,
    secret: process.env.WEBHOOK_SECRET || 'test-secret',
    ssl: process.env.WEBHOOK_SSL === 'true',
    cert: process.env.WEBHOOK_CERT,
    key: process.env.WEBHOOK_KEY,
    logLevel: process.env.LOG_LEVEL || 'info'
};

// Colors for console output
const colors = {
    reset: '\x1b[0m',
    red: '\x1b[31m',
    green: '\x1b[32m',
    yellow: '\x1b[33m',
    blue: '\x1b[34m',
    magenta: '\x1b[35m',
    cyan: '\x1b[36m'
};

// Logging functions
function log(level, message, data = null) {
    const timestamp = new Date().toISOString();
    const levelColors = {
        error: colors.red,
        warn: colors.yellow,
        info: colors.blue,
        debug: colors.cyan
    };
    
    const color = levelColors[level] || colors.reset;
    console.log(`${color}[${timestamp}] [${level.toUpperCase()}]${colors.reset} ${message}`);
    
    if (data && config.logLevel === 'debug') {
        console.log(JSON.stringify(data, null, 2));
    }
}

// Verify webhook signature
function verifySignature(payload, signature) {
    if (!signature) {
        return false;
    }
    
    // Remove "sha256=" prefix if present
    signature = signature.replace('sha256=', '');
    
    // Create HMAC
    const hmac = crypto.createHmac('sha256', config.secret);
    hmac.update(payload);
    const expectedSignature = hmac.digest('hex');
    
    return crypto.timingSafeEqual(
        Buffer.from(signature, 'hex'),
        Buffer.from(expectedSignature, 'hex')
    );
}

// Process webhook payload
function processWebhook(payload) {
    try {
        const data = JSON.parse(payload);
        
        log('info', `Received webhook: ${data.event} from ${data.source}`);
        
        // Process different event types
        switch (data.event) {
            case 'network_failure':
                handleNetworkFailure(data);
                break;
            case 'starlink_obstruction':
                handleStarlinkObstruction(data);
                break;
            case 'cellular_issue':
                handleCellularIssue(data);
                break;
            case 'system_alert':
                handleSystemAlert(data);
                break;
            default:
                log('warn', `Unknown event type: ${data.event}`);
        }
        
        return true;
    } catch (error) {
        log('error', `Failed to process webhook: ${error.message}`);
        return false;
    }
}

// Event handlers
function handleNetworkFailure(data) {
    log('info', 'Processing network failure event', {
        source: data.source,
        severity: data.severity,
        message: data.message
    });
    
    // Simulate processing time
    setTimeout(() => {
        log('info', 'Network failure processed successfully');
    }, 1000);
}

function handleStarlinkObstruction(data) {
    log('info', 'Processing Starlink obstruction event', {
        source: data.source,
        severity: data.severity,
        message: data.message
    });
    
    // Simulate processing time
    setTimeout(() => {
        log('info', 'Starlink obstruction processed successfully');
    }, 1000);
}

function handleCellularIssue(data) {
    log('info', 'Processing cellular issue event', {
        source: data.source,
        severity: data.severity,
        message: data.message
    });
    
    // Simulate processing time
    setTimeout(() => {
        log('info', 'Cellular issue processed successfully');
    }, 1000);
}

function handleSystemAlert(data) {
    log('info', 'Processing system alert event', {
        source: data.source,
        severity: data.severity,
        message: data.message
    });
    
    // Simulate processing time
    setTimeout(() => {
        log('info', 'System alert processed successfully');
    }, 1000);
}

// Rate limiting
class RateLimiter {
    constructor(limit = 100, window = 60000) {
        this.limit = limit;
        this.window = window;
        this.requests = new Map();
    }
    
    isAllowed(clientIP) {
        const now = Date.now();
        const clientRequests = this.requests.get(clientIP) || [];
        
        // Remove old requests
        const validRequests = clientRequests.filter(time => now - time < this.window);
        
        if (validRequests.length >= this.limit) {
            return false;
        }
        
        // Add current request
        validRequests.push(now);
        this.requests.set(clientIP, validRequests);
        
        return true;
    }
}

const rateLimiter = new RateLimiter();

// Get client IP
function getClientIP(req) {
    return req.headers['x-forwarded-for']?.split(',')[0] ||
           req.headers['x-real-ip'] ||
           req.connection.remoteAddress ||
           req.socket.remoteAddress;
}

// Request handler
function handleRequest(req, res) {
    const clientIP = getClientIP(req);
    
    // Set CORS headers
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type, X-Hub-Signature-256');
    
    // Handle preflight requests
    if (req.method === 'OPTIONS') {
        res.writeHead(200);
        res.end();
        return;
    }
    
    // Only allow POST requests
    if (req.method !== 'POST') {
        log('warn', `Invalid method: ${req.method} from ${clientIP}`);
        res.writeHead(405, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ error: 'Method not allowed' }));
        return;
    }
    
    // Check rate limit
    if (!rateLimiter.isAllowed(clientIP)) {
        log('warn', `Rate limit exceeded for ${clientIP}`);
        res.writeHead(429, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ error: 'Rate limit exceeded' }));
        return;
    }
    
    let body = '';
    
    req.on('data', chunk => {
        body += chunk.toString();
    });
    
    req.on('end', () => {
        try {
            // Verify signature
            const signature = req.headers['x-hub-signature-256'];
            if (!verifySignature(body, signature)) {
                log('error', `Invalid signature from ${clientIP}`);
                res.writeHead(401, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: 'Invalid signature' }));
                return;
            }
            
            // Process webhook
            const success = processWebhook(body);
            
            if (success) {
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ status: 'processed' }));
            } else {
                res.writeHead(400, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: 'Failed to process webhook' }));
            }
            
        } catch (error) {
            log('error', `Request processing error: ${error.message}`);
            res.writeHead(500, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ error: 'Internal server error' }));
        }
    });
    
    req.on('error', (error) => {
        log('error', `Request error: ${error.message}`);
    });
}

// Health check endpoint
function handleHealthCheck(req, res) {
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({
        status: 'healthy',
        timestamp: new Date().toISOString(),
        uptime: process.uptime()
    }));
}

// Metrics endpoint
function handleMetrics(req, res) {
    res.writeHead(200, { 'Content-Type': 'text/plain' });
    res.end(`# HELP webhook_requests_total Total number of webhook requests
# TYPE webhook_requests_total counter
webhook_requests_total{status="processed"} 0
webhook_requests_total{status="failed"} 0
`);
}

// Main request router
function requestHandler(req, res) {
    const parsedUrl = url.parse(req.url, true);
    
    switch (parsedUrl.pathname) {
        case '/webhook':
            handleRequest(req, res);
            break;
        case '/health':
            handleHealthCheck(req, res);
            break;
        case '/metrics':
            handleMetrics(req, res);
            break;
        default:
            res.writeHead(404, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ error: 'Not found' }));
    }
}

// Test function
function runTest() {
    log('info', 'Running webhook receiver test...');
    
    // Create test payload
    const testPayload = {
        event: 'network_failure',
        timestamp: new Date().toISOString(),
        source: 'test-client',
        severity: 'high',
        message: 'Test network failure event',
        data: {
            interface: 'eth0',
            error: 'Connection timeout'
        }
    };
    
    // Create signature
    const payload = JSON.stringify(testPayload);
    const hmac = crypto.createHmac('sha256', config.secret);
    hmac.update(payload);
    const signature = hmac.digest('hex');
    
    // Send test request
    const options = {
        hostname: 'localhost',
        port: config.port,
        path: '/webhook',
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'X-Hub-Signature-256': `sha256=${signature}`
        }
    };
    
    const req = http.request(options, (res) => {
        let data = '';
        res.on('data', chunk => data += chunk);
        res.on('end', () => {
            log('info', `Test response: ${res.statusCode} - ${data}`);
            process.exit(0);
        });
    });
    
    req.on('error', (error) => {
        log('error', `Test request failed: ${error.message}`);
        process.exit(1);
    });
    
    req.write(payload);
    req.end();
}

// Start server
function startServer() {
    const server = config.ssl ? https.createServer({
        cert: config.cert,
        key: config.key
    }, requestHandler) : http.createServer(requestHandler);
    
    server.listen(config.port, () => {
        log('info', `Webhook receiver started on port ${config.port}`);
        log('info', `SSL: ${config.ssl ? 'enabled' : 'disabled'}`);
        log('info', `Secret: ${config.secret.substring(0, 8)}...`);
        log('info', `Log level: ${config.logLevel}`);
    });
    
    server.on('error', (error) => {
        log('error', `Server error: ${error.message}`);
        process.exit(1);
    });
    
    return server;
}

// Handle process signals
process.on('SIGINT', () => {
    log('info', 'Shutting down webhook receiver...');
    process.exit(0);
});

process.on('SIGTERM', () => {
    log('info', 'Shutting down webhook receiver...');
    process.exit(0);
});

// Main execution
if (require.main === module) {
    // Check if test mode
    if (process.argv.includes('--test')) {
        // Start server and run test
        const server = startServer();
        setTimeout(runTest, 1000);
    } else {
        // Start server normally
        startServer();
    }
}

module.exports = {
    startServer,
    processWebhook,
    verifySignature,
    RateLimiter
};
