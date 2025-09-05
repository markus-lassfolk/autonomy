include $(TOPDIR)/rules.mk

PKG_NAME:=autonomy-daemon
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/markus-lassfolk/autonomy.git
PKG_SOURCE_VERSION:=main
PKG_SOURCE_SUBDIR:=$(PKG_NAME)
PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz

PKG_MAINTAINER:=Markus Lassfolk <markus@example.com>
PKG_LICENSE:=GPL-2.0
PKG_LICENSE_FILES:=LICENSE

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/autonomy-daemon
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Autonomy Network Management Daemon
  DEPENDS:=+libcurl +libjson-c +libubox +libubus +libuci +mwan3
  URL:=https://github.com/markus-lassfolk/autonomy
endef

define Package/autonomy-daemon/description
  Intelligent multi-interface failover system for RUTOS routers.
  Provides autonomous network management with predictive analytics.
endef

define Build/Prepare
	$(call Build/Prepare/Default)
	$(CP) $(PKG_BUILD_DIR)/src/c/autonomy-daemon/* $(PKG_BUILD_DIR)/
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

define Package/autonomy-daemon/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/autonomy-daemon $(1)/usr/bin/
	
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/autonomy.init $(1)/etc/init.d/autonomy
	
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DATA) ./files/autonomy.config $(1)/etc/config/autonomy
endef

$(eval $(call BuildPackage,autonomy-daemon))



