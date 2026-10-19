#ifndef AIQOS_NETLINK_CTRL_H
#define AIQOS_NETLINK_CTRL_H

#include <stdint.h>

/* ── CAKE 参数「单一真源」（v63.5.1 治理，2026-09-11）────────────────────
 * 背景：memlimit / ack-filter 等字面量原散落于 netlink_ctrl.c ×3、setup_ifb.sh ×2、
 *       init.d/aiqosd ×2 —— 改一处漏一处（当日调 memlimit 时靠全量 grep 才覆盖到 10 处）。
 * 现集中到本头文件：C 代码统一引用宏；shell 侧（无法用宏）交由构建脚本做一致性校验拦截。
 *
 * memlimit 取值依据（2026-09-11 定稿）：1Gbit @ RTT 65ms 的 BDP = 8.1MB；
 *   且 CAKE 自身 AQM 的队列目标 ≈ rtt×rate = 65ms×894M ≈ 7.3MB —— 故 memlimit 只要
 *   ≥ ~8MB 即不再成为约束，再往上【无吞吐收益，只增大最坏时延】。
 *   取 64M = 8×BDP：可覆盖到 ~8Gbit@65ms；最坏额外时延 64M/894M ≈ 72ms
 *   （UDP 洪泛等 AQM 管不住时才会走到上限）；与 TCP 缓冲拐点同取 64 便于记忆。
 *   实测 16M/64M 区间对稳态时延无影响（负载 RTT 仅 +2~3ms，0% 丢包）。
 *   ✗ 不建议 100M：最坏时延升至 112ms，且 32M 以上 CAKE 已不会触及。
 * 注意：ifb0(下行) 不加 ack-filter；wwan0(上行) 才加 AIQOS_CAKE_ACKFILTER。
 * ack-filter 取"设备实际生效值"(普通 ack-filter)——dev 树原为 ack-filter-aggressive，
 *   本次治理一并归一到实际值，避免"改治理顺带改行为"。若日后要试 aggressive，
 *   改这一个宏 + 做 A/B 即可（历史上轻载 A/B 无差异）。
 */
#define AIQOS_CAKE_MEMLIMIT   "64M"
#define AIQOS_CAKE_ACKFILTER  "ack-filter"


int nlctrl_init(void);
void nlctrl_close(void);

/* Legacy: set CAKE bandwidth on a single interface */
int nlctrl_set_cake_bw(const char *iface, uint32_t bw_bps, uint32_t rtt_ms);

/* Dual-interface CAKE: ifb0 (downlink ingress) + wwan0 (uplink egress) */
int nlctrl_set_cake_dual(uint32_t dl_bps, uint32_t ul_bps, uint32_t rtt_ms);

#endif