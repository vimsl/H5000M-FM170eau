#include "netlink_ctrl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

static int nl_sock = -1;

int nlctrl_init(void) {
    nl_sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (nl_sock < 0) return -1;
    struct sockaddr_nl addr = { .nl_family = AF_NETLINK };
    if (bind(nl_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(nl_sock); nl_sock = -1; return -1;
    }
    return 0;
}

void nlctrl_close(void) {
    if (nl_sock >= 0) { close(nl_sock); nl_sock = -1; }
}

int nlctrl_set_cake_bw(const char *iface, uint32_t bw_bps, uint32_t rtt_ms) {
    if (!iface) return -1;
    if (!rtt_ms) rtt_ms = 45U;
    char cmd[320];
    snprintf(cmd, sizeof(cmd),
        "tc qdisc change dev %s root handle 1: cake bandwidth %ubit diffserv4 " AIQOS_CAKE_ACKFILTER " memlimit " AIQOS_CAKE_MEMLIMIT " rtt %ums",
        iface, bw_bps, rtt_ms);
    int r = system(cmd);
    (void)r;
    return 0;
}

/*
 * Dual-interface CAKE dispatch:
 *   - ifb0  root CAKE: downlink (ingress redirected from wwan0)
 *   - wwan0 root CAKE: uplink   (egress, with ack-filter)
 *
 * Uses "tc qdisc change" + fixed handles so live flows keep their queue state.
 */
int nlctrl_set_cake_dual(uint32_t dl_bps, uint32_t ul_bps, uint32_t rtt_ms) {
    char cmd[640];
    int ret = 0;
    if (!rtt_ms) rtt_ms = 45U;

    /* Downlink: ifb0 CAKE (no ack-filter, bidirectional fairness) */
    snprintf(cmd, sizeof(cmd),
        "tc qdisc change dev ifb0 root handle 2: cake bandwidth %ubit diffserv4 memlimit " AIQOS_CAKE_MEMLIMIT " rtt %ums",
        dl_bps, rtt_ms);
    if (system(cmd) != 0) ret = -1;

    /* Uplink: wwan0 CAKE (with ack-filter for latency) */
    snprintf(cmd, sizeof(cmd),
        "tc qdisc change dev wwan0 root handle 1: cake bandwidth %ubit diffserv4 " AIQOS_CAKE_ACKFILTER " memlimit " AIQOS_CAKE_MEMLIMIT " rtt %ums",
        ul_bps, rtt_ms);
    if (system(cmd) != 0) ret = -1;

    return ret;
}
