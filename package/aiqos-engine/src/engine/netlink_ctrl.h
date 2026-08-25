#ifndef AIQOS_NETLINK_CTRL_H
#define AIQOS_NETLINK_CTRL_H

#include <stdint.h>

int nlctrl_init(void);
void nlctrl_close(void);

/* Legacy: set CAKE bandwidth on a single interface */
int nlctrl_set_cake_bw(const char *iface, uint32_t bw_bps);

/* Dual-interface CAKE: ifb0 (downlink ingress) + wwan0 (uplink egress) */
int nlctrl_set_cake_dual(uint32_t dl_bps, uint32_t ul_bps);

#endif