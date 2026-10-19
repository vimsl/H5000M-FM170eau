/*
 * adrc_calibrate_collect.c —— ADRC b0 标定采集器（v70 落地实现）
 *
 * 关联：150分的计划-2.c 的 adrc_calibrate_b0()（最小二乘辨识）
 * 本文件补齐缺口：运行时采集 (u_k, y_k) 样本对。
 *
 * 采集原理：
 *   y_k ≈ b0 * u_k + disturbance
 *   u_k = 下发 CAKE 速率(bps)，y_k = 实测吞吐(bps)
 *   方波激励：rate 在 base*(1-AMP) / base*(1+AMP) 间切换（默认 ±10%，周期 30s）
 *   每 3s 采样一对，≥50 样本后调用 adrc_calibrate_b0()。
 *
 * 集成方式（v70）：
 *   1. aiqosd 检测 uci aiqos.@main[0].calibration=1 时进入标定模式
 *   2. 调用 adrc_calib_run(&ctrl, 180, 3) 完成采集+辨识+持久化
 *   3. 标定完成写 /tmp/aiqos_b0.json；aiqosd 启动时优先读该文件
 *
 * 编译注意：本文件**禁止** -ffast-math（与 adrc_engine.c 同约束，
 * 见 150分的计划-2.c 第六章 Makefile）。
 *
 * 2026-08-28 v0.1（未编译，v70 落地时合入）
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h> /* compat: Windows mkdir(path) 1-arg */         /* mkdir/rmdir 单实例锁 */
#include "compat/mkdir_compat.h"
#include <math.h>
#include <time.h>
#include <syslog.h>

#include "adrc_engine.h"          /* adrc_controller_t, b0_calibration_samples_t, adrc_calibrate_b0 */
#include "netlink_ctrl.h"         /* nlctrl_set_cake_bw()（v70 换成 fast_netlink） */

/* ---------- 可调参数 ---------- */
#define CALIB_MIN_SAMPLES     50      /* 最少样本（adrc_calibrate_b0 门槛）*/
#define CALIB_MAX_SAMPLES     80      /* 最多样本（防跑太久）*/
#define CALIB_DEFAULT_SECS    180     /* 默认时长 s（60 样本×3s）*/
#define CALIB_INTERVAL_SECS   3       /* 采样间隔 s */
#define CALIB_EXCITE_PERIOD   30      /* 方波半周期 s */
#define CALIB_EXCITE_AMP      0.10f   /* 激励幅度 ±10% */
#define CALIB_NO_TRAFFIC_Y    1000000 /* y < 1Mbps 视为空流量段，丢弃样本 */
#define CALIB_IFACE           "ifb0"  /* 下发速率接口 */
#define CALIB_BYTES_IFACE     "wwan0" /* 实测吞吐统计接口 */
#define CALIB_OUT_FILE        "/tmp/aiqos_b0.json"

/* 样本结构（复用 150分的计划-2.c 的 b0_calibration_samples_t，不重复定义）*/

/* ---------- 工具 ---------- */

static long long calib_read_rx_bytes(const char *iface) {
    char path[128];
    char buf[32];
    int fd, n;

    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/rx_bytes", iface);
    fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';
    return strtoll(buf, NULL, 10);
}

/* 读取当前 CAKE 下发速率（bps）*/
static long long calib_read_set_rate(void) {
    FILE *fp;
    char line[512];
    long long rate = -1;

    fp = popen("tc qdisc show dev " CALIB_IFACE " 2>/dev/null | grep -oE 'bandwidth [0-9]+[KkMm]?bit' | head -1", "r");
    if (!fp) return -1;
    if (fgets(line, sizeof(line), fp)) {
        /* 格式: bandwidth 300000000 或 bandwidth 300Mbit */
        char *p = strstr(line, "bandwidth");
        if (p) {
            p += 9; /* strlen("bandwidth") == 9 */
            while (*p == ' ') p++;
            if (strstr(p, "Mbit") || strstr(p, "mbit"))    rate = strtoll(p, NULL, 10) * 1000000LL;
            else if (strstr(p, "Kbit") || strstr(p, "kbit")) rate = strtoll(p, NULL, 10) * 1000LL;
            else                                           rate = strtoll(p, NULL, 10);
        }
    }
    pclose(fp);
    return rate;
}

/* 方波激励：切换 CAKE bandwidth（复用现有 netlink 控制通道）*/
static int calib_excite(int phase, long long base_rate) {
    long long target = (phase == 0)
        ? (long long)(base_rate * (1.0f - CALIB_EXCITE_AMP))
        : (long long)(base_rate * (1.0f + CALIB_EXCITE_AMP));
    int rc = nlctrl_set_cake_bw(CALIB_IFACE, (uint32_t)target, 65);
    syslog(LOG_DEBUG, "ADRC calib: phase=%d rate=%lld bps", phase, target);
    return rc;
}

/* ---------- 主流程 ---------- */

/*
 * adrc_calib_run —— 执行一次 b0 标定
 *  @ctrl:        ADRC 控制器（标定结果写入 ctrl->params.b0 并置 calibrated）
 *  @base_rate_bps: 激励基线 bps（0 = 自动读当前 CAKE；调用方应传"实测可达吞吐×1.1"）
 *                  —— 2026-08-28 实机验证：若 CAKE 上限远高于实际流量，y 不跟随 u，
 *                     最小二乘得 b0≈0（实测 0.0825 低于下限）。基线必须贴近实测吞吐。
 *  @max_secs:    最长采集时长 s（<=0 用默认 180）
 *  @interval_s:  采样间隔 s（<=0 用默认 3）
 *  返回：true=标定成功，false=失败（保留原 b0）
 */
