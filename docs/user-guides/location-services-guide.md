# Multi-Source Location Strategy

**Version:** 3.0.0 | **Updated:** 2025-08-22

This document describes Autonomy's comprehensive multi-source location strategy, combining GPS, Starlink, cellular, and WiFi data for maximum reliability and accuracy.

## 🎯 Overview

Autonomy implements an intelligent multi-source location system that automatically selects the best available location source based on accuracy, reliability, and availability. The system includes advanced features like predictive loading, geographic clustering, and comprehensive 5G support.

## 🚀 Key Features

### **1. Multi-Source Location Fusion**
- **GPS Sources**: RUTOS GPS, Starlink GPS, External GPS devices
- **Cellular Location**: OpenCellID, Google Geolocation, Mozilla Location Service
- **WiFi Positioning**: MAC address-based location estimation
- **IP Geolocation**: Fallback location from IP address

### **2. Advanced 5G Support**
- **Comprehensive 5G NR Data Collection**: Multiple AT command parsing (QNWINFO, QCSQ, QENG)
- **Carrier Aggregation Detection**: Intelligent detection of multi-carrier scenarios
- **Network Operator Identification**: Automatic operator detection and classification
- **Signal Quality Analysis**: RSRP, RSRQ, SINR parsing with bounds checking
- **Confidence Scoring**: 0.0-1.0 confidence calculation based on data quality

### **3. Intelligent Cell Caching**
- **Predictive Loading**: Preemptive location data loading based on tower changes
- **Geographic Clustering**: Location-based clustering for efficient caching
- **Advanced Environment Hashing**: SHA256-based cellular environment fingerprinting
- **Multi-Level Decision Making**: Serving cell, neighbor changes, and geographic factors
- **Cache Performance Metrics**: Detailed cache efficiency tracking

### **4. Comprehensive Starlink GPS**
- **Multi-API Integration**: Combines data from get_location, get_status, and get_diagnostics
- **Quality Scoring**: Automatic quality assessment (excellent/good/fair/poor)
- **Confidence Calculation**: Data-driven confidence scoring
- **Performance Metrics**: Collection time tracking and efficiency monitoring
- **Comprehensive Data Fusion**: Merges multiple Starlink API responses

## 📊 Location Sources

### **Primary Sources (High Accuracy)**

#### **1. RUTOS GPS**
```go
// Direct GPS integration with RUTOS
type RUTOSGPSCollector struct {
    devicePath string
    baudRate   int
    timeout    time.Duration
}

func (r *RUTOSGPSCollector) GetLocation(ctx context.Context) (*GPSLocation, error) {
    // Direct NMEA parsing from GPS device
    // Accuracy: 2-5 meters
    // Update rate: 1Hz
}
```

#### **2. Starlink GPS (Enhanced)**
```go
// Comprehensive Starlink GPS with multi-API integration
type StarlinkAPICollector struct {
    starlinkClient *starlink.Client
    enableAllAPIs  bool
}

func (sc *StarlinkAPICollector) CollectComprehensiveGPS(ctx context.Context) (*ComprehensiveStarlinkGPS, error) {
    // Collects from get_location, get_status, and get_diagnostics
    // Accuracy: 5-10 meters
    // Quality scoring: excellent/good/fair/poor
    // Confidence: 0.0-1.0
}
```

#### **3. Enhanced 5G Cellular**
```go
// Advanced 5G NR data collection with carrier aggregation
type Enhanced5GCollector struct {
    enableAdvancedParsing bool
    retryAttempts         int
}

func (e5g *Enhanced5GCollector) Collect5GNetworkInfo(ctx context.Context) (*Enhanced5GNetworkInfo, error) {
    // Multiple AT commands: QNWINFO, QCSQ, QENG
    // Carrier aggregation detection
    // Network operator identification
    // Signal quality analysis with confidence scoring
}
```

### **Secondary Sources (Medium Accuracy)**

#### **4. OpenCellID Integration**
```go
// Intelligent cell tower location with caching
type IntelligentCellCache struct {
    enablePredictiveLoading    bool
    enableGeographicClustering bool
    clusterRadius              float64
}

func (cache *IntelligentCellCache) ShouldQueryLocation(currentEnv *CellEnvironment) (bool, string) {
    // Predictive loading based on tower changes
    // Geographic clustering for efficient caching
    // SHA256-based environment fingerprinting
    // Multi-level decision making
}
```

#### **5. Google Geolocation**
```go
// High-accuracy cellular and WiFi positioning
type GoogleLocationCollector struct {
    apiKey     string
    timeout    time.Duration
    maxRetries int
}

func (g *GoogleLocationCollector) GetLocation(ctx context.Context, cells []CellTower, wifis []WiFiAP) (*GPSLocation, error) {
    // Combines cellular and WiFi data
    // Accuracy: 10-50 meters
    // Rate limited with intelligent caching
}
```

### **Fallback Sources (Low Accuracy)**

