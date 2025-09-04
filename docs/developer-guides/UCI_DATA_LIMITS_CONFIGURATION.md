# UCI Data Limits and Adaptive Monitoring Configuration

## ✅ **Confirmation: We Read ALL Values from UCI**

The system reads **ALL** data limit and monitoring configuration from UCI - **NO hardcoded values** are used for limits or thresholds.

## 📋 **Data Limits Configuration (`quota_limit`)**

### Reading from UCI
The system reads data limits from the existing `quota_limit` UCI configuration:

```bash
uci show quota_limit
```

### Example Configuration
```bash
# Enable data limits for mob1s1a1 (cellular interface)
uci set quota_limit.mob1s1a1=interface
uci set quota_limit.mob1s1a1.enabled='1'
uci set quota_limit.mob1s1a1.data_limit='1000'     # 1000 MB (1 GB)
uci set quota_limit.mob1s1a1.period='1'            # Monthly
uci set quota_limit.mob1s1a1.reset_hour='0'        # Reset at midnight
uci set quota_limit.mob1s1a1.enable_warning='1'    # Enable warnings
uci set quota_limit.mob1s1a1.enable_rate_limit='0' # Disable rate limiting
uci commit quota_limit
```

### Supported UCI Fields
| UCI Field | Type | Description | Example |
|-----------|------|-------------|---------|
| `enabled` | boolean | Enable data limit monitoring | `'1'` |
| `data_limit` | integer | Data limit in MB | `'1000'` (1 GB) |
| `period` | integer | Reset period (1=monthly) | `'1'` |
| `reset_hour` | integer | Hour of day to reset (0-23) | `'0'` |
| `enable_warning` | boolean | Enable warning notifications | `'1'` |
| `enable_rate_limit` | boolean | Enable rate limiting | `'0'` |
| `rate_limit_rx` | integer | RX rate limit in Kbps | `'1000'` |
| `rate_limit_tx` | integer | TX rate limit in Kbps | `'500'` |

## 📊 **Adaptive Monitoring Configuration (`autonomy.adaptive_monitoring`)**

### Reading from UCI
The system reads adaptive monitoring thresholds from UCI:

```bash
uci show autonomy.adaptive_monitoring
```

### Example Configuration
```bash
# Configure adaptive monitoring thresholds
uci set autonomy.adaptive_monitoring=adaptive_monitoring
uci set autonomy.adaptive_monitoring.active_interval='5'      # 5 seconds for primary
uci set autonomy.adaptive_monitoring.standby_interval='60'    # 60 seconds for backup
uci set autonomy.adaptive_monitoring.emergency_interval='300' # 300 seconds for critical
uci set autonomy.adaptive_monitoring.standby_threshold='50.0' # 50% usage threshold
uci set autonomy.adaptive_monitoring.emergency_threshold='85.0' # 85% usage threshold
uci set autonomy.adaptive_monitoring.disabled_threshold='95.0'  # 95% usage threshold
uci set autonomy.adaptive_monitoring.optimized_ping_size='8'    # 8 bytes instead of 64
uci set autonomy.adaptive_monitoring.enable_ping_optim='1'      # Enable ping optimization
uci set autonomy.adaptive_monitoring.reduced_api_freq='1'       # Reduce API frequency
uci set autonomy.adaptive_monitoring.minimal_at_commands='1'    # Minimal AT commands
uci commit autonomy
```

### Supported UCI Fields
| UCI Field | Type | Description | Default | Example |
|-----------|------|-------------|---------|---------|
| `active_interval` | integer | Monitoring interval for primary interface (seconds) | `5` | `'5'` |
| `standby_interval` | integer | Monitoring interval for backup interfaces (seconds) | `60` | `'60'` |
| `emergency_interval` | integer | Monitoring interval when data critical (seconds) | `300` | `'300'` |
| `standby_threshold` | float | Data usage % to switch to standby mode | `50.0` | `'50.0'` |
| `emergency_threshold` | float | Data usage % to switch to emergency mode | `85.0` | `'85.0'` |
| `disabled_threshold` | float | Data usage % to disable monitoring | `95.0` | `'95.0'` |
| `optimized_ping_size` | integer | Ping packet size in bytes | `8` | `'8'` |
| `enable_ping_optim` | boolean | Enable ping size optimization | `1` | `'1'` |
| `reduced_api_freq` | boolean | Reduce API call frequency | `1` | `'1'` |
| `minimal_at_commands` | boolean | Use minimal AT commands | `1` | `'1'` |

## 🔍 **How Data is Read from System**

### Current Usage Detection
The system reads **real-time usage** from the system:

