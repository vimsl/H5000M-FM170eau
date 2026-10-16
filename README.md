# CloseWRT-CI
云编译 Airpi H5000M (FM170-EAU) CloseWRT 固件 | 5G 优化 + 硬件加速 + AI QoS 整合版

> 本仓库原名 `Airpi-AP5000M-CloseWRT`，2026-10 重命名为 `H5000M-FM170eau`（AP5000M 与 H5000M 实为同一台设备，命名已统一）。GitHub 旧链接会自动跳转。

PADAVANONLY-24.10源码：
https://github.com/padavanonly/immortalwrt-mt798x-6.6

# 仓库整理记录

- **AI QoS**：当前实现是本仓库的 `package/aiqos-engine/`（v29，含 C 引擎 aiqosd + collector + nftables + LuCI js）。独立的 `luci-app-aiqos` 仓库是**早期 shell 版**（v1.1.0，2026-08），已被 aiqos-engine 取代，该仓库已归档，其设计文档已迁入 `docs/`。
- **qmi 修复**：本仓库 `package/qmi_fix_skb/` 是固件构建用的包副本；`patches/qmi-wwan-tailroom.patch` 是**改驱动源码**的另一条路线。热修复模块的唯一上游是 [qmi-headroom-fix](https://github.com/vimsl/qmi-headroom-fix)（v3），两条路线保留其一即可。
- **补丁单一来源（2026-10 整理）**：构建时**实际生效的是 `Scripts/fm170_patch.py`**（CI 的 `Apply FM170eau USB ID & Tailroom patches` 步骤调用它，直接改 `KERNEL_SRC` 下的内核源码）。`patches/` 下的三个 `.patch` 是与之等价的可读版本，供人工审阅/归档，**不被构建读取**。对应关系：
  | patch 文件 | 等价于 fm170_patch.py 的哪一步 |
  |---|---|
  | `patches/fm170-option.patch` | 步骤 1：option.c 加 FM170 USB ID |
  | `patches/fm170-qmi-wwan.patch` | 步骤 2：qmi_wwan.c 加 QMI_FIXED_INTF |
  | `patches/qmi-wwan-tailroom.patch` | 步骤 3：qmi_wwan.c tailroom 修复（**保留版**） |
  已删除的两份重复补丁：`Scripts/fix-qmi-skb.patch`（旧版，补丁位置在 rx_fixup 出口，与 tailroom 版同一问题）、`Scripts/fix-qmi-wwan-f.patch`（针对闭源 `qmi_wwan_f.c`，源码树里不存在该文件，且 hunk 行号是 `@@ -xxx` 占位符，从未被任何流程调用）。两文件内容保留在 git 历史中。
- **⚠️ fm170_patch.py 三步的实际生效情况（2026-10 实测，用内核 v6.6.94 源码模拟 KERNEL_SRC 跑通验证）**：
  | 步骤 | 目标 | 结果 |
  |---|---|---|
  | 1. option.c 加 FM170 USB ID | `0x0102` ECM/RNDIS + `0x01a0` MBIM | **不生效**——判定条件是同一行同时含 `Fibocom FM101-GL` 和 `RSVD(4)`，但真实文件里注释在 2368 行、`RSVD(4)` 在 2369 行，**永不匹配**。（另：`0x01a0` 上游本就有，NL668-AM/NL652-EU/FM101-GL 条目已覆盖） |
  | 2. qmi_wwan.c 加 QMI_FIXED_INTF | `0x01a0` / `0x0102` | **生效**——锚点 `Fibocom FG132` 存在且唯一 |
  | 3. qmi_wwan.c tailroom 修复 | 给 rx 路径补 tailroom | **不生效**——锚点 `while (skb->len > 0) {` 在 6.6 主线 `qmi_wwan.c` 里**不存在**（该循环只存在于旧版本/其他分支的写法）。且主线 `qmimux_rx_fixup()` 早已是 `netdev_alloc_skb(net, pkt_len + LL_MAX_HEADER)`，即上游已修 |
  > 结论：**真正在起作用的是 kprobe 热修复模块**（`package/qmi_fix_skb/`，作用于闭源 `qmi_wwan_f` 驱动）——FM170-EAU 绑定的正是 `qmi_wwan_f`，而不是主线的 `qmi_wwan.c`。步骤 3 的 `patches/qmi-wwan-tailroom.patch` 因此只作历史/参考留存。
  > 该脚本原先三步无论是否插入成功**都打印 "Patched ..."**，日志会误报；2026-10 已改为：未命中锚点时打印 `WARNING: ... NOT applied`，不再谎报成功。
- **tailroom 片段的安全修复（2026-10）**：`dev_kfree_skb_any(skb); skb = new_skb ?: skb;` 在 `skb_copy_expand()`（`GFP_ATOMIC`）返回 NULL 时会释放原 skb 后回指，形成 **use-after-free**。已改为 `if (new_skb) { dev_kfree_skb_any(skb); skb = new_skb; }`，`fm170_patch.py` 步骤 3 与 `patches/qmi-wwan-tailroom.patch` 两处同步修改。
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
