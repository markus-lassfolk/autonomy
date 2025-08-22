# 🎯 autonomy GPS Integration Guide

## 📊 **Standardized GPS Output Format**

### **🎯 Design Goals:**
- **Consistent format** across all GPS sources
- **Rich metadata** for decision making
- **Multiple output formats** (JSON, CLI, CSV)
- **Error handling** and fallback support
- **Cross-validation** between sources

## 🏗️ **Core Data Structure**

### **autonomyGPSData (Individual Source)**
```json
{
  "latitude": 59.48007000,
  "longitude": 18.27985000,
  "altitude": 9.5,
  "accuracy": 0.4,
  "speed": 0.0,
  "course": 0.0,
  "satellites": 37,
  "valid": true,
  "fix_type": "3d",
  "fix_quality": 3,
  "source": "quectel_gnss",
  "priority": 1,
  "method": "AT+QGPSLOC=2",
  "timestamp": 1755308573,
  "datetime": "2025-08-16 01:42:53 UTC",
  "age_seconds": 0.1,
  "hdop": 0.4,
  "collected_at": "2025-08-16T01:42:53Z",
  "response_time_ms": 245.5
}
```

### **autonomyGPSResponse (Complete Response)**
```json
{
  "status": "success",
  "message": "All GPS sources operational",
  "primary": { /* autonomyGPSData */ },
  "secondary": { /* autonomyGPSData */ },
  "tertiary": { /* autonomyGPSData */ },
  "best_source": "quectel_gnss",
  "confidence": "high",
  "source_agreement": true,
  "max_distance_m": 3.2,
  "collection_time_ms": 401.8,
  "timestamp": 1755308573
}
```

## 🔧 **Implementation in autonomy Daemon**

### **1. GPS Command Interface**
```bash
# Basic GPS query
autonomy gps

# Detailed GPS with all sources
autonomy gps --all-sources

# JSON output for scripts
autonomy gps --json

# Continuous monitoring
autonomy gps --monitor --interval=30s

# Specific source only
autonomy gps --source=quectel
```

### **2. Priority-Based Collection**
```go
func CollectGPS(ctx context.Context) (*autonomyGPSResponse, error) {
    response := &autonomyGPSResponse{
        Timestamp: time.Now().Unix(),
    }
    
    // Try primary source (Quectel Multi-GNSS)
    if primary, err := collectQuectelGPS(ctx); err == nil && primary.Valid {
        response.Primary = primary
        response.BestSource = "quectel_gnss"
        response.Status = "success"
        response.Confidence = "high"
    }
    
    // Try secondary source (Starlink) for verification
    if secondary, err := collectStarlinkGPS(ctx); err == nil && secondary.Valid {
        response.Secondary = secondary
        
        // Cross-validate if we have primary
        if response.Primary != nil {
            distance := calculateDistance(response.Primary, secondary)
            response.MaxDistance = distance
            response.SourceAgreement = distance < 50 // Within 50m
        }
    }
    
    // Try tertiary source (Basic GPS) if needed
    if response.Primary == nil {
        if tertiary, err := collectBasicGPS(ctx); err == nil && tertiary.Valid {
            response.Tertiary = tertiary
            if response.BestSource == "" {
                response.BestSource = "basic_gps"
                response.Status = "partial"
                response.Confidence = "medium"
            }
        }
    }
    
    // Determine final status
    if response.BestSource == "" {
        response.Status = "failed"
        response.Message = "No GPS sources available"
        response.Confidence = "none"
    }
    
    return response, nil
}
```

## 📋 **Output Formats**

### **🖥️ Command Line Formats**

#### **Compact (for scripts):**
```bash
SUCCESS|quectel_gnss|59.48007000,18.27985000|9.5|0.4|37|3d
```

#### **Detailed (for humans):**
```bash
✅ GPS Status: SUCCESS
📍 Location: 59.48007000°, 18.27985000°
🏔️  Altitude: 9.5 meters
🎯 Accuracy: 0.4 HDOP (excellent)
🛰️  Satellites: 37 (multi-constellation)
📡 Source: Quectel Multi-GNSS (primary)
⏰ Time: 2025-08-16 01:42:53 UTC
🗺️  Maps: https://www.google.com/maps?q=59.48007000,18.27985000
```

#### **CSV (for logging):**
```csv
timestamp,status,source,latitude,longitude,altitude,accuracy,satellites,fix_type
1755308573,success,quectel_gnss,59.48007000,18.27985000,9.5,0.4,37,3d
```

