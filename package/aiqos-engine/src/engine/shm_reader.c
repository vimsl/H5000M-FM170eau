#include "shm_reader.h"
#include "aiqos_types.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>

static int fd = -1;
static ShmTelemetry *ptr = NULL;
static uint32_t last_seq = 0;

int shm_init(void) {
    fd = open(AIQOS_SHM_PATH, O_RDONLY);
    if (fd < 0) return -1;

    /* Verify file size matches expected struct */
    struct stat st;
    if (fstat(fd, &st) != 0 || (size_t)st.st_size < sizeof(ShmTelemetry)) {
        close(fd);
        fd = -1;
        return -1;
    }

    ptr = (ShmTelemetry *)mmap(NULL, sizeof(ShmTelemetry), PROT_READ, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        close(fd);
        fd = -1;
        ptr = NULL;
        return -1;
    }

    /* Validate magic on first map */
    if (ptr->magic != AIQOS_SHM_MAGIC) {
        munmap(ptr, sizeof(ShmTelemetry));
        close(fd);
        fd = -1;
        ptr = NULL;
        return -1;
    }

    return 0;
}

int shm_read(ShmTelemetry *out) {
    if (!ptr || !out) return -1;

    /* SeqLock read protocol:
     * 1. Read seq (snapshot A)
     * 2. If odd -> writer active, skip
     * 3. If unchanged since last successful read -> no new data, skip
     * 4. Copy full struct
     * 5. Read seq again (snapshot B)
     * 6. If A != B -> torn read during copy, retry on next cycle
     */
    uint32_t seq_a = ptr->seq;
    if (seq_a & 1) return -1;              /* odd = write in progress */
    if (seq_a == last_seq) return -1;      /* no new data */
    if (ptr->magic != AIQOS_SHM_MAGIC) return -1;  /* re-validate magic */

    memcpy(out, ptr, sizeof(ShmTelemetry));

    uint32_t seq_b = ptr->seq;
    if (seq_a != seq_b) return -1;         /* torn read detected */

    last_seq = seq_a;
    return 0;
}

void shm_close(void) {
    if (ptr && ptr != MAP_FAILED) munmap(ptr, sizeof(ShmTelemetry));
    if (fd >= 0) close(fd);
    ptr = NULL;
    fd = -1;
    last_seq = 0;
}