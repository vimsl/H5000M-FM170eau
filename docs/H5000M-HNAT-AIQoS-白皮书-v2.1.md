---
AIGC:
    Label: "1"
    ContentProducer: 001191440300708461136T1XGW3
    ProduceID: 565790b44d7aba67beaf6ed2a84b6dbf_c1f40143711511f1b2f55254006c9bbf
    ReservedCode1: J0OQLSJEcCo/gXqB5m7rhpnIQkz2EugLC3r+qqKarAADfGrZ+XspBOR/5PxL7bwGQiF241Iq39v0dmLNY8ebx+9pSbruL4Fm/ZUm/yTaZUKFTAVo2MHdNw8ibF7mgJ+4EchLo42dam41wFh5HZ/dQZmYqvCqtDdeK5HXowicOAsSadnxpFaUypS7Frg=
    ContentPropagator: 001191440300708461136T1XGW3
    PropagateID: 565790b44d7aba67beaf6ed2a84b6dbf_c1f40143711511f1b2f55254006c9bbf
    ReservedCode2: J0OQLSJEcCo/gXqB5m7rhpnIQkz2EugLC3r+qqKarAADfGrZ+XspBOR/5PxL7bwGQiF241Iq39v0dmLNY8ebx+9pSbruL4Fm/ZUm/yTaZUKFTAVo2MHdNw8ibF7mgJ+4EchLo42dam41wFh5HZ/dQZmYqvCqtDdeK5HXowicOAsSadnxpFaUypS7Frg=
---

# 广和通 FM170-EAU 5G 模组 MT7987 平台硬件加速适配与 AIQoS 集成项目白皮书

> **项目代号**：H5000M-HNAT-FM170
> **版本**：v2.1
> **日期**：2026-06-27
> **目标平台**：Hiveton H5000M (MediaTek MT7987 + Fibocom FM170-EAU)
> **基于固件**：ImmortalWrt 24.10-SNAPSHOT (padavanonly/immortalwrt-mt798x)

---

## 一、项目背景

### 1.1 硬件平台概述

| 组件 | 型号 | 说明 |
|------|------|------|
| **SoC** | MediaTek MT7987 (Filogic 660) | ARMv8 Cortex-A53 四核 |
| **WiFi** | MT7992 | BE7200 |
| **5G 模组** | Fibocom FM170-EAU (2cb7:0104) | USB 3.0，广和通 5G Sub-6 模组，基于骁龙 X65 |
| **固件基础** | ImmortalWrt 24.10-SNAPSHOT | padavanonly/immortalwrt-mt798x，Linux 6.6 + 闭源硬件加速驱动 |
| **模组管理** | qmodem-next + uqmi | 非 ModemManager |
| **当前 QoS** | eqos-mtk (MTK 硬件 QoS) | 非 cake-autorate |

### 1.2 问题陈述

在 Hiveton H5000M 平台实际运行中，MT7987 的 HNAT 硬件加速与 Fibocom FM170-EAU 5G 模组存在驱动冲突，表现为三个层面的问题：

#### 层面一：usbnet 驱动导致 skb headroom 不足（根因）

FM170-EAU 通过 USB 3.0 连接，usbnet 驱动在收包时额外消耗 skb headroom，导致 MTK 闭源 HNAT 驱动创建 FOE 条目时 headroom 不足。具体表现：

- HNAT 能创建 BIND 条目（42 条），但 **bytes 全部为 0**，未实际转发
- 开启 HNAT 后出现 tailroom 报错（约 2 次/1.5 小时）
- 关闭 HNAT 后 tailroom=0，软转发正常

#### 层面二：HNAT 不识别 WWAN 接口

HNAT 默认不识别 5G 模组的 WWAN 接口数据流，即使 headroom 问题解决，数据包仍无法进入硬件加速路径。

#### 层面三：现有 QoS 方案与 AIQoS 目标不兼容

