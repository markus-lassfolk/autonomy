# Predictive Obstruction Management System

## Overview

The Predictive Obstruction Management System is an advanced feature that provides proactive failover capabilities for Starlink connections by predicting obstruction events before they cause complete signal loss. This system uses machine learning techniques, trend analysis, and environmental pattern recognition to trigger failover decisions before users experience service interruption.

## Key Features

### 1. **Proactive Failover Logic**
- Triggers failover before complete signal loss occurs
- Reduces user-perceived downtime by switching connections preemptively
- Maintains service continuity during predicted obstruction events

### 2. **Obstruction Acceleration Detection**
- Monitors rapid increases in obstruction percentage
- Detects acceleration patterns that indicate worsening conditions
- Configurable thresholds for early warning triggers

### 3. **SNR Trend Analysis**
- Tracks Signal-to-Noise Ratio degradation over time
- Provides early warning when signal quality is declining
- Uses statistical analysis to predict future signal levels

### 4. **Movement-Triggered Obstruction Map Refresh**
- Detects device movement using GPS data
- Automatically refreshes obstruction predictions when location changes
- Adapts to new environmental conditions in real-time

### 5. **Environmental Pattern Learning**
- Learns from historical obstruction patterns
- Recognizes location-specific and time-based obstruction events
- Builds a knowledge base of environmental conditions

### 6. **Multi-Factor Obstruction Assessment**
- Combines multiple data sources for comprehensive analysis
- Weighs signal strength, obstruction percentage, and data quality
- Provides confidence scoring for prediction accuracy

### 7. **False Positive Reduction**
- Uses data quality validation to filter unreliable measurements
- Implements hysteresis to prevent flapping between states
- Considers measurement validity and patch quality

## Architecture

### Core Components

#### 1. **ObstructionPredictor** (`pkg/obstruction/predictor.go`)
The main prediction engine that analyzes obstruction trends and determines when to trigger failover.

**Key Features:**
- Ring buffer for storing recent obstruction samples
- Linear regression for trend calculation
- Configurable thresholds for prediction sensitivity
- Confidence scoring based on data quality

**Configuration Options:**
```go
type PredictorConfig struct {
    MaxSamples                   int           // Ring buffer size (default: 300)
    MinSamplesForAnalysis        int           // Minimum samples needed (default: 10)
    CriticalObstructionThreshold float64       // Obstruction % trigger (default: 0.15)
    CriticalSNRThreshold         float64       // SNR threshold (default: 8.0 dB)
    AccelerationThreshold        float64       // Rate of change trigger (default: 0.02)
    PredictionWindow             time.Duration // Prediction horizon (default: 30s)
    ConfidenceThreshold          float64       // Minimum confidence (default: 0.7)
}
```

#### 2. **TrendAnalyzer** (`pkg/obstruction/trend_analyzer.go`)
Advanced statistical analysis engine for detecting patterns and trends in obstruction data.

**Capabilities:**
- Linear regression analysis with R-squared correlation
- Anomaly detection using standard deviation thresholds
- Seasonal pattern recognition using autocorrelation
- Confidence interval calculations for predictions
- Multi-metric trend analysis (obstruction, SNR, latency)

**Analysis Types:**
- **Trend Direction**: Increasing, decreasing, or stable
- **Trend Strength**: Weak, moderate, or strong based on correlation
- **Anomaly Detection**: Identifies outliers beyond 2σ threshold
- **Seasonal Patterns**: Detects recurring patterns with configurable periods

#### 3. **PatternLearner** (`pkg/obstruction/pattern_learner.go`)
Machine learning component that builds a knowledge base of environmental obstruction patterns.

**Learning Capabilities:**
- Location-based pattern recognition using GPS coordinates
- Time-based patterns (daily, weekly, seasonal)
- Environmental condition correlation
- Pattern confidence scoring and validation
- Automatic pattern expiry and cleanup

**Pattern Types:**
```go
type EnvironmentalPattern struct {
    Location        *LocationInfo          // Geographic pattern area
    TimePattern     *TimePattern           // Temporal occurrence pattern
    WeatherPattern  *WeatherPattern        // Weather correlation (future)
    ObstructionData *ObstructionSignature  // Obstruction characteristics
    Confidence      float64                // Pattern reliability (0-1)
    SampleCount     int                    // Number of observations
}
```

