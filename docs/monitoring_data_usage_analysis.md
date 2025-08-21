# Monitoring Data Usage Analysis

## Ping Monitoring Data Usage

### Basic Ping Data
- **ICMP ping packet size**: 64 bytes (typical default)
- **Ping frequency**: 1 ping per second
- **Bidirectional**: Request (64 bytes) + Reply (64 bytes) = 128 bytes per ping

### Daily Usage
- **Per second**: 128 bytes
- **Per minute**: 128 × 60 = 7,680 bytes (7.5 KB)
- **Per hour**: 7,680 × 60 = 460,800 bytes (450 KB)
- **Per day**: 460,800 × 24 = 11,059,200 bytes (10.5 MB)

### Monthly Usage (30 days)
- **Monthly ping data**: 10.5 MB × 30 = **315 MB per interface**

## Starlink gRPC API Monitoring

### gRPC API Call Analysis
Based on our `grpcurl` implementation:

```bash
grpcurl -plaintext -d '{"get_status":{}}' 192.168.100.1:9200 SpaceX.API.Device.Device/Handle
```

### Typical Response Sizes
- **get_status response**: ~2-4 KB (includes detailed metrics)
- **get_location response**: ~500 bytes (GPS coordinates)
- **Request overhead**: ~200 bytes per call

### Our Current Collection Pattern
- **Collection frequency**: Every 5 seconds (decision interval)
- **APIs called per collection**: 1 (get_status)
- **Data per collection**: ~4 KB (request + response + overhead)

### Starlink API Daily Usage
- **Per collection**: 4 KB
- **Collections per day**: (24 × 60 × 60) / 5 = 17,280
- **Daily usage**: 17,280 × 4 KB = **69.1 MB per day**

### Starlink API Monthly Usage
- **Monthly usage**: 69.1 MB × 30 = **2.07 GB per month**

## Cellular AT Command Monitoring

### AT Command Data Usage
- **Typical AT command**: 50-100 bytes
- **Typical response**: 100-500 bytes
- **Average per query**: ~300 bytes

### Our Cellular Collection
- **Commands per collection**: ~5 (signal strength, network info, etc.)
- **Data per collection**: 5 × 300 = 1.5 KB
- **Collections per day**: 17,280 (same as decision interval)
- **Daily usage**: 17,280 × 1.5 KB = **25.9 MB per day**

### Cellular Monitoring Monthly Usage
- **Monthly usage**: 25.9 MB × 30 = **777 MB per month**

## Total Monitoring Data Usage

### Per Interface (Cellular with 1GB limit)
| Component | Daily | Monthly | % of 1GB |
|-----------|-------|---------|----------|
| Ping monitoring | 10.5 MB | 315 MB | 31.5% |
| Cellular AT commands | 25.9 MB | 777 MB | 77.7% |
| **Total per cellular** | **36.4 MB** | **1.09 GB** | **109%** |

### Starlink Interface (Unlimited)
| Component | Daily | Monthly |
|-----------|-------|---------|
| Ping monitoring | 10.5 MB | 315 MB |
| Starlink gRPC API | 69.1 MB | 2.07 GB |
| **Total Starlink** | **79.6 MB** | **2.39 GB** |

## Critical Findings

### 🚨 **MAJOR ISSUE**: Monitoring Exceeds Data Limits!
- **Cellular monitoring alone uses 109% of a 1GB monthly limit**
- **This makes 1GB cellular connections unusable for failover**

### Recommendations

#### 1. **Adaptive Monitoring Frequency**
```go
// Reduce monitoring frequency on limited connections
if dataLimit.UsagePercentage > 80 {
    // Reduce to every 30 seconds instead of 5
    monitoringInterval = 30 * time.Second
} else if dataLimit.UsagePercentage > 50 {
    // Reduce to every 15 seconds
    monitoringInterval = 15 * time.Second
}
```

#### 2. **Optimized Ping Sizes**
```bash
# Use smaller ping packets
ping -s 8 -c 1 target  # 8 bytes instead of 64 bytes
# Reduces usage by 87.5%: 315 MB → 39 MB monthly
```

#### 3. **Smart Monitoring Modes**

**Active Mode** (Primary interface):
- Full monitoring every 5 seconds
- Complete metrics collection

**Standby Mode** (Backup interfaces):
- Basic ping every 60 seconds
- Reduced AT command frequency
- **Savings**: ~90% reduction

**Emergency Mode** (>90% data usage):
- Ping every 5 minutes
- Minimal metrics
- **Savings**: ~98% reduction

#### 4. **Data Usage Calculation**

**Optimized Monitoring (Standby Mode)**:
| Component | Current Monthly | Optimized Monthly | Savings |
|-----------|----------------|-------------------|---------|
| Ping (60s interval) | 315 MB | 26 MB | 92% |
| AT commands (60s) | 777 MB | 65 MB | 92% |
| **Total** | **1.09 GB** | **91 MB** | **92%** |

#### 5. **Implementation Strategy**

```go
type MonitoringMode int

const (
    MonitoringActive    MonitoringMode = iota // Primary interface
    MonitoringStandby                        // Backup interface  
    MonitoringEmergency                      // Data limit critical
    MonitoringDisabled                       // Data limit exceeded
)

func (d *DecisionEngine) getMonitoringMode(member *pkg.Member) MonitoringMode {
    if member.DataLimitConfig == nil {
        return MonitoringActive // Unlimited
    }
    
    usage := member.DataLimitConfig.UsagePercentage
    switch {
    case usage >= 95:
        return MonitoringDisabled
    case usage >= 85:
        return MonitoringEmergency
    case member.IsPrimary:
        return MonitoringActive
    default:
        return MonitoringStandby
    }
}
```

## Conclusion

**Current monitoring would consume 109% of a 1GB cellular limit**, making it unusable. We need:

1. **Immediate**: Implement adaptive monitoring frequencies
2. **Short-term**: Add data usage awareness to monitoring decisions
3. **Long-term**: Implement smart monitoring modes based on interface role and data limits

This analysis shows why data limit integration is critical for the failover system to be practical with cellular connections.