当前固件使用 eqos-mtk（MTK 硬件 QoS），与 AIQoS 白皮书设计的 cake-autorate 链路存在冲突。需在 condition_detect 层增加 eqos-mtk 检测与降级适配。

### 1.3 项目目标

1. **主要目标**：在不牺牲硬件加速性能的前提下，使 HNAT 与 FM170-EAU 5G 模组共存
2. **次要目标**：验证 5G 模组作为 WAN 口时硬件加速的转发性能（iperf3 单流/多流，TCP/UDP 对比）
3. **AIQoS 集成目标**：打通 FM170 → HNAT → CAKE → cake-autorate → aiqosd 的完整 AIQoS 智能 QoS 链路
4. **产出目标**：形成可复用的补丁集、编译指南和 AIQoS 插件包，回馈社区

---

## 二、现有开源项目生态调研

### 2.1 核心上游项目

| 项目 | 地址 | 说明 |
|------|------|------|
| **联发科官方 mtk-openwrt-feeds** | `git01.mediatek.com/openwrt/feeds/mtk-openwrt-feeds` | 官方 HNAT 驱动源码，支持 MT7987 |
| **padavanonly/immortalwrt-mt798x** | `github.com/padavanonly/immortalwrt-mt798x` | 社区维护的 MT798x 闭源驱动固件仓库 |
| **chasey-dev/immortalwrt-mt798x-rebase** | `github.com/chasey-dev/immortalwrt-mt798x-rebase` | 25.12 分支，已集成 WWAN HNAT 支持 |

### 2.2 关键社区成果

**恩山论坛开发者 "237176253" 已验证**：

> "798x闭源驱动仓库现在默认均支持。dts里hnat字段如下添加自己的nr接口名 `ext-devices = "wwan0_0","wwan0_1","wwan0.1","wwan0.2";` 添加后，7981使用5g nr模块在qmi模式下wifi测速可轻松达到700m。"

**关键补丁来源**：

| 补丁 | 来源 | 说明 |
|------|------|------|
| `[kernel-6.12][mt7988/mt7987][hnat][Refactor HNAT driver support]` | 联发科官方 feed | HNAT 驱动重构，支持 MT7987 |
| `[kernel][common][hnat][Fix HNAT UNBIND issue of PPPoE WAN to LAN]` | 联发科官方 feed | 修复 PPPoE WAN 的 HNAT 解绑问题 |
| `[kernel][common][hnat][Add macvlan device HNAT offload support]` | 联发科官方 feed | 添加 macvlan 设备的 HNAT 卸载支持 |
| `[kernel][common][hnat][Fix issue of PPE cache control fail]` | 联发科官方 feed | 修复 PPE 缓存控制失败问题 |
| `增加对USB、WWAN等外部设备作为WAN接口的HNAT加速支持` | 移植自 padavanonly 仓库 | **WWAN 作为 WAN 口的 HNAT 支持** |

### 2.3 参考设备 / 平行项目

| 设备 / 项目 | 芯片 | 参考价值 |
|-------------|------|----------|
| **Cetron CT3003** | MT7986 | 同系列 SoC，FM170 QMI 模式，QMAP 聚合问题可互鉴 |
| Banana Pi BPi-R4 | MT7987A | 同平台参考 DTS（但用 PCIe 5G 模组非 USB，QMI 路径不同） |
| 移远 5G NR 模块 | QMI 模式 | 同类型 WWAN 接口配置 |

---

## 三、技术方案

### 3.1 总体架构

**核心思路**：三层修复，逐层递进，每层解决一类问题。

```
第一层：上游修复 → pskb_expand_head 加判空保护（软转发路径 tailroom 报错已消除）
第二层：KPROBES 兜底 → CONFIG_KPROBES=y + qmi_fix_skb.ko（运行时拦截 headroom 不足的包）
第三层：DTS 白名单 → ext-devices-prefix 加入 wwan（HNAT 识别 wwan0 接口）
```

