include $(TOPDIR)/rules.mk

# Load version information from centralized VERSION file
include $(CURDIR)/../../../../VERSION

PKG_NAME:=tlt-autonomy-ui
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

define Package/tlt-autonomy-ui
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Telia Autonomy Web UI
  DEPENDS:=+tlt-autonomy-daemon +uhttpd +uhttpd-mod-ubus
  URL:=https://github.com/markus-lassfolk/autonomy
endef

define Package/tlt-autonomy-ui/description
  Enhanced web interface for the Telia Autonomy Network Management Daemon.
  Provides comprehensive dashboard with real-time monitoring, configuration,
  and control capabilities for all daemon features.
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) -r $(CURDIR)/../../../../src/web/autonomy-ui/* $(PKG_BUILD_DIR)/
endef

define Build/Configure
	# Configure build if needed
endef

define Build/Compile
	# Process and optimize UI files
	$(INSTALL_DIR) $(PKG_BUILD_DIR)/processed
	
	# Copy and process HTML files
	$(CP) $(PKG_BUILD_DIR)/enhanced-autonomy-dashboard.html $(PKG_BUILD_DIR)/processed/index.html
	$(CP) $(PKG_BUILD_DIR)/ml-analytics-dashboard.html $(PKG_BUILD_DIR)/processed/ml-analytics.html
	
	# Copy and minify JavaScript files
	$(CP) $(PKG_BUILD_DIR)/autonomy-dashboard.js $(PKG_BUILD_DIR)/processed/autonomy-dashboard.js
	
	# Create VUCI integration files
	$(INSTALL_DIR) $(PKG_BUILD_DIR)/processed/vuci
	$(CP) $(PKG_BUILD_DIR)/ui/data/usr/share/vuci/menu.d/autonomy.json $(PKG_BUILD_DIR)/processed/vuci/
	
	# Create web server configuration
	$(INSTALL_DIR) $(PKG_BUILD_DIR)/processed/config
	cat > $(PKG_BUILD_DIR)/processed/config/autonomy-ui << 'EOF'
#!/bin/sh
# Autonomy UI Web Server Configuration

# Enable UBUS support for API calls
uci set uhttpd.main.ubus_prefix="/ubus"
uci set uhttpd.main.ubus_timeout=30

# Add Autonomy UI location
uci add_list uhttpd.main.location="/autonomy"
uci add_list uhttpd.main.location="/autonomy/"

# Set document root for Autonomy UI
uci set uhttpd.main.home="/www/autonomy"

# Commit changes
uci commit uhttpd

# Restart uhttpd
/etc/init.d/uhttpd restart
EOF
	
	chmod +x $(PKG_BUILD_DIR)/processed/config/autonomy-ui
endef

define Package/tlt-autonomy-ui/install
	# Install web interface files
	$(INSTALL_DIR) $(1)/www/autonomy
	$(CP) $(PKG_BUILD_DIR)/processed/index.html $(1)/www/autonomy/
	$(CP) $(PKG_BUILD_DIR)/processed/ml-analytics.html $(1)/www/autonomy/
	$(CP) $(PKG_BUILD_DIR)/processed/autonomy-dashboard.js $(1)/www/autonomy/
	
	# Install VUCI integration
	$(INSTALL_DIR) $(1)/usr/share/vuci/menu.d
	$(CP) $(PKG_BUILD_DIR)/processed/vuci/autonomy.json $(1)/usr/share/vuci/menu.d/
	
	# Install configuration script
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/processed/config/autonomy-ui $(1)/etc/init.d/autonomy-ui
	
	# Create symlink for easy access
	$(INSTALL_DIR) $(1)/www
	$(LN) /www/autonomy/index.html $(1)/www/autonomy.html
endef

define Package/tlt-autonomy-ui/postinst
#!/bin/sh
# Post-installation script for Autonomy UI

# Run configuration script
/etc/init.d/autonomy-ui

# Set proper permissions
chmod 644 /www/autonomy/*.html
chmod 644 /www/autonomy/*.js
chmod 644 /usr/share/vuci/menu.d/autonomy.json

# Create backup of original uhttpd config
if [ ! -f /etc/config/uhttpd.backup ]; then
    cp /etc/config/uhttpd /etc/config/uhttpd.backup
fi

echo "Autonomy UI installed successfully!"
echo "Access the dashboard at: http://$(uci get network.lan.ipaddr)/autonomy"
echo "Or directly at: http://$(uci get network.lan.ipaddr)/autonomy.html"
endef

define Package/tlt-autonomy-ui/prerm
#!/bin/sh
# Pre-removal script for Autonomy UI

# Remove VUCI menu entry
rm -f /usr/share/vuci/menu.d/autonomy.json

# Remove web files
rm -rf /www/autonomy
rm -f /www/autonomy.html

# Restore original uhttpd config if backup exists
if [ -f /etc/config/uhttpd.backup ]; then
    cp /etc/config/uhttpd.backup /etc/config/uhttpd
    uci commit uhttpd
    /etc/init.d/uhttpd restart
    rm -f /etc/config/uhttpd.backup
fi
endef

$(eval $(call BuildPackage,tlt-autonomy-ui))

