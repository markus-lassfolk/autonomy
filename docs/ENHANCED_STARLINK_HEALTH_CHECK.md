# Enhanced Starlink Health Check - Proposed Implementation

## 🚨 **Current Issues with Starlink Health Check**

The existing `StarlinkManager` is **inadequate** for real Starlink monitoring:

### **What It Currently Does (Poorly)**
1. **Process Check**: Looks for non-existent "starlink_monitor" process
2. **Log Check**: Searches for word "starlink" in generic logs
3. **Fix Action**: Restarts cron daemon (doesn't help gRPC issues)
4. **No IP Detection**: Doesn't know where Starlink dish is located
5. **No API Testing**: Doesn't verify gRPC communication works

## ✅ **Enhanced Starlink Health Check (Needed)**

### **1. Starlink IP Discovery**
```go
func (sm *StarlinkManager) discoverStarlinkIP() (string, error) {
    // Method 1: Check for CGNAT range (100.64.0.0/10)
    cmd := exec.Command("ip", "route", "show")
    output, err := cmd.Output()
    if err != nil {
        return "", err
    }
    
    lines := strings.Split(string(output), "\n")
    for _, line := range lines {
        if strings.Contains(line, "100.") {
            // Extract gateway IP from route
            fields := strings.Fields(line)
            for i, field := range fields {
                if field == "via" && i+1 < len(fields) {
                    ip := fields[i+1]
                    if strings.HasPrefix(ip, "100.") {
                        return ip, nil
                    }
                }
            }
        }
    }
    
    // Method 2: Try common Starlink IPs
    commonIPs := []string{"192.168.100.1", "192.168.1.1"}
    for _, ip := range commonIPs {
        if sm.testStarlinkAPI(ip) {
            return ip, nil
        }
    }
    
    return "", fmt.Errorf("Starlink IP not found")
}
```

### **2. Real gRPC API Testing**
```go
func (sm *StarlinkManager) testStarlinkAPI(ip string) bool {
    // Test using our working grpcurl approach
    grpcurlPath := "/tmp/grpcurl"
    if _, err := os.Stat("/usr/bin/grpcurl"); err == nil {
        grpcurlPath = "/usr/bin/grpcurl"
    }
    
    cmd := exec.Command(grpcurlPath, 
        "-plaintext", 
        "-max-time", "5",
        "-d", `{"get_status":{}}`,
        fmt.Sprintf("%s:9200", ip),
        "SpaceX.API.Device.Device/Handle")
    
    output, err := cmd.Output()
    if err != nil {
        return false
    }
    
    // Check if response contains expected Starlink data
    return strings.Contains(string(output), "uptime") || 
           strings.Contains(string(output), "obstruction")
}
```

### **3. Comprehensive Health Checks**
```go
func (sm *StarlinkManager) Check(ctx context.Context) error {
    // Step 1: Discover Starlink IP
    starlinkIP, err := sm.discoverStarlinkIP()
    if err != nil {
        sm.logger.Error("Cannot find Starlink dish IP", "error", err)
        return sm.handleStarlinkNotFound(ctx)
    }
    
    // Step 2: Test gRPC API
    if !sm.testStarlinkAPI(starlinkIP) {
        sm.logger.Error("Starlink gRPC API not responding", "ip", starlinkIP)
        return sm.handleAPIFailure(ctx, starlinkIP)
    }
    
    // Step 3: Check grpcurl availability
    if !sm.isGrpcurlAvailable() {
        sm.logger.Error("grpcurl not available for Starlink API")
        return sm.installGrpcurl(ctx)
    }
    
    // Step 4: Verify autonomyd is collecting Starlink metrics
    if !sm.hasRecentStarlinkMetrics() {
        sm.logger.Error("autonomyd not collecting Starlink metrics")
        return sm.restartautonomyd(ctx)
    }
    
    sm.logger.Debug("Starlink health check passed", "ip", starlinkIP)
    return nil
}
```

### **4. Smart Fix Actions**
```go
func (sm *StarlinkManager) handleAPIFailure(ctx context.Context, ip string) error {
    sm.logger.Info("Attempting to fix Starlink API issues", "ip", ip)
    
    // Try 1: Clear any cached connections
    exec.Command("ip", "route", "flush", "cache").Run()
    
    // Try 2: Restart network interface to Starlink
    if iface := sm.findStarlinkInterface(ip); iface != "" {
        exec.Command("ifdown", iface).Run()
        time.Sleep(2 * time.Second)
        exec.Command("ifup", iface).Run()
    }
    
    // Try 3: Restart autonomyd to reset collectors
    return sm.restartautonomyd(ctx)
}

func (sm *StarlinkManager) installGrpcurl(ctx context.Context) error {
    // Download and install grpcurl if missing
    sm.logger.Info("Installing grpcurl for Starlink API")
    
    cmd := exec.Command("wget", "-O", "/tmp/grpcurl.tar.gz",
        "https://github.com/fullstorydev/grpcurl/releases/download/v1.9.3/grpcurl_1.9.3_linux_armv7.tar.gz")
    if err := cmd.Run(); err != nil {
        return fmt.Errorf("failed to download grpcurl: %w", err)
    }
    
    cmd = exec.Command("tar", "-xzf", "/tmp/grpcurl.tar.gz", "-C", "/tmp/")
    return cmd.Run()
}
```

### **5. Starlink-Specific Metrics Validation**
```go
func (sm *StarlinkManager) hasRecentStarlinkMetrics() bool {
    // Check autonomyd logs for successful Starlink metric collection
    cmd := exec.Command("logread", "-l", "300") // Last 5 minutes
    output, err := cmd.Output()
    if err != nil {
        return false
    }
    
    logContent := string(output)
    
    // Look for successful Starlink API calls
    successPatterns := []string{
        "Successfully collected metrics.*starlink",
        "Starlink gRPC API response",
        "grpcurl.*get_status.*success",
    }
    
    for _, pattern := range successPatterns {
        if strings.Contains(logContent, pattern) {
            return true
        }
    }
    
    // Check for mock data warnings (indicates API failure)
    if strings.Contains(logContent, "MOCK DATA") || 
       strings.Contains(logContent, "All Starlink API methods failed") {
        return false
    }
    
    return false
}
```

## 📊 **Enhanced Monitoring Capabilities**

### **What the Enhanced Version Would Monitor:**
1. **Starlink IP Discovery**: Automatically find dish IP (192.168.100.1)
2. **gRPC API Health**: Test actual API calls with grpcurl
3. **API Response Quality**: Verify real data vs mock data
4. **grpcurl Availability**: Ensure tool is installed and working
5. **Collector Status**: Verify autonomyd is getting real Starlink metrics
6. **Network Connectivity**: Test route to Starlink dish

### **Actions It Would Take:**
1. **API Failures**: Restart network interface, clear route cache
2. **Missing grpcurl**: Download and install automatically
3. **Mock Data Detection**: Restart autonomyd to reset collectors
4. **IP Discovery Failure**: Scan network for Starlink dish
5. **Persistent Issues**: Send detailed diagnostic notifications

### **Notifications It Would Send:**
```
🛰️ Starlink Health Alert
Issue: gRPC API not responding
Starlink IP: 192.168.100.1
Actions taken:
• Restarted network interface
• Cleared route cache  
• Restarted autonomyd daemon
Status: Monitoring for recovery
```

## 🎯 **Implementation Priority**

The current Starlink health check is **essentially useless** for our gRPC-based system. Given our recent issues with:
- Starlink API failures
- Mock data fallbacks  
- grpcurl installation
- Real metrics collection

An enhanced Starlink health check should be **high priority** to prevent future API issues from going undetected.
