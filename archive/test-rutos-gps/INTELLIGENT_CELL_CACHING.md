# Intelligent Cell Location Caching

## Overview

The intelligent cell location caching system implements smart triggers for querying OpenCellID, reducing API usage while maintaining location accuracy. Instead of time-based intervals, it monitors the cellular environment and only queries when meaningful changes occur.

## Trigger Logic

The system queries OpenCellID for a new location when:

1. **Serving Cell Change**: The primary cell tower changes (different Cell ID)
2. **Significant Tower Changes**: ≥35% of neighbor towers differ from the last fix
3. **Top Tower Changes**: ≥2 of the top-5 strongest towers have changed
4. **Cache Expiration**: As a fallback, if no changes occur for 1 hour

All triggers are debounced by 10 seconds to prevent excessive queries during rapid changes.

## Configuration

Default settings:
- **Max Cache Age**: 1 hour (fallback)
- **Debounce Delay**: 10 seconds
- **Tower Change Threshold**: 35%
- **Top Towers Monitored**: 5

These can be customized via `SetCacheConfiguration()`.

## Usage

```go
// Create service
service := NewSmartCellLocationService(apiKey)

// Customize settings (optional)
service.SetCacheConfiguration(
    30*time.Minute, // Cache for 30 minutes
    5*time.Second,  // 5 second debounce
    0.30,           // 30% change threshold
    3,              // Monitor top 3 towers
)

// Get location (automatically decides whether to query or use cache)
location, err := service.GetLocation(cellularIntelligence, gpsReference)
```

## Benefits

1. **Reduced API Usage**: Only queries when environment actually changes
2. **Better Accuracy**: Updates location when cellular environment suggests movement
3. **Intelligent Fallback**: Still updates periodically even without changes
4. **Debounce Protection**: Prevents rapid-fire queries during transitions

## Implementation Details

### Environment Comparison

The system tracks:
- **Serving Cell**: Primary cell tower (Cell ID, signal strength)
- **Neighbor Cells**: All visible towers with signal measurements
- **Signal Quality**: RSRP, RSRQ values for comparison

### Change Detection

1. **Tower Change Percentage**: 
   - Calculates union of all towers (current + previous)
   - Counts towers that appear/disappear
   - Triggers if change ≥ threshold

2. **Top Tower Analysis**:
   - Sorts towers by signal strength (RSRP)
   - Compares top N strongest towers
   - Triggers if ≥2 of top towers changed

### Debouncing

- Prevents queries within debounce window after a trigger
- Allows environment to stabilize during transitions
- Configurable delay (default: 10 seconds)

## Testing

Run the intelligent caching test:

```bash
go run . -test-smart-cell
```

This demonstrates:
- Initial query (no previous data)
- Cache usage (same environment)
- Serving cell change trigger
- Debounce behavior
- Major neighbor change trigger
- Detailed cache status reporting

## Integration with autonomy

The smart cell location service can be integrated into the autonomy daemon as a fourth GPS source:

1. **Primary**: Quectel GPS (multi-GNSS)
2. **Secondary**: gpsctl (external antenna)
3. **Tertiary**: Starlink GPS
4. **Quaternary**: Smart Cell Location (OpenCellID with intelligent caching)

The cell location provides coarse positioning when GPS sources fail, with minimal API usage through intelligent caching.

## API Usage Optimization

With intelligent caching, typical usage patterns:

- **Stationary**: 1 query per hour (cache expiration only)
- **Moving**: Queries only when cellular environment changes
- **Transitioning**: Debounced to prevent excessive queries
- **Daily Limit**: Well within 5,000 requests/day for normal usage

This approach balances location accuracy with API efficiency, making it suitable for production deployment in the autonomy system.
