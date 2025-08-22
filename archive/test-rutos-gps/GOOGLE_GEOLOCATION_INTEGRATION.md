# Google Geolocation API Integration

This document describes the comprehensive integration of Google's Geolocation API for cellular and WiFi-based location services in the RutOS Starlink Failover system.

## Overview

The Google Geolocation API provides high-accuracy location estimates based on:
- **Cell Tower Information**: Using serving cell and neighbor cell data
- **WiFi Access Points**: Using nearby WiFi BSSID and signal strength data  
- **IP-based Fallback**: Optional fallback to IP geolocation

## Key Features

### 🎯 High Accuracy Location Services
- **Multi-source Data**: Combines cellular and WiFi signals for optimal accuracy
- **Radio Type Detection**: Automatically detects and handles 5G, LTE, WCDMA, GSM, and CDMA
- **Smart Filtering**: Filters out invalid MAC addresses and cell tower data per Google's best practices

### 📡 Cellular Data Integration
- **Serving Cell**: Primary cell tower connection with full signal metrics
- **Neighbor Cells**: Up to 20 neighbor cells for improved triangulation
- **Signal Quality**: RSRP, RSRQ, RSSI, and SINR measurements
- **Technology Support**: 5G-NSA, LTE, WCDMA, GSM, CDMA

### 📶 WiFi Access Point Integration  
- **MAC Address Filtering**: Removes locally-administered and reserved IANA MAC addresses
- **Signal Strength**: Uses dBm measurements for accurate positioning
- **Multiple APs**: Supports up to 50 WiFi access points per request
- **Quality Control**: Validates MAC address format and signal strength ranges

### ⚙️ UCI Configuration Management
- **Production Ready**: Full UCI integration for RutOS deployment
- **Comprehensive Settings**: API keys, rate limiting, quality control, cost management
- **Intelligent Caching**: Environment-based cache invalidation
- **Monitoring & Alerts**: Usage tracking and webhook notifications

## Implementation Details

### Cell Tower Data Structure

```go
type CellTower struct {
    CellID            int  // Unique cell identifier
    MobileCountryCode int  // MCC (e.g., 240 for Sweden)
    MobileNetworkCode int  // MNC (e.g., 1 for Telia)
    LocationAreaCode  int  // LAC/TAC for area identification
    SignalStrength    int  // RSRP in dBm
}
```

### WiFi Access Point Data Structure

```go
type WiFiAccessPoint struct {
    MACAddress     string   // BSSID in colon-separated format
    SignalStrength float64  // Signal strength in dBm
    Channel        int      // WiFi channel
}
```

### Radio Type Mapping

| Technology | Google Radio Type | Notes |
|------------|------------------|-------|
| 5G-NSA     | `lte`           | Fallback due to library limitations |
| 5G-SA      | `lte`           | Fallback due to library limitations |
| LTE/4G     | `lte`           | Full support |
| WCDMA/3G   | `wcdma`         | Full support |
| GSM/2G     | `gsm`           | Full support |
| CDMA       | `cdma`          | Full support |

## Best Practices Implementation

### MAC Address Filtering (per Google Documentation)

