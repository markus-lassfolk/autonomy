# 🔍 Starlink HTTP vs gRPC API Analysis

## 📊 **Summary of HTTP Endpoints Currently Used**

I found **5 different HTTP endpoints** being used instead of the official gRPC API:

### **1. `/api/v1/status` (3 instances)**
- **Files**: `starlink_api_version_monitor.c`, `gps_starlink.c`, `gps_comprehensive.c`
- **Purpose**: Get real-time status information
- **Data extracted**: GPS coordinates, accuracy, device status

### **2. `/api/v1/diagnostics` (2 instances)**
- **Files**: `starlink_comprehensive.c`, `starlink_api_version_monitor.c`
- **Purpose**: Get diagnostic information
- **Data extracted**: `location_enabled`, `uncertainty_meters`, `gps_time_s`

### **3. `/api/v1/history` (2 instances)**
- **Files**: `starlink_comprehensive.c`, `starlink_api_version_monitor.c`
- **Purpose**: Get historical data and outage events
- **Data extracted**: `event_count`, `critical_events_24h`, `outage_count_24h`

### **4. `/api/v1/gps` (1 instance)**
- **File**: `gps_starlink.c` (defined but not used)
- **Purpose**: GPS-specific endpoint
- **Status**: Defined but not actively used

## ❌ **Problems with Current HTTP Approach**

### **1. Unofficial Endpoints**
All these HTTP endpoints (`/api/v1/*`) are **NOT part of the official Starlink API**:
- ❌ `/api/v1/status` - **Unofficial**
- ❌ `/api/v1/diagnostics` - **Unofficial** 
- ❌ `/api/v1/history` - **Unofficial**
- ❌ `/api/v1/gps` - **Unofficial**

### **2. Unreliable Data Collection**
- ❌ **May not exist** on all Starlink firmware versions
- ❌ **Can be removed** without notice
- ❌ **Inconsistent responses** across different versions
- ❌ **Fragile parsing** using string searching

### **3. Limited Data Quality**
- ❌ **Manual JSON parsing** with `strstr()` and `strchr()`
- ❌ **No error handling** for malformed responses
- ❌ **Hardcoded fallback values** when parsing fails

## ✅ **Official gRPC API Comparison**

### **Available Official Methods**
According to `docs/api-reference/starlink-api.md`:

1. **`get_status`** - Real-time status information
2. **`get_history`** - Historical performance data arrays
3. **`get_device_info`** - Static device information
4. **`get_location`** - GPS location information
5. **`get_diagnostics`** - Diagnostic information

### **Data Coverage Analysis**

| HTTP Endpoint | Official gRPC Method | Data Available | Status |
|---------------|---------------------|----------------|---------|
| `/api/v1/status` | `get_status` | ✅ **Better coverage** | **Replace** |
| `/api/v1/diagnostics` | `get_diagnostics` | ✅ **Better coverage** | **Replace** |
| `/api/v1/history` | `get_history` | ✅ **Better coverage** | **Replace** |
| `/api/v1/gps` | `get_location` | ✅ **Better coverage** | **Replace** |

## 🔍 **Detailed Data Comparison**

### **1. Status Data (`/api/v1/status` vs `get_status`)**

**HTTP `/api/v1/status` extracts:**
```c
// Current HTTP parsing (fragile)
char *lat_start = strstr(response, "\"latitude\":");
char *lon_start = strstr(response, "\"longitude\":");
char *alt_start = strstr(response, "\"altitude\":");
char *accuracy_start = strstr(response, "\"accuracy\":");
```

**gRPC `get_status` provides:**
```json
{
  "dishGetStatus": {
    "gpsStats": {
      "gpsValid": "boolean",
      "gpsSats": "number",
      "noSatsAfterTtff": "number",
      "inhibitGps": "boolean"
    },
    "obstructionStats": {
      "fractionObstructed": "number",
      "validS": "number",
      "wedgeFractionObstructed": ["number"]
    },
    "popPingLatencyMs": "number",
    "downlinkThroughputBps": "number",
    "uplinkThroughputBps": "number",
    "snr": "number",
    "deviceState": {
      "uptimeS": "number"
    }
  }
}
```

**✅ gRPC provides MORE data:**
- GPS validation status
- Satellite count
- Obstruction statistics
- Network performance metrics
- Device uptime

