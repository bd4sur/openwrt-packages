include $(TOPDIR)/rules.mk

LUCI_TITLE:=LuCI Support for Nano Language Model Inference
LUCI_DEPENDS:=+luci-base +luci-compat +nano-infer-ws-server
LUCI_PKGARCH:=all

# define Package/$(PKG_NAME)/install
#   $(INSTALL_DIR) $(1)/usr/lib/lua/luci
#   cp -pR ./luasrc/* $(1)/usr/lib/lua/luci
#   $(INSTALL_DIR) $(1)/
#   cp -pR ./root/* $(1)/
# endef

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
