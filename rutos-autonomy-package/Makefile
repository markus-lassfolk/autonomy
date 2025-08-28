#
# Copyright (C) 2025 Teltonika
#

include $(TOPDIR)/rules.mk

PKG_NAME:=autonomy
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_LICENSE:=MIT
PKG_MAINTAINER:=autonomy Team <autonomy@example.com>

include $(INCLUDE_DIR)/package.mk

define Package/autonomy
        SECTION:=net
        CATEGORY:=Network
        TITLE:=Autonomous networking system
        DEPENDS:=+uci +mwan3 +ubus +luci-base +luci-compat
        ifeq (m, $(CONFIG_PACKAGE_autonomy))
                PKG_TLT_NAME:=autonomy
                PKG_ROUTER:=$(TLT_PLATFORM_NAME)
        endif
endef

define Package/autonomy/description
        Autonomous networking system for RUTOS devices
        Provides intelligent network failover, GPS tracking, and monitoring
        with Starlink integration and cellular failover capabilities.
        Includes web UI and comprehensive configuration management.
        Built with real ARM binaries for optimal performance.
endef

define Build/Compile
endef

define Package/autonomy/install
        # Install binaries
        $(INSTALL_DIR) $(1)/usr/local/bin
        $(INSTALL_BIN) ./files/usr/local/bin/autonomyd $(1)/usr/local/bin/
        $(INSTALL_BIN) ./files/usr/local/bin/autonomysysmgmt $(1)/usr/local/bin/
        
        # Install configuration files
        $(INSTALL_DIR) $(1)/usr/local/etc/autonomy
        $(CP) ./files/usr/local/etc/autonomy/* $(1)/usr/local/etc/autonomy/
        
        # Install web UI files
        $(INSTALL_DIR) $(1)/usr/local/share/autonomy-ui
        $(CP) ./files/usr/local/share/autonomy-ui/* $(1)/usr/local/share/autonomy-ui/
        
        # Install init script
        $(INSTALL_DIR) $(1)/etc/init.d
        $(INSTALL_BIN) ./files/etc/init.d/autonomy $(1)/etc/init.d/
        
        # Install LuCI web interface
        $(INSTALL_DIR) $(1)/usr/lib/lua/luci/controller/admin
        $(INSTALL_DATA) ./files/usr/lib/lua/luci/controller/admin/autonomy.lua $(1)/usr/lib/lua/luci/controller/admin/
        
        $(INSTALL_DIR) $(1)/usr/lib/lua/luci/model/cbi/admin_autonomy
        $(INSTALL_DATA) ./files/usr/lib/lua/luci/model/cbi/admin_autonomy/config.lua $(1)/usr/lib/lua/luci/model/cbi/admin_autonomy/
        
        $(INSTALL_DIR) $(1)/usr/lib/lua/luci/view/admin_autonomy
        $(INSTALL_DATA) ./files/usr/lib/lua/luci/view/admin_autonomy/overview.htm $(1)/usr/lib/lua/luci/view/admin_autonomy/
        
        # Install UCI defaults
        $(INSTALL_DIR) $(1)/etc/uci-defaults
        $(INSTALL_BIN) ./files/etc/uci-defaults/70-autonomy $(1)/etc/uci-defaults/
        
        # Install ACL configuration
        $(INSTALL_DIR) $(1)/usr/share/rpcd/acl.d
        $(INSTALL_DATA) ./files/usr/share/rpcd/acl.d/luci-app-autonomy.json $(1)/usr/share/rpcd/acl.d/
endef

define Package/autonomy/postinst
#!/bin/sh
# Post-installation script for autonomy package

# Set executable permissions
chmod +x /usr/local/bin/autonomyd
chmod +x /usr/local/bin/autonomysysmgmt

# Create necessary directories
mkdir -p /etc/autonomy
mkdir -p /var/log/autonomy
mkdir -p /var/lib/autonomy

# Set up default configuration if not exists
if [ ! -f /etc/config/autonomy ]; then
    cp /usr/local/etc/autonomy/autonomy.config /etc/config/autonomy
fi

# Run UCI defaults
/etc/uci-defaults/70-autonomy

# Restart LuCI to pick up new menu items
/etc/init.d/rpcd restart
/etc/init.d/uhttpd restart

# Enable and start service
/etc/init.d/autonomy enable
/etc/init.d/autonomy start

echo "Autonomy package installed successfully!"
echo "Web interface available at: http://your-router-ip/cgi-bin/luci/admin/autonomy"
echo "Configuration available at: /usr/local/etc/autonomy/"
exit 0
endef

define Package/autonomy/prerm
#!/bin/sh
# Pre-removal script for autonomy package

# Stop and disable service
/etc/init.d/autonomy stop
/etc/init.d/autonomy disable

# Remove from UCI tracking
uci -q delete ucitrack.@autonomy[-1] 2>/dev/null || true
uci commit ucitrack

# Restart LuCI
/etc/init.d/rpcd restart
/etc/init.d/uhttpd restart

echo "Autonomy service stopped and disabled."
exit 0
endef

$(eval $(call BuildPackage,autonomy))





