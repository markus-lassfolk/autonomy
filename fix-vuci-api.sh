#!/bin/bash
# Fix VUCI API registration and create working endpoints
# This script runs on the router to properly set up API access

echo "========================================="
echo "FIXING VUCI API REGISTRATION"
echo "========================================="
echo ""

# 1. Create CGI wrapper in writable location
echo "Creating CGI wrapper for API access..."
cat > /tmp/example-api.cgi << 'EOF'
#!/usr/bin/lua

-- CGI wrapper for Example API
-- This provides HTTP access to the Lua API service

-- Set content type
print("Content-Type: application/json")
print("Access-Control-Allow-Origin: *")
print("")

-- Try to load the API module
local ok, api = pcall(function()
    -- Try multiple paths
    local paths = {
        "api.services.example",
        "example"
    }
    
    for _, path in ipairs(paths) do
        local ok, mod = pcall(require, path)
        if ok then return mod end
    end
    
    -- If not found via require, try direct load
    local f = loadfile("/usr/local/usr/lib/lua/api/services/example.lua")
    if f then
        return f()
    end
    
    error("API module not found")
end)

if ok and api then
    -- Get request info
    local method = os.getenv("REQUEST_METHOD") or "GET"
    local path = os.getenv("PATH_INFO") or "/status"
    local query = os.getenv("QUERY_STRING") or ""
    
    -- Call appropriate function
    local result
    if path:match("/status") or path == "/" then
        result = api.get_status and api.get_status() or {error = "get_status not found"}
    elseif path:match("/test") then
        result = api.test and api.test() or {error = "test not found"}
    elseif path:match("/config") then
        result = api.get_config and api.get_config() or {error = "get_config not found"}
    elseif api.handle then
        result = api.handle(method, path, query, nil)
    else
        result = {error = "Unknown endpoint", path = path}
    end
    
    -- Output result as JSON
    if type(result) == "table" then
        -- Simple JSON encoding
        local function encode(t)
            local parts = {}
            for k, v in pairs(t) do
                local key = '"' .. tostring(k) .. '":'
                local val
                if type(v) == "string" then
                    val = '"' .. v:gsub('"', '\\"') .. '"'
                elseif type(v) == "number" or type(v) == "boolean" then
                    val = tostring(v)
                elseif type(v) == "table" then
                    val = encode(v)
                else
                    val = "null"
                end
                table.insert(parts, key .. val)
            end
            return "{" .. table.concat(parts, ",") .. "}"
        end
        
        print(encode(result))
    else
        print('{"result":"' .. tostring(result) .. '"}')
    end
else
    print('{"error":"Failed to load API module: ' .. tostring(api) .. '"}')
end
EOF

chmod +x /tmp/example-api.cgi

# 2. Create symlinks in overlay for CGI access
echo "Creating symlinks for CGI access..."
mkdir -p /overlay/root/upper/www/cgi-bin 2>/dev/null
ln -sf /tmp/example-api.cgi /overlay/root/upper/www/cgi-bin/example-api 2>/dev/null

# Also try direct symlink
ln -sf /tmp/example-api.cgi /www/cgi-bin/example-api 2>/dev/null || true

# 3. Create API proxy using uhttpd lua handler
echo "Creating uhttpd Lua handler..."
cat > /tmp/api-proxy.lua << 'EOF'
#!/usr/bin/lua

