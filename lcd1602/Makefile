include $(TOPDIR)/rules.mk

PKG_NAME:=lcd1602
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_MAINTAINER:=BD4SUR <20069428+bd4sur@users.noreply.github.com>
PKG_LICENSE:=MIT

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=LCD1602/2004
endef

define Package/$(PKG_NAME)/description
	LCD1602/2004
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/$(PKG_NAME) $(1)/usr/bin/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
