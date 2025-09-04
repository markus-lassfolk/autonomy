# ⚡ Quick Setup Reference

## 🚀 5-Minute Setup

### 1. Basic Installation
```bash
opkg update && opkg install autonomy
/etc/init.d/autonomy enable && /etc/init.d/autonomy start
```

### 2. Essential Configuration
```bash
# Enable basic features
uci set autonomy.main.enabled='1'
uci set autonomy.starlink.enabled='1'
uci set autonomy.gps.enabled='1'
uci commit autonomy && /etc/init.d/autonomy restart
```

### 3. Verify Setup
```bash
ubus call autonomy status
```

## 📋 Configuration Cheat Sheet

### Quick Commands
| Task | Command |
|------|---------|
| **Enable service** | `uci set autonomy.main.enabled='1'` |
| **Set log level** | `uci set autonomy.main.log_level='debug'` |
| **Enable Starlink** | `uci set autonomy.starlink.enabled='1'` |
| **Enable GPS** | `uci set autonomy.gps.enabled='1'` |
| **Set Pushover token** | `uci set autonomy.notifications.pushover_token='TOKEN'` |
| **Commit changes** | `uci commit autonomy` |
| **Restart service** | `/etc/init.d/autonomy restart` |
| **Check status** | `ubus call autonomy status` |

### Essential APIs Setup
```bash
# Space-Track (for Starlink tracking)
uci set autonomy.external_apis.space_track_username='your_username'
uci set autonomy.external_apis.space_track_password='your_password'

# Pushover (for notifications) 
uci set autonomy.notifications.pushover_token='your_app_token'
uci set autonomy.notifications.pushover_user='your_user_key'

# OpenCellID (for cellular location)
uci set autonomy.external_apis.opencellid_api_key='your_api_key'

uci commit autonomy
```

## 🎯 Common Scenarios

### Scenario 1: Starlink + Cellular Backup
```bash
uci set autonomy.main.enabled='1'
uci set autonomy.starlink.enabled='1'
uci set autonomy.cellular.enabled='1'
uci set autonomy.interfaces.starlink_priority='1'
uci set autonomy.interfaces.cellular_priority='2'
uci commit autonomy
```

### Scenario 2: Full Feature Setup
```bash
uci set autonomy.main.enabled='1'
uci set autonomy.starlink.enabled='1'
uci set autonomy.cellular.enabled='1'
uci set autonomy.wifi.optimization='1'
uci set autonomy.gps.enabled='1'
uci set autonomy.predictive.enabled='1'
uci set autonomy.notifications.pushover_enabled='1'
uci commit autonomy
```

### Scenario 3: Minimal Setup (No External APIs)
```bash
uci set autonomy.main.enabled='1'
uci set autonomy.starlink.enabled='1'
uci set autonomy.cellular.enabled='1'
uci set autonomy.gps.enabled='0'
uci set autonomy.predictive.enabled='0'
uci set autonomy.notifications.pushover_enabled='0'
uci commit autonomy
```

## 🔍 Quick Troubleshooting

### Check Service Status
```bash
/etc/init.d/autonomy status      # Service status
ubus call autonomy status        # System health
logread | grep autonomy | tail   # Recent logs
```

### Common Fixes
```bash
# Restart service
/etc/init.d/autonomy restart

# Reload configuration
ubus call autonomy reload_config

# Reset to defaults
autonomy-cli config reset

# Validate configuration
autonomy-cli config validate
```

### API Testing
```bash
# Test Starlink connection
grpcurl -plaintext 192.168.100.1:9200 list

# Test cellular signal
gsmctl -A AT+CSQ

# Test GPS
gpsctl -i
```

## 📱 Mobile App Setup

### Pushover Mobile App
1. **Download**: Install Pushover from App Store/Play Store
2. **Login**: Use your Pushover account
3. **Test**: Send test notification from web interface

### Configuration Check
```bash
# Test Pushover
curl -X POST https://api.pushover.net/1/messages.json \
  -d "token=YOUR_TOKEN" \
  -d "user=YOUR_USER" \
  -d "message=Autonomy test notification"
```

## 📚 Next Steps

- **Complete Setup**: [Configuration Guide](configuration-guide.md)
- **API Setup**: [API Integrations Guide](api-integrations-guide.md)  
- **Advanced Features**: [User Guide](USER_GUIDE.md)
- **Production Deploy**: [Production Guide](rutx50-production-guide.md)

---

**Need help?** See the [Troubleshooting Guide](../developer-guides/TROUBLESHOOTING.md) or check the logs with `logread | grep autonomy`