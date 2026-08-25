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

int nlctrl_set_cake_bw(const char *iface, uint32_t bw_bps) {
    if (!iface) return -1;
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
        "tc qdisc change dev %s root cake bandwidth %ubit diffserv4 ack-filter-aggressive memlimit 4M",
        iface, bw_bps);
    int r = system(cmd);
    (void)r;
    return 0;
}

/*
 * Dual-interface CAKE dispatch:
 *   - ifb0  root CAKE: downlink (ingress redirected from wwan0)
 *   - wwan0 root CAKE: uplink   (egress, with ack-filter-aggressive)
 *
 * Uses "tc qdisc change" which only modifies the bandwidth parameter
 * on existing CAKE qdiscs (does not recreate them).
 */
int nlctrl_set_cake_dual(uint32_t dl_bps, uint32_t ul_bps) {
    char cmd[512];
    int ret = 0;

    /* Downlink: ifb0 CAKE (no ack-filter, bidirectional fairness) */
    snprintf(cmd, sizeof(cmd),
        "tc qdisc change dev ifb0 root cake bandwidth %ubit diffserv4 memlimit 4M",
        dl_bps);
    if (system(cmd) != 0) ret = -1;

    /* Uplink: wwan0 CAKE (with ack-filter-aggressive for latency) */
    snprintf(cmd, sizeof(cmd),
        "tc qdisc change dev wwan0 root cake bandwidth %ubit diffserv4 ack-filter-aggressive memlimit 4M",
        ul_bps);
    if (system(cmd) != 0) ret = -1;

    return ret;
}