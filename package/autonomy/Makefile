include $(TOPDIR)/rules.mk

PKG_NAME:=autonomy
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_MAINTAINER:=autonomy Team <autonomy@example.com>
PKG_LICENSE:=GPL-3.0-or-later
PKG_LICENSE_FILES:=LICENSE

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/autonomy/autonomy.git
PKG_SOURCE_VERSION:=HEAD
PKG_MIRROR_HASH:=skip

PKG_BUILD_DEPENDS:=golang/host
PKG_BUILD_PARALLEL:=1
PKG_USE_MIPS16:=0

GO_PKG:=github.com/autonomy/autonomy
GO_PKG_BUILD_PKG:=./cmd/autonomyd
GO_PKG_LDFLAGS:=-s -w
GO_PKG_LDFLAGS_X:=main.Version=$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/golang.mk

define Package/autonomy
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Autonomous Multi-Interface Failover Daemon
  URL:=https://github.com/autonomy/autonomy
  DEPENDS:=+ubus +ubox +libubox +libubus +libuci +libblobmsg-json +libjson-c
  USERID:=autonomy=1000:autonomy=1000
endef

define Package/autonomy/description
  Autonomous multi-interface failover daemon for OpenWrt/RutOS routers.
  Provides intelligent failover between multiple network interfaces including
  Starlink, cellular, and WiFi connections with advanced monitoring and
  predictive switching capabilities.
endef

define Package/autonomy/conffiles
/etc/config/autonomy
/etc/autonomy/
endef

define Package/autonomy/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/autonomyd $(1)/usr/bin/
	
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) files/autonomy.init $(1)/etc/init.d/autonomy
	
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) files/autonomy.config $(1)/etc/config/autonomy
	$(INSTALL_CONF) files/autonomy.watchdog.example $(1)/etc/config/autonomy.watchdog.example
	
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) files/autonomyctl $(1)/usr/bin/
	
	$(INSTALL_DIR) $(1)/usr/libexec
	$(INSTALL_BIN) files/autonomy-metrics.sh $(1)/usr/libexec/
	
	$(INSTALL_DIR) $(1)/etc/crontabs
	$(INSTALL_CONF) files/autonomy-cron $(1)/etc/crontabs/root
	
	$(INSTALL_DIR) $(1)/var/lib/autonomy
	$(INSTALL_DIR) $(1)/var/lib/autonomy/rrd
	$(INSTALL_DIR) $(1)/var/lib/autonomy/metrics
	
	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) files/99-autonomy-defaults $(1)/etc/uci-defaults/
endef

define Package/autonomy/postinst
#!/bin/sh
# Create autonomy user and group if they don't exist
if ! grep -q "^autonomy:" /etc/passwd; then
    echo "autonomy:x:1000:1000:autonomy:/var/lib/autonomy:/bin/false" >> /etc/passwd
fi
if ! grep -q "^autonomy:" /etc/group; then
    echo "autonomy:x:1000:" >> /etc/group
fi

# Create necessary directories
mkdir -p /var/lib/autonomy
mkdir -p /var/log/autonomy
chown autonomy:autonomy /var/lib/autonomy /var/log/autonomy
chmod 755 /var/lib/autonomy /var/log/autonomy

# Enable service by default
/etc/init.d/autonomy enable

# Show installation message
echo "Autonomy daemon installed successfully!"
echo "Configuration: /etc/config/autonomy"
echo "Logs: /var/log/autonomyd.log"
echo "Control: /etc/init.d/autonomy {start|stop|restart|status}"
echo ""
echo "Please review and configure /etc/config/autonomy before starting the service."
exit 0
endef

define Package/autonomy/prerm
#!/bin/sh
# Disable service before removal
/etc/init.d/autonomy disable
/etc/init.d/autonomy stop
exit 0
endef

define Package/autonomy/postrm
#!/bin/sh
# Clean up user and group if no other packages use them
if [ ! -f /etc/passwd ] || ! grep -q "^autonomy:" /etc/passwd; then
    exit 0
fi

# Check if any files are owned by autonomy user
if ! find / -user autonomy 2>/dev/null | grep -q .; then
    sed -i '/^autonomy:/d' /etc/passwd
    sed -i '/^autonomy:/d' /etc/group
fi
exit 0
endef

$(eval $(call GoBinPackage,autonomy))
$(eval $(call BuildPackage,autonomy))

