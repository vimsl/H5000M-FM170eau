/**
 * AiQoS v4.5 - aiqosd main daemon (P1 Dual-Track)
 *
 * P1 Architecture:
 *   Fast track (100ms): sysfs rx/tx bytes -> PID rate control -> TC dispatch
 *   Slow track (~2s):   AT/QMI via SHM -> Kalman -> scenario detection
 *
 * Control loop:
 *   1. Read sysfs rx_bytes (fast, <50us)
 *   2. Poll SHM (only when timestamp changes, ~2s)
 *   3. Scenario detection (3-sample hysteresis)
 *   4. Kalman channel estimation (on SHM update)
 *   5. PID control (every 100ms cycle)
 *   6. TC dispatch (2% change threshold)
 */

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "aiqos_types.h"
#include "aiqosd.h"
#include "aiqos_helpers.h"
#include "scenario.h"
#include "kalman.h"
#include "pid.h"
#include "netlink_ctrl.h"
#include "shm_reader.h"
#include "spatial_guidance.h"
#include "adrc_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <sys/stat.h>
#include <syslog.h>

static volatile int g_running = 1;
static volatile sig_atomic_t g_reload_scenario = 0;
static int g_cold_start = 1;
static float g_uptime = 0.0f;
static int g_no_data_count = 0;
static int g_prev_pci = -1;
static int g_shm_ready = 0;
static uint64_t g_last_shm_ts = 0;
static int g_watchdog_level = 0;
static int g_collector_restarts = 0;
static uint64_t g_watchdog_stage_us = 0;
static int g_stale_safety = 0;

static adrc_controller_t g_adrc;
static int g_adrc_shadow = 0;
static int g_adrc_log_cnt = 0;
static AiqosRttTracker g_rtt = { .active_rtt_ms = 45 };

static KalmanFilter g_kalman;
static PIDController g_pid;
static ScenarioType g_active_scenario = SCENARIO_URBAN;
static const ScenarioConfig *g_active_cfg = NULL;

static void sig_handler(int sig) {
    if (sig == SIGUSR1) {
        g_reload_scenario = 1;
        return;
    }
    g_running = 0;
}

static uint64_t get_monotonic_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static void modem_hard_reset(void) {
    fprintf(stderr, "aiqosd: modem hard reset via GPIO\n");
    /* R0-4/H5 治理 (2026-09-12)：原先两处 system() 返回值被直接丢弃 —— GPIO 写失败（目录不存在/
     * 权限/被占用）时**完全静默**，看门狗"硬复位 modem"这条兜底路径会误以为已复位，且不留任何证据。
     * 改为显式判失败并记录（这是真异常，按 procd 日志语义走 stderr）。 */
    if (system("echo 0 > /sys/class/gpio/modem:reset/value 2>/dev/null") != 0)
        fprintf(stderr, "aiqosd: modem reset ASSERT failed (write 0 to modem:reset/value)\n");
    usleep(500000);
    if (system("echo 1 > /sys/class/gpio/modem:reset/value 2>/dev/null") != 0)
        fprintf(stderr, "aiqosd: modem reset DEASSERT failed (write 1 to modem:reset/value)\n");
    sleep(3);
}

static bool antenna_mode_is(const char *wanted) {
    FILE *f = fopen("/tmp/aiqos_antenna_mode", "r");
    char mode[24] = "auto";
    if (f) {
        if (fscanf(f, "%23s", mode) != 1) strcpy(mode, "auto");
        fclose(f);
    }
    return strcmp(mode, wanted) == 0;
}

static void apply_uci_scenario(void) {
    FILE *p = popen("uci -q get aiqos.global.scenario; uci -q get aiqos.@main[0].scenario", "r");
    char value[32] = "auto";
    if (p) {
        if (fscanf(p, "%31s", value) != 1) strcpy(value, "auto");
        pclose(p);
    }

    if (strcmp(value, "auto") == 0) {
        scenario_unlock();
        return;
    }

    ScenarioType s = SCENARIO_URBAN;
    if (strcmp(value, "rural") == 0) s = SCENARIO_RURAL;
    else if (strcmp(value, "wilderness") == 0) s = SCENARIO_WILDERNESS;
    else if (strcmp(value, "high_speed_rail") == 0) s = SCENARIO_HIGH_SPEED_RAIL;
    else if (strcmp(value, "dark_cci") == 0) s = SCENARIO_DARK_CCI;
    else if (strcmp(value, "dark_throttle") == 0) s = SCENARIO_DARK_THROTTLE;

    scenario_lock(s);
    g_active_scenario = s;
    g_active_cfg = scenario_get_config(s);
    printf("aiqosd: scenario locked to %s\n", scenario_name(s));
}

