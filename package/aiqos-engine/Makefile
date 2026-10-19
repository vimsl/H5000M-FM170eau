include $(TOPDIR)/rules.mk

PKG_NAME:=aiqos-engine
PKG_VERSION:=4.7.0
PKG_RELEASE:=1

PKG_MAINTAINER:=AiQoS Team
PKG_LICENSE:=GPL-2.0-only

include $(INCLUDE_DIR)/package.mk

define Package/aiqos-engine
  SECTION:=net
  CATEGORY:=Network
  TITLE:=AiQoS 5G Adaptive Traffic Shaping Engine
  DEPENDS:=+kmod-sched-cake +kmod-ifb +tc-full +libubus +libblobmsg-json +libubox +libnl-tiny +librt +rpcd +luci-base
endef

define Package/aiqos-engine/description
  100ms adaptive QCI/AMBR-aware CAKE rate controller for 5G CPE.
  Reads signal telemetry from /dev/shm/aiqos_state and dynamically
  adjusts CAKE bandwidth via Netlink tc.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
		-I$(PKG_BUILD_DIR)/include \
		-I$(STAGING_DIR)/usr/include/libnl-tiny \
		-I$(STAGING_DIR)/usr/include \
		-Wall -Wextra -O2 -std=gnu11 -D_GNU_SOURCE \
		-o $(PKG_BUILD_DIR)/aiqosd \
		$(PKG_BUILD_DIR)/engine/aiqosd.c \
		$(PKG_BUILD_DIR)/engine/adrc_engine.c \
		$(PKG_BUILD_DIR)/engine/adrc_calibrate_collect.c \
		$(PKG_BUILD_DIR)/engine/aiqos_helpers.c \
		$(PKG_BUILD_DIR)/engine/pid.c \
		$(PKG_BUILD_DIR)/engine/kalman.c \
		$(PKG_BUILD_DIR)/engine/netlink_ctrl.c \
		$(PKG_BUILD_DIR)/engine/scenario.c \
		$(PKG_BUILD_DIR)/engine/shm_reader.c \
		$(PKG_BUILD_DIR)/engine/spatial_guidance.c \
		-lubus -lblobmsg_json -lubox -lnl-tiny -lm -lrt -lpthread
endef

define Package/aiqos-engine/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/aiqosd $(1)/usr/bin/aiqosd

	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/etc/init.d/aiqosd $(1)/etc/init.d/aiqosd
	$(INSTALL_BIN) ./files/etc/init.d/aiqos-collector $(1)/etc/init.d/aiqos-collector
	$(INSTALL_BIN) ./files/etc/init.d/aiqos-lan-foolproof $(1)/etc/init.d/aiqos-lan-foolproof
	$(INSTALL_BIN) ./files/etc/init.d/aiqos-route-fix $(1)/etc/init.d/aiqos-route-fix
	$(INSTALL_BIN) ./files/etc/init.d/aiqos-dataplane-guard $(1)/etc/init.d/aiqos-dataplane-guard

	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/aiqos.config $(1)/etc/config/aiqos

	$(INSTALL_DIR) $(1)/etc/nftables.d
	# 仅安装 MSS fix。10-aiqos-flowtable / 40-aiqos-selective-hnat 已停用(.disabled):
	# USB QMI 路径 offload 不可用, 硬编码注入 flowtable 绕过禁用补丁导致高延迟 (2026-09-03 事故, docs/06)
	# 禁止用通配符 *.nft 安装, 否则会复活加速注入
	$(INSTALL_DATA) ./files/etc/nftables.d/20-aiqos-mss.nft $(1)/etc/nftables.d/

	$(INSTALL_DIR) $(1)/etc/sysctl.d
	$(INSTALL_DATA) ./files/etc/sysctl.d/99-aiqos-tcp.conf $(1)/etc/sysctl.d/

	$(INSTALL_DIR) $(1)/usr/libexec/rpcd
	$(INSTALL_BIN) ./files/usr/libexec/rpcd/aiqos $(1)/usr/libexec/rpcd/aiqos

	# v63.5.1 (H5 治理): 天线模式「单一写者」helper——rpcd 与 LuCI controller 共用同一实现
	$(INSTALL_BIN) ./files/usr/libexec/aiqos-set-antenna $(1)/usr/libexec/aiqos-set-antenna

	$(INSTALL_DIR) $(1)/usr/sbin
	# v64.5 重构 (9/11, R0-③): 三驻留死资产移出 rootfs（零引用实测）:
	#   aiqos_b0_calibrate.sh / aiqos-observe / aiqos-abtest → 05_调试工具/archive-aiqos-tools/（按需手动装）
	#   aiqos-eth-monitor（守护+init+产物零读者）→ 同上, 见 R0-① 死代码链

	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) ./files/etc/uci-defaults/99-zz-aiqos-enable $(1)/etc/uci-defaults/

	$(INSTALL_DIR) $(1)/usr/share/rpcd/acl.d
	$(INSTALL_DATA) ./files/usr/share/rpcd/acl.d/aiqos.json $(1)/usr/share/rpcd/acl.d/

	$(INSTALL_DIR) $(1)/usr/share/luci/menu.d
	$(INSTALL_CONF) ./files/usr/share/luci/menu.d/luci-app-aiqos.json $(1)/usr/share/luci/menu.d/luci-app-aiqos.json

	$(INSTALL_DIR) $(1)/www/luci-static/resources/view/aiqos
	$(INSTALL_DATA) ./files/www/luci-static/resources/view/aiqos/aiqos.js $(1)/www/luci-static/resources/view/aiqos/
endef

define Package/aiqos-engine/conffiles
/etc/config/aiqos
/etc/sysctl.d/99-aiqos-tcp.conf
endef



define Package/aiqos-engine/postinst
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] && exit 0
/etc/init.d/aiqosd enable
/etc/init.d/aiqos-collector enable
/etc/init.d/aiqos-lan-foolproof enable
/etc/init.d/aiqos-route-fix enable
/etc/init.d/aiqos-dataplane-guard enable
exit 0
endef
$(eval $(call BuildPackage,aiqos-engine))
