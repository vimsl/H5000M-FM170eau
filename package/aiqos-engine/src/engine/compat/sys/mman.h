#ifndef AIQOS_COMPAT_SYS_MMAN_H
#define AIQOS_COMPAT_SYS_MMAN_H
/* Windows stub: mmap not available, always fails */
#define PROT_READ  0x1
#define MAP_SHARED 0x01
#define MAP_FAILED ((void *)-1)
static inline void *mmap(void *addr, size_t len, int prot, int flags, int fd, long offset) {
    (void)addr; (void)len; (void)prot; (void)flags; (void)fd; (void)offset;
    return MAP_FAILED;
}
static inline int munmap(void *addr, size_t len) { (void)addr; (void)len; return -1; }
#endif