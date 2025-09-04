# 🔌 API Reference

Complete API documentation for the Autonomy system and its components.

## 📋 Configuration & Integration

### [Configuration Reference](configuration-reference.md)
Complete UCI configuration reference with all available settings and options.

### [External APIs](external-apis.md)  
Complete guide to all external API integrations, signup processes, and configuration.

## 📡 Starlink APIs

### [Starlink API Reference](starlink-api.md)
Complete documentation of the Starlink gRPC API integration, including:
- Dish status and diagnostics
- Obstruction mapping and analysis
- GPS location services
- Real-time connectivity monitoring

### [Starlink API Analysis](STARLINK_API_ANALYSIS.md)
Detailed analysis of Starlink API capabilities and data structures.

## 🤖 Autonomy System APIs

### Core System APIs
- **Status API**: System health and interface status
- **Control API**: Manual failover and configuration
- **Monitoring API**: Real-time metrics and telemetry
- **Configuration API**: UCI integration and settings management

### Network Interface APIs
- **Cellular API**: RSRP, RSRQ, SINR monitoring
- **WiFi API**: Channel analysis and optimization
- **GPS API**: Multi-source location services
- **Starlink API**: Satellite tracking and prediction

## 🔗 Integration APIs

### UBUS Integration
All APIs are accessible via UBUS for system integration:

```bash
# System status
ubus call autonomy status

# Interface control
ubus call autonomy switch '{"interface": "cellular"}'

# Starlink predictions
ubus call starlink_tracker predictions

# GPS location
ubus call gps location
```

### RPCD Integration
Web UI integration via JSON-RPC calls to rpcd daemon.

## 📊 Data Formats

### Standard Response Format
```json
{
  "success": true,
  "data": { /* API-specific data */ },
  "timestamp": 1642688400,
  "version": "1.0.0"
}
```

### Error Response Format
```json
{
  "success": false,
  "error": {
    "code": "ERROR_CODE",
    "message": "Human readable error message"
  },
  "timestamp": 1642688400
}
```

## 🔧 API Authentication

Most APIs use UBUS authentication. External APIs (Space-Track, OpenCellID) require separate credentials configured in the system.

## 📈 Rate Limiting

- **Internal APIs**: No rate limiting
- **Space-Track API**: <20 calls/hour (system managed)
- **OpenCellID API**: Intelligent rate limiting with contribution system

---

For implementation examples, see the [tutorials](../tutorials/) section.