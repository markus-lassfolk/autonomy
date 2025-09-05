include $(TOPDIR)/rules.mk

PKG_NAME:=starlink-tracking
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

define Package/starlink-tracking
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Starlink Satellite Tracking
  DEPENDS:=+libcurl +libjson-c +libubox +libubus +libuci
  URL:=https://github.com/markus-lassfolk/autonomy
endef

define Package/starlink-tracking/description
  Starlink satellite tracking and prediction system.
  Provides real-time satellite position and obstruction prediction.
endef

define Build/Prepare
	$(call Build/Prepare/Default)
	$(CP) $(PKG_BUILD_DIR)/src/c/starlink-tracking/* $(PKG_BUILD_DIR)/
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

define Package/starlink-tracking/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/starlink-tracker $(1)/usr/bin/
	
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/starlink-tracking.init $(1)/etc/init.d/starlink-tracking
	
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DATA) ./files/starlink-tracking.config $(1)/etc/config/starlink-tracking
endef

$(eval $(call BuildPackage,starlink-tracking))



