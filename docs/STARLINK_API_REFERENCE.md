# Starlink API Documentation

**Version:** 2.6.0 | **Updated:** 2025-07-24  
_API Documentation v2.6.0_

## Overview

This document provides comprehensive information about the Starlink gRPC API used by the monitoring system.

> **⚠️ Important**: This is an **unofficial** API that may change without notice. SpaceX does not provide official
> documentation or support for this API.

## API Endpoint

- **Host**: `192.168.100.1` (Starlink dish in Bypass Mode)
- **Port**: `9200`
- **Protocol**: gRPC (HTTP/2)
- **Encryption**: None (plaintext)

## Authentication

The API does not require authentication when accessed from the local network.

## API Methods

### get_status

Returns real-time status information about the Starlink dish.

**Request**:

```bash
grpcurl -plaintext -d '{"get_status":{}}' 192.168.100.1:9200 SpaceX.API.Device.Device/Handle
```

**Response Structure**:

```json
{
  "dishGetStatus": {
    "deviceInfo": {
      "id": "string",
      "hardwareVersion": "string",
      "softwareVersion": "string",
      "countryCode": "string"
    },
    "deviceState": {
      "uptimeS": "number"
    },
    "obstructionStats": {
      "fractionObstructed": "number",
      "validS": "number",
      "wedgeFractionObstructed": ["number"],
      "wedgeAbsFractionObstructed": ["number"]
    },
    "popPingLatencyMs": "number",
    "downlinkThroughputBps": "number",
    "uplinkThroughputBps": "number",
    "snr": "number",
    "secondsToFirstNonemptySlot": "number",
    "popPingDropRate": "number",
    "boresightAzimuthDeg": "number",
    "boresightElevationDeg": "number",
    "gpsStats": {
      "gpsValid": "boolean",
      "gpsSats": "number",
      "noSatsAfterTtff": "number",
      "inhibitGps": "boolean"
    },
    "ethSpeedMbps": "number",
    "mobilityClass": "string",
    "isSnrAboveNoiseFloor": "boolean",
    "classOfService": "string",
    "softwareUpdateState": "string",
    "isSnrPersistentlyLow": "boolean",
    "swupdateRebootReady": "boolean"
  }
}
```

**Key Fields for Monitoring**:

- `obstructionStats.fractionObstructed`: Percentage of sky view obstructed (0.0-1.0)
- `popPingLatencyMs`: Latency to Point of Presence in milliseconds
- `popPingDropRate`: Packet drop rate (0.0-1.0)
- `snr`: Signal-to-noise ratio
- `gpsStats.gpsValid`: GPS fix status
- `deviceState.uptimeS`: Device uptime in seconds

### get_history

