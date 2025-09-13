include $(TOPDIR)/rules.mk

# Load version information from centralized VERSION file
include $(CURDIR)/../../../../VERSION

PKG_NAME:=tlt-autonomy-complete
PKG_VERSION:=$(AUTONOMY_VERSION)
PKG_RELEASE:=$(AUTONOMY_VERSION_BUILD)

# Use local source instead of downloading
PKG_SOURCE_PROTO:=
PKG_SOURCE_URL:=
PKG_SOURCE_VERSION:=
PKG_SOURCE_SUBDIR:=
PKG_SOURCE:=

PKG_MAINTAINER:=Markus Lassfolk <markus@example.com>
PKG_LICENSE:=GPL-2.0
PKG_LICENSE_FILES:=LICENSE

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/tlt-autonomy-complete
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Telia Autonomy Complete System
  DEPENDS:=+libcurl +libjson-c +libubox +libubus +libuci +mwan3 +libsqlite3 +uhttpd +uhttpd-mod-ubus
  PROVIDES:=libcjson
  URL:=https://github.com/markus-lassfolk/autonomy
endef

define Package/tlt-autonomy-complete/description
  Complete Telia Autonomy Network Management System.
  Includes the intelligent daemon with enhanced web interface.
  Provides autonomous network management with predictive analytics,
  real-time monitoring, and comprehensive configuration capabilities.
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	# Copy daemon source
	$(CP) -r $(CURDIR)/../../../../src/c/autonomy-daemon/* $(PKG_BUILD_DIR)/
	# Create UI directory and copy UI source
	$(INSTALL_DIR) $(PKG_BUILD_DIR)/ui
	$(CP) -r $(CURDIR)/../../../../src/web/autonomy-ui/* $(PKG_BUILD_DIR)/ui/
endef

define Build/Configure
	# Configure build if needed
endef

define Build/Compile
	# Build the daemon
	$(MAKE) -C $(PKG_BUILD_DIR) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)"
	
	# Process and optimize UI files
	$(INSTALL_DIR) $(PKG_BUILD_DIR)/ui/processed
	
	# Copy and process HTML files
	$(CP) $(PKG_BUILD_DIR)/ui/enhanced-autonomy-dashboard.html $(PKG_BUILD_DIR)/ui/processed/index.html
	$(CP) $(PKG_BUILD_DIR)/ui/ml-analytics-dashboard.html $(PKG_BUILD_DIR)/ui/processed/ml-analytics.html
	
	# Copy and minify JavaScript files
	$(CP) $(PKG_BUILD_DIR)/ui/autonomy-dashboard.js $(PKG_BUILD_DIR)/ui/processed/autonomy-dashboard.js
	
	# Create VUCI integration files
	$(INSTALL_DIR) $(PKG_BUILD_DIR)/ui/processed/vuci
	$(CP) $(PKG_BUILD_DIR)/ui/ui/data/usr/share/vuci/menu.d/autonomy.json $(PKG_BUILD_DIR)/ui/processed/vuci/ || ( \
		printf '%s\n' '{' \
		'  "services/autonomy": {' \
		'    "title": "Autonomy",' \
		'    "index": 50,' \
		'    "view": "services/Autonomy",' \
		'    "acls": ["services/autonomy"]' \
		'  },' \
		'  "services/autonomy/overview": {' \
		'    "title": "Overview",' \
		'    "index": 1,' \
		'    "view": "services/AutonomyOverview",' \
		'    "acls": ["services/autonomy"]' \
		'  },' \
		'  "services/autonomy/configuration": {' \
		'    "title": "Configuration",' \
		'    "index": 2,' \
		'    "view": "services/AutonomyConfig",' \
		'    "acls": ["services/autonomy"]' \
		'  },' \
		'  "services/autonomy/status": {' \
		'    "title": "Status",' \
		'    "index": 3,' \
		'    "view": "services/AutonomyStatus",' \
		'    "acls": ["services/autonomy"]' \
		'  },' \
		'  "services/autonomy/logs": {' \
		'    "title": "Logs",' \
		'    "index": 4,' \
		'    "view": "services/AutonomyLogs",' \
		'    "acls": ["services/autonomy"]' \
		'  }' \
		'}' \
	) > $(PKG_BUILD_DIR)/ui/processed/vuci/autonomy.json
	
	# Create web server configuration
	$(INSTALL_DIR) $(PKG_BUILD_DIR)/ui/processed/config
	printf '#!/bin/sh\n# Autonomy UI Web Server Configuration\n\n# Enable UBUS support for API calls\nuci set uhttpd.main.ubus_prefix="/ubus"\nuci set uhttpd.main.ubus_timeout=30\n\n# Add Autonomy UI location\nuci add_list uhttpd.main.location="/autonomy"\nuci add_list uhttpd.main.location="/autonomy/"\n\n# Set document root for Autonomy UI\nuci set uhttpd.main.home="/www/autonomy"\n\n# Commit changes\nuci commit uhttpd\n\n# Restart uhttpd\n/etc/init.d/uhttpd restart\n' > $(PKG_BUILD_DIR)/ui/processed/config/autonomy-ui
	chmod +x $(PKG_BUILD_DIR)/ui/processed/config/autonomy-ui
endef

define Build/Install
	# Build/Install is handled by Package/install
endef

define Package/tlt-autonomy-complete/install
	# Install daemon binary
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/autonomy-daemon $(1)/usr/bin/
	
	# Install daemon init script
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/files/autonomy.init $(1)/etc/init.d/autonomy
	
	# Install daemon configuration
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/files/autonomy.config $(1)/etc/config/autonomy
	
	# Copy cJSON library from staging directory
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(STAGING_DIR)/usr/lib/libcjson.so* $(1)/usr/lib/ || true
	
	# Install web interface files
	$(INSTALL_DIR) $(1)/www/autonomy
	$(CP) $(PKG_BUILD_DIR)/ui/processed/index.html $(1)/www/autonomy/
	$(CP) $(PKG_BUILD_DIR)/ui/processed/ml-analytics.html $(1)/www/autonomy/
	$(CP) $(PKG_BUILD_DIR)/ui/processed/autonomy-dashboard.js $(1)/www/autonomy/
	
	# Install VUCI integration
	$(INSTALL_DIR) $(1)/usr/share/vuci/menu.d
	$(CP) $(PKG_BUILD_DIR)/ui/processed/vuci/autonomy.json $(1)/usr/share/vuci/menu.d/
	
	# Install UI configuration script
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ui/processed/config/autonomy-ui $(1)/etc/init.d/autonomy-ui
	
	# Create symlink for easy access
	$(INSTALL_DIR) $(1)/www
	$(LN) /www/autonomy/index.html $(1)/www/autonomy.html
	
	# Create combined startup script
	$(INSTALL_DIR) $(1)/usr/bin
	printf '#!/bin/sh\n# Autonomy Complete System Setup Script\n\necho "Setting up Autonomy Complete System..."\n\n# Start the daemon\n/etc/init.d/autonomy start\n\n# Configure web interface\n/etc/init.d/autonomy-ui\n\n# Set proper permissions\nchmod 644 /www/autonomy/*.html\nchmod 644 /www/autonomy/*.js\nchmod 644 /usr/share/vuci/menu.d/autonomy.json\n\n# Get router IP\nROUTER_IP=$(uci get network.lan.ipaddr 2>/dev/null || echo "192.168.1.1")\n\necho "Autonomy Complete System setup complete!"\necho ""\necho "Access the dashboard at:"\necho "  http://$ROUTER_IP/autonomy"\necho "  http://$ROUTER_IP/autonomy.html"\necho ""\necho "Or through VUCI menu: Services > Autonomy"\necho ""\necho "Daemon status: /etc/init.d/autonomy status"\necho "UI status: /etc/init.d/autonomy-ui status"\n' > $(1)/usr/bin/autonomy-setup
	chmod +x $(1)/usr/bin/autonomy-setup
endef

define Package/tlt-autonomy-complete/postinst
#!/bin/sh
# Post-installation script for Autonomy Complete System

# Run the setup script
/usr/bin/autonomy-setup

# Create backup of original uhttpd config
if [ ! -f /etc/config/uhttpd.backup ]; then
    cp /etc/config/uhttpd /etc/config/uhttpd.backup
fi

echo ""
echo "🎉 Autonomy Complete System installed successfully!"
echo ""
echo "📊 Dashboard: http://$(uci get network.lan.ipaddr 2>/dev/null || echo "192.168.1.1")/autonomy"
echo "🔧 VUCI Menu: Services > Autonomy"
echo "⚙️  Setup: /usr/bin/autonomy-setup"
echo ""
echo "📚 Features included:"
echo "  • Intelligent multi-interface failover"
echo "  • Real-time network monitoring"
echo "  • ML-powered predictions"
echo "  • Starlink integration"
echo "  • GPS tracking"
echo "  • Comprehensive web dashboard"
echo "  • VUCI integration"
echo ""
endef

define Package/tlt-autonomy-complete/prerm
#!/bin/sh
# Pre-removal script for Autonomy Complete System

# Stop services
/etc/init.d/autonomy stop
/etc/init.d/autonomy-ui stop

# Remove VUCI menu entry
rm -f /usr/share/vuci/menu.d/autonomy.json

# Remove web files
rm -rf /www/autonomy
rm -f /www/autonomy.html

# Remove setup script
rm -f /usr/bin/autonomy-setup

# Restore original uhttpd config if backup exists
if [ -f /etc/config/uhttpd.backup ]; then
    cp /etc/config/uhttpd.backup /etc/config/uhttpd
    uci commit uhttpd
    /etc/init.d/uhttpd restart
    rm -f /etc/config/uhttpd.backup
fi
endef

$(eval $(call BuildPackage,tlt-autonomy-complete))