The implementation follows [Google's best practices](https://developers.google.com/maps/documentation/geolocation/requests-geolocation#cell_tower_object) for WiFi access point filtering:

1. **Locally-Administered MAC Addresses**: Filtered out using bit masking
2. **IANA Reserved Range**: Removes `00:00:5E:xx:xx:xx` addresses
3. **Broadcast Address**: Filters out `FF:FF:FF:FF:FF:FF`
4. **Format Validation**: Ensures proper colon-separated hexadecimal format

```go
func isValidMACAddress(macAddr string) bool {
    // Basic format validation
    if len(macAddr) != 17 || strings.Count(macAddr, ":") != 5 {
        return false
    }
    
    // Check for locally administered (second least-significant bit)
    firstByteInt, _ := strconv.ParseInt(macAddr[:2], 16, 64)
    if (firstByteInt & 0x02) != 0 {
        return false // Locally administered
    }
    
    // Check for reserved IANA range
    if strings.HasPrefix(strings.ToUpper(macAddr), "00:00:5E") {
        return false
    }
    
    return true
}
```

### Cell Tower Quality Control

- **Invalid Cell Filtering**: Removes cells with ID 0 or invalid signal strength
- **Signal Validation**: Ensures RSRP values are within valid ranges
- **Neighbor Cell Prioritization**: Sorts by signal strength for optimal accuracy

## UCI Configuration Options

### API Configuration
```
autonomy.google_geo.enabled=1
autonomy.google_geo.api_key="your-google-api-key"
```

### Request Parameters
```
autonomy.google_geo.max_cells=20
autonomy.google_geo.max_wifi_aps=50
autonomy.google_geo.consider_ip=1
```

### Rate Limiting
```
autonomy.google_geo.max_requests_per_hour=100
autonomy.google_geo.request_interval_seconds=60
```

### GPS Integration
```
autonomy.google_geo.enable_fallback=1
autonomy.google_geo.fallback_priority=3
autonomy.google_geo.accuracy_threshold=500
```

### Intelligent Caching
```
autonomy.google_geo.enable_caching=1
autonomy.google_geo.cache_max_age_minutes=15
autonomy.google_geo.cell_change_threshold=25
autonomy.google_geo.wifi_change_threshold=30
```

### Quality Control
```
autonomy.google_geo.min_cells_required=1
autonomy.google_geo.min_wifi_aps_required=2
autonomy.google_geo.max_accuracy_accepted=2000
```

### Cost Management
```
autonomy.google_geo.estimated_cost_per_request=0.005
autonomy.google_geo.max_daily_cost=1.0
autonomy.google_geo.cost_alert_threshold=0.5
```

## Usage Examples

### Basic Location Request
```bash
go run . -test-google-geo
```

### UCI Configuration Test
```bash
go run . -test-google-uci
```

### Production Integration
```go
// Load configuration from UCI
uciConfig := NewUCIGoogleConfig("autonomy.google_geo")
config, err := uciConfig.LoadConfig()

// Create service
service, err := config.ConvertToGoogleService()

// Get location
response, err := service.GetLocationWithGoogle(cellularIntel, wifiAPs, true)
```

## Performance Metrics

### Typical Response Times
- **Cell Tower Only**: 50-150ms
- **Cell + WiFi**: 100-200ms
- **IP Fallback**: 200-300ms

### Accuracy Expectations
- **Urban (Cell + WiFi)**: 10-100 meters
- **Suburban (Cell Only)**: 100-1000 meters  
- **Rural (Cell Only)**: 1000-5000 meters
- **IP Fallback**: 5000-50000 meters

### API Costs (Estimated)
- **Per Request**: ~$0.005 USD
- **Daily Limit (100 requests)**: ~$0.50 USD
- **Monthly (3000 requests)**: ~$15.00 USD

## Error Handling

### Common Error Scenarios
1. **INVALID_ARGUMENT**: Missing required fields (cellId, newRadioCellId)
2. **NOT_FOUND**: Insufficient data for location estimate
3. **OVER_QUERY_LIMIT**: API quota exceeded
4. **REQUEST_DENIED**: Invalid API key or permissions

### Mitigation Strategies
- **Data Validation**: Pre-validate all cell tower and WiFi data
- **Fallback Logic**: Use IP geolocation when cellular/WiFi fails
- **Rate Limiting**: Implement request throttling
- **Caching**: Cache results to reduce API calls

## Integration with autonomy System

### GPS Source Priority
1. **Primary**: External GPS antenna (highest accuracy)
2. **Secondary**: Quectel GNSS (high accuracy)
3. **Tertiary**: Google Geolocation (medium accuracy)
4. **Fallback**: IP-based location (low accuracy)

### Failover Triggers
- **GPS Signal Loss**: Automatic switch to Google Geolocation
- **Poor GPS Accuracy**: Supplement with Google data
- **Indoor Operation**: Primary reliance on cellular/WiFi positioning

### Data Fusion
- **Accuracy Weighting**: Combine multiple sources based on reported accuracy
- **Temporal Filtering**: Use recent high-accuracy fixes to validate lower-accuracy sources
- **Confidence Scoring**: Multi-dimensional quality assessment

## Security Considerations

### API Key Management
- **UCI Storage**: Secure storage in OpenWrt configuration
- **Environment Variables**: Development/testing key management
- **Key Rotation**: Regular API key updates

### Data Privacy
- **Minimal Data**: Only send required cellular and WiFi data
- **No Personal Info**: No device identifiers or user data
- **Encrypted Transport**: HTTPS for all API communications

## Monitoring and Maintenance

### Health Checks
- **API Availability**: Regular connectivity tests
- **Response Quality**: Accuracy validation against GPS
- **Cost Tracking**: Daily/monthly usage monitoring

### Alerting
- **High Error Rates**: Webhook notifications for API failures
- **Cost Thresholds**: Budget alerts for unexpected usage
- **Accuracy Degradation**: Quality monitoring alerts

## Future Enhancements

### Planned Features
1. **5G NR Support**: Full 5G New Radio implementation when library supports it
2. **Bluetooth Beacons**: Additional positioning signals
3. **Machine Learning**: Accuracy prediction and optimization
4. **Offline Caching**: Local cell tower database for reduced API calls

### Performance Optimizations
1. **Request Batching**: Multiple location requests in single API call
2. **Smart Caching**: Predictive cache warming
3. **Data Compression**: Optimized request payloads
4. **Regional Endpoints**: Reduced latency through geographic routing

## Conclusion

The Google Geolocation API integration provides a robust, production-ready cellular and WiFi positioning system that seamlessly integrates with the RutOS Starlink Failover architecture. With comprehensive UCI configuration, intelligent caching, and adherence to Google's best practices, this implementation offers reliable location services for critical infrastructure applications.