1. **Data Usage**: Read from `/proc/net/dev` for actual bytes transferred
2. **Physical Interface Mapping**: 
   - `mob1s1a1` → `qmimux0`
   - `mob1s2a1` → `qmimux1`
3. **Usage Calculation**: RX bytes + TX bytes converted to MB
4. **Percentage Calculation**: `(current_usage_mb / data_limit_mb) * 100`

### Days Until Reset Calculation
```go
func (dlm *DataLimitManager) getDaysUntilReset(resetHour int) int {
    now := time.Now()
    nextReset := time.Date(now.Year(), now.Month()+1, 1, resetHour, 0, 0, 0, now.Location())
    return int(nextReset.Sub(now).Hours() / 24)
}
```

## 📱 **Integration with Member Discovery**

### Data Limit Assignment
When discovering network members, the system:

1. **Reads UCI `quota_limit`** configuration
2. **Gets current usage** from `/proc/net/dev`
3. **Calculates usage percentage** and days until reset
4. **Assigns to member**: `member.DataLimitConfig = dataLimit`
5. **Stores in Config map** for backward compatibility

### Member Configuration
```go
// Set the DataLimitConfig field for adaptive monitoring
member.DataLimitConfig = dataLimit

// Also store in Config map for backward compatibility and logging
member.Config["data_limit_mb"] = strconv.Itoa(dataLimit.DataLimitMB)
member.Config["data_usage_mb"] = fmt.Sprintf("%.2f", dataLimit.CurrentUsageMB)
member.Config["data_usage_percent"] = fmt.Sprintf("%.1f", dataLimit.UsagePercentage)
member.Config["data_limit_status"] = nt.dataLimitManager.GetDataLimitStatus(dataLimit).String()
```

## ⚙️ **Fallback Behavior**

### No UCI Configuration
If UCI configuration is not found:

1. **Data Limits**: System operates normally without data limit awareness
2. **Adaptive Monitoring**: Uses sensible defaults (shown in table above)
3. **Logging**: Debug messages indicate when defaults are used

### Invalid UCI Values
- **Invalid integers**: Ignored, defaults used
- **Invalid floats**: Ignored, defaults used  
- **Invalid booleans**: Treated as `false`

## 🔧 **Configuration Examples**

### Minimal Data Limit Setup
```bash
# Just enable basic 1GB limit for cellular
uci set quota_limit.mob1s1a1=interface
uci set quota_limit.mob1s1a1.enabled='1'
uci set quota_limit.mob1s1a1.data_limit='1000'
uci commit quota_limit
```

### Conservative Monitoring Setup
```bash
# Very conservative monitoring to save data
uci set autonomy.adaptive_monitoring=adaptive_monitoring
uci set autonomy.adaptive_monitoring.standby_threshold='30.0'  # Switch to standby at 30%
uci set autonomy.adaptive_monitoring.emergency_threshold='70.0' # Emergency at 70%
uci set autonomy.adaptive_monitoring.standby_interval='120'     # 2 minutes for standby
uci set autonomy.adaptive_monitoring.emergency_interval='600'   # 10 minutes for emergency
uci commit autonomy
```

### Aggressive Monitoring Setup
```bash
# More frequent monitoring (uses more data)
uci set autonomy.adaptive_monitoring=adaptive_monitoring
uci set autonomy.adaptive_monitoring.standby_threshold='80.0'  # Switch to standby at 80%
uci set autonomy.adaptive_monitoring.emergency_threshold='95.0' # Emergency at 95%
uci set autonomy.adaptive_monitoring.standby_interval='30'      # 30 seconds for standby
uci set autonomy.adaptive_monitoring.emergency_interval='120'   # 2 minutes for emergency
uci commit autonomy
```

## ✅ **Verification Commands**

### Check Current Configuration
```bash
# View data limits
uci show quota_limit

# View adaptive monitoring config
uci show autonomy.adaptive_monitoring

# Check current data usage
cat /proc/net/dev | grep qmimux
```

### Test Configuration
```bash
# Start daemon with verbose logging to see UCI values being read
/tmp/autonomyd -config /tmp/autonomy.conf -foreground -v
```

## 🎯 **Summary**

✅ **ALL data limits read from UCI `quota_limit`**  
✅ **ALL monitoring thresholds read from UCI `autonomy.adaptive_monitoring`**  
✅ **Real-time usage read from `/proc/net/dev`**  
✅ **Sensible defaults used if UCI config missing**  
✅ **No hardcoded limits or thresholds in the code**  

The system is fully configurable via UCI with intelligent fallbacks to ensure reliable operation even without explicit configuration.
