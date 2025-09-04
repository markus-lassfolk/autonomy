# 👥 User Guides

Comprehensive guides for users of the Autonomy intelligent network failover system.

## 🚀 Getting Started

### Essential Guides
- [Starlink Tracking Overview](starlink-tracking-overview.md) - Understanding satellite tracking and prediction
- [OpenWrt Support Status](openwrt-support-status.md) - **Important**: OpenWrt compatibility and limitations
- [Troubleshooting Guide](troubleshooting-guide.md) - Common issues and solutions

## 🔧 Feature Guides

### Network Optimization
- [WiFi Optimization Guide](wifi-optimization-guide.md) - Automatic WiFi channel optimization
  - *Why it's here*: Users need to understand how WiFi optimization works, configure settings, and troubleshoot WiFi issues
- [Cellular Monitoring Guide](cellular-monitoring-guide.md) - Cellular signal monitoring and optimization  
  - *Why it's here*: Users need to understand signal quality metrics, configure thresholds, and optimize cellular performance
- [Predictive Failover Guide](predictive-failover-guide.md) - Understanding how predictions work
  - *Why it's here*: Users need to understand why the system switches networks and how to configure prediction sensitivity

### Location and GPS
- [GPS System Guide](gps-system-guide.md) - Multi-source GPS and location services
  - *Why it's here*: Users need to configure GPS sources, understand location accuracy, and troubleshoot GPS issues
- [Location Services Guide](location-services-guide.md) - Location strategy and configuration
  - *Why it's here*: Users need to understand how location affects network decisions and configure location services
- [Intelligent Caching Guide](intelligent-caching-guide.md) - Understanding the caching system
  - *Why it's here*: Users benefit from understanding how caching improves performance and reduces API costs

### System Management
- [Decision Making Guide](decision-making-guide.md) - How the system makes network decisions
  - *Why it's here*: Users need to understand why certain networks are chosen and how to adjust decision weights
- [Security Features Guide](security-features-guide.md) - Security monitoring and configuration
  - *Why it's here*: Users need to configure security settings and understand security alerts
- [System Maintenance Guide](system-maintenance-guide.md) - Automated maintenance features
  - *Why it's here*: Users need to understand what maintenance the system performs and configure maintenance settings

### Alerts and Notifications  
- [Notification Setup Guide](notification-setup-guide.md) - Setting up alerts and notifications
  - *Why it's here*: Users need to configure Pushover, email, and other notification channels

### Configuration and Management
- [Advanced Configuration](advanced-configuration.md) - Complete UCI configuration reference
  - *Why it's here*: Users need detailed configuration options for advanced setups
- [Logging and Monitoring Guide](logging-and-monitoring-guide.md) - Understanding logs and monitoring
  - *Why it's here*: Users need to understand logging for troubleshooting and monitoring system health
- [Watchdog System Guide](watchdog-system-guide.md) - Understanding the watchdog and failsafe system
  - *Why it's here*: Users need to understand how the system protects itself and recovers from failures

## 🎯 Quick Reference

### Most Important for New Users
1. [Starlink Tracking Overview](starlink-tracking-overview.md) - Understand the main feature
2. [WiFi Optimization Guide](wifi-optimization-guide.md) - Configure WiFi optimization
3. [Notification Setup Guide](notification-setup-guide.md) - Set up alerts
4. [Troubleshooting Guide](troubleshooting-guide.md) - Fix common problems

### Advanced Users
1. [GPS System Guide](gps-system-guide.md) - Configure advanced GPS features
2. [Cellular Monitoring Guide](cellular-monitoring-guide.md) - Optimize cellular performance  
3. [Decision Making Guide](decision-making-guide.md) - Understand and tune decision logic
4. [Predictive Failover Guide](predictive-failover-guide.md) - Configure predictive features

## 💡 Why These Are User Guides

**Previous Classification Error**: I originally classified these as "developer guides" because they contain technical details and code examples.

**Corrected Understanding**: These are **user guides** because:
- Users need to **understand** how features work to configure them properly
- Users need to **troubleshoot** issues when they arise  
- Users need to **optimize** settings for their specific environment
- Users benefit from **understanding** system behavior, even if technical

**Real Developer Guides** should only cover:
- How to modify the source code
- How to build and package the system
- How to extend APIs and add new features
- Internal architecture for code contributors

## 📚 Related Documentation

- [Tutorials](../tutorials/) - Step-by-step setup guides
- [API Reference](../api-reference/) - Complete API documentation  
- [Developer Guides](../developer-guides/) - Code contribution and development
- [Deployment Guides](../deployment/) - Production deployment

---

**Philosophy**: If a user would benefit from reading it to better use, configure, or troubleshoot the system, it belongs in user guides - regardless of technical complexity.