static void write_status_files(const ShmTelemetry *tele, uint32_t target_bw) {
    FILE *f;
    f = fopen("/tmp/aiqos_scenario", "w");
    if (f) { fprintf(f, "%s", scenario_name(g_active_scenario)); fclose(f); }
    f = fopen("/tmp/aiqos_rsrp", "w");
    if (f) { fprintf(f, "%.0f", tele->rsrp[0]); fclose(f); }
    f = fopen("/tmp/aiqos_sinr", "w");
    if (f) { fprintf(f, "%.1f", tele->sinr); fclose(f); }
    f = fopen("/tmp/aiqos_rsrq", "w");
    if (f) { fprintf(f, "%.1f", tele->rsrq); fclose(f); }
    f = fopen("/tmp/aiqos_target_bw", "w");
    if (f) { fprintf(f, "%u", target_bw); fclose(f); }
    f = fopen("/tmp/aiqos_handover", "w");
    if (f) { fprintf(f, "%u", tele->handover_count); fclose(f); }

    if (g_active_cfg) {
        f = fopen("/tmp/aiqos_profile_kf_q", "w");
        if (f) { fprintf(f, "%.2f", g_active_cfg->Q); fclose(f); }
        f = fopen("/tmp/aiqos_profile_kf_r", "w");
        if (f) { fprintf(f, "%.2f", g_active_cfg->R); fclose(f); }
        f = fopen("/tmp/aiqos_profile_pid_kp", "w");
        if (f) { fprintf(f, "%.2f", g_active_cfg->Kp); fclose(f); }
        f = fopen("/tmp/aiqos_profile_pid_ki", "w");
        if (f) { fprintf(f, "%.2f", g_active_cfg->Ki); fclose(f); }
        f = fopen("/tmp/aiqos_profile_pid_kd", "w");
        if (f) { fprintf(f, "%.2f", g_active_cfg->Kd); fclose(f); }
        f = fopen("/tmp/aiqos_profile_min_bw", "w");
        if (f) { fprintf(f, "%u", g_active_cfg->min_bw_kbit); fclose(f); }
        f = fopen("/tmp/aiqos_profile_ack_filter", "w");
        if (f) { fprintf(f, "%s", "ack-filter"); fclose(f); }
    }

    /* v64.5 重构 (9/11): 钳制分离——ant 供评分（完整钳制, 防 GTCCINFO 解析错位污染 SRQI）;
       ant_disp 供面板显示（仅清物理无效值, 保留真实极差——面板不再显示"假均衡"）。
       背景: v4.7 的极差>20dB 钳制把真实极差（如 -51/-81/-81/-92, 41dB）一并抹平, 面板 4 柱同值失真。 */
    AntennaRawTelemetry ant, ant_disp;
    memset(&ant, 0, sizeof(ant));
    memset(&ant_disp, 0, sizeof(ant_disp));
    for (int i = 0; i < 4; i++) {
        ant.rsrp[i] = tele->rsrp[i];
        ant_disp.rsrp[i] = tele->rsrp[i];
    }
    ant.sinr = tele->sinr;
    ant_disp.sinr = tele->sinr;
    ant.ri = tele->ri;
    ant_disp.ri = tele->ri;
    ant.pci_switch_rate = tele->pci_switch_rate;
    ant_disp.pci_switch_rate = tele->pci_switch_rate;
    ant.feeder_length_m = 1;
    ant_disp.feeder_length_m = 1;
    /* v4.7 钳制条件（评分侧完整保留；显示侧仅清物理无效值）:
       >-40dBm 物理不可能 / <-140dBm 过低无效 / 极差>20dB 疑似解析错位 */
    {
        float b4 = ant.rsrp[0];
        int k;
        for (k = 1; k < 4; k++) if (ant.rsrp[k] > b4) b4 = ant.rsrp[k];
        for (k = 0; k < 4; k++) {
            if ((ant.rsrp[k] > -40.0f) || (ant.rsrp[k] < -140.0f) ||
                ((b4 - ant.rsrp[k]) > 20.0f)) ant.rsrp[k] = b4;
            if ((ant_disp.rsrp[k] > -40.0f) || (ant_disp.rsrp[k] < -140.0f))
                ant_disp.rsrp[k] = b4;
        }
    }

    bool directional = antenna_mode_is("directional");
    bool omni = antenna_mode_is("omni");
    SpatialGuidanceResult sg = calculate_3d_alignment(&ant, directional);
    const char *fs = directional
                     ? (sg.rank >= 3 ? "directional_tracking" : "directional_locked")
                     : (omni ? "omni_locked" : "auto");
    write_spatial_files(&sg, &ant_disp, fs); /* v64.5: 散文件写真值（ant_disp 不抹极差） */
}

