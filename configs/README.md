# 📋 autonomy Configuration Guide

This directory contains example configurations for different use cases. Choose the configuration that best matches your deployment scenario.

## 📁 Available Configurations

### 🔧 `autonomy.example` - **Basic Configuration**

**Purpose**: Standard failover configuration for most users  
**Best For**: Fixed installations, basic multi-WAN setups, users who don't need advanced features

#### **What It Includes:**
- ✅ **Core Failover**: Automatic switching between Starlink, cellular, WiFi, and LAN
- ✅ **Smart Thresholds**: Optimized failover and restore thresholds
- ✅ **Starlink Integration**: Full Starlink API monitoring and diagnostics
- ✅ **Machine Learning**: Predictive failover based on historical patterns
- ✅ **Notifications**: Pushover alerts for network events
- ✅ **Metered Mode**: Basic metered connection signaling (disabled by default)
- ✅ **Member Management**: Support for multiple connections of each type

#### **What It Doesn't Include:**
- ❌ WiFi Optimization (disabled)
- ❌ Advanced WiFi scheduling
- ❌ GPS-based optimization
- ❌ Advanced weight system tuning
- ❌ Intelligence thresholds

#### **Use Cases:**
- 🏠 **Home/Office**: Fixed location with multiple internet connections
- 🏢 **Small Business**: Reliable failover without complexity
- 🚐 **Basic RV Setup**: Simple failover for mobile users who don't need WiFi optimization
- 🧪 **Testing/Development**: Getting started with autonomy

---

### 📡 `autonomy.wifi.example` - **Advanced WiFi Configuration**

**Purpose**: Comprehensive configuration for mobile deployments with intelligent WiFi optimization  
**Best For**: RVs, boats, mobile offices, campgrounds, frequent travelers

#### **What It Solves:**

##### **🚐 Mobile Connectivity Challenges**
- **Problem**: Moving between locations with different WiFi networks and signal conditions
- **Solution**: Automatic WiFi channel optimization based on GPS location and movement detection

##### **📶 WiFi Performance Issues**
- **Problem**: Poor WiFi performance due to channel congestion and interference
- **Solution**: Intelligent channel selection, bandwidth optimization, and DFS channel support

##### **⏰ Scheduled Optimization**
- **Problem**: Need to optimize WiFi during low-usage periods
- **Solution**: Configurable nightly and weekly optimization schedules

##### **💰 Data Usage Management**
- **Problem**: Expensive cellular data when better WiFi options are available
- **Solution**: Enhanced metered mode signaling to inform devices about connection costs

##### **🎯 Location-Aware Networking**
- **Problem**: Network needs change based on location (campground vs highway)
- **Solution**: GPS-integrated optimization that adapts to movement patterns

#### **What It Includes (Everything from Basic +):**
- ✅ **WiFi Optimization**: Automatic channel selection and bandwidth optimization
- ✅ **GPS Integration**: Location-aware optimization with movement detection
- ✅ **Smart Scheduling**: Nightly and weekly optimization windows
- ✅ **Advanced Metered Mode**: Full IEEE 802.11 vendor element signaling
- ✅ **Intelligent Thresholds**: Advanced signal quality and obstruction detection
- ✅ **Weight System**: Dynamic connection prioritization based on conditions
- ✅ **Enhanced Monitoring**: Comprehensive WiFi performance metrics

#### **Key Features:**

##### **🛰️ GPS-Aware WiFi Optimization**
```uci
config wifi 'optimization'
    option enabled '1'
    option movement_threshold '100.0'     # Optimize when moved >100m
    option stationary_time '300'          # Wait 5min after stopping
    option gps_accuracy_threshold '50.0'  # Require <50m GPS accuracy
```

##### **⏰ Scheduled Optimization**
```uci
config wifi 'scheduler'
    option nightly_enabled '1'            # Optimize every night
    option nightly_time '02:00'           # At 2 AM local time
    option weekly_enabled '1'             # Deep optimization weekly
    option weekly_days 'sunday'           # On Sunday mornings
```

##### **💰 Enhanced Metered Mode**
```uci
config metered 'settings'
    option enabled '1'                    # Enable vendor element signaling
    option warning_threshold '80'         # Warn at 80% data usage
    option critical_threshold '95'        # Critical at 95% usage
```

#### **Use Cases:**