### 3.2 完整数据链路

```
FM170-EAU (USB 3.0)
    │
    ▼
qmodem-next + uqmi (拨号管理)
    │
    ▼
qmi-fix-skb.ko [KPROBES 层] → 修复 headroom 不足的 skb
    │
    ▼
wwan0 → HNAT 硬件加速引擎 (DTS 白名单已注册)
    │
    ▼
CAKE qdisc (kmod-sched-cake)
    │
    ▼
cake-autorate (自动速率调整)
    │
    ▼
aiqosd (AIQoS 守护进程)
    │  ├─ sinr_injector.sh   → 每 2 秒读 SINR → /tmp/aiqos_sinr_coeff
    │  ├─ night_lock.sh      → 凌晨锁最优小区
    │  └─ condition_detect.sh → 能力探测 (WiFi / eBPF / eqos-mtk / modem)
    │
    ▼
LuCI 界面 (SimpleForm，七开关+预设联动)
```

### 3.3 架构图

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      Hiveton H5000M (MT7987)                             │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌─────────────────┐         ┌──────────────────────────────────────┐  │
│  │ FM170-EAU       │────────▶│  qmodem-next + uqmi 拨号管理         │  │
│  │ (USB 3.0)       │         │  → 网络接口: wwan0                   │  │
│  └─────────────────┘         └──────────────┬───────────────────────┘  │
│                                              │                          │
│                    ┌─────────────────────────▼──────────────────────┐   │
│                    │  Layer 1: 内核层                               │   │
│                    │  pskb_expand_head 判空保护 (tailroom 修复)     │   │
│                    └─────────────────────────┬──────────────────────┘   │
│                                              │                          │
│                    ┌─────────────────────────▼──────────────────────┐   │
│                    │  Layer 2: KPROBES 兜底层                       │   │
│                    │  qmi_fix_skb.ko → 拦截 headroom 不足的包       │   │
│                    └─────────────────────────┬──────────────────────┘   │
│                                              │                          │
│  ┌───────────────────────────────────────────▼──────────────────────┐   │
│  │  Layer 3: HNAT 硬件加速引擎 (ext-devices-prefix = "wwan")        │   │
│  │  ┌─────────────────────────────────────────────────────────────┐ │   │
│  │  │  BIND 条目 → bytes > 0 (实际转发生效)                       │ │   │
│  │  └─────────────────────────────────────────────────────────────┘ │   │
│  └─────────────────────────────────┬───────────────────────────────┘   │
│                                    │                                    │
│  ┌─────────────────────────────────▼───────────────────────────────┐   │
│  │  AIQoS 链路                                                      │   │
│  │  CAKE → cake-autorate → aiqosd                                  │   │
│  │  ├── sinr_injector  (SINR 注入)                                  │   │
│  │  ├── night_lock     (夜间小区锁定)                               │   │
│  │  └── condition_detect (能力探测与降级适配)                        │   │
│  └─────────────────────────────────┬───────────────────────────────┘   │
│                                    │                                    │
│                                    ▼                                    │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │  LAN / Wi-Fi (MT7992 BE7200)  ← 硬件加速 + AIQoS 转发目标        │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │  LuCI 管理界面 (SimpleForm CBI)                                  │   │
│  │  ├── 状态面板: 信号 / 带宽 / 延迟 / 运行时间                      │   │
│  │  ├── 七开关: 主控 / SINR注入 / 夜间锁频 / CAKE / 延迟控制 ...    │   │
│  │  └── 预设联动: 游戏 / 视频 / 下载 / 办公                         │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 3.4 实施步骤

#### Step 1：确认 FM170-EAU 的网络接口名

在 H5000M 终端执行：

```bash
ip link show
lsusb | grep 2cb7
```

FM170-EAU 的接口名已确认：**`wwan0`**，设备 ID：`2cb7:0104`。

#### Step 2：第一层 — pskb_expand_head 判空保护（已生效）

