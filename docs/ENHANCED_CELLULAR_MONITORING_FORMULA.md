# 📊 Enhanced Cellular Monitoring & Scoring Formula

**Comprehensive Analysis: Signal Strength + Connectivity + Stability = Perfect Failover**

## 🎯 **Overview**

Your question about including latency and drop rate monitoring alongside signal strength data is spot-on! Our enhanced system now provides the most comprehensive cellular monitoring available, combining:

1. **Signal Strength Data** (same as RUTOS GUI) - RSRP, RSRQ, SINR
2. **Real-Time Connectivity** - Latency, packet loss, jitter via multiple methods
3. **mwan3 Integration** - Leverage existing tracking when available
4. **Standby Interface Monitoring** - Continuous monitoring even when not primary
5. **Predictive Analytics** - Trend analysis and failure prediction

## 🚀 **Monitoring Methods & Data Sources**

### **1. Signal Strength Monitoring (35% of Score)**
```bash
# RUTOS Native (Priority 1) - Same data as GUI signal graphs
ubus -S call mobiled signal '{}'
ubus -S call mobiled cell_info '{}'

# QMI Fallback (Priority 2)
uqmi -d /dev/cdc-wdm0 --get-signal-info

# AT Commands (Priority 3)
echo -e 'AT+QCSQ\r' > /dev/ttyUSB2
```

**Metrics Collected:**
- **RSRP** (Reference Signal Received Power) - dBm
- **RSRQ** (Reference Signal Received Quality) - dB  
- **SINR** (Signal-to-Interference-plus-Noise Ratio) - dB
- **Cell ID, Band, Network Type** - For handoff detection

### **2. Connectivity Monitoring (40% of Score)**
```bash
# Method 1: mwan3 tracking data (when available)
mwan3 status
mwan3 interfaces

# Method 2: Enhanced probing (active and standby interfaces)
# ICMP ping with interface binding
ping -c 1 -W 3 -I mob1s1a1 8.8.8.8

# TCP connect probes (ports 80, 443, 53)
# UDP DNS probes for additional validation
```

**Metrics Collected:**
- **Latency** - Network round-trip time (ms)
- **Packet Loss** - Percentage of failed probes (%)
- **Jitter** - Latency variation (ms)
- **Probe Method** - ICMP/TCP/UDP for tracking
- **Target Hosts** - Multiple targets for reliability

### **3. Stability Analysis (15% of Score)**
```bash
# Rolling window analysis (10-minute default)
# Signal variance calculation
# Cell handoff frequency tracking
# Throughput performance monitoring
```

**Metrics Collected:**
- **Signal Variance** - RSRP stability over time
- **Cell Changes** - Handoff frequency per window
- **Throughput** - Data transfer performance (Kbps)

### **4. Quality Factors (10% of Score)**
- **Network Type** - 5G > LTE > 3G > 2G scoring
- **Band Quality** - Frequency band performance characteristics
- **Modem Health** - Hardware status indicators

## 📊 **Enhanced Scoring Formula**

### **Total Score Calculation**
```
Total Score = Signal Strength (35%) + Connectivity (40%) + Stability (15%) + Quality (10%) + Bonuses - Penalties

Where:
Signal Strength = RSRP(15%) + RSRQ(10%) + SINR(10%)
Connectivity = Latency(20%) + Loss(15%) + Jitter(5%)  
Stability = Variance(8%) + Cell Changes(4%) + Throughput(3%)
Quality = Network Type(5%) + Band(3%) + Modem Health(2%)
```

### **Detailed Scoring Breakdown**

#### **Signal Strength Component (35%)**
| Metric | Weight | Excellent | Good | Fair | Poor | Unusable |
|--------|--------|-----------|------|------|------|----------|
| **RSRP** | 15% | ≥-80 dBm | ≥-90 dBm | ≥-100 dBm | ≥-110 dBm | <-120 dBm |
| **RSRQ** | 10% | ≥-8 dB | ≥-12 dB | ≥-15 dB | ≥-18 dB | <-25 dB |
| **SINR** | 10% | ≥20 dB | ≥10 dB | ≥5 dB | ≥0 dB | <-5 dB |

#### **Connectivity Component (40%)**
| Metric | Weight | Excellent | Good | Fair | Poor | Unusable |
|--------|--------|-----------|------|------|------|----------|
| **Latency** | 20% | ≤50 ms | ≤100 ms | ≤200 ms | ≤500 ms | >1000 ms |
| **Loss** | 15% | 0% | ≤1% | ≤3% | ≤7% | >15% |
| **Jitter** | 5% | ≤5 ms | ≤15 ms | ≤30 ms | ≤60 ms | >120 ms |

