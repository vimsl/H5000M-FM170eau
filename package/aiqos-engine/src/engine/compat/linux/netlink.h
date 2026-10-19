#ifndef AIQOS_COMPAT_LINUX_NETLINK_H
#define AIQOS_COMPAT_LINUX_NETLINK_H
/* Windows stub: AF_NETLINK not available */
#ifndef AF_NETLINK
#define AF_NETLINK 16
#endif
#ifndef NETLINK_ROUTE
#define NETLINK_ROUTE 0
#endif
struct sockaddr_nl {
    unsigned short nl_family;
    unsigned short nl_pad;
    unsigned int   nl_pid;
    unsigned int   nl_groups;
};
#endif