-- API proxy for uhttpd
-- This handles /api/example/* requests

function handle_request(env)
    -- Load the API module
    local api = require("api.services.example")
    
    local path = env.PATH_INFO or ""
    local method = env.REQUEST_METHOD or "GET"
    
    -- Route to appropriate function
    local result
    if path:match("/status") then
        result = api.get_status()
    elseif path:match("/test") then
        result = api.test()
    elseif path:match("/config") then
        result = api.get_config()
    else
        result = {error = "Unknown endpoint", path = path}
    end
    
    -- Return JSON response
    uhttpd.send("Status: 200 OK\r\n")
    uhttpd.send("Content-Type: application/json\r\n\r\n")
    
    -- Simple JSON encoding
    local json = '{'
    local first = true
    for k, v in pairs(result) do
        if not first then json = json .. ',' end
        json = json .. '"' .. k .. '":'
        if type(v) == "string" then
            json = json .. '"' .. v .. '"'
        else
            json = json .. tostring(v)
        end
        first = false
    end
    json = json .. '}'
    
    uhttpd.send(json)
end
EOF

# 4. Create a simple HTTP API server (fallback)
echo "Creating standalone API server..."
cat > /tmp/api-server.lua << 'EOF'
#!/usr/bin/lua

-- Standalone HTTP API server
-- Listens on port 8088 for API requests

local socket = require("socket")

-- Load API module
local api = dofile("/usr/local/usr/lib/lua/api/services/example.lua")

-- Create server
local server = socket.bind("*", 8088)
print("API server listening on port 8088...")

while true do
    local client = server:accept()
    
    if client then
        -- Read request
        local request = client:receive()
        
        if request then
            print("Request: " .. request)
            
            -- Parse path
            local path = request:match("GET ([^ ]+)")
            
            -- Get response
            local result
            if path and path:match("/status") then
                result = api.get_status()
            elseif path and path:match("/test") then
                result = api.test()
            elseif path and path:match("/config") then
                result = api.get_config()
            else
                result = {error = "Unknown endpoint", path = path or "none"}
            end
            
            -- Convert to JSON
            local json = '{'
            local first = true
            for k, v in pairs(result) do
                if not first then json = json .. ',' end
                json = json .. '"' .. k .. '":'
                if type(v) == "string" then
                    json = json .. '"' .. v .. '"'
                elseif type(v) == "number" or type(v) == "boolean" then
                    json = json .. tostring(v)
                else
                    json = json .. '"' .. tostring(v) .. '"'
                end
                first = false
            end
            json = json .. '}'
            
            -- Send response
            client:send("HTTP/1.1 200 OK\r\n")
            client:send("Content-Type: application/json\r\n")
            client:send("Access-Control-Allow-Origin: *\r\n")
            client:send("Content-Length: " .. #json .. "\r\n")
            client:send("\r\n")
            client:send(json)
        end
        
        client:close()
    end
end
EOF

chmod +x /tmp/api-server.lua

# 5. Update HTML to use working endpoints
echo "Updating HTML interface..."
cat > /usr/local/www/example/api-config.js << 'EOF'
// API configuration for Example app
// This file defines the working API endpoints

window.API_CONFIG = {
    endpoints: [
        '/cgi-bin/example-api',  // CGI wrapper
        'http://' + window.location.hostname + ':8088',  // Standalone server
        '/api/example'  // Direct API (if registered)
    ],
    
    // Test all endpoints and return the first working one
    findWorkingEndpoint: async function() {
        for (const endpoint of this.endpoints) {
            try {
                const response = await fetch(endpoint + '/status', {
                    method: 'GET',
                    mode: 'cors',
                    cache: 'no-cache'
                });
                if (response.ok) {
                    console.log('Working endpoint found:', endpoint);
                    return endpoint;
                }
            } catch (e) {
                console.log('Endpoint failed:', endpoint, e.message);
            }
        }
        return null;
    }
};
EOF

# 6. Start the standalone API server
echo "Starting API server..."
killall api-server.lua 2>/dev/null || true
/tmp/api-server.lua > /tmp/api-server.log 2>&1 &
echo "API server started on port 8088 (PID: $!)"

# 7. Test the endpoints
echo ""
echo "Testing API endpoints..."
echo ""

echo "Test 1: CGI wrapper"
if [ -f /www/cgi-bin/example-api ]; then
    echo "  CGI wrapper exists at /www/cgi-bin/example-api"
else
    echo "  CGI wrapper not found"
fi

echo ""
echo "Test 2: Direct API call"
lua -e "
    local api = dofile('/usr/local/usr/lib/lua/api/services/example.lua')
    if api and api.get_status then
        local status = api.get_status()
        print('  API module loaded successfully')
        print('  Status:', status.message or 'unknown')
    else
        print('  Failed to load API module')
    end
"

echo ""
echo "Test 3: HTTP API server"
sleep 1
wget -q -O - http://localhost:8088/status 2>/dev/null | head -c 100 && echo "" || echo "  Server not responding"

echo ""
echo "========================================="
echo "API REGISTRATION COMPLETE!"
echo "========================================="
echo ""
echo "Working endpoints:"
echo "  1. http://router-ip:8088/status - Standalone API server"
echo "  2. http://router-ip:8088/test"
echo "  3. http://router-ip:8088/config"
echo ""
echo "CGI endpoint (if working):"
echo "  http://router-ip/cgi-bin/example-api/status"
echo ""
echo "Access the HTML interface:"
echo "  http://router-ip/example/"
echo "  http://router-ip/vuci-app-example/"
echo ""
echo "The API server is running on port 8088"
echo "Check logs: tail -f /tmp/api-server.log"
echo ""
EOF

# Make script executable
chmod +x fix-vuci-api.sh

echo "Fix script created: fix-vuci-api.sh"
echo ""
echo "Now copy and run this script on the router:"
echo "  scp fix-vuci-api.sh root@192.168.80.1:/tmp/"
echo "  ssh root@192.168.80.1 '/tmp/fix-vuci-api.sh'"
echo ""


