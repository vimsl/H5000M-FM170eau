# CloseWRT-CI
云编译 Airpi H5000M (FM170-EAU) CloseWRT 固件 | 5G 优化 + 硬件加速 + AI QoS 整合版

> 本仓库原名 `Airpi-AP5000M-CloseWRT`，2026-10 重命名为 `H5000M-FM170eau`（AP5000M 与 H5000M 实为同一台设备，命名已统一）。GitHub 旧链接会自动跳转。

PADAVANONLY-24.10源码：
https://github.com/padavanonly/immortalwrt-mt798x-6.6

# 仓库整理记录

- **AI QoS**：当前实现是本仓库的 `package/aiqos-engine/`（v29，含 C 引擎 aiqosd + collector + nftables + LuCI js）。独立的 `luci-app-aiqos` 仓库是**早期 shell 版**（v1.1.0，2026-08），已被 aiqos-engine 取代，该仓库已归档，其设计文档已迁入 `docs/`。
- **qmi 修复**：本仓库 `package/qmi_fix_skb/` 是固件构建用的包副本；`Scripts/fix-qmi-skb.patch`、`patches/qmi-wwan-tailroom.patch` 是**改驱动源码**的旧路线。热修复模块的唯一上游是 [qmi-headroom-fix](https://github.com/vimsl/qmi-headroom-fix)（v3），两条路线保留其一即可。
- **AIQoS 白皮书**：`docs/H5000M-HNAT-AIQoS-白皮书-v2.1.md`（原 luci-app-aiqos 仓库迁入）。

# 特性

- **5G 优化**: qmi_fix_skb 内核模块 (kprobe 热修复 skb tailroom) + 5G 信号看板
- **硬件加速**: HNAT DTS 自动适配 (usb0 白名单 + wwan 前缀)
- **AI QoS**: CAKE qdisc + cake-autorate + SINR 信号注入 + 条件检测
- **编译鲁棒性**: 自动清理无保护的 HNAT 内核补丁 + defconfig 设备符号恢复

# 注意：

本项目仅支持编译带有defconfig目录的MTK SDK闭源项目。

# 固件简要说明：

固件每天早上4点自动编译。

固件信息里的时间为编译开始的时间，方便核对上游源码提交时间。

MEDIATEK系列，配套的UBOOT：
https://github.com/VIKINGYFY/UBOOT-CI/releases

# 目录简要说明：

workflows——自定义CI配置

Scripts——自定义脚本

Config——自定义配置

#
[![Stargazers over time](https://starchart.cc/VIKINGYFY/CloseWRT-CI.svg?variant=adaptive)](https://starchart.cc/VIKINGYFY/CloseWRT-CI)

<!-- ci-trigger: kmod-sched-cake + cake-autorate + hnat-robustness + defconfig-recovery -->
