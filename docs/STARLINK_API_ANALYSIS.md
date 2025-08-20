# Starlink API Analysis & Data Available

## 🛰️ Connection Status

✅ **TCP Connectivity**: Successfully connected to `192.168.100.1:9200`  
✅ **gRPC Port Open**: The Starlink dish is listening on the correct gRPC port  
❌ **HTTP API**: No REST/HTTP API available (404 on all HTTP endpoints)  
⚠️  **gRPC Protobuf**: Requires proper protobuf message encoding (not JSON)

## 📡 Available API Methods

Based on the official Starlink gRPC API documentation, the following methods are available:

### 1. `get_status` - Real-time Status Information
**Most Important for Failover Monitoring**

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
      "noSatsAfterTtff": "number",
      "inhibitGps": "boolean"
    },
    "ethSpeedMbps": "number",              // Ethernet speed
    "mobilityClass": "string",             // Mobility type
    "isSnrAboveNoiseFloor": "boolean",     // 🔥 KEY: Signal health
    "classOfService": "string",            // Service tier
    "softwareUpdateState": "string",       // Update status
    "isSnrPersistentlyLow": "boolean",     // 🔥 KEY: Degraded signal
    "swupdateRebootReady": "boolean"       // 🔥 KEY: Reboot pending
  }
}
```

### 2. `get_history` - Historical Performance Data
**Useful for Trend Analysis & Predictive Failover**

**Available Data:**
```json
{
  "dishGetHistory": {
    "current": "number",                   // Current index
    "popPingDropRate": ["number"],         // Historical packet loss
    "popPingLatencyMs": ["number"],        // Historical latency  
    "downlinkThroughputBps": ["number"],   // Download history
    "uplinkThroughputBps": ["number"],     // Upload history
    "snr": ["number"],                     // SNR history
    "scheduled": ["boolean"],              // Scheduled events
    "obstructed": ["boolean"]              // Obstruction history
  }
}
```

### 3. `get_device_info` - Static Device Information
**Device Identification & Capabilities**

**Available Data:**
```json
{
  "deviceInfo": {
    "id": "string",                        // Unique device ID
    "hardwareVersion": "string",           // Hardware revision
    "softwareVersion": "string",           // Firmware version
    "countryCode": "string",               // Location
    "utcOffsetS": "number",               // Timezone
    "softwarePartNumber": "string",        // Part number
    "generationNumber": "number",          // Dish generation
    "dishCohoused": "boolean",            // Installation type
    "utcnsOffsetNs": "number"             // Precise time offset
  }
}
```

### 4. `get_location` - GPS Position Data
**Location Services & Geofencing**

**Available Data:**
```json
{
  "getLocation": {
    "lla": {
      "lat": "number",                     // Latitude
      "lon": "number",                     // Longitude  
      "alt": "number"                      // Altitude
    },
    "ecef": {
      "x": "number",                       // ECEF X coordinate
      "y": "number",                       // ECEF Y coordinate
      "z": "number"                        // ECEF Z coordinate
    },
    "source": "string"                     // Position source
  }
}
```

### 5. `get_diagnostics` - Hardware Health & Alerts
**Critical for Predictive Failover**

**Available Data:**
```json
{
  "dishGetDiagnostics": {
    "id": "string",
    "hardwareVersion": "string",
    "softwareVersion": "string", 
    "alerts": {
      "roaming": "boolean",                // 🔥 Roaming mode
      "thermalThrottle": "boolean",        // 🔥 Overheating
      "thermalShutdown": "boolean",        // 🔥 Critical temp
      "mastNotNearVertical": "boolean",    // Installation issue
      "unexpectedLocation": "boolean",     // Geofence violation
      "slowEthernetSpeeds": "boolean",     // Network issue
      "softwareUpdateReboot": "boolean",   // 🔥 Reboot pending
      "lowPowerMode": "boolean"            // Power saving
    },
    "disablementCode": "string",           // Service status
    "softwareUpdateState": "string",       // Update progress
    "isSnrAboveNoiseFloor": "boolean",     // Signal health
    "classOfService": "string"             // Service tier
  }
}
```

## 🔥 Key Metrics for Failover Decision Engine

### Primary Failover Triggers:
1. **`popPingLatencyMs`** - Network latency (threshold: >150ms)
2. **`popPingDropRate`** - Packet loss (threshold: >5%)
3. **`snr`** - Signal quality (threshold: <8dB)
4. **`isSnrAboveNoiseFloor`** - Signal health (false = critical)
5. **`fractionObstructed`** - Sky view blockage (threshold: >10%)

### Predictive Failover Indicators:
1. **`isSnrPersistentlyLow`** - Degrading signal trend
2. **`thermalThrottle`** - Performance limiting due to heat
3. **`thermalShutdown`** - Imminent shutdown due to overheating
4. **`swupdateRebootReady`** - Scheduled reboot pending
5. **`softwareUpdateReboot`** - Update reboot pending
6. **Historical trends** from `get_history` arrays

### Health Monitoring:
1. **`uptimeS`** - Service availability
2. **`gpsValid`** - Position lock status
3. **`currentlyObstructed`** - Real-time blockage
4. **`ethSpeedMbps`** - Local network performance
5. **Alert flags** from diagnostics

## 🚧 Implementation Challenges

### ✅ What Works:
- TCP connection to `192.168.100.1:9200`
- gRPC server is responding
- API structure is well-documented

### ❌ Current Issues:
- **Protobuf Encoding Required**: Cannot send JSON to gRPC service
- **No Generated Code**: Need proper `.proto` files and code generation
- **Message Structure**: Must construct proper protobuf messages

### 🔧 Solutions:
1. **Install grpcurl**: Use external tool for testing
2. **Generate Protobuf Code**: Create proper Go structs from `.proto` files
3. **Use grpc-gateway**: If available, use HTTP/JSON proxy
4. **Manual Protobuf**: Construct raw protobuf messages

## 📊 Data Collection Strategy

### For Production Failover System:
```go
// Priority 1: Critical metrics (every 10-30 seconds)
- popPingLatencyMs
- popPingDropRate  
- snr
- isSnrAboveNoiseFloor
- fractionObstructed
- currentlyObstructed

// Priority 2: Health monitoring (every 1-2 minutes)
- thermalThrottle
- thermalShutdown
- swupdateRebootReady
- isSnrPersistentlyLow
- uptimeS

// Priority 3: Historical analysis (every 5-10 minutes)
- get_history for trend analysis
- Predictive failure detection
- Performance baseline updates
```

## 🎯 Next Steps

1. **Install grpcurl** for immediate testing
2. **Generate proper protobuf code** from `.proto` files
3. **Implement robust gRPC client** in Go
4. **Test with real Starlink dish** to validate data structure
5. **Integrate with failover decision engine**

## 💡 Alternative Approaches

If gRPC proves too complex:
1. **Use grpcurl subprocess** calls from Go
2. **Implement grpc-web proxy** for HTTP access
3. **Use reflection API** to discover message structure
4. **Monitor via dish mobile app API** (if available)

The Starlink API provides extremely rich data for intelligent failover decisions, including both real-time metrics and predictive indicators. Once the gRPC implementation is working, this will be one of the most sophisticated failover triggers available.