#### **Stability Component (15%)**
| Metric | Weight | Excellent | Good | Fair | Poor | Critical |
|--------|--------|-----------|------|------|------|----------|
| **Variance** | 8% | ≤2 dB | ≤4 dB | ≤6 dB | ≤8 dB | >8 dB |
| **Cell Changes** | 4% | 0 | 1 | ≤2 | ≤4 | >4 |
| **Throughput** | 3% | ≥5 Mbps | ≥2 Mbps | ≥1 Mbps | ≥500 Kbps | <500 Kbps |

#### **Quality Component (10%)**
| Factor | Weight | Bonus/Score |
|--------|--------|-------------|
| **5G Network** | 5% | +15 points |
| **LTE Network** | 5% | +10 points |
| **3G Network** | 5% | 0 points |
| **2G Network** | 5% | -20 points |
| **Band Quality** | 3% | Variable by band |
| **Modem Health** | 2% | 0-100% |

## 🔧 **Adaptive Monitoring Strategy**

### **Active vs Standby Interface Monitoring**
```go
// Active interface (primary) - Full monitoring
ProbeInterval: 5 seconds
ProbeTargets: ["8.8.8.8", "1.1.1.1", "208.67.222.222", "9.9.9.9"]
ProbesPerCycle: 3

// Standby interface - Reduced monitoring
ProbeInterval: 30 seconds  
ProbeTargets: ["8.8.8.8", "1.1.1.1"]
ProbesPerCycle: 2
```

### **Health-Based Adaptive Intervals**
```go
// Critical (Score < 30): Monitor every 1 second
// Degraded (Score 30-65): Monitor every 3 seconds  
// Healthy (Score > 65): Monitor every 10 seconds
```

### **mwan3 Integration Strategy**
```bash
# Priority 1: Use mwan3 tracking data if available
mwan3 status | grep "interface mob1s1a1"
mwan3 interfaces | grep -A 5 "mob1s1a1"

# Priority 2: Perform our own enhanced probing
# - Interface-specific routing
# - Multiple probe methods (ICMP, TCP, UDP)
# - Comprehensive target coverage
```

## 🎯 **Scoring Examples & Interpretations**

### **Excellent Connection (Score: 90-100)**
```json
{
  "total_score": 95,
  "grade": "A+",
  "recommendation": "Excellent - No action required",
  "signal_score": 33.0,    // RSRP: -85 dBm, RSRQ: -10 dB, SINR: 15 dB
  "connectivity_score": 38.0, // Latency: 45ms, Loss: 0%, Jitter: 3ms
  "stability_score": 14.0,    // Low variance, no handoffs, good throughput
  "quality_score": 10.0,     // 5G network, good band
  "bonuses": {"5g_network": 15},
  "penalties": {}
}
```

### **Degraded Connection (Score: 60-75)**
```json
{
  "total_score": 68,
  "grade": "C+",
  "recommendation": "Fair - Consider optimization",
  "signal_score": 24.5,      // RSRP: -105 dBm, RSRQ: -16 dB, SINR: 3 dB
  "connectivity_score": 28.0,   // Latency: 180ms, Loss: 2%, Jitter: 25ms
  "stability_score": 10.5,      // Some variance, 1 handoff
  "quality_score": 8.0,         // LTE network, average band
  "bonuses": {"lte_network": 10},
  "penalties": {"high_variance": -3}
}
```

### **Critical Connection (Score: 0-30)**
```json
{
  "total_score": 25,
  "grade": "F",
  "recommendation": "Critical - Consider failover",
  "signal_score": 8.5,       // RSRP: -118 dBm, RSRQ: -22 dB, SINR: -3 dB
  "connectivity_score": 5.0,    // Latency: 850ms, Loss: 12%, Jitter: 95ms
  "stability_score": 3.5,       // High variance, frequent handoffs
  "quality_score": 8.0,         // LTE network but poor performance
  "bonuses": {"lte_network": 10},
  "penalties": {
    "high_variance": -5,
    "frequent_handoffs": -3,
    "low_throughput": -2
  }
}
```

## ⚡ **Additional Factors to Consider**

Based on your question about what else to include in the formula, here are additional factors we could incorporate:

### **1. Environmental Factors**
- **Time of Day** - Network congestion patterns
- **Location Stability** - GPS-based location tracking
- **Weather Conditions** - Impact on signal propagation (if available)

### **2. Historical Performance**
- **Success Rate Trending** - 24-hour success rate history
- **Performance Consistency** - Standard deviation of performance over time
- **Recovery Speed** - How quickly connection recovers after issues