内核路径中 `pskb_expand_head` 增加判空保护，确保 headroom 不足时不会触发 skb 损坏。此修复已消除软转发路径的 tailroom 报错。

#### Step 3：第二层 — KPROBES 兜底（qmi_fix_skb.ko）

编译时启用：

```bash
# .config
CONFIG_KPROBES=y
CONFIG_PACKAGE_kmod-qmi-fix-skb=y
```

`qmi_fix_skb.ko` 在运行时通过 KPROBES 拦截进入 hnattrack 的 skb，检测 headroom 不足时动态调用 `pskb_expand_head` 安全扩容。此方案为外挂式补丁，不改动 MTK 闭源驱动源码。

**⚠️ 过渡方案提示**：KPROBES 兜底依赖内核配置（`CONFIG_KPROBES=y`），且 ko 模块跨内核版本可能失效。DTS 白名单（Layer 3）生效并上游合入后，本层应计划移除。

#### Step 4：第三层 — DTS 白名单配置

在 `padavanonly/immortalwrt-mt798x` 源码中找到 H5000M 对应的 DTS 文件，在 `hnat` 节点中添加白名单：

```dts
hnat {
    // ... 其他配置保持不变 ...
    ext-devices-prefix = "wwan";   // 匹配 wwan0
};
```

**备注**：使用 `ext-devices-prefix` 而非 `ext-devices`，通过前缀匹配涵盖所有 wwan 子接口。非破坏式修改，通过 sed 智能追加：

```bash
sed -i '/hnat {/a\\t\text-devices-prefix = "wwan";' target/linux/mediatek/dts/mt7987-hiveton-h5000m.dts
```

#### Step 5：编译固件

```bash
# 1. 更新 feeds
./scripts/feeds update -a
./scripts/feeds install -a

# 2. 配置内核
make menuconfig
# 目标平台: MediaTek MT7987
# 确保以下全部选中:
# - HNAT 驱动
# - kmod-sched-cake (CAKE qdisc)
# - kmod-qmi-fix-skb (KPROBES 兜底)
# - cake-autorate (自动速率调整)
# - aiqosd (AIQoS 守护进程)

# 3. 编译
make -j$(nproc)
```

**编译配置写死清单**（防 defconfig 静默移除）：

```yaml
CONFIG_PACKAGE_kmod-qmi-fix-skb=y
CONFIG_PACKAGE_kmod-sched-cake=y
CONFIG_PACKAGE_cake-autorate=y
CONFIG_PACKAGE_aiqosd=y
CONFIG_PACKAGE_kmod-usb-net-qmi-wwan=y
CONFIG_KPROBES=y
```

#### Step 6：刷入后验收

```bash
# 1. tailroom 清零
dmesg | grep tailroom    # → 0

# 2. KPROBES 内核支持确认
zcat /proc/config.gz | grep KPROBES    # → y

# 3. qmi_fix_skb 加载确认
dmesg | grep qmi_fix_skb    # → 有加载日志

# 4. HNAT 实际转发验证
cat /sys/kernel/debug/hnat/hnat_stats    # → BIND 条目 bytes > 0

# 5. AIQoS 链路完整性验证（CAKE 是否正确挂载到 wwan0）
tc -s qdisc show dev wwan0    # → 确认 CAKE qdisc 在 wwan0 上

# 6. 软中断负载验证
grep -r "cpu" /proc/interrupts | head    # → 确认软中断未打满

# 7. AIQoS SINR 数据确认
cat /tmp/aiqos_sinr_coeff    # → 格式正常，持续更新

# 8. 能力探测结果确认
cat /tmp/aiqos_capability.json    # → modem/wifi/eqos 检测正确

# 9. LuCI 界面验证
# → 无乱码、预设联动正常、开关置灰正确
```

---

## 四、AIQoS 插件设计

### 4.1 设计架构

