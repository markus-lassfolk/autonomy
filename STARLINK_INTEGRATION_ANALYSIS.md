# 🔍 Starlink Integration Analysis: Outage Events Collection

## 📊 **Current Implementation Analysis**

### **Current HTTP-based Approach**
The current implementation in `starlink_comprehensive.c` uses HTTP endpoints that are **NOT officially available**:

```c
// Current implementation (PROBLEMATIC)
snprintf(request_config.url, sizeof(request_config.url), 
         "http://%s/api/v1/history", g_starlink_comprehensive.config.host);
```

**Issues with Current Approach:**
1. ❌ **Unofficial HTTP endpoint**: `/api/v1/history` is not part of the official Starlink API
2. ❌ **Unreliable**: These endpoints may not exist or may be removed without notice
3. ❌ **Limited data**: Only provides aggregated statistics, not detailed outage events
4. ❌ **No structured data**: Parses JSON manually with string searching

### **What We're Currently Trying to Collect:**
```c
// Current parsing attempts (fragile)
char *event_count_start = strstr(response.data, "\"event_count\":");
char *critical_events_start = strstr(response.data, "\"critical_events_24h\":");
char *warning_events_start = strstr(response.data, "\"warning_events_24h\":");
char *outage_count_start = strstr(response.data, "\"outage_count_24h\":");
```

## ✅ **Recommended gRPC-based Approach**

### **Official Starlink gRPC API**
According to the documentation in `docs/api-reference/starlink-api.md`, the official API provides:

**Endpoint**: `192.168.100.1:9200` (gRPC)
**Method**: `get_history`
**Request**:
```bash
grpcurl -plaintext -d '{"get_history":{}}' 192.168.100.1:9200 SpaceX.API.Device.Device/Handle
```