### **📊 JSON API Response**
```json
{
  "status": "success",
  "message": "Primary GPS operational",
  "primary": {
    "latitude": 59.48007000,
    "longitude": 18.27985000,
    "altitude": 9.5,
    "accuracy": 0.4,
    "satellites": 37,
    "valid": true,
    "fix_type": "3d",
    "source": "quectel_gnss",
    "method": "AT+QGPSLOC=2",
    "timestamp": 1755308573,
    "response_time_ms": 245.5
  },
  "best_source": "quectel_gnss",
  "confidence": "high",
  "collection_time_ms": 245.5
}
```

## 🔄 **Integration Points**

### **1. Failover Decision Engine**
```go
// Use GPS data for failover decisions
gpsResponse, err := CollectGPS(ctx)
if err != nil || gpsResponse.Status == "failed" {
    log.Warn("GPS unavailable for location-based decisions")
    return
}

// Check if location changed significantly
if hasLocationChanged(gpsResponse.Primary, lastKnownLocation) {
    log.Info("Location change detected", 
        "old", lastKnownLocation,
        "new", gpsResponse.Primary,
        "distance", calculateDistance(...))
    
    // Trigger location-based failover logic
    handleLocationChange(gpsResponse.Primary)
}
```

### **2. Monitoring & Alerting**
```go
// Monitor GPS health
func MonitorGPSHealth(ctx context.Context) {
    ticker := time.NewTicker(30 * time.Second)
    defer ticker.Stop()
    
    for {
        select {
        case <-ticker.C:
            gpsResponse, err := CollectGPS(ctx)
            
            // Alert on GPS failures
            if gpsResponse.Status == "failed" {
                sendAlert("GPS_FAILURE", "All GPS sources unavailable")
            }
            
            // Alert on source disagreement
            if gpsResponse.SourceAgreement == false {
                sendAlert("GPS_DISAGREEMENT", 
                    fmt.Sprintf("GPS sources disagree by %.1fm", 
                        gpsResponse.MaxDistance))
            }
            
            // Log GPS status
            logGPSStatus(gpsResponse)
            
        case <-ctx.Done():
            return
        }
    }
}
```

### **3. Configuration Options**
```yaml
# autonomy.yaml
gps:
  enabled: true
  sources:
    quectel:
      enabled: true
      priority: 1
      timeout: 5s
      command: "gsmctl -A 'AT+QGPSLOC=2'"
    starlink:
      enabled: true
      priority: 2
      timeout: 10s
      method: "api"
    basic:
      enabled: true
      priority: 3
      timeout: 3s
      command: "gpsctl"
  
  validation:
    max_age_seconds: 300
    min_satellites: 4
    max_hdop: 5.0
    agreement_threshold_m: 100
  
  monitoring:
    interval: 30s
    log_level: "info"
    alerts_enabled: true
```

## 🎯 **Usage Examples**

### **Basic GPS Query:**
```bash
$ autonomy gps
✅ GPS Status: SUCCESS
📍 Location: 59.48007000°, 18.27985000°
🛰️  Satellites: 37 (Quectel Multi-GNSS)
```

### **JSON for Scripts:**
```bash
$ autonomy gps --json | jq '.primary | {lat: .latitude, lon: .longitude}'
{
  "lat": 59.48007,
  "lon": 18.27985
}
```

### **All Sources:**
```bash
$ autonomy gps --all-sources
🏆 PRIMARY (Quectel): 59.48007°, 18.27985° (37 sats, 0.4 HDOP)
🥈 SECONDARY (Starlink): 59.48006°, 18.27982° (12 sats, ~3m accuracy)
🥉 TERTIARY (Basic): 59.48007°, 18.27985° (10 sats, 0.5m accuracy)
📏 Agreement: ✅ All sources within 3.2m
```

## 🚀 **Benefits for autonomy**

1. **🎯 Precise Location Tracking** - Know exactly where the system is
2. **🔄 Location-Based Failover** - Different rules for different locations
3. **📊 Rich Telemetry** - Detailed GPS health and performance data
4. **🛡️ Redundancy** - Multiple GPS sources for reliability
5. **🔍 Cross-Validation** - Detect GPS spoofing or errors
6. **📈 Historical Tracking** - Log location changes over time

This standardized format gives autonomy powerful location intelligence while maintaining simplicity and reliability! 🛰️🎯