static void main_loop(void) {
    ShmTelemetry tele;
    memset(&tele, 0, sizeof(tele));
    uint64_t prev_cycle_us = get_monotonic_us();
    uint64_t fast_prev_rx = 0;
    uint32_t last_applied_dl = 0;
    uint64_t last_shm_good_us = 0;
    int log_cnt = 0;
    int log60_cnt = 0; /* 34.5j: 场景日志 60s 一条计数器 */

    {
        uint64_t rx64 = 0, tx64 = 0;
        sysfs_read_iface_bytes("wwan0", &rx64, &tx64);
        fast_prev_rx = rx64;
    }

    while (g_running) {
        if (g_reload_scenario) {
            g_reload_scenario = 0;
            apply_uci_scenario();
            nlctrl_init();
            g_rtt.pending = true;
        }

        usleep(AIQOSD_CONTROL_MS * 1000);

        uint64_t now_us = get_monotonic_us();
        float dt = (float)(now_us - prev_cycle_us) / 1000000.0f;
        if (dt < 0.05f) dt = 0.05f;
        if (dt > 10.0f) dt = 10.0f;
        prev_cycle_us = now_us;
        g_uptime += dt;

        int shm_updated = 0;
        if (!g_shm_ready) {
            if (shm_init() == 0) {
                g_shm_ready = 1;
                printf("aiqosd: SHM attached\n");
            }
        }
        if (g_shm_ready && shm_read(&tele) == 0) {
            if (tele.timestamp_us != g_last_shm_ts) {
                g_last_shm_ts = tele.timestamp_us;
                shm_updated = 1;
                g_no_data_count = 0;
                last_shm_good_us = now_us;
                g_watchdog_level = 0;
                g_collector_restarts = 0;
                g_stale_safety = 0;
            }
        }
        if (last_shm_good_us > 0) {
            uint64_t stall_us = now_us - last_shm_good_us;
            if (stall_us > (uint64_t)AIQOSD_WATCHDOG_WARN_S * 1000000ULL) {
                g_stale_safety = 1;
                if (g_watchdog_level < 1) {
                    g_watchdog_level = 1;
                    g_watchdog_stage_us = now_us;
                    fprintf(stderr, "aiqosd: no SHM data %ds, safe mode\n", AIQOSD_WATCHDOG_WARN_S);
                }
            }
            if (g_watchdog_level >= 1 &&
                (now_us - g_watchdog_stage_us) > (uint64_t)AIQOSD_WATCHDOG_RESTART_S * 1000000ULL) {
                if (g_watchdog_level < 2) {
                    g_watchdog_level = 2;
                    g_collector_restarts++;
                    g_watchdog_stage_us = now_us;
                    printf("aiqosd: restart collector #%d\n", g_collector_restarts);
                    /* H5 治理 (2026-09-12) 标注：此处结尾的 `&` 是**有意**的 —— 去掉它，system()
                     * 会阻塞控制环直到 restart 跑完（数秒）。故其返回值**本就无意义**（shell 立即退出 0），
                     * 不是"未捕获的报错点"。「restart 是否真生效」由看门狗后续的 SHM 新鲜度复核承担。
                     * ⚠️ 勿改为同步调用。 */
                    system("/etc/init.d/aiqos-collector restart >/dev/null 2>&1 &");
                }
            }
            if (g_watchdog_level >= 2 &&
                g_collector_restarts >= AIQOSD_WATCHDOG_RESTART_MAX &&
                (now_us - g_watchdog_stage_us) > (uint64_t)AIQOSD_WATCHDOG_RESET_S * 1000000ULL) {
                fprintf(stderr, "aiqosd: collector restarts failed, hard reset modem\n");
                modem_hard_reset();
                last_shm_good_us = now_us;
                g_watchdog_level = 0;
                g_collector_restarts = 0;
                g_stale_safety = 0;
            }
        }

        if (shm_updated) {
            static uint64_t prev_shm_rx = 0;
            static uint64_t prev_shm_ts = 0;
            if (prev_shm_ts > 0 && tele.timestamp_us > prev_shm_ts) {
                float shm_dt = (float)(tele.timestamp_us - prev_shm_ts) / 1000000.0f;
                if (shm_dt > 0) {
                    uint32_t shm_rx_kbps = (uint32_t)((uint64_t)(tele.rx_bytes - prev_shm_rx) * 8 / (uint32_t)(shm_dt * 1000));
                    scenario_set_measured_rate(shm_rx_kbps);
                }
            }
            prev_shm_rx = tele.rx_bytes;
            prev_shm_ts = tele.timestamp_us;

            g_active_scenario = scenario_detect(&tele);
            g_active_cfg = scenario_get_config(g_active_scenario);

            aiqos_rtt_update(&g_rtt, scenario_get_target_rtt(&tele));

            kalman_set_params(&g_kalman, g_active_cfg->Q, g_active_cfg->R);

            tele.cqi = aiqos_estimate_cqi(tele.cqi, tele.sinr);

            float qci_w = aiqos_qci_weight(tele.qci);
            float z_k = kalman_estimate_capacity(&tele, qci_w);
            kalman_update(&g_kalman, z_k);
        }

        if (!g_active_cfg) g_active_cfg = scenario_get_config(SCENARIO_URBAN);

        uint64_t rx64 = 0, tx64 = 0;
        sysfs_read_iface_bytes("wwan0", &rx64, &tx64);
        uint32_t current_rx = (uint32_t)(rx64 & 0xFFFFFFFF);

        uint32_t fast_rx_kbps = 0;
        if (current_rx >= (uint32_t)fast_prev_rx && dt > 0) {
            uint32_t delta = current_rx - (uint32_t)fast_prev_rx;
            fast_rx_kbps = (uint32_t)((uint64_t)delta * 8 / (uint32_t)(dt * 1000));
        }
        fast_prev_rx = rx64;

        uint32_t target_bw;
        if (g_cold_start && g_uptime < AIQOS_COLD_START_S) {
            target_bw = (uint32_t)aiqos_cold_start_bw(tele.sinr);
            if (target_bw < g_active_cfg->min_bw_kbit)
                target_bw = g_active_cfg->min_bw_kbit;
        } else {
            if (g_cold_start) { g_cold_start = 0; pid_reset(&g_pid); }
            pid_update(&g_pid, g_kalman.x, fast_rx_kbps, tele.cqi, dt, g_active_cfg, tele.ambr_dl_kbps);
            target_bw = pid_get_target(&g_pid, g_active_cfg, tele.ambr_dl_kbps);

            if (g_adrc_shadow) {
                float target_bps = (float)target_bw * 1000.0f;
                float adrc_out = adrc_compute_control(&g_adrc, target_bps, 1.0f, dt);
                adrc_eso_update(&g_adrc, (float)fast_rx_kbps / 1000.0f, adrc_out / 1000000.0f, dt);
                if (++g_adrc_log_cnt >= 100) {
                    g_adrc_log_cnt = 0;
                    syslog(LOG_INFO, "ADRC shadow: pid=%ukbps adrc=%.0fkbps z3=%.0f",
                           target_bw, adrc_out / 1000.0f, g_adrc.eso.z3 / 1000.0f);
                }
            }
        }

        if (aiqos_should_freeze(g_active_cfg->freeze_on_handover, g_prev_pci, tele.pci, tele.handover_count)) {
            pid_freeze(&g_pid, 3);
            target_bw = target_bw * 50 / 100;
        }
        g_prev_pci = tele.pci;

        target_bw = aiqos_stale_cap(target_bw, g_stale_safety);

        bool rtt_dirty = g_rtt.pending;
        g_rtt.pending = false;

        if (aiqos_should_dispatch(last_applied_dl, target_bw, rtt_dirty)) {
            uint32_t ul_bw = aiqos_calc_ul_bw(target_bw, tele.ambr_ul_kbps);
            nlctrl_set_cake_dual(target_bw * 1000, ul_bw * 1000, g_rtt.active_rtt_ms);
            last_applied_dl = target_bw;
        }

        /* write_status_files 保持 1s 节奏(面板数据源); 34.5j: 场景 stderr 日志降频 60s 一条(1s/条挤占取证 ring buffer) */
        if (++log_cnt >= 10) {
            log_cnt = 0;
            write_status_files(&tele, target_bw);
        }
        if (++log60_cnt >= 600) {
            log60_cnt = 0;
            printf("aiqosd: [%s] SINR=%.1f CQI=%d dl=%ukbps fast=%ukbps dt=%.3fs\n",
                    scenario_name(g_active_scenario), tele.sinr, tele.cqi,
                    target_bw, fast_rx_kbps, dt);
        }
    }
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGUSR1, sig_handler);

    printf("aiqosd v%s starting (P1 dual-track)\n", AIQOSD_VERSION);

    scenario_init();
    kalman_init(&g_kalman, 0.15f, 1.0f);
    g_active_cfg = scenario_get_config(SCENARIO_URBAN);
    pid_init(&g_pid, g_active_cfg);
    apply_uci_scenario();
    aiqos_rtt_init(&g_rtt, 45);
    nlctrl_init();

    {
        FILE *b0fp = fopen("/tmp/aiqos_b0.json", "r");
        float b0_val = 0.0f;
        if (b0fp) {
            char jbuf[128] = {0};
            if (fgets(jbuf, sizeof(jbuf), b0fp)) {
                char *bp = strstr(jbuf, "\"b0\":");
                if (bp) b0_val = strtof(bp + 5, NULL);
            }
            fclose(b0fp);
        }
        adrc_configure_params(&g_adrc, 5.0f, b0_val);
        if (b0_val > 0.01f && b0_val < 10.0f) {
            g_adrc.calibrated = 1;
            FILE *sfp = popen("uci -q get aiqos.@main[0].adrc_shadow 2>/dev/null", "r");
            if (sfp) {
                char sbuf[8] = {0};
                if (fgets(sbuf, sizeof(sbuf), sfp) && strncmp(sbuf, "1", 1) == 0)
                    g_adrc_shadow = 1;
                pclose(sfp);
            }
            if (g_adrc_shadow)
                syslog(LOG_INFO, "ADRC shadow ON (b0=%.4f, w0=%.0f)", g_adrc.params.b0, g_adrc.params.w0);
        }
    }

    {
        FILE *cfp = popen("uci -q get aiqos.@main[0].calibration 2>/dev/null", "r");
        if (cfp) {
            char cbuf[8] = {0};
            if (fgets(cbuf, sizeof(cbuf), cfp) && strncmp(cbuf, "1", 1) == 0) {
                adrc_controller_t adrc = {0};
                adrc_configure_params(&adrc, 50.0f, 0.0f);
                syslog(LOG_INFO, "ADRC: calibration mode detected, starting b0");
                long long calib_base = 0;
                FILE *bwfp = fopen("/tmp/aiqos_target_bw", "r");
                if (bwfp) { unsigned int bwtmp = 0; if (fscanf(bwfp, "%u", &bwtmp) == 1 && bwtmp > 5000) calib_base = (long long)bwtmp * 1000; fclose(bwfp); }
                adrc_calib_run(&adrc, calib_base, 300, 3);
                /* R0-4/H5 治理 (2026-09-12)：原 `(void)cret;` 显式丢弃返回值 —— 若 uci set/commit
                 * 失败（flash 满、config 锁冲突、权限），calibration 会**保持 1** → 每次重启都重跑
                 * 一遍 300 轮 ADRC 校准（启动期长阻塞 + 行为漂移），且**无任何日志**。
                 * 改为判失败并记录，把静默失败变成可定位证据。 */
                int cret = system("uci set aiqos.@main[0].calibration=0 && uci commit aiqos");
                if (cret != 0)
                    syslog(LOG_WARNING,
                           "ADRC: clear calibration flag failed (rc=%d) — next boot will re-run calibration",
                           cret);
            }
            pclose(cfp);
        }
    }

    shm_init();
    main_loop();

    shm_close();
    nlctrl_close();
    printf("aiqosd: stopped\n");
    return 0;
}