```
LuCI 界面 (SimpleForm CBI)
  ├── 状态面板 (status.htm)          → 信号/带宽/延迟/运行时间
  ├── 七开关                          → 主控 | SINR注入 | 夜间锁频 | CAKE | 延迟控制 | ...
  └── 预设联动 (游戏/视频/下载/办公)  → 一键场景切换

胶水层 (Shell 脚本)
  ├── sinr_injector.sh               → 每2秒读 SINR → /tmp/aiqos_sinr_coeff
  ├── night_lock.sh                  → 凌晨锁最优小区，三级降级
  ├── condition_detect.sh            → 探测 WiFi/eBPF/eqos-mtk/modem 能力
  └── aiqosd.sh                      → 主守护进程，管理所有子模块启停

开源组件层
  ├── CAKE qdisc (kmod-sched-cake)
  ├── cake-autorate (自动速率调整)
  └── 三级降级 SINR 读取: uqmi → AT 串口 → mmcli
```

### 4.2 与路由器生态的冲突与适配

| 白皮书原始设计 | H5000M 实际环境 | 适配方案 |
|----------------|-----------------|----------|
| ModemManager + mmcli | qmodem-next + uqmi | 全部脚本改为 uqmi 调用，三级降级：uqmi → AT串口 → mmcli |
| cake-autorate | eqos-mtk (MTK 硬件 QoS) | condition_detect 增加 eqos-mtk 检测与降级适配 |
| mmcli 读 SINR | uqmi --get-signal-info | 加 timeout 3 + 退避 + uqmi FIFO 串行队列（见附录 D） |
| 独立运行 | 多进程竞争 uqmi 设备 | FIFO 串行队列互斥 + kill -STOP/CONT qmodem + resume 后 health check（ping uqmi，失败则 restart qmodem） |

### 4.3 文件清单

| 文件 | 设备路径 | 代码量 | 职责 |
|------|----------|--------|------|
| `sinr_injector.sh` | `/usr/bin/sinr_injector.sh` | ~80 行 | 每 2 秒读 SINR → 写入 `/tmp/aiqos_sinr_coeff` |
| `night_lock.sh` | `/usr/bin/night_lock.sh` | ~100 行 | 凌晨锁最优小区，三级降级 (uqmi→AT串口→mmcli)，30 秒看门狗+自动回滚 |
| `condition_detect.sh` | `/usr/bin/condition_detect.sh` | ~60 行 | 探测 WiFi / eBPF / eqos-mtk / ModemManager 能力，输出 `/tmp/aiqos_capability.json` |
| `aiqosd.sh` | `/etc/init.d/aiqosd` | ~150 行 | 主守护进程，管理所有子模块启停，trap EXIT 自动恢复 qmodem |
| `aiqos.lua` (controller) | `/usr/lib/lua/luci/controller/aiqos.lua` | ~80 行 | LuCI 路由 + `status_json` API |
| `aiqos.lua` (CBI) | `/usr/lib/lua/luci/model/cbi/aiqos.lua` | ~150 行 | SimpleForm 配置界面，七开关+预设联动 |
| `status.htm` | `/usr/lib/lua/luci/view/aiqos/status.htm` | ~100 行 | 状态概览面板（信号/带宽/延迟/运行时间） |
| **合计** | | **~720 行** | |

### 4.4 已修复的关键 Bug

