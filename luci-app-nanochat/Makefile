include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-nanochat

PKG_MAINTAINER:=BD4SUR <20069428+bd4sur@users.noreply.github.com>
PKG_LICENSE:=MIT

LUCI_TITLE:=LuCI Support for Nano Language Model Inference
LUCI_DEPENDS:=+luci-base +luci-compat +libwebsockets
LUCI_PKGARCH:=all

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/
	cp -pR ./root/* $(1)/
	chmod +x $(1)/etc/init.d/nanochat
endef

define Package/$(PKG_NAME)/postinst
#!/bin/sh
	chmod +x /etc/init.d/nanochat
exit 0
endef

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot signature