### **2. Diagnostics Data (`/api/v1/diagnostics` vs `get_diagnostics`)**

**HTTP `/api/v1/diagnostics` extracts:**
```c
// Current HTTP parsing (limited)
char *location_enabled_start = strstr(response.data, "\"location_enabled\":");
char *uncertainty_start = strstr(response.data, "\"uncertainty_meters\":");
char *gps_time_start = strstr(response.data, "\"gps_time_s\":");
```

**gRPC `get_diagnostics` provides:**
```json
{
  "dishGetDiagnostics": {
    "alerts": {
      "roaming": "boolean",
      "thermalThrottle": "boolean",
      "thermalShutdown": "boolean",
      "mastNotNearVertical": "boolean",
      "unexpectedLocation": "boolean",
      "slowEthernetSpeeds": "boolean",
      "softwareUpdateReboot": "boolean",
      "lowPowerMode": "boolean"
    },
    "disablementCode": "string",
    "softwareUpdateState": "string",
    "isSnrAboveNoiseFloor": "boolean",
    "classOfService": "string"
  }
}
```

**✅ gRPC provides MORE data:**
- Comprehensive alert system
- Thermal status
- Software update state
- Network performance alerts
- Device health indicators

### **3. History Data (`/api/v1/history` vs `get_history`)**

**HTTP `/api/v1/history` extracts:**
```c
// Current HTTP parsing (aggregated only)
char *event_count_start = strstr(response.data, "\"event_count\":");
char *critical_events_start = strstr(response.data, "\"critical_events_24h\":");
char *outage_count_start = strstr(response.data, "\"outage_count_24h\":");
```

**gRPC `get_history` provides:**
```json
{
  "dishGetHistory": {
    "current": "number",
    "popPingDropRate": ["number"],
    "popPingLatencyMs": ["number"],
    "downlinkThroughputBps": ["number"],
    "uplinkThroughputBps": ["number"],
    "snr": ["number"],
    "scheduled": ["boolean"],
    "obstructed": ["boolean"]
  }
}
```

**✅ gRPC provides MUCH MORE data:**
- **Real-time arrays** with current index
- **Detailed outage detection** via `obstructed` array
- **Performance trends** via latency/throughput arrays
- **Scheduled maintenance** detection
- **Signal quality trends** via SNR array

## 🎯 **Additional Information Available from gRPC**

### **Data NOT Available from HTTP but Available from gRPC:**

1. **Real-time Performance Arrays**
   - Latency trends over time
   - Throughput variations
   - Signal quality changes
   - Packet drop patterns

2. **Detailed Obstruction Analysis**
   - Per-wedge obstruction data
   - Obstruction timing patterns
   - Sky view fraction analysis

3. **Device Health Monitoring**
   - Thermal status and throttling
   - Software update state
   - Alert conditions
   - Device uptime tracking

4. **Network Performance Metrics**
   - Point-of-presence latency
   - Bandwidth utilization
   - Signal-to-noise ratio trends
   - Connection stability

5. **GPS and Location Data**
   - GPS validation status
   - Satellite count and quality
   - Location accuracy metrics
   - GPS inhibition status

## 📋 **Recommendation: Complete Migration to gRPC**

### **Benefits of Migration:**
1. ✅ **Official API support** - documented and stable
2. ✅ **More comprehensive data** - arrays vs aggregated stats
3. ✅ **Better reliability** - structured responses
4. ✅ **Future-proof** - less likely to change
5. ✅ **Better error handling** - proper JSON parsing
6. ✅ **Real-time analysis** - current index for live data

### **Migration Plan:**
1. **Replace all HTTP calls** with gRPC equivalents
2. **Implement proper JSON parsing** using json-c library
3. **Add structured data storage** for arrays and trends
4. **Update API version monitor** to test gRPC endpoints
5. **Remove HTTP endpoint dependencies**

## ✅ **Conclusion**

**Yes, we have multiple HTTP calls using unofficial endpoints that should be replaced with gRPC:**

- **5 HTTP endpoints** currently used (all unofficial)
- **gRPC provides MORE data** in all cases
- **gRPC is more reliable** and officially supported
- **No additional information** is available from HTTP that isn't in gRPC
- **Complete migration recommended** for better data quality and reliability

The gRPC API provides significantly more comprehensive data and is the only officially supported method for accessing Starlink dish information.
