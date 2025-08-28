include $(TOPDIR)/rules.mk

APP_TITLE:=VuCI UI Support for Autonomy APP
APP_NAME:=$(notdir ${CURDIR})
APP_DEPENDS:=+vuci-app-autonomy-api
CLOSED_GPL_INSTALL:=y

ifeq (m, $(CONFIG_PACKAGE_$(APP_NAME)))
	APP_TLT_NAME:=Autonomy Application
	APP_ROUTER:=$(TLT_PLATFORM_NAME)
endif

define Package/$(APP_NAME)/description
	Autonomy UI application for autonomous networking system
endef

include ../app.mk

# call BuildPackage - OpenWrt buildroot signature
