/* speed_sampler.c — v63.3 C 采样器 (任务⑥)
 * 500ms 精确节拍读 wwan0 统计, 原子写 /tmp/speed_now.json
 * 输出格式与 shell speed_loop 完全一致: {"dl":%s,"ul":%s}
 * 单实例: flock /tmp/speed_sampler.lock
 * 编译: ${CC} -O2 -o speed_sampler speed_sampler.c
 * 兜底: shell speed_loop 保留 (v63.3 init: 有 sampler 则不起 speed_loop)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/file.h>
#include <stdint.h>

#define IFACE "wwan0"
#define OUT_PATH "/tmp/speed_now.json"
#define OUT_TMP  "/tmp/speed_now.json.tmp"
#define LOCK_PATH "/tmp/speed_sampler.lock"
#define INTERVAL_MS 500

static uint64_t read_counter(const char *iface, const char *which) {
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/%s", iface, which);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    uint64_t v = 0;
    if (fscanf(f, "%llu", (unsigned long long *)&v) != 1) v = 0;
    fclose(f);
    return v;
}

static void write_json(double dl, double ul) {
    FILE *f = fopen(OUT_TMP, "w");
    if (!f) return;
    fprintf(f, "{\"dl\":%d,\"ul\":%d}", (int)(dl + 0.5), (int)(ul + 0.5));
    fclose(f);
    rename(OUT_TMP, OUT_PATH); /* 原子替换, CGI 读不到半截 */
}

int main(void) {
    int lfd = open(LOCK_PATH, O_RDONLY | O_CREAT, 0644);
    if (lfd < 0) return 1;
    if (flock(lfd, LOCK_EX | LOCK_NB) != 0) return 0; /* 已有实例, 静默退出 */

    uint64_t prev_rx = read_counter(IFACE, "rx_bytes");
    uint64_t prev_tx = read_counter(IFACE, "tx_bytes");
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t prev_ms = (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

    for (;;) {
        long req = INTERVAL_MS;
        struct timespec req_ts = { req / 1000, (req % 1000) * 1000000L };
        while (nanosleep(&req_ts, &req_ts) != 0) { /* EINTR 重试剩余 */ }

        uint64_t rx = read_counter(IFACE, "rx_bytes");
        uint64_t tx = read_counter(IFACE, "tx_bytes");
        clock_gettime(CLOCK_MONOTONIC, &ts);
        uint64_t now_ms = (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
        uint64_t dt = now_ms > prev_ms ? now_ms - prev_ms : INTERVAL_MS;
        if (dt == 0) dt = INTERVAL_MS;

        double dl = 0, ul = 0;
        if (rx >= prev_rx) dl = (double)(rx - prev_rx) * 8.0 / (double)dt / 1000.0; /* kbps→面板口径 Mbps 取整由 js 处理? 与 shell 一致: *8/dt(ms)/1e6*1000=Mbps */
        if (tx >= prev_tx) ul = (double)(tx - prev_tx) * 8.0 / (double)dt / 1000.0;
        /* shell 公式: (rx-prev)*8/(dt_ms*1000) = Mbps → 这里 *8/dt/1000 等价 (dt 单位 ms) */
        if (rx < prev_rx) { /* 计数器回绕(重启) */ }
        prev_rx = rx; prev_tx = tx; prev_ms = now_ms;

        write_json(dl, ul);
    }
    return 0;
}
