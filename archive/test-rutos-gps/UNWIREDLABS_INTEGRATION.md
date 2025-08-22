# UnwiredLabs LocationAPI Integration

## Overview

UnwiredLabs LocationAPI provides comprehensive geolocation services using cell towers and WiFi access points. This implementation supports up to **7 cell towers** and **2-15 WiFi access points** per request, making it much more cost-effective than OpenCellID's single-cell approach.

## Key Features

### 📡 **Multi-Cell Support**
- **Up to 7 cell towers** per request (serving cell + 6 neighbors)
- **Automatic radio type detection** (LTE, UMTS, GSM, CDMA)
- **Signal strength prioritization** (strongest cells first)
- **Comprehensive cell data** (CID, LAC/TAC, PCI, EARFCN, signal strength)

### 📶 **WiFi Integration**
- **2-15 WiFi access points** per request
- **Automatic WiFi scanning** via RutOS
- **Signal strength sorting** (strongest APs first)
- **BSSID, SSID, channel, and signal data**

### 🌍 **Regional Endpoints**
- **EU1**: `https://eu1.unwiredlabs.com/v2` (Europe - France)
- **US1**: `https://us1.unwiredlabs.com/v2` (US East)
- **US2**: `https://us2.unwiredlabs.com/v2` (US West)
- **AP1**: `https://ap1.unwiredlabs.com/v2` (Asia Pacific)

### 💰 **Credit Management**
- **Balance checking** before requests
- **Credit consumption tracking**
- **Insufficient credit protection**
- **Usage monitoring and alerts**

## API Request Structure

### Cell Tower Data (LTE Example)
```json
{
  "token": "your-api-token",
  "radio": "lte",
  "mcc": 240,
  "mnc": 1,
  "cells": [
    {
      "cid": 25939743,
      "tac": 23,
      "pci": 443,
      "earfcn": 1300,
      "signal": -84
    },
    {
      "cid": 263,
      "tac": 23,
      "pci": 263,
      "earfcn": 1300,
      "signal": -90
    }
  ],
  "wifi": [
    {
      "bssid": "aa:bb:cc:dd:ee:ff",
      "signal": -45,
      "channel": 6,
      "ssid": "MyWiFi"
    }
  ],
  "address": 1,
  "fallbacks": ["lacf", "scf"]
}
```

### Response Format
```json
{
  "status": "ok",
  "lat": 59.48007,
  "lon": 18.27985,
  "accuracy": 100,
  "address": "Stockholm, Sweden",
  "balance": 9995,
  "fallback": "lacf"
}
```

## Radio Type Support

### 🔴 **LTE (4G/5G)**
- **CID**: Cell ID
- **TAC**: Tracking Area Code
- **PCI**: Physical Cell ID
- **EARFCN**: E-UTRA Absolute Radio Frequency Channel Number
- **Signal**: RSRP in dBm

### 🟡 **UMTS (3G)**
- **CID**: Cell ID
- **LAC**: Location Area Code
- **UC**: UMTS Cell ID
- **PSC**: Primary Scrambling Code
- **UARFCN**: UTRA Absolute Radio Frequency Channel Number
- **Signal**: RSCP in dBm

### 🟢 **GSM (2G)**
- **CID**: Cell ID
- **LAC**: Location Area Code
- **Signal**: RSSI in dBm

### 🔵 **CDMA**
- **CID**: Cell ID
- **SID**: System ID
- **NID**: Network ID
- **BID**: Base Station ID
- **Signal**: Signal strength in dBm

## Implementation Details

### Cell Tower Collection
```go
// Collect serving cell + up to 6 neighbors
cells, radioType, err := BuildCellTowersFromIntelligence(intel, 7)

// Automatic radio type detection
radioType := determineRadioType(band, technology)

// Signal strength sorting (strongest first)
sort.Slice(neighbors, func(i, j int) bool {
    return neighbors[i].RSRP > neighbors[j].RSRP
})
```

### WiFi Access Point Collection
```go
// Scan for WiFi networks
wifiAPs, err := CollectWiFiAccessPoints(client, 15)

// Parse scan results for BSSID, SSID, signal, channel
// Sort by signal strength (strongest first)
// Limit to 2-15 access points
```

### Balance Management
```go
// Check balance before requests
balance, err := api.GetBalance()
if balance.Balance < 10 {
    return fmt.Errorf("insufficient credits: %d remaining", balance.Balance)
}

// Track usage after requests
fmt.Printf("💰 Remaining credits: %d\n", response.Balance)
```

## Error Handling

### HTTP Status Codes
- **200**: OK - Request successful
- **400**: Bad Request - Invalid parameters
- **401**: Unauthorized - Invalid API token
- **402**: Payment Required - Insufficient credits
- **403**: Forbidden - Rate limited or access denied
- **404**: Not Found - Location not found
- **429**: Too Many Requests - Rate limit exceeded
- **500**: Internal Server Error - Server error
- **503**: Service Unavailable - Service down