#### 4. **MovementDetector** (`pkg/obstruction/movement_detector.go`)
GPS-based movement detection system that triggers obstruction map updates when location changes.

**Movement Detection:**
- Haversine distance calculation for accurate positioning
- Speed and bearing calculation from GPS data
- Configurable movement thresholds and timeouts
- Movement state tracking with callbacks

**Key Thresholds:**
- **Minimum Movement Distance**: 10 meters
- **Movement Speed Threshold**: 1.0 m/s
- **Stationary Time Required**: 2 minutes
- **Significant Distance**: 50 meters (triggers map refresh)

#### 5. **PatternMatcher** (`pkg/obstruction/pattern_matcher.go`)
Real-time pattern matching engine that compares current conditions against learned patterns.

**Matching Algorithms:**
- Location similarity using Haversine distance
- Time pattern matching with configurable tolerance
- Obstruction signature comparison with weighted scoring
- Multi-factor similarity calculation

**Matching Weights:**
- Location similarity: 30%
- Time pattern similarity: 30%
- Obstruction signature: 40%

## Integration Points

### 1. **Starlink Collector Integration**
The predictive system is integrated into the Starlink collector (`pkg/collector/starlink.go`) to analyze real-time obstruction data.

**Integration Features:**
- Automatic sample collection during metrics gathering
- Data quality assessment for reliable predictions
- Predictive failover flag injection into metrics
- GPS location updates for movement detection

### 2. **Decision Engine Integration**
The decision engine (`pkg/decision/engine.go`) checks for predictive failover triggers during normal decision processing.

**Decision Logic:**
```go
// Check for predictive obstruction failover trigger
if metrics.PredictiveFailover != nil && *metrics.PredictiveFailover {
    reason := "unknown"
    if metrics.PredictiveReason != nil {
        reason = *metrics.PredictiveReason
    }
    e.logger.Info("Predictive obstruction failover triggered", "reason", reason)
    return true
}
```

### 3. **Metrics Extension**
New fields added to the `pkg.Metrics` struct to support predictive failover:

```go
// Predictive Obstruction Management
PredictiveFailover *bool   `json:"predictive_failover,omitempty"` // Trigger flag
PredictiveReason   *string `json:"predictive_reason,omitempty"`   // Reason description
```

## Configuration

### Global Configuration
Predictive obstruction management can be enabled/disabled via collector configuration:

```json
{
    "predictive_enabled": true,
    "starlink_host": "192.168.100.1",
    "starlink_port": 9200
}
```

### Component-Specific Configuration
Each component has its own configuration structure with sensible defaults:

#### ObstructionPredictor Configuration
```go
config := &obstruction.PredictorConfig{
    MaxSamples:                   300,    // 5 minutes at 1s intervals
    MinSamplesForAnalysis:        10,     // Minimum data points
    CriticalObstructionThreshold: 0.15,   // 15% obstruction threshold
    CriticalSNRThreshold:         8.0,    // 8 dB SNR threshold
    AccelerationThreshold:        0.02,   // 2% per sample acceleration
    PredictionWindow:             30 * time.Second,
    ConfidenceThreshold:          0.7,    // 70% confidence required
}
```

#### TrendAnalyzer Configuration
```go
config := &obstruction.TrendAnalyzerConfig{
    MaxHistoryPoints:     1440,  // 24 hours at 1-minute intervals
    MinPointsForAnalysis: 10,    // Minimum points for trends
    AnalysisWindow:       30 * time.Minute,
    PredictionHorizon:    5 * time.Minute,
    AnomalyThreshold:     2.0,   // 2 standard deviations
    SeasonalMinPeriod:    10 * time.Minute,
    SeasonalMaxPeriod:    24 * time.Hour,
}
```

## Prediction Algorithms

### 1. **Obstruction Acceleration Detection**
Uses linear regression to calculate the rate of change in obstruction percentage:

```go
func (op *ObstructionPredictor) calculateObstructionAcceleration() float64 {
    // Use last 20 samples for acceleration calculation
    recentSamples := op.samples[max(0, n-20):]
    
    // Calculate slope using least squares regression
    slope := (n*sumXY - sumX*sumY) / (n*sumX2 - sumX*sumX)
    return slope
}
```

### 2. **SNR Trend Analysis**
Similar regression analysis for Signal-to-Noise Ratio trends:

```go
func (op *ObstructionPredictor) calculateSNRTrend() float64 {
    // Linear regression on SNR values over time
    // Returns rate of change in dB per sample
}
```

### 3. **Future Value Prediction**
Simple linear extrapolation with bounds checking:

```go
func (op *ObstructionPredictor) predictFutureObstruction() float64 {
    current := op.samples[len(op.samples)-1].FractionObstructed
    acceleration := op.calculateObstructionAcceleration()
    
    predicted := current + acceleration
    return math.Max(0, math.Min(1, predicted)) // Clamp to [0,1]
}
```

### 4. **Confidence Calculation**
Multi-factor confidence scoring:

```go
func (op *ObstructionPredictor) calculateConfidence() float64 {
    confidence := 0.0
    
    // Data quality (40% weight)
    dataQuality := calculateDataQuality(recent)
    confidence += dataQuality * 0.4
    
    // Sample count (30% weight)
    sampleConfidence := min(sampleCount/maxSamples, 1.0)
    confidence += sampleConfidence * 0.3
    
    // Trend consistency (30% weight)
    trendConsistency := calculateTrendConsistency()
    confidence += trendConsistency * 0.3
    
    return confidence
}
```

## Trigger Conditions

### Primary Triggers
1. **Rapid Obstruction Increase**: Acceleration > 2% per sample
2. **Predicted Obstruction Threshold**: Future obstruction > 15%
3. **SNR Degradation**: Predicted SNR < 8.0 dB
4. **Time to Failure**: Predicted failure within 30 seconds

### Secondary Triggers
1. **Pattern Match**: Current conditions match known problematic patterns
2. **Movement Detection**: Significant location change detected
3. **Data Quality**: Sufficient confidence in predictions (>70%)

### Trigger Logic
```go
func (op *ObstructionPredictor) ShouldTriggerFailover(ctx context.Context) (bool, string, error) {
    trend, err := op.AnalyzeTrends(ctx)
    if err != nil {
        return false, "", err
    }

    // Check confidence threshold
    if trend.Confidence < op.config.ConfidenceThreshold {
        return false, "insufficient confidence", nil
    }

    // Check individual trigger conditions
    if trend.ObstructionAcceleration > op.accelerationThreshold {
        return true, "rapid obstruction increase detected", nil
    }
    
    if trend.PredictedObstruction > op.criticalObstructionThreshold {
        return true, "predicted obstruction exceeds threshold", nil
    }
    
    // ... additional checks
}
```

## Data Quality Assessment

### Quality Factors
1. **GPS Validity**: Contributes 30% to quality score
2. **Valid Measurement Duration**: Contributes 30% to quality score
3. **Measurement Patches**: Contributes 20% to quality score
4. **SNR Availability**: Contributes 20% to quality score

### Quality Calculation
```go
func (sc *StarlinkCollector) calculateDataQuality(metrics *pkg.Metrics) float64 {
    quality := 0.0
    
    if metrics.GPSValid != nil && *metrics.GPSValid {
        quality += 0.3
    }
    
    if metrics.ObstructionValidS != nil {
        validScore := float64(*metrics.ObstructionValidS) / 300.0 // 5 minutes
        quality += math.Min(validScore, 1.0) * 0.3
    }
    
    if metrics.ObstructionPatchesValid != nil {
        patchScore := float64(*metrics.ObstructionPatchesValid) / 100.0
        quality += math.Min(patchScore, 1.0) * 0.2
    }
    
    if metrics.SNR != nil && *metrics.SNR > 0 {
        quality += 0.2
    }
    
    return quality
}
```

## Performance Characteristics

### Memory Usage
- **ObstructionPredictor**: ~24KB (300 samples × 80 bytes/sample)
- **TrendAnalyzer**: ~115KB (1440 history points × 80 bytes/point)
- **PatternLearner**: Variable (up to 100 patterns × ~1KB/pattern)
- **MovementDetector**: ~8KB (100 location points × 80 bytes/point)

### CPU Usage
- **Prediction Analysis**: ~1-2ms per sample (O(n) complexity)
- **Trend Analysis**: ~5-10ms per analysis window (O(n log n) for regression)
- **Pattern Matching**: ~10-20ms per match attempt (O(p×m) where p=patterns, m=metrics)