| # | Bug | 影响 | 修复方案 |
|---|-----|------|----------|
| 1 | UCI section 创建参数错误 | 配置无法保存 | 修正 uci.set() 参数 |
| 2 | 每次页面加载覆盖 UCI | 配置被页面刷新覆盖 | 增加 `cbi.submit` 判断，仅 POST 时写入 |
| 3 | 首次运行无配置文件 | aiqosd 启动失败 | 增加 `ensure_uci()` 自动创建默认配置 |
| 4 | uqmi 超时卡死 | sinr_injector 永久阻塞 | timeout 3 + 退避 30→15s + MAX_TIMEOUTS 3→5 |
| 5 | qmodem 与 sinr 争抢 uqmi | SINR 读不到数据 | uqmi FIFO 串行队列互斥（见附录 D） |
| 6 | night_lock AT 串口冲突 | qmodem 串口被占用 | kill -STOP/CONT qmodem + /proc/PID/status 验证 + resume 后 uqmi health check（ping 失败则 restart qmodem） |
| 7 | DTS sed 误删整行 | DTS 文件损坏 | 改为智能追加 wwan（非替换整行） |
| 8 | 异常退出后 qmodem 卡死 | 路由器断网 | trap EXIT 自动 resume qmodem |

---

## 五、平行项目：Cetron CT3003 (MT7986)

### 5.1 设备对比

| 维度 | Hiveton H5000M | Cetron CT3003 |
|------|----------------|---------------|
| **SoC** | MT7987 (Filogic 660) | MT7986 (Filogic 830) |
| **5G 模组** | FM170-EAU, USB 3.0 | FM170, QMI, `wwan0` |
| **固件** | ImmortalWrt 24.10-SNAPSHOT | padavanonly/immortalwrt-mt798x-6.6 |
| **模组管理** | qmodem-next + uqmi | 待确认 |
| **HNAT 冲突根因** | usbnet headroom 不足 | QMAP 聚合绕过驱动入口 |
| **修复路径** | 三层修复 (pskb + KPROBES + DTS) | 关闭 QMAP + 白名单 (待确认 DTS/hnattrack) |
| **AIQoS 进度** | 代码 100%，编译中 | 待启动 |

### 5.2 CT3003 特有风险点

| 风险 | 说明 | 优先级 |
|------|------|--------|
| **QMAP 数据聚合** | FM170 默认开启 QMAP，下行包绕过驱动入口，即使配了 HNAT 白名单也看不到包 | P0 |
| **ext-devices 兼容性未知** | MT7986 + 内核 6.6 的 hnat 驱动可能不支持 `ext-devices` 属性 | P0 |
| **备选 hnattrack 路径** | 若 DTS 不支持，可走 `/sys/kernel/debug/hnat/` 动态添加，不改驱动源码 | P1 |

### 5.3 可复用资产（H5000M → CT3003）

| 资产 | 复用方式 |
|------|----------|
| `sinr_injector.sh` | 改接口名后直接使用 |
| `night_lock.sh` | 三级降级 + trap EXIT 回滚机制完整可用 |
| `condition_detect.sh` | 需将 eqos-mtk 检测改为 cake-autorate |
| `aiqosd.sh` | 主守护进程框架不变 |
| LuCI SimpleForm CBI | 七开关+预设联动完整可用 |
| DTS sed 智能追加脚本 | 改为操作 `ext-devices` 而非 `ext-devices-prefix` |

---

## 六、当前进度与待办

### 6.1 H5000M 进度

```
代码层  ████████████████ 100%  全模块完成 + 回归修复 + 互斥机制
编译层  ████████████░░░░  CI 编译中
部署层  ░░░░░░░░░░░░░░░░  等固件产物
验证层  ░░░░░░░░░░░░░░░░  待刷入后实测
```

### 6.2 CT3003 进度

```
方案层  ████████████████ 100%  方案已定
代码层  ░░░░░░░░░░░░░░░░  待启动
编译层  ░░░░░░░░░░░░░░░░  待启动
验证层  ░░░░░░░░░░░░░░░░  待启动
```

### 6.3 待执行清单