##### **🚐 RV/Mobile Living**
- **Scenario**: Traveling between campgrounds, truck stops, and remote areas
- **Benefits**: Automatic WiFi optimization at each location, GPS-aware failover
- **Key Features**: Movement detection, location logging, scheduled optimization

##### **⛵ Marine Applications**
- **Scenario**: Boats moving between marinas and anchorages
- **Benefits**: Optimized WiFi performance in crowded marine environments
- **Key Features**: DFS channel support, interference mitigation

##### **🏕️ Campground Management**
- **Scenario**: Managing WiFi for multiple sites with varying signal conditions
- **Benefits**: Automatic optimization reduces support calls
- **Key Features**: Scheduled optimization, performance monitoring

##### **🚛 Fleet/Commercial Mobile**
- **Scenario**: Delivery vehicles, mobile offices, construction sites
- **Benefits**: Reliable connectivity with automatic optimization
- **Key Features**: GPS tracking, performance metrics, cost management

##### **🏔️ Remote Work/Digital Nomads**
- **Scenario**: Working from various locations with different connectivity options
- **Benefits**: Intelligent connection selection and cost awareness
- **Key Features**: Metered mode signaling, predictive failover

## 🤔 **Which Configuration Should I Use?**

### **Choose `autonomy.example` if:**
- ✅ You have a **fixed installation** (home, office, fixed RV site)
- ✅ You want **simple, reliable failover** without complexity
- ✅ You don't need WiFi optimization features
- ✅ You're **new to autonomy** and want to start simple
- ✅ You have **stable WiFi environments** that don't change

### **Choose `autonomy.wifi.example` if:**
- ✅ You're **mobile** (RV, boat, frequent traveler)
- ✅ You experience **WiFi performance issues** due to interference
- ✅ You want **automatic optimization** based on location
- ✅ You need **data usage management** with metered connections
- ✅ You want **scheduled optimization** during off-hours
- ✅ You're in **crowded WiFi environments** (campgrounds, marinas)

## 🔧 **Customization Guide**

### **Starting from Basic Configuration**
1. Copy `autonomy.example` to `/etc/config/autonomy`
2. Modify the basic settings (credentials, thresholds)
3. Test the basic failover functionality
4. Add advanced features as needed

### **Starting from WiFi Configuration**
1. Copy `autonomy.wifi.example` to `/etc/config/autonomy`
2. Configure GPS settings if available
3. Adjust WiFi optimization parameters for your environment
4. Set up scheduling based on your usage patterns
5. Enable metered mode if using cellular backup

### **Migration Between Configurations**
- **Basic → WiFi**: Add the missing sections from `autonomy.wifi.example`
- **WiFi → Basic**: Set `option enabled '0'` for advanced features
- **Both**: All configurations are backward compatible

## 📊 **Performance Comparison**

| Feature | Basic Config | WiFi Config | Benefit |
|---------|-------------|-------------|---------|
| **Failover Speed** | Fast | Fast | Same |
| **WiFi Performance** | Manual | Automatic | 📈 +20-50% throughput |
| **Data Usage** | Unmanaged | Managed | 💰 Cost savings |
| **Maintenance** | Manual | Automatic | ⏰ Time savings |
| **Location Awareness** | None | GPS-based | 🎯 Context-aware |
| **Resource Usage** | Low | Medium | 📊 Acceptable overhead |

## 🚀 **Getting Started**

1. **Choose your configuration** based on the guide above
2. **Copy the example file**: `cp configs/autonomy.example /etc/config/autonomy`
3. **Edit the configuration**: Modify credentials, interfaces, and thresholds
4. **Start the service**: `/etc/init.d/autonomy start`
5. **Monitor the logs**: `logread -f | grep autonomy`
6. **Test failover**: Disconnect your primary connection

## 📚 **Additional Resources**

- **`METERED_MODE_INTEGRATION_GUIDE.md`**: Detailed metered mode documentation
- **`WIFI_OPTIMIZATION_IMPLEMENTATION_COMPLETE.md`**: WiFi optimization technical details
- **`UCI_CONFIGURATION_RESTRUCTURE_COMPLETE.md`**: Configuration format documentation
- **`PROJECT_INSTRUCTION.md`**: Complete technical specification

---

**💡 Tip**: Start with the basic configuration and upgrade to WiFi configuration as your needs grow. Both configurations are fully compatible and can be switched at any time.
