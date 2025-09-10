include $(TOPDIR)/rules.mk

# Load version information from centralized VERSION file
include $(CURDIR)/../../../../VERSION

PKG_NAME:=tlt-autonomy-daemon
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
# PKG_BUILD_DEPENDS:=cjson

include $(INCLUDE_DIR)/package.mk

define Package/tlt-autonomy-daemon
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Telia Autonomy Network Management Daemon
  DEPENDS:=+libcurl +libjson-c +libubox +libubus +libuci +mwan3 +libsqlite3
  PROVIDES:=libcjson
  URL:=https://github.com/markus-lassfolk/autonomy
endef

define Package/tlt-autonomy-daemon/description
  Intelligent multi-interface failover system for RUTOS routers.
  Provides autonomous network management with predictive analytics.
  Telia-specific version with enhanced features.
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) -r $(CURDIR)/../../../../src/c/autonomy-daemon/* $(PKG_BUILD_DIR)/
endef

define Build/Configure
	# Configure build if needed
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)"
endef

define Package/tlt-autonomy-daemon/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/autonomy-daemon $(1)/usr/bin/
	
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/files/autonomy.init $(1)/etc/init.d/autonomy
	
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/files/autonomy.config $(1)/etc/config/autonomy
	
	# Copy cJSON library from staging directory
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(STAGING_DIR)/usr/lib/libcjson.so* $(1)/usr/lib/ || true
endef

# Removed postinst script to avoid symlink loops
# Users can manually set LD_LIBRARY_PATH=/usr/local/usr/lib when running the daemon

$(eval $(call BuildPackage,tlt-autonomy-daemon))