#### **6. IP Geolocation**
```go
// IP-based location estimation
type IPGeolocationCollector struct {
    services []string // Multiple IP geolocation services
    timeout  time.Duration
}

func (ip *IPGeolocationCollector) GetLocation(ctx context.Context) (*GPSLocation, error) {
    // Fallback location from IP address
    // Accuracy: 1-50 km
    // Used when all other sources fail
}
```

## 🔄 Source Selection Algorithm

### **Intelligent Source Selection**
```go
func (lm *LocationManager) selectBestSource(ctx context.Context) (LocationSource, error) {
    // Priority-based selection with quality assessment
    sources := []LocationSource{
        &RUTOSGPSCollector{},      // Highest priority
        &StarlinkAPICollector{},   // High priority with quality scoring
        &Enhanced5GCollector{},    // Advanced 5G with confidence
        &OpenCellIDCollector{},    // Intelligent caching
        &GoogleLocationCollector{}, // High accuracy cellular/WiFi
        &IPGeolocationCollector{}, // Fallback
    }
    
    for _, source := range sources {
        if source.IsAvailable(ctx) && source.GetQuality() > threshold {
            return source, nil
        }
    }
    
    return nil, fmt.Errorf("no suitable location source available")
}
```

### **Quality Assessment**
```go
type LocationQuality struct {
    Accuracy    float64 // meters
    Confidence  float64 // 0.0-1.0
    Freshness   time.Duration
    SourceType  string
    QualityScore string // excellent/good/fair/poor
}
```

## 🧠 Intelligent Caching System

### **Predictive Loading**
```go
// Preemptive location data loading
func (cache *IntelligentCellCache) ShouldPredictiveLoad(currentEnv *CellEnvironment) bool {
    // Check if approaching significant change
    changePercentage := cache.calculateTowerChangePercentage(currentEnv)
    if changePercentage > cache.towerChangeThreshold*0.8 {
        return true
    }
    
    // Check top tower changes
    topChanges := cache.countTopTowerChanges(currentEnv)
    return topChanges >= 1
}
```

### **Geographic Clustering**
```go
// Location-based clustering for efficient caching
func (cache *IntelligentCellCache) shouldQueryForGeographicReason(currentEnv *CellEnvironment) bool {
    currentHash := cache.generateEnvironmentHash(currentEnv)
    hashSimilarity := cache.calculateHashSimilarity(currentHash, cache.lastEnvironment.LocationHash)
    
    return hashSimilarity < 0.5 // Less than 50% similarity triggers query
}
```

### **Cache Performance Metrics**
```go
type CacheMetrics struct {
    CacheHits           int64
    CacheMisses         int64
    PredictiveLoads     int64
    GeographicClusters  int64
    AverageCacheAge     time.Duration
    CacheEfficiency     float64
}
```

## 📈 Performance Optimization

### **Memory Management**
- **Ring Buffer Storage**: Efficient memory usage for location history
- **Intelligent Cleanup**: Automatic cleanup of old location data
- **Cache Size Limits**: Configurable cache size to prevent memory bloat

### **Network Optimization**
- **Rate Limiting**: Intelligent rate limiting for external APIs
- **Connection Pooling**: Reuse connections for better performance
- **Timeout Management**: Configurable timeouts for different sources

### **CPU Optimization**
- **Async Processing**: Non-blocking location collection
- **Background Updates**: Periodic updates without blocking main operations
- **Efficient Algorithms**: Optimized algorithms for location calculations

## 🔧 Configuration

### **UCI Configuration**
```bash
# Location strategy configuration
uci set autonomy.location.strategy='multi_source'
uci set autonomy.location.primary_sources='rutos,starlink,5g'
uci set autonomy.location.fallback_sources='opencellid,google,ip'
uci set autonomy.location.cache_enabled='1'
uci set autonomy.location.predictive_loading='1'
uci set autonomy.location.geographic_clustering='1'

# 5G configuration
uci set autonomy.location.5g_enabled='1'
uci set autonomy.location.5g_advanced_parsing='1'
uci set autonomy.location.5g_carrier_aggregation='1'
uci set autonomy.location.5g_retry_attempts='3'

# Starlink configuration
uci set autonomy.location.starlink_enabled='1'
uci set autonomy.location.starlink_all_apis='1'
uci set autonomy.location.starlink_confidence_threshold='0.3'

# Cache configuration
uci set autonomy.location.cache_max_age='3600'
uci set autonomy.location.cache_debounce_delay='10'
uci set autonomy.location.cache_tower_threshold='0.35'
uci set autonomy.location.cache_top_towers='5'
uci set autonomy.location.cache_cluster_radius='1000'

uci commit autonomy
```

