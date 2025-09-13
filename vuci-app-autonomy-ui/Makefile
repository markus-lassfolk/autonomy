include $(TOPDIR)/rules.mk

# Load version information from centralized VERSION file
include $(CURDIR)/../VERSION

PKG_NAME:=vuci-app-autonomy-ui
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

define Package/$(PKG_NAME)
  SECTION:=vuci
  CATEGORY:=VUCI
  TITLE:=VuCI - Autonomy Web UI
  DEPENDS:=+vuci-ui-core
  URL:=https://github.com/markus-lassfolk/autonomy
endef

define Package/$(PKG_NAME)/description
  VuCI-based web interface for Autonomy system management.
  Installs static HTML/JS assets and VUCI menu JSON.
endef

define Build/Prepare
	$(call Build/Prepare/Default)
	# Copy UI sources into build dir
	$(INSTALL_DIR) $(PKG_BUILD_DIR)/ui
	$(CP) -r $(PKG_BUILD_DIR)/src/web/autonomy-ui/* $(PKG_BUILD_DIR)/ui/
endef

define Build/Configure
	# No configure step required
endef

define Build/Compile
	# Prepare processed assets
	$(INSTALL_DIR) $(PKG_BUILD_DIR)/ui/processed
	# HTML
	$(CP) $(PKG_BUILD_DIR)/ui/enhanced-autonomy-dashboard.html $(PKG_BUILD_DIR)/ui/processed/index.html
	$(CP) $(PKG_BUILD_DIR)/ui/ml-analytics-dashboard.html $(PKG_BUILD_DIR)/ui/processed/ml-analytics.html
	# JS
	$(CP) $(PKG_BUILD_DIR)/ui/autonomy-dashboard.js $(PKG_BUILD_DIR)/ui/processed/autonomy-dashboard.js
	# Gzip pre-compress (optional on device)
	( gzip -c9 $(PKG_BUILD_DIR)/ui/processed/index.html > $(PKG_BUILD_DIR)/ui/processed/index.html.gz ) || true
	( gzip -c9 $(PKG_BUILD_DIR)/ui/processed/ml-analytics.html > $(PKG_BUILD_DIR)/ui/processed/ml-analytics.html.gz ) || true
	( gzip -c9 $(PKG_BUILD_DIR)/ui/processed/autonomy-dashboard.js > $(PKG_BUILD_DIR)/ui/processed/autonomy-dashboard.js.gz ) || true
endef

define Package/$(PKG_NAME)/install
	# Install web assets
	$(INSTALL_DIR) $(1)/www/autonomy
	$(CP) $(PKG_BUILD_DIR)/ui/processed/index.html $(1)/www/autonomy/
	$(CP) $(PKG_BUILD_DIR)/ui/processed/ml-analytics.html $(1)/www/autonomy/
	$(CP) $(PKG_BUILD_DIR)/ui/processed/autonomy-dashboard.js $(1)/www/autonomy/
	$(CP) $(PKG_BUILD_DIR)/ui/processed/*.gz $(1)/www/autonomy/ || true

	# Install VUCI menu entry
	$(INSTALL_DIR) $(1)/usr/share/vuci/menu.d
	$(CP) $(PKG_BUILD_DIR)/ui/ui/data/usr/share/vuci/menu.d/autonomy.json $(1)/usr/share/vuci/menu.d/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))



