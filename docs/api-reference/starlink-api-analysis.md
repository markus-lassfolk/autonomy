# Starlink API Analysis & Data Available

## 🛰️ Connection Status

✅ **TCP Connectivity**: Successfully connected to `192.168.100.1:9200`  
✅ **gRPC Port Open**: The Starlink dish is listening on the correct gRPC port  
❌ **HTTP API**: No REST/HTTP API available (404 on all HTTP endpoints)  
⚠️  **gRPC Protobuf**: Requires proper protobuf message encoding (not JSON)

## 📡 Available API Methods

Based on the official Starlink gRPC API documentation, the following methods are available:

### 1. `get_status` - Real-time Status Information

#### Most Important for Failover Monitoring

**Available Data:**

```json
{
  "dishGetStatus": {
    "deviceInfo": {
      "id": "string",
      "hardwareVersion": "string", 
      "softwareVersion": "string",
      "countryCode": "string",
      "utcOffsetS": "number",
      "generationNumber": "number",
      "dishCohoused": "boolean"
    },
    "deviceState": {
      "uptimeS": "number"
    },
    "obstructionStats": {
      "fractionObstructed": "number",      // 0.0-1.0 (% sky blocked)
      "validS": "number",                  // Seconds of valid data
      "currentlyObstructed": "boolean",    // Currently blocked?
      "wedgeFractionObstructed": ["number"], // Per-wedge obstruction
      "wedgeAbsFractionObstructed": ["number"]
    },
    "popPingLatencyMs": "number",          // 🔥 KEY: Latency to PoP
    "downlinkThroughputBps": "number",     // Download speed
    "uplinkThroughputBps": "number",       // Upload speed  
    "popPingDropRate": "number",           // 🔥 KEY: Packet loss rate
    "snr": "number",                       // 🔥 KEY: Signal quality
    "secondsToFirstNonemptySlot": "number",
    "boresightAzimuthDeg": "number",       // Dish pointing
    "boresightElevationDeg": "number",
    "gpsStats": {
      "gpsValid": "boolean",               // GPS fix status
      "gpsSats": "number",                 // Satellite count
      "noSatsAfterTtff": "boolean"         // No satellites after time to first fix
    },
    "inhibitGps": "boolean"                // GPS inhibited
  }
}
```

### 2. `get_location` - GPS Location Data

#### Primary GPS Source

**Available Data:**

```json
{
  "dishGetLocation": {
    "latitude": "number",                  // 🔥 KEY: GPS latitude
    "longitude": "number",                 // 🔥 KEY: GPS longitude  
    "altitude": "number",                  // 🔥 KEY: GPS altitude
    "sigmaM": "number",                    // 🔥 KEY: GPS accuracy (meters)
    "horizontalSpeedMps": "number",        // Horizontal speed
    "verticalSpeedMps": "number",          // Vertical speed
    "source": "string"                     // GPS source (GNC_FUSED, etc.)
  }
}
```

### 3. `get_diagnostics` - Enhanced Diagnostics

#### Additional GPS and System Data

**Available Data:**

```json
{
  "dishGetDiagnostics": {
    "locationEnabled": "boolean",          // Location service enabled
    "latitude": "number",                  // GPS latitude (alternative)
    "longitude": "number",                 // GPS longitude (alternative)
    "altitudeMeters": "number",            // GPS altitude (alternative)
    "uncertaintyMeters": "number",         // 🔥 KEY: GPS uncertainty
    "uncertaintyMetersValid": "boolean",   // Uncertainty validity
    "gpsTimeS": "number",                  // 🔥 KEY: GPS timestamp
    "deviceInfo": {
      "id": "string",
      "hardwareVersion": "string",
      "softwareVersion": "string"
    }
  }
}
```

### 4. `get_history` - Historical Performance Data

#### For Trend Analysis

**Available Data:**

```json
{
  "dishGetHistory": {
    "popPingLatencyMs": ["number"],        // Historical latency
    "popPingDropRate": ["number"],         // Historical packet loss
    "downlinkThroughputBps": ["number"],   // Historical download speed
    "uplinkThroughputBps": ["number"],     // Historical upload speed
    "snr": ["number"],                     // Historical signal quality
    "obstructionStats": {
      "fractionObstructed": ["number"]     // Historical obstruction
    }
  }
}
```

### 5. `get_device_info` - Device Information

#### Static Device Data

**Available Data:**

```json
{
  "dishGetDeviceInfo": {
    "id": "string",
    "hardwareVersion": "string",
    "softwareVersion": "string", 
    "countryCode": "string",
    "utcOffsetS": "number",
    "generationNumber": "number",
    "dishCohoused": "boolean",
    "utcnsOffsetNs": "number"
  }
}
```

## 🔥 **Key Fields for Autonomy Failover**

### **Primary Failover Triggers:**

1. **`popPingLatencyMs`** - Latency to PoP (should be <100ms)
2. **`popPingDropRate`** - Packet loss rate (should be <1%)
3. **`snr`** - Signal-to-noise ratio (higher is better)
4. **`fractionObstructed`** - Sky obstruction (should be <5%)
5. **`currentlyObstructed`** - Currently blocked (should be false)

### **GPS Data for Location Services:**