### Error Schema Implementation
```go
type ErrorSchema struct {
    Code        int    `json:"code"`
    Message     string `json:"message"`
    Description string `json:"description"`
    Suggestion  string `json:"suggestion,omitempty"`
}

// Handle specific errors
errorInfo := api.getErrorInfo(resp.StatusCode, response.Message)
return fmt.Errorf("API error (%d): %s - %s", 
    errorInfo.Code, errorInfo.Message, errorInfo.Description)
```

## UCI Configuration

### Configuration Structure
```bash
# Enable/disable UnwiredLabs LocationAPI
uci set autonomy.unwiredlabs.enabled='1'

# API token (secure storage)
uci set autonomy.unwiredlabs.api_token='your-token-here'

# Regional endpoint
uci set autonomy.unwiredlabs.region='eu1'

# Request parameters
uci set autonomy.unwiredlabs.max_cells='7'
uci set autonomy.unwiredlabs.max_wifi_aps='15'
uci set autonomy.unwiredlabs.include_address='1'

# Credit management
uci set autonomy.unwiredlabs.min_credits='50'
uci set autonomy.unwiredlabs.credit_alert_threshold='100'

# Fallback options
uci set autonomy.unwiredlabs.fallbacks='lacf,scf'

# Rate limiting
uci set autonomy.unwiredlabs.max_requests_per_hour='100'
uci set autonomy.unwiredlabs.request_interval_seconds='30'

# Commit changes
uci commit autonomy
```

### UCI Integration Code
```go
type UnwiredLabsConfig struct {
    Enabled              bool     `uci:"enabled"`
    APIToken             string   `uci:"api_token"`
    Region               string   `uci:"region"`
    MaxCells             int      `uci:"max_cells"`
    MaxWiFiAPs           int      `uci:"max_wifi_aps"`
    IncludeAddress       bool     `uci:"include_address"`
    MinCredits           int      `uci:"min_credits"`
    CreditAlertThreshold int      `uci:"credit_alert_threshold"`
    Fallbacks            []string `uci:"fallbacks"`
    MaxRequestsPerHour   int      `uci:"max_requests_per_hour"`
    RequestInterval      int      `uci:"request_interval_seconds"`
}
```

## Performance Comparison

| Service | Cells/Request | WiFi Support | Cost/Request | Accuracy | Regional |
|---------|---------------|--------------|--------------|----------|----------|
| **UnwiredLabs** | **7 cells** | **✅ 2-15 APs** | **1 credit** | **50-500m** | **✅ 4 regions** |
| OpenCellID | 1 cell | ❌ No | 1 request | 100-1000m | ❌ Global only |

## Integration Benefits

### 🎯 **Higher Accuracy**
- **Multiple data sources** (cells + WiFi)
- **Signal strength weighting**
- **Fallback mechanisms**
- **Regional optimization**

### 💰 **Cost Efficiency**
- **7x more cell data** per request vs OpenCellID
- **WiFi data included** at no extra cost
- **Bulk data collection** reduces API calls
- **Credit tracking** prevents overuse

### 🚀 **Better Performance**
- **Regional endpoints** reduce latency
- **Comprehensive error handling**
- **Automatic retries** with backoff
- **Balance monitoring**

### 🔧 **Production Ready**
- **UCI configuration** integration
- **Secure token storage**
- **Rate limiting** and throttling
- **Comprehensive logging**

## Deployment Strategy

### Phase 1: Testing
1. **Load API token** from secure storage
2. **Test balance checking** functionality
3. **Verify cell tower** data collection
4. **Test WiFi scanning** capabilities
5. **Validate request/response** handling

### Phase 2: Integration
1. **Implement UCI configuration** system
2. **Add to GPS source priority** list
3. **Configure as fallback** option
4. **Set up monitoring** and alerts
5. **Deploy with rate limiting**

### Phase 3: Optimization
1. **Monitor accuracy** vs GPS sources
2. **Optimize cell/WiFi selection**
3. **Fine-tune regional** endpoints
4. **Implement intelligent** caching
5. **Add predictive** requests

## Usage Examples

### Basic Location Request
```bash
# Test with live data
go run . -test-unwiredlabs

# Expected output:
# 💰 Remaining credits: 9995
# 🗼 Cell Towers: 7 (serving + 6 neighbors)
# 📶 WiFi APs: 12
# ✅ Location: 59.480070°, 18.279850° (±150m)
```

### Production Integration
```go
// Load configuration from UCI
config, err := LoadUnwiredLabsConfig()

// Create API client
api := NewUnwiredLabsLocationAPI(config.APIToken, config.Region)

// Get location with live data
response, err := GetLocationWithUnwiredLabs(client, config.Region)
```

This comprehensive implementation provides a robust, cost-effective alternative to OpenCellID with significantly better accuracy and feature coverage! 🚀
