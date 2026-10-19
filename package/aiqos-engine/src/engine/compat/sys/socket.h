#ifndef AIQOS_COMPAT_SYS_SOCKET_H
#define AIQOS_COMPAT_SYS_SOCKET_H
#include <io.h>
/* Windows stub: socket APIs return -1 immediately */
#ifndef AF_NETLINK
#define AF_NETLINK 16
#endif
#ifndef SOCK_RAW
#define SOCK_RAW 3
#endif
typedef int socklen_t;
static inline int socket(int domain, int type, int protocol) {
    (void)domain; (void)type; (void)protocol; return -1;
}
static inline int bind(int fd, const void *addr, socklen_t len) {
    (void)fd; (void)addr; (void)len; return -1;
}
#endif