### Storage Requirements
- **Pattern Persistence**: ~100KB for typical pattern database
- **Trend History**: ~1MB for 24-hour trend data across all metrics

## Monitoring and Observability

### Status APIs
The system provides comprehensive status information through the Starlink collector:

```go
func (sc *StarlinkCollector) GetPredictiveStatus() map[string]interface{} {
    return map[string]interface{}{
        "enabled":                true,
        "last_predictive_check":  sc.lastPredictiveCheck,
        "obstruction_predictor":  sc.obstructionPredictor.GetStatus(),
        "trend_analyzer":         sc.trendAnalyzer.GetStatus(),
        "pattern_learner":        sc.patternLearner.GetStatus(),
        "movement_detector":      sc.movementDetector.GetStatus(),
        "pattern_matcher":        sc.patternMatcher.GetStatus(),
    }
}
```

### Logging
The system provides detailed logging at multiple levels:

- **Info Level**: Prediction triggers, pattern matches, movement detection
- **Debug Level**: Sample additions, trend calculations, confidence scores
- **Warn Level**: Configuration issues, data quality problems

### Metrics Integration
Predictive events are integrated into the existing metrics and telemetry system:

- Predictive failover events are logged in decision audit trail
- Pattern learning statistics are available via status APIs
- Trend analysis results are exposed for monitoring

## Future Enhancements

### Planned Features
1. **Weather Integration**: Correlate obstruction patterns with weather data
2. **Satellite Constellation Tracking**: Predict obstructions based on satellite positions
3. **Machine Learning Models**: Advanced ML algorithms for pattern recognition
4. **Cloud Synchronization**: Share patterns across multiple devices
5. **Predictive Routing**: Route optimization based on predicted conditions

### Configuration Improvements
1. **Dynamic Thresholds**: Adaptive thresholds based on historical performance
2. **Location-Specific Tuning**: Different parameters for different environments
3. **Time-Based Configuration**: Different settings for different times of day

### Performance Optimizations
1. **Incremental Analysis**: Only analyze changed data points
2. **Parallel Processing**: Multi-threaded analysis for large datasets
3. **Memory Optimization**: Compressed storage for historical data

## Troubleshooting

### Common Issues

#### 1. **Insufficient Data for Predictions**
**Symptoms**: Predictions not triggering, low confidence scores
**Solutions**: 
- Reduce `MinSamplesForAnalysis` threshold
- Check GPS validity and data quality
- Verify Starlink API connectivity

#### 2. **False Positive Predictions**
**Symptoms**: Frequent unnecessary failovers
**Solutions**:
- Increase `ConfidenceThreshold`
- Adjust `AccelerationThreshold` to be less sensitive
- Review data quality assessment

#### 3. **Missed Obstruction Events**
**Symptoms**: Obstructions not predicted, reactive failovers
**Solutions**:
- Decrease prediction thresholds
- Increase sampling frequency
- Check for GPS movement detection issues

#### 4. **High Memory Usage**
**Symptoms**: Excessive memory consumption
**Solutions**:
- Reduce `MaxSamples` and `MaxHistoryPoints`
- Implement pattern cleanup more aggressively
- Monitor pattern learning database size

### Debug Information

#### Enable Debug Logging
```go
logger := logx.NewLogger("debug", "obstruction")
predictor := obstruction.NewObstructionPredictor(logger, config)
```

#### Status Monitoring
```bash
# Check predictive system status
curl -s "http://localhost:8080/api/starlink/predictive/status" | jq .

# Monitor prediction triggers
tail -f /var/log/autonomy/autonomy.log | grep "PREDICTIVE FAILOVER"
```

## Security Considerations

### Data Privacy
- GPS location data is processed locally only
- No external data transmission for pattern learning
- Pattern data can be optionally encrypted at rest

### System Security
- Prediction thresholds prevent malicious trigger manipulation
- Confidence requirements prevent low-quality data exploitation
- Rate limiting prevents prediction spam

## Conclusion

The Predictive Obstruction Management System represents a significant advancement in proactive network failover technology. By combining real-time signal analysis, machine learning pattern recognition, and environmental awareness, the system provides users with seamless connectivity even in challenging RF environments.

The modular architecture allows for easy customization and future enhancements, while the comprehensive monitoring and debugging capabilities ensure reliable operation in production environments.

For technical support or feature requests, please refer to the project documentation or submit issues through the appropriate channels.
