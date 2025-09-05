include $(TOPDIR)/rules.mk

# Load version information from centralized VERSION file
include $(CURDIR)/../VERSION

PKG_NAME:=autonomy-api
PKG_VERSION:=$(AUTONOMY_VERSION)
PKG_RELEASE:=$(AUTONOMY_VERSION_BUILD)

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

define Package/autonomy-api
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Autonomy API (Lua/RPCD)
  DEPENDS:=+libubox +libubus +libuci +rpcd +rpcd-mod-luci
  URL:=https://github.com/markus-lassfolk/autonomy
endef

define Package/autonomy-api/description
  Lua-based API for Autonomy system integration with RPCD.
  Provides ubus interface for web UI and external applications.
endef

define Build/Prepare
	$(call Build/Prepare/Default)
	$(CP) $(PKG_BUILD_DIR)/src/api/autonomy-api/* $(PKG_BUILD_DIR)/
endef

define Build/Configure
	# No configuration needed for Lua scripts
endef

define Build/Compile
	# No compilation needed for Lua scripts
endef

define Package/autonomy-api/install
	$(INSTALL_DIR) $(1)/usr/lib/rpcd
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/autonomy.lua $(1)/usr/lib/rpcd/
	
	$(INSTALL_DIR) $(1)/usr/share/rpcd/acl.d
	$(INSTALL_DATA) ./files/autonomy.json $(1)/usr/share/rpcd/acl.d/
endef

$(eval $(call BuildPackage,autonomy-api))