| 优先级 | 设备 | 动作 | 状态 |
|--------|------|------|------|
| 🔴 P0 | CT3003 | SSH 执行 `grep -r "ext-devices"` 确认驱动支持 | 待执行 |
| 🔴 P0 | CT3003 | 确认 FM170 QMAP 状态并关闭 (`cat /sys/class/net/wwan0/qmi/raw_ip`) | 待执行 |
| 🔴 P0 | H5000M | CI 编译完成后刷入固件 | 等待中 |
| 🔴 P0 | H5000M | 执行七项验收检查 | 待固件就绪 |
| 🟡 P1 | H5000M | 补充中文 .po 翻译文件 | 待补 |
| 🟡 P1 | H5000M | FM170 AT 端口确认 (ttyUSB2 or ttyUSB3) | 待补 |
| 🟡 P1 | CT3003 | HNAT 白名单方案落地 (DTS / hnattrack) | 待 grep 结果 |
| 🟢 P2 | CT3003 | AIQoS 模块移植与适配 | 待 HNAT 修复后 |

---

## 七、附录

### A. 验收检查项速查

```bash
# 1. tailroom 清零
dmesg | grep tailroom

# 2. KPROBES 支持
zcat /proc/config.gz | grep KPROBES

# 3. qmi_fix_skb 加载
dmesg | grep qmi_fix_skb

# 4. HNAT 转发验证
cat /sys/kernel/debug/hnat/hnat_stats

# 5. CAKE 挂载验证
tc -s qdisc show dev wwan0

# 6. 软中断负载
grep -r "cpu" /proc/interrupts | head

# 7. SINR 数据更新
cat /tmp/aiqos_sinr_coeff

# 8. 能力探测
cat /tmp/aiqos_capability.json

# 9. LuCI 界面检查
# 无乱码、预设联动正常、开关置灰正确
```

### B. 编译配置写死参考

```yaml
CONFIG_PACKAGE_kmod-qmi-fix-skb=y
CONFIG_PACKAGE_kmod-sched-cake=y
CONFIG_PACKAGE_cake-autorate=y
CONFIG_PACKAGE_aiqosd=y
CONFIG_PACKAGE_kmod-usb-net-qmi-wwan=y
CONFIG_KPROBES=y
```

### C. DTS sed 智能追加参考

```bash
# H5000M (ext-devices-prefix)
sed -i '/hnat {/a\\t\text-devices-prefix = "wwan";' target/linux/mediatek/dts/mt7987-hiveton-h5000m.dts

# CT3003 (ext-devices，待 grep 确认支持后使用)
sed -i '/hnat {/a\\t\text-devices = "wwan0";' target/linux/mediatek/dts/mt7986-cetron-ct3003.dts
```

### D. uqmi FIFO 串行队列参考

```bash
# 基于 FIFO 的确定性串行队列，替代 flock + 随机抖动的概率性方案
# 初始化（aiqosd 启动时执行一次）
QUEUE_FIFO="/tmp/uqmi_queue.fifo"
[ -p "$QUEUE_FIFO" ] || mkfifo "$QUEUE_FIFO"

# 队列消费者（后台常驻）
uqmi_queue_consumer() {
    while read -r cmd; do
        eval "$cmd"
    done < "$QUEUE_FIFO"
}

# 生产者：向队列投递 uqmi 命令
uqmi_enqueue() {
    echo "$*" > "$QUEUE_FIFO"
}

# 使用示例
uqmi_enqueue "uqmi -d /dev/cdc-wdm0 --get-signal-info"
```

**优势**：FIFO 是确定性串行化——内核保证同一时刻只有一个进程能写入，消费端顺序执行，彻底消除 flock + 随机抖动的竞态窗口。与原方案相比，无需超时退避逻辑。

### E. night_lock resume 后 health check 参考

```bash
# resume qmodem 后验证 uqmi 连接状态
resume_qmodem() {
    local pid=$(cat /var/run/qmodem.pid 2>/dev/null)
    [ -n "$pid" ] && kill -CONT "$pid"
    sleep 2
    # health check: ping uqmi，失败则 restart
    if ! timeout 5 uqmi -d /dev/cdc-wdm0 --get-signal-info >/dev/null 2>&1; then
        /etc/init.d/qmodem restart
    fi
}
*（内容由AI生成，仅供参考）*