### **3. Data Usage Considerations**
- **Metered Connection Penalties** - Reduce probing frequency
- **Data Cap Proximity** - Factor in remaining data allowance
- **Cost Per MB** - Economic factor in failover decisions

### **4. Application-Specific Factors**
- **VoIP Quality** - Specific latency/jitter requirements for voice
- **Video Streaming** - Bandwidth consistency requirements
- **IoT/Sensor** - Reliability over performance requirements

### **5. Predictive Factors**
- **Signal Trend Direction** - Improving vs degrading trends
- **Failure Pattern Recognition** - Known problematic times/locations
- **Seasonal Adjustments** - Historical performance by time of year

## 🔧 **Configuration Examples**

### **Balanced Configuration (Default)**
```json
{
  "signal_weight": 0.35,
  "connectivity_weight": 0.40,
  "stability_weight": 0.15,
  "quality_weight": 0.10,
  "probe_interval": "5s",
  "standby_interval": "30s"
}
```

### **Latency-Sensitive Configuration (VoIP/Gaming)**
```json
{
  "signal_weight": 0.25,
  "connectivity_weight": 0.55,    // Increased focus on connectivity
  "stability_weight": 0.15,
  "quality_weight": 0.05,
  "latency_weight": 0.30,         // Higher latency importance
  "jitter_weight": 0.15,          // Higher jitter importance
  "probe_interval": "2s"          // More frequent monitoring
}
```

### **Reliability-Focused Configuration (IoT/Sensors)**
```json
{
  "signal_weight": 0.30,
  "connectivity_weight": 0.30,
  "stability_weight": 0.30,       // Increased stability focus
  "quality_weight": 0.10,
  "variance_weight": 0.15,        // Higher variance penalty
  "cell_change_weight": 0.10,     // Higher handoff penalty
  "probe_interval": "10s"         // Less frequent monitoring
}
```

## 🎉 **Benefits of Enhanced Formula**

### **1. Comprehensive Coverage**
- **Signal + Connectivity** - Both RF and network performance
- **Active + Standby** - Continuous monitoring of all interfaces
- **Multiple Methods** - mwan3, ICMP, TCP, UDP fallbacks
- **Real-Time + Predictive** - Current status + failure prediction

### **2. Intelligent Adaptation**
- **Interface-Aware** - Different strategies for active vs standby
- **Health-Responsive** - Monitoring frequency adapts to performance
- **Method-Flexible** - Multiple probe methods ensure data availability
- **Context-Sensitive** - Considers network type, band, and conditions

### **3. Production-Ready Reliability**
- **Fallback Strategies** - Multiple data collection methods
- **Error Handling** - Graceful degradation when methods fail
- **Resource Efficient** - Adaptive monitoring reduces unnecessary overhead
- **mwan3 Compatible** - Leverages existing RUTOS infrastructure

## 🔍 **Monitoring Commands**

### **Real-Time Enhanced Monitoring**
```bash
# Comprehensive cellular status with all factors
ubus call autonomy cellular_status | jq '
  .cellular[] | {
    interface: .interface,
    total_score: .assessment.score,
    signal: .current_signal.rsrp,
    latency: (.current_signal.latency // "N/A"),
    loss: (.assessment.packet_loss // "N/A"),
    recommendation: .recommendation
  }'

# Detailed scoring breakdown
ubus call autonomy cellular_analysis '{"interface":"mob1s1a1"}' | jq '.assessment'
```

### **Trend Analysis**
```bash
# Monitor scoring trends over time
while true; do
  echo "$(date): $(ubus call autonomy cellular_status | jq -r '.cellular.mob1s1a1.stability_score')"
  sleep 10
done
```

---

## 🎯 **Conclusion**

Your intuition about including latency and drop rate monitoring was absolutely correct! Our enhanced system now provides:

✅ **Signal Strength** (35%) - Same data as RUTOS GUI  
✅ **Connectivity Performance** (40%) - Latency, loss, jitter via multiple methods  
✅ **Stability Analysis** (15%) - Variance, handoffs, throughput consistency  
✅ **Quality Factors** (10%) - Network type, band quality, modem health  
✅ **mwan3 Integration** - Leverage existing tracking when available  
✅ **Standby Monitoring** - Continuous monitoring of non-primary interfaces  
✅ **Adaptive Intelligence** - Health-responsive monitoring frequency  
✅ **Predictive Analytics** - Trend analysis and failure prediction  

This creates the most comprehensive cellular monitoring system available, providing the intelligence needed for truly predictive failover that prevents users from ever noticing connectivity issues! 🚀