**Response Structure**:
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
    "obstructed": ["boolean"]  // ← This is the key for outage detection
  }
}
```

### **Key Advantages of gRPC Approach:**
1. ✅ **Official API**: Part of the documented Starlink gRPC API
2. ✅ **Structured data**: Proper JSON response with arrays
3. ✅ **Real-time data**: Historical arrays with current index
4. ✅ **Outage detection**: `obstructed` boolean array for outage events
5. ✅ **Performance data**: Latency, throughput, SNR arrays
6. ✅ **Reliable**: Official API that's less likely to change

## 🔧 **Implementation Recommendation**

### **1. Replace HTTP with gRPC**
Update `collect_from_history_api()` function to use gRPC instead of HTTP:

```c
// Recommended implementation
int collect_from_history_api(starlink_events_outages_analysis_t* analysis) {
    if (!analysis) {
        return AUTONOMY_ERROR_INVALID_PARAM;
    }
    
    // Use gRPC instead of HTTP
    char grpc_cmd[512];
    snprintf(grpc_cmd, sizeof(grpc_cmd),
             "grpcurl -plaintext -d '{\"get_history\":{}}' %s:%d SpaceX.API.Device.Device/Handle 2>/dev/null",
             g_starlink_comprehensive.config.host, 
             g_starlink_comprehensive.config.port);
    
    FILE *fp = popen(grpc_cmd, "r");
    if (!fp) {
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    char response[8192];
    size_t bytes_read = fread(response, 1, sizeof(response) - 1, fp);
    response[bytes_read] = '\0';
    
    int status = pclose(fp);
    if (status != 0) {
        return AUTONOMY_ERROR_SYSTEM;
    }
    
    // Parse structured JSON response
    return parse_grpc_history_response(response, analysis);
}
```

### **2. Parse Structured Outage Events**
Create a proper JSON parser for the gRPC response:

```c
int parse_grpc_history_response(const char* json_response, starlink_events_outages_analysis_t* analysis) {
    // Parse JSON using proper JSON library (json-c)
    json_object *root = json_tokener_parse(json_response);
    if (!root) {
        return AUTONOMY_ERROR_INVALID_DATA;
    }
    
    json_object *dishGetHistory;
    if (json_object_object_get_ex(root, "dishGetHistory", &dishGetHistory)) {
        
        // Get current index
        json_object *current;
        if (json_object_object_get_ex(dishGetHistory, "current", &current)) {
            int current_idx = json_object_get_int(current);
            
            // Get obstructed array for outage detection
            json_object *obstructed_array;
            if (json_object_object_get_ex(dishGetHistory, "obstructed", &obstructed_array)) {
                int array_len = json_object_array_length(obstructed_array);
                
                // Analyze obstruction patterns for outages
                int outage_count = 0;
                bool in_outage = false;
                time_t outage_start = 0;
                
                for (int i = 0; i < array_len; i++) {
                    json_object *obstructed = json_object_array_get_idx(obstructed_array, i);
                    bool is_obstructed = json_object_get_boolean(obstructed);
                    
                    if (is_obstructed && !in_outage) {
                        // Start of outage
                        in_outage = true;
                        outage_start = time(NULL) - (array_len - i) * 15; // 15-second intervals
                    } else if (!is_obstructed && in_outage) {
                        // End of outage
                        in_outage = false;
                        outage_count++;
                        
                        // Store outage event
                        starlink_outage_t outage = {
                            .start_time = outage_start,
                            .end_time = time(NULL) - (array_len - i) * 15,
                            .duration_s = (time(NULL) - (array_len - i) * 15) - outage_start,
                            .type = OUTAGE_TYPE_OBSTRUCTION
                        };
                        
                        // Add to analysis
                        if (analysis->outage_count < MAX_OUTAGES) {
                            analysis->outages[analysis->outage_count++] = outage;
                        }
                    }
                }
                
                analysis->outage_count_24h = outage_count;
            }
            
            // Get performance data
            json_object *latency_array;
            if (json_object_object_get_ex(dishGetHistory, "popPingLatencyMs", &latency_array)) {
                // Analyze latency patterns
                analyze_latency_patterns(latency_array, analysis);
            }
        }
    }
    
    json_object_put(root);
    return AUTONOMY_SUCCESS;
}
```

### **3. Persist Structured Outage Events**
Store outage events in a structured format:

```c
typedef struct {
    time_t start_time;
    time_t end_time;
    int duration_s;
    outage_type_t type;
    float severity;  // 0.0-1.0
    char reason[64]; // "obstruction", "scheduled", "weather", etc.
} starlink_outage_t;

typedef struct {
    starlink_outage_t outages[MAX_OUTAGES];
    int outage_count;
    int critical_events_24h;
    int warning_events_24h;
    float stability_score;
    bool outage_pattern_detected;
} starlink_events_outages_analysis_t;
```

## 🎯 **Benefits of the New Approach**

### **1. Reliable Data Collection**
- ✅ Uses official gRPC API
- ✅ Structured JSON responses
- ✅ Real-time historical data arrays

### **2. Better Outage Detection**
- ✅ `obstructed` boolean array for precise outage timing
- ✅ Performance data (latency, throughput) for correlation
- ✅ Current index for real-time analysis

### **3. Structured Event Storage**
- ✅ Proper outage event structures
- ✅ Timestamped events with duration
- ✅ Categorization by type and severity

### **4. Future-Proof**
- ✅ Official API less likely to change
- ✅ Better error handling
- ✅ Extensible for additional data fields

## 📋 **Implementation Plan**

1. **Phase 1**: Replace HTTP with gRPC in `collect_from_history_api()`
2. **Phase 2**: Implement proper JSON parsing for gRPC response
3. **Phase 3**: Add structured outage event storage
4. **Phase 4**: Update API version monitor to test gRPC endpoints
5. **Phase 5**: Add outage pattern analysis and persistence

## ✅ **Conclusion**

**Yes, you are absolutely correct!** The current implementation tries to use HTTP endpoints (`/api/v1/history`) that are **not officially available**. We should switch to the official gRPC `get_history` method which provides:

- ✅ **Official API support**
- ✅ **Structured outage data** via `obstructed` boolean arrays
- ✅ **Real-time historical data** with current index
- ✅ **Performance metrics** for correlation analysis
- ✅ **Reliable data collection** using documented endpoints

The gRPC approach will provide much better outage event collection and persistence capabilities.