1. **`latitude/longitude`** - Primary coordinates
2. **`sigmaM/uncertaintyMeters`** - Accuracy measurements
3. **`gpsValid`** - GPS fix status
4. **`gpsSats`** - Satellite count
5. **`gpsTimeS`** - GPS timestamp

### **Performance Monitoring:**

1. **`downlinkThroughputBps`** - Download speed
2. **`uplinkThroughputBps`** - Upload speed
3. **`secondsToFirstNonemptySlot`** - Connection time

## 🚀 **Implementation Strategy**

### **1. Real-time Monitoring (get_status)**

```go
func (s *StarlinkMonitor) CheckFailoverConditions() bool {
    status, err := s.getStatus()
    if err != nil {
        return true // Failover on connection error
    }
    
    // Check critical thresholds
    if status.PopPingLatencyMs > 500 { // 500ms threshold
        return true
    }
    
    if status.PopPingDropRate > 0.05 { // 5% packet loss
        return true
    }
    
    if status.FractionObstructed > 0.1 { // 10% obstruction
        return true
    }
    
    if status.CurrentlyObstructed {
        return true
    }
    
    return false
}
```

### **2. GPS Location Collection (get_location + get_diagnostics)**

```go
func (s *StarlinkGPS) GetLocation() (*Location, error) {
    // Try get_location first
    location, err := s.getLocation()
    if err != nil {
        // Fallback to get_diagnostics
        diag, err := s.getDiagnostics()
        if err != nil {
            return nil, err
        }
        
        return &Location{
            Latitude:  diag.Latitude,
            Longitude: diag.Longitude,
            Altitude:  diag.AltitudeMeters,
            Accuracy:  diag.UncertaintyMeters,
        }, nil
    }
    
    return &Location{
        Latitude:  location.Latitude,
        Longitude: location.Longitude,
        Altitude:  location.Altitude,
        Accuracy:  location.SigmaM,
    }, nil
}
```

### **3. Historical Analysis (get_history)**

```go
func (s *StarlinkAnalyzer) AnalyzeTrends() *TrendAnalysis {
    history, err := s.getHistory()
    if err != nil {
        return nil
    }
    
    return &TrendAnalysis{
        AvgLatency:     calculateAverage(history.PopPingLatencyMs),
        AvgPacketLoss:  calculateAverage(history.PopPingDropRate),
        AvgThroughput:  calculateAverage(history.DownlinkThroughputBps),
        AvgObstruction: calculateAverage(history.ObstructionStats.FractionObstructed),
        TrendDirection: calculateTrend(history.PopPingLatencyMs),
    }
}
```

## 📊 **Data Quality Assessment**

### **GPS Data Quality:**

- **Excellent**: `gpsValid=true`, `gpsSats>=8`, `sigmaM<5`
- **Good**: `gpsValid=true`, `gpsSats>=4`, `sigmaM<20`
- **Fair**: `gpsValid=true`, `gpsSats>=2`, `sigmaM<100`
- **Poor**: `gpsValid=false` or `sigmaM>100`

### **Network Quality:**

- **Excellent**: `latency<50ms`, `packetLoss<0.1%`, `snr>10`
- **Good**: `latency<100ms`, `packetLoss<1%`, `snr>5`
- **Fair**: `latency<200ms`, `packetLoss<5%`, `snr>2`
- **Poor**: `latency>200ms`, `packetLoss>5%`, `snr<2`

## 🎯 **Integration with Autonomy**

### **Failover Decision Matrix:**

```go
func (a *Autonomy) ShouldFailoverFromStarlink() bool {
    status := a.getStarlinkStatus()
    
    // Critical conditions (immediate failover)
    if status.CurrentlyObstructed || 
       status.PopPingDropRate > 0.1 || 
       status.PopPingLatencyMs > 1000 {
        return true
    }
    
    // Warning conditions (monitor closely)
    if status.FractionObstructed > 0.05 ||
       status.PopPingLatencyMs > 200 ||
       status.Snr < 3 {
        a.logWarning("Starlink performance degraded")
    }
    
    return false
}
```

### **Location Service Integration:**

```go
func (l *LocationService) GetStarlinkLocation() (*Location, error) {
    // Collect from multiple APIs for best accuracy
    location, err := l.starlink.getLocation()
    if err != nil {
        return nil, err
    }
    
    // Validate with status API
    status, err := l.starlink.getStatus()
    if err != nil {
        return location, nil // Use location even without status
    }
    
    // Adjust confidence based on GPS quality
    if !status.GpsValid {
        location.Confidence *= 0.5
    }
    
    if status.GpsSats < 4 {
        location.Confidence *= 0.8
    }
    
    return location, nil
}
```

## 🎯 **Summary**

The Starlink API provides comprehensive data for:

1. **🔄 Real-time Failover**: Latency, packet loss, obstruction monitoring
2. **📍 GPS Location**: High-accuracy coordinates with uncertainty data
3. **📊 Performance Analysis**: Historical trends and quality metrics
4. **🛰️ Satellite Status**: GPS fix quality and satellite count
5. **⚡ System Health**: Device status and connection quality

This makes Starlink an excellent primary connection with intelligent failover capabilities! 🚀
