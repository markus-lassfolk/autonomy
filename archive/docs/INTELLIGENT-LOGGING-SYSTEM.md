# Intelligent Starlink Logging System v3.0

## Overview

The Intelligent Starlink Logging System v3.0 provides comprehensive, efficient metrics collection for RUTOS devices with
intelligent data management and statistical analysis capabilities.

## Key Features

### 🚀 **Efficient Data Collection**

- **MWAN3 Integration**: Extracts existing performance data without generating additional network traffic
- **Smart Frequency Control**: 1-second intervals for unlimited connections, 60-second intervals for data-limited
  connections
- **Multi-Source GPS**: Primary RUTOS GPS with Starlink API fallback
- **Real-time Interface Discovery**: Automatically detects and classifies all MWAN3-managed interfaces

### 📊 **Advanced Analytics**

- **Statistical Aggregation**: 60-second windows with min/max/average/percentile calculations
- **Performance Trend Analysis**: Historical data analysis for predictive insights
- **Connection Quality Scoring**: Intelligent scoring based on latency, packet loss, and availability
- **Multi-Interface Monitoring**: Supports up to 8 cellular modems plus WiFi/Ethernet/satellite connections

### 💾 **Intelligent Storage Management**

- **24-Hour Retention**: Detailed metrics for 24 hours before rotation
- **Automatic Compression**: Logs compressed and archived for 7 days
- **Persistent Storage**: Survives RUTOS firmware upgrades using `/usr/local/starlink/` directory structure
- **Space-Efficient**: Smart rotation prevents disk space issues

### 🔄 **Daemon-Based Architecture**

- **Continuous Operation**: Background daemon for uninterrupted data collection
- **Resource-Conscious**: Efficient memory and CPU usage
- **Fault Tolerant**: Automatic restart and error recovery
- **Service Integration**: Full init.d integration with system startup

## Architecture

```
/usr/local/starlink/logs/
├── metrics/           # Raw metrics (24-hour retention)
│   ├── metrics_20250801.csv
│   └── metrics_20250802.csv
├── gps/              # GPS tracking data
│   ├── gps_20250801.csv
│   └── gps_20250802.csv
├── aggregated/       # Statistical summaries
│   ├── aggregated_20250801.csv
│   └── aggregated_20250802.csv
└── archive/          # Compressed historical data (7-day retention)
    ├── metrics_20250725.csv.gz
    └── gps_20250725.csv.gz
```

## Data Collection Strategy

### **Connection Type Detection**

The system automatically detects connection types and adjusts collection frequency:

- **Cellular Modems** (`mob1s1a1-mob8s1a1`): 60-second intervals (data-conscious)
- **Satellite** (`wwan*`, `starlink*`): 1-second intervals (unlimited)
- **Wired/WiFi** (`eth*`, `wifi*`): 1-second intervals (unlimited)
- **Unknown**: 60-second intervals (conservative default)

### **MWAN3 Metrics Extraction**

Instead of generating additional network traffic, the system efficiently extracts:

- **Interface Status**: Online/offline state from MWAN3
- **Real-time Ping Statistics**: Latency and packet loss from existing MWAN3 tracking
- **Network Counters**: Bytes/packets transmitted and received
- **Quality Metrics**: Calculated connection quality scores
- **Metric Values**: Current MWAN3 metric assignments

### **GPS Data Collection**

Dual-source GPS tracking with intelligent fallback:

1. **Primary Source**: RUTOS cellular GPS via `gsmctl`
2. **Secondary Source**: Starlink API GPS coordinates
3. **Collection Frequency**: Every 60 seconds
4. **Data Points**: Latitude, longitude, altitude, accuracy, source

## Statistical Aggregation

### **60-Second Windows**

Every minute, the system creates statistical summaries:

- **Latency**: Min, max, average, 50th/90th/95th/99th percentiles
- **Packet Loss**: Min, max, average, 90th/95th percentiles
- **Quality Scores**: Average connection quality
- **Traffic Counters**: Total bytes transmitted and received
- **State Changes**: Interface state and metric changes

### **Percentile Calculations**

Advanced statistical analysis provides insights into:

- **Performance Consistency**: How stable is the connection?
- **Outlier Detection**: Identify intermittent issues
- **Trend Analysis**: Performance degradation over time
- **SLA Monitoring**: 95th percentile latency tracking

## Usage

### **Service Control**

```bash
# Start logging daemon
/etc/init.d/starlink-logger start

# Stop logging daemon
/etc/init.d/starlink-logger stop

# Check status
/etc/init.d/starlink-logger status

# Restart logging
/etc/init.d/starlink-logger restart
```

### **Manual Operations**

```bash
# Test collection functionality
/usr/local/starlink/bin/starlink_intelligent_logger-rutos.sh test

# Check daemon status
/usr/local/starlink/bin/starlink_intelligent_logger-rutos.sh status

# Manual log rotation
/usr/local/starlink/bin/starlink_intelligent_logger-rutos.sh rotate

# Start daemon manually
/usr/local/starlink/bin/starlink_intelligent_logger-rutos.sh start
```

### **Log Analysis**

#### **View Recent Metrics**

```bash
# Latest metrics for all interfaces
tail -50 /usr/local/starlink/logs/metrics/metrics_$(date +%Y%m%d).csv

# GPS tracking
tail -20 /usr/local/starlink/logs/gps/gps_$(date +%Y%m%d).csv

# Statistical summaries
tail -20 /usr/local/starlink/logs/aggregated/aggregated_$(date +%Y%m%d).csv
```

#### **Performance Analysis**