Returns historical performance data arrays.

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
    "obstructed": ["boolean"]
  }
}
```

**Key Fields for Monitoring**:

- `current`: Current index in the data arrays
- `popPingDropRate`: Array of packet drop rates
- `popPingLatencyMs`: Array of latency measurements
- `obstructed`: Array of obstruction status booleans

### get_device_info

Returns static device information.

**Request**:

```bash
grpcurl -plaintext -d '{"get_device_info":{}}' 192.168.100.1:9200 SpaceX.API.Device.Device/Handle
```

**Response Structure**:

```json
{
  "deviceInfo": {
    "id": "string",
    "hardwareVersion": "string",
    "softwareVersion": "string",
    "countryCode": "string",
    "utcOffsetS": "number",
    "softwarePartNumber": "string",
    "generationNumber": "number",
    "dishCohoused": "boolean",
    "utcnsOffsetNs": "number"
  }
}
```

### get_location

Returns GPS location information.

**Request**:

```bash
grpcurl -plaintext -d '{"get_location":{}}' 192.168.100.1:9200 SpaceX.API.Device.Device/Handle
```

**Response Structure**:

```json
{
  "getLocation": {
    "lla": {
      "lat": "number",
      "lon": "number",
      "alt": "number"
    },
    "ecef": {
      "x": "number",
      "y": "number",
      "z": "number"
    },
    "source": "string"
  }
}
```

### get_diagnostics

Returns diagnostic information.

**Request**:

```bash
grpcurl -plaintext -d '{"get_diagnostics":{}}' 192.168.100.1:9200 SpaceX.API.Device.Device/Handle
```

**Response Structure**:

```json
{
  "dishGetDiagnostics": {
    "id": "string",
    "hardwareVersion": "string",
    "softwareVersion": "string",
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

## Data Types and Ranges

### Obstruction

- **Type**: Float (0.0 - 1.0)
- **Unit**: Fraction of sky view obstructed
- **Typical Values**: 0.0 (clear) to 0.1 (10% obstructed)
- **Monitoring Threshold**: > 0.001 (0.1%)

### Latency

- **Type**: Float
- **Unit**: Milliseconds
- **Typical Values**: 20-80ms (depends on location)
- **Monitoring Threshold**: > 150ms

### Packet Loss

- **Type**: Float (0.0 - 1.0)
- **Unit**: Fraction of packets lost
- **Typical Values**: 0.0 (no loss) to 0.1 (10% loss)
- **Monitoring Threshold**: > 0.05 (5%)

### Signal-to-Noise Ratio (SNR)

- **Type**: Float
- **Unit**: dB
- **Typical Values**: 8-15 dB
- **Good Signal**: > 10 dB

### Throughput

- **Type**: Float
- **Unit**: Bits per second
- **Typical Values**:
  - Download: 50-200 Mbps
  - Upload: 10-40 Mbps

## Error Handling

### Common Errors

#### Connection Refused

```bash
grpcurl: error: failed to dial target host "192.168.100.1:9200": context deadline exceeded
```

**Causes**:

- Starlink dish not in Bypass Mode
- Network routing issues
- Dish powered off

#### Invalid Method

```bash
grpcurl: error: rpc error: code = Unimplemented desc = unknown service SpaceX.API.Device.Device
```

**Causes**:

- API version changed
- Incorrect service name
- Firmware update broke compatibility

#### Timeout

```bash
grpcurl: error: context deadline exceeded
```

**Causes**:

- Network congestion
- Dish overloaded
- API temporarily unavailable

### Error Handling in Scripts

```bash
# Example error handling
if ! timeout 10 grpcurl -plaintext -max-time 10 -d '{"get_status":{}}' "$STARLINK_IP:$STARLINK_PORT" SpaceX.API.Device.Device/Handle 2>/dev/null; then
    log "error" "Failed to get data from Starlink API"
    # Fallback logic here
fi
```

## API Versioning

The API version can be determined from the device info:

```bash
# Get software version
grpcurl -plaintext -d '{"get_device_info":{}}' 192.168.100.1:9200 SpaceX.API.Device.Device/Handle | jq -r '.deviceInfo.softwareVersion'
```

### Version Compatibility

| Software Version | API Changes          | Compatibility |
| ---------------- | -------------------- | ------------- |
| 1.0.x            | Initial release      | Full          |
| 2.0.x            | Added location API   | Full          |
| 3.0.x            | Enhanced diagnostics | Full          |
| 4.0.x            | GPS improvements     | Full          |

## Best Practices

### Rate Limiting

- **Recommended**: 1 request per minute for monitoring
- **Maximum**: 10 requests per minute
- **Burst**: Up to 3 requests in quick succession

### Connection Management

- Use connection pooling when possible
- Implement exponential backoff for retries
- Set appropriate timeouts (10-30 seconds)

### Data Processing

- Always validate JSON responses
- Handle missing fields gracefully
- Use appropriate data types for comparisons

### Example Implementation

```bash
#!/bin/sh
# Robust API call implementation

call_starlink_api() {
    local method="$1"
    local retry_count=0
    local max_retries=3
    local delay=2

    while [ $retry_count -lt $max_retries ]; do
        local response
        if response=$(timeout 10 grpcurl -plaintext -max-time 10 -d "{\"$method\":{}}" "$STARLINK_IP:$STARLINK_PORT" SpaceX.API.Device.Device/Handle 2>/dev/null); then
            echo "$response"
            return 0
        fi

        retry_count=$((retry_count + 1))
        if [ $retry_count -lt $max_retries ]; then
            sleep $delay
            delay=$((delay * 2))
        fi
    done

    return 1
}

# Usage
if status_data=$(call_starlink_api "get_status"); then
    obstruction=$(echo "$status_data" | jq -r '.dishGetStatus.obstructionStats.fractionObstructed // 0')
    latency=$(echo "$status_data" | jq -r '.dishGetStatus.popPingLatencyMs // 0')
    # Process data...
else
    echo "API call failed"
fi
```

## Monitoring Recommendations

### Critical Metrics

1. **Obstruction**: Monitor for physical blockages
2. **Latency**: Track connection quality
3. **Packet Loss**: Detect network issues
4. **SNR**: Monitor signal quality

### Thresholds

- **Obstruction**: > 0.1% (0.001)
- **Latency**: > 150ms
- **Packet Loss**: > 5% (0.05)
- **SNR**: < 8 dB (poor signal)

### Alerting

- **Immediate**: Hard failures (API unavailable)
- **Delayed**: Soft failures (quality degradation)
- **Informational**: Recovery events

## Security Considerations

### Network Security

- API is only accessible from local network
- No authentication required
- Consider firewall rules for additional protection

### Data Privacy

- API responses may contain location data
- Device IDs are unique identifiers
- Don't log sensitive information

### API Abuse

- Respect rate limits
- Don't overload the dish
- Monitor for API changes

## Future Considerations

### API Evolution

- SpaceX may change API without notice
- New methods may be added
- Existing methods may be deprecated

### Monitoring Strategy

- Implement version checking
- Plan for API changes
- Have fallback mechanisms

### Community

- Share findings with community
- Contribute to API documentation
- Report issues and changes

---

**Disclaimer**: This API is unofficial and unsupported. Use at your own risk and be prepared for changes without notice.
