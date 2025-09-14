include $(TOPDIR)/rules.mk

# Load version information from centralized VERSION file
include $(CURDIR)/../../../../VERSION

PKG_NAME:=tlt-starlink-standalone
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

define Package/tlt-starlink-standalone
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Starlink Standalone Tracking Client
  DEPENDS:=+libcurl +libjson-c +libubox +libubus +libuci +libsqlite3
  URL:=https://github.com/markus-lassfolk/autonomy
endef

define Package/tlt-starlink-standalone/description
  Standalone Starlink satellite tracking and outage prediction client.
  Provides independent satellite tracking without requiring the full autonomy daemon.
  Features include TLE data management, obstruction analysis, and outage predictions.
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) -r $(CURDIR)/../../../../src/c/starlink-tracking/* $(PKG_BUILD_DIR)/
endef

define Build/Configure
	# Configure build if needed
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) -f Makefile.standalone \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)"
endef

define Package/tlt-starlink-standalone/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/starlink-standalone $(1)/usr/bin/
	
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/files/starlink-standalone.init $(1)/etc/init.d/starlink-standalone || true
	
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/files/starlink-standalone.config $(1)/etc/config/starlink-standalone || true
	
	# Create default configuration directory
	$(INSTALL_DIR) $(1)/etc/starlink-standalone
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/files/standalone.conf $(1)/etc/starlink-standalone/ || true
endef

$(eval $(call BuildPackage,tlt-starlink-standalone))