bool adrc_calib_run(adrc_controller_t * RESTRICT ctrl,
                    long long base_rate_bps,
                    int max_secs, int interval_s)
{
    b0_calibration_samples_t samples = {0};
    long long base_rate, prev_bytes, cur_bytes;
    time_t t_start, t_last, t_phase, t_samp;
    int phase = 0;
    int empty_streak = 0;

    if (!ctrl) return false;
    if (max_secs <= 0) max_secs = CALIB_DEFAULT_SECS;
    if (interval_s <= 0) interval_s = CALIB_INTERVAL_SECS;

    /* 0. 单实例锁：mkdir 原子性，防双进程并发采集污染样本
     *    （2026-08-28 实机遇到 SIGTERM 未生效 + 新进程并存，样本文件 96 行被污染）*/
    if (mkdir("/tmp/aiqos_calib.lock", 0700) != 0) {
        syslog(LOG_WARNING, "ADRC calib: 已有标定实例在跑(/tmp/aiqos_calib.lock 存在), 拒绝并发");
        return false;
    }

    /* 1. 基线：优先用调用方传入(贴近实测吞吐)，否则读当前 CAKE */
    base_rate = base_rate_bps;
    if (base_rate < 1000000LL)
        base_rate = calib_read_set_rate();
    if (base_rate < 1000000LL) {
        syslog(LOG_WARNING, "ADRC calib: 基线速率异常 (rate=%lld bps), 放弃标定", base_rate);
        rmdir("/tmp/aiqos_calib.lock");
        return false;
    }

    syslog(LOG_INFO, "ADRC calib: 标定开始 base_rate=%lld bps, 方波 ±%.0f%%, 周期 %ds",
           base_rate, CALIB_EXCITE_AMP * 100.0f, CALIB_EXCITE_PERIOD);

    t_start = t_last = t_phase = t_samp = time(NULL);
    prev_bytes = calib_read_rx_bytes(CALIB_BYTES_IFACE);
    if (prev_bytes < 0) {
        syslog(LOG_WARNING, "ADRC calib: 无法读 %s rx_bytes", CALIB_BYTES_IFACE);
        rmdir("/tmp/aiqos_calib.lock");
        return false;
    }

    /* 2. 采集循环 */
    while (samples.count < CALIB_MAX_SAMPLES &&
           (time(NULL) - t_start) < max_secs) {

        /* 2a. 方波相位切换 */
        if ((time(NULL) - t_phase) >= CALIB_EXCITE_PERIOD) {
            phase = 1 - phase;
            t_phase = time(NULL);
            calib_excite(phase, base_rate);
        }

        /* 2b. 周期采样 */
        if ((time(NULL) - t_samp) >= interval_s) {
            cur_bytes = calib_read_rx_bytes(CALIB_BYTES_IFACE);
            if (cur_bytes >= 0) {
                long long dt = (long long)(time(NULL) - t_last);
                if (dt > 0) {
                    long long y = (cur_bytes - prev_bytes) * 8LL / dt; /* bps */
                    long long u = calib_read_set_rate();

                    if (y >= 0 && u > 0) {
                        if (y < CALIB_NO_TRAFFIC_Y) {
                            /* 空流量段：激励未产生响应，丢弃避免污染 */
                            empty_streak++;
                            if (empty_streak % 10 == 1)
                                syslog(LOG_DEBUG, "ADRC calib: 空流量段丢弃样本 (y=%lld bps)", y);
                        } else {
                            empty_streak = 0;
                            samples.u[samples.count] = (float)u;
                            samples.y[samples.count] = (float)y;
                            samples.count++;
                        }
                    }
                }
                prev_bytes = cur_bytes;
                t_last = time(NULL);
            }
            t_samp = time(NULL);
        }

        usleep(200 * 1000); /* 200ms 粒度轮询 */
    }

    /* 3. 恢复基线速率 */
    nlctrl_set_cake_bw(CALIB_IFACE, (uint32_t)base_rate, 65);

    /* 4. 辨识 + 持久化 */
    if (samples.count < CALIB_MIN_SAMPLES) {
        syslog(LOG_WARNING, "ADRC calib: 有效样本 %d < %d, 标定失败（流量不足?）",
               samples.count, CALIB_MIN_SAMPLES);
        rmdir("/tmp/aiqos_calib.lock");
        return false;
    }

    if (!adrc_calibrate_b0(ctrl, (const b0_calibration_samples_t *)&samples)) {
        syslog(LOG_WARNING, "ADRC calib: 最小二乘辨识失败");
        rmdir("/tmp/aiqos_calib.lock");
        return false;
    }

    /* 持久化：写 json，aiqosd 启动时读入 */
    FILE *fp = fopen(CALIB_OUT_FILE, "w");
    if (fp) {
        fprintf(fp, "{\"b0\":%.4f,\"samples\":%d,\"ts\":%lld}\n",
                ctrl->params.b0, samples.count, (long long)time(NULL));
        fclose(fp);
        syslog(LOG_INFO, "ADRC calib: 已持久化 -> %s (b0=%.4f, %d 样本)",
               CALIB_OUT_FILE, ctrl->params.b0, samples.count);
    }
    rmdir("/tmp/aiqos_calib.lock");
    return true;
}