```bash
# Find highest latency events
awk -F',' '$5 > 500 {print $1","$2","$5"ms"}' /usr/local/starlink/logs/metrics/metrics_$(date +%Y%m%d).csv

# Count packet loss events
awk -F',' '$6 > 0 {print $1","$2","$6"%"}' /usr/local/starlink/logs/metrics/metrics_$(date +%Y%m%d).csv

# Interface state changes
awk -F',' 'prev_state[$2] && prev_state[$2] != $3 {print $1","$2": "$3" (was "prev_state[$2]")"} {prev_state[$2] = $3}' /usr/local/starlink/logs/metrics/metrics_$(date +%Y%m%d).csv
```

## Configuration

### **Collection Frequency**

```bash
# /etc/starlink-config/logging.conf
HIGH_FREQ_INTERVAL=1           # Unlimited connections
LOW_FREQ_INTERVAL=60          # Limited data connections
GPS_COLLECTION_INTERVAL=60    # GPS collection interval
```

### **Retention Policies**

```bash
LOG_RETENTION_HOURS=24        # Raw data retention
ARCHIVE_RETENTION_DAYS=7      # Compressed archive retention
```

### **Interface Classification**

```bash
CELLULAR_INTERFACES_PATTERN="^mob[0-9]s[0-9]a[0-9]$"
SATELLITE_INTERFACES_PATTERN="^wwan|^starlink"
UNLIMITED_INTERFACES_PATTERN="^eth|^wifi"
```

## Data Formats

### **Raw Metrics CSV**

```csv
timestamp,interface,state,metric,latency_ms,loss_percent,quality_score,rx_bytes,tx_bytes,rx_packets,tx_packets,rx_errors,tx_errors,track_ip,track_method,last_online
2025-08-01 10:30:00,mob1s1a1,online,10,45.2,0,100,1048576,524288,1024,512,0,0,8.8.8.8,ping,1722510600
```

### **GPS Tracking CSV**

```csv
timestamp,source,latitude,longitude,altitude,accuracy,satellites,fix_type,speed,heading
2025-08-01 10:30:00,rutos,59.334591,18.063240,45,5,8,3D,0,0
```

### **Aggregated Statistics CSV**

```csv
window_start,window_end,interface,sample_count,latency_min,latency_max,latency_avg,latency_p50,latency_p90,latency_p95,latency_p99,loss_min,loss_max,loss_avg,loss_p90,loss_p95,quality_avg,bytes_rx_total,bytes_tx_total,state_changes,metric_changes
2025-08-01 10:30:00,2025-08-01 10:31:00,mob1s1a1,60,42.1,67.8,48.9,47.2,58.1,62.3,67.8,0,0,0,0,0,100,1048576,524288,0,0
```

## Firmware Upgrade Recovery

The system is designed to survive RUTOS firmware upgrades:

1. **Persistent Storage**: All data stored in `/usr/local/starlink/` (persistent across upgrades)
2. **Service Templates**: Daemon service templates preserved in persistent storage
3. **Automatic Recovery**: Run recovery script after firmware upgrade:

```bash
/usr/local/starlink/bin/recover-after-firmware-upgrade.sh
```

## Troubleshooting

### **Common Issues**

#### **Daemon Not Starting**

```bash
# Check configuration
cat /etc/starlink-config/config.sh

# Verify MWAN3 availability
mwan3 status

# Check permissions
ls -la /usr/local/starlink/logs/
```

#### **No Data Collection**

```bash
# Test manual collection
/usr/local/starlink/bin/starlink_intelligent_logger-rutos.sh test

# Check MWAN3 interfaces
mwan3 interfaces

# Verify binary availability
ls -la /usr/local/starlink/bin/grpcurl /usr/local/starlink/bin/jq
```

#### **High Disk Usage**

```bash
# Check log sizes
du -sh /usr/local/starlink/logs/*

# Manual rotation
/usr/local/starlink/bin/starlink_intelligent_logger-rutos.sh rotate

# Verify retention settings
grep -E "(RETENTION|ARCHIVE)" /etc/starlink-config/logging.conf
```

### **Debug Mode**

Enable debug logging for troubleshooting:

```bash
# Enable debug mode
DEBUG=1 /usr/local/starlink/bin/starlink_intelligent_logger-rutos.sh test

# Check system logs
logread | grep starlink-logger
```

## Performance Impact

### **Resource Usage**

- **Memory**: ~2-4MB for daemon process
- **CPU**: <1% on RUTX50 during collection
- **Disk I/O**: Minimal, optimized for embedded devices
- **Network**: Zero additional traffic (uses existing MWAN3 data)

### **Storage Requirements**

- **Raw Metrics**: ~1MB per day per interface (estimated)
- **GPS Data**: ~100KB per day
- **Aggregated Data**: ~50KB per day per interface
- **Total with 4 interfaces**: ~5MB per day, ~35MB per week

## Integration with Monitoring System

The intelligent logging system works seamlessly with the monitoring daemon:

- **Shared Configuration**: Uses same config files and settings
- **Coordinated Operation**: Monitoring daemon makes decisions, logger records results
- **Data Correlation**: Timestamps allow correlation between decisions and metrics
- **Performance Feedback**: Logger data feeds into predictive algorithms

## Future Enhancements

Planned improvements for future versions:

- **Machine Learning**: Predictive failure detection based on historical patterns
- **Advanced Analytics**: Real-time anomaly detection
- **Export Capabilities**: Direct export to time-series databases
- **Mobile App Integration**: Real-time metrics viewing
- **Alert Thresholds**: Configurable alerting based on collected metrics

---

**For support and updates**: https://github.com/markus-lassfolk/autonomy