### **Go Configuration**
```go
type LocationConfig struct {
    Strategy              string        `json:"strategy"`
    PrimarySources        []string      `json:"primary_sources"`
    FallbackSources       []string      `json:"fallback_sources"`
    CacheEnabled          bool          `json:"cache_enabled"`
    PredictiveLoading     bool          `json:"predictive_loading"`
    GeographicClustering  bool          `json:"geographic_clustering"`
    
    // 5G Configuration
    Enable5G              bool          `json:"enable_5g"`
    EnableAdvancedParsing bool          `json:"enable_advanced_parsing"`
    EnableCarrierAggregation bool       `json:"enable_carrier_aggregation"`
    RetryAttempts         int           `json:"retry_attempts"`
    
    // Starlink Configuration
    EnableStarlink        bool          `json:"enable_starlink"`
    EnableAllAPIs         bool          `json:"enable_all_apis"`
    ConfidenceThreshold   float64       `json:"confidence_threshold"`
    
    // Cache Configuration
    MaxCacheAge           time.Duration `json:"max_cache_age"`
    DebounceDelay         time.Duration `json:"debounce_delay"`
    TowerChangeThreshold  float64       `json:"tower_change_threshold"`
    TopTowersCount        int           `json:"top_towers_count"`
    ClusterRadius         float64       `json:"cluster_radius"`
}
```

## 📊 Monitoring and Metrics

### **Location Quality Metrics**
```go
type LocationMetrics struct {
    SourceAccuracy    map[string]float64 // Accuracy by source
    SourceConfidence  map[string]float64 // Confidence by source
    CacheEfficiency   float64            // Cache hit rate
    ResponseTime      time.Duration      // Average response time
    ErrorRate         float64            // Error rate percentage
    Uptime            time.Duration      // System uptime
}
```

### **Performance Monitoring**
```go
// Prometheus metrics
var (
    locationRequestsTotal = prometheus.NewCounterVec(
        prometheus.CounterOpts{
            Name: "autonomy_location_requests_total",
            Help: "Total number of location requests",
        },
        []string{"source", "status"},
    )
    
    locationAccuracy = prometheus.NewGaugeVec(
        prometheus.GaugeOpts{
            Name: "autonomy_location_accuracy_meters",
            Help: "Location accuracy in meters",
        },
        []string{"source"},
    )
    
    locationConfidence = prometheus.NewGaugeVec(
        prometheus.GaugeOpts{
            Name: "autonomy_location_confidence",
            Help: "Location confidence score",
        },
        []string{"source"},
    )
)
```

## 🚀 Advanced Features

### **1. Predictive Failover**
- **Movement Detection**: GPS-based movement correlation
- **Coverage Prediction**: Predict coverage gaps
- **Intelligent Timing**: Prevents unnecessary early failovers

### **2. Machine Learning Integration**
- **Pattern Recognition**: ML-based obstruction pattern learning
- **Trend Analysis**: Predictive obstruction detection
- **Quality Prediction**: Predict location quality based on conditions

### **3. Geographic Intelligence**
- **Location Fingerprinting**: Unique cellular environment signatures
- **Coverage Mapping**: Dynamic coverage quality assessment
- **Route Optimization**: Location-aware route planning

## 🔒 Security Considerations

### **Data Privacy**
- **Local Processing**: Location data processed locally when possible
- **Encrypted Storage**: Sensitive location data encrypted at rest
- **Access Control**: Role-based access to location data

### **API Security**
- **Token Management**: Secure API token storage and rotation
- **Rate Limiting**: Prevent API abuse and quota exhaustion
- **Input Validation**: Comprehensive validation of all inputs

## 📞 Support and Troubleshooting

### **Common Issues**
1. **GPS Signal Loss**: Automatic fallback to cellular location
2. **API Rate Limits**: Intelligent rate limiting and caching
3. **Network Connectivity**: Graceful degradation when networks fail
4. **Cache Corruption**: Automatic cache validation and recovery

### **Debug Commands**
```bash
# Check location status
autonomyctl location status

# View cache metrics
autonomyctl location cache-metrics

# Test specific source
autonomyctl location test-source rutos
autonomyctl location test-source starlink
autonomyctl location test-source 5g

# View quality metrics
autonomyctl location quality-metrics

# Clear cache
autonomyctl location clear-cache
```

### **Log Analysis**
```bash
# Monitor location collection
journalctl -u autonomy | grep location

# Check cache performance
journalctl -u autonomy | grep cache

# Monitor 5G collection
journalctl -u autonomy | grep 5g

# Check Starlink GPS
journalctl -u autonomy | grep starlink
```

## 📈 Future Enhancements

### **Planned Features**
1. **Advanced ML Models**: Deep learning for pattern recognition
2. **Weather Integration**: Real-time weather data correlation
3. **Satellite Coverage**: Starlink satellite position prediction
4. **Network Topology**: Dynamic network topology awareness
5. **User Behavior**: Learning from user interaction patterns

### **Performance Targets**
- **Response Time**: <1 second for cached locations
- **Accuracy**: <10 meters for GPS sources, <100 meters for cellular
- **Uptime**: 99.9% availability with intelligent failover
- **Memory Usage**: <25MB for location system
- **Cache Efficiency**: >80% cache hit rate
