/**
 * AiQoS v4.4 - aiqosd main daemon (P1 Dual-Track)
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
#include "scenario.h"
#include "kalman.h"
#include "pid.h"
#include "netlink_ctrl.h"
#include "shm_reader.h"
#include "spatial_guidance.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <sys/stat.h>

static volatile int g_running = 1;
static volatile sig_atomic_t g_reload_scenario = 0;
static int g_cold_start = 1;
static float g_uptime = 0.0f;
static int g_no_data_count = 0;
static int g_prev_pci = -1;
static int g_shm_ready = 0;
static uint64_t g_last_shm_ts = 0;  /* slow-track SHM timestamp */

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

/* Monotonic microsecond timestamp */
static uint64_t get_monotonic_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/* GPIO modem hard reset */
static void modem_hard_reset(void) {
    fprintf(stderr, "aiqosd: modem hard reset via GPIO\n");
    system("echo 0 > /sys/class/gpio/modem:reset/value 2>/dev/null");
    usleep(500000);
    system("echo 1 > /sys/class/gpio/modem:reset/value 2>/dev/null");
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
    FILE *p = popen("uci -q get aiqos.global.scenario 2>/dev/null", "r");
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
    fprintf(stderr, "aiqosd: scenario locked to %s\n", scenario_name(s));
}

/* Cold-start MCS lookup */
static float cold_start_bw(float sinr) {
    if (sinr >= 20.0f) return 200000.0f;
    if (sinr >= 15.0f) return 100000.0f;
    if (sinr >= 10.0f) return 50000.0f;
    if (sinr >=  5.0f) return 25000.0f;
    if (sinr >=  0.0f) return 10000.0f;
    return 5000.0f;
}

/* Write status files for rpcd/ubus */
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

    /* Profile params for LuCI panel */
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
        if (f) { fprintf(f, "%s", "ack-filter-aggressive"); fclose(f); }
    }

    /* Spatial guidance results for dashboard */
    AntennaRawTelemetry ant;
    memset(&ant, 0, sizeof(ant));
    for (int i = 0; i < 4; i++) ant.rsrp[i] = tele->rsrp[i];
    ant.sinr = tele->sinr;
    ant.ri = tele->ri;
    ant.pci_switch_rate = tele->pci_switch_rate;
    ant.feeder_length_m = 1;

    bool directional = antenna_mode_is("directional");
    bool omni = antenna_mode_is("omni");
    SpatialGuidanceResult sg = calculate_3d_alignment(&ant, directional);
    const char *fs = directional
                     ? (sg.rank >= 3 ? "directional_tracking" : "directional_locked")
                     : (omni ? "omni_locked" : "auto");
    write_spatial_files(&sg, &ant, fs);
}

static void main_loop(void) {
    ShmTelemetry tele;
    memset(&tele, 0, sizeof(tele));
    uint64_t prev_cycle_us = get_monotonic_us();
    uint64_t fast_prev_rx = 0;
    uint32_t last_applied_dl = 0;
    uint64_t last_shm_good_us = 0;
    int log_cnt = 0;

    /* Initial sysfs read */
    {
        uint64_t rx64 = 0, tx64 = 0;
        sysfs_read_iface_bytes("wwan0", &rx64, &tx64);
        fast_prev_rx = rx64;
    }

    while (g_running) {
        if (g_reload_scenario) {
            g_reload_scenario = 0;
            apply_uci_scenario();
        }

        /* Fast-track sleep: 100ms */
        usleep(AIQOSD_CONTROL_MS * 1000);

        uint64_t now_us = get_monotonic_us();
        float dt = (float)(now_us - prev_cycle_us) / 1000000.0f;
        if (dt < 0.05f) dt = 0.05f;   /* floor 50ms */
        if (dt > 10.0f) dt = 10.0f;    /* ceiling 10s */
        prev_cycle_us = now_us;
        g_uptime += dt;

        /* ========== SLOW TRACK: SHM poll (only when new data arrives) ========== */
        int shm_updated = 0;
        if (!g_shm_ready) {
            if (shm_init() == 0) {
                g_shm_ready = 1;
                fprintf(stderr, "aiqosd: SHM attached\n");
            }
        }
        if (g_shm_ready && shm_read(&tele) == 0) {
            if (tele.timestamp_us != g_last_shm_ts) {
                g_last_shm_ts = tele.timestamp_us;
                shm_updated = 1;
                g_no_data_count = 0;
                last_shm_good_us = now_us;
            }
        }
        /* Watchdog: monotonic time since last successful SHM update */
        if (last_shm_good_us > 0 && (now_us - last_shm_good_us) > (uint64_t)AIQOS_WATCHDOG_S * 1000000ULL) {
            fprintf(stderr, "aiqosd: no SHM data %ds, hard reset modem\n", AIQOS_WATCHDOG_S);
            modem_hard_reset();
            last_shm_good_us = now_us;  /* reset to avoid repeated triggers */
        }

        /* Slow-track updates (only on new SHM data) */
        if (shm_updated) {
            /* Scenario detection */
            /* Compute measured rate from SHM for scenario dark_throttle detection */
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

            /* Update Kalman with new RF data */
            kalman_set_params(&g_kalman, g_active_cfg->Q, g_active_cfg->R);

            /* CQI estimation from SINR (FM170 AT doesn't return CQI) */
            int effective_cqi = tele.cqi;
            if (effective_cqi == 0 && tele.sinr > -10.0f) {
                effective_cqi = (int)(tele.sinr / 2.0f + 5.0f);
                if (effective_cqi < 1) effective_cqi = 1;
                if (effective_cqi > 15) effective_cqi = 15;
            }
            tele.cqi = effective_cqi;

            /* QCI-weighted Kalman capacity estimate */
            float qci_w = (tele.qci == 1) ? 1.05f : ((tele.qci <= 5) ? 1.0f : 0.92f);
            float z_k = kalman_estimate_capacity(&tele, qci_w);
            kalman_update(&g_kalman, z_k);
        }

        /* Ensure we have a valid config */
        if (!g_active_cfg) g_active_cfg = scenario_get_config(SCENARIO_URBAN);

        /* ========== FAST TRACK: sysfs rate measurement (every 100ms) ========== */
        uint64_t rx64 = 0, tx64 = 0;
        sysfs_read_iface_bytes("wwan0", &rx64, &tx64);
        uint32_t current_rx = (uint32_t)(rx64 & 0xFFFFFFFF);

        /* Compute instantaneous rate (kbps) from sysfs delta */
        uint32_t fast_rx_kbps = 0;
        if (current_rx >= (uint32_t)fast_prev_rx && dt > 0) {
            uint32_t delta = current_rx - (uint32_t)fast_prev_rx;
            fast_rx_kbps = (uint32_t)((uint64_t)delta * 8 / (uint32_t)(dt * 1000));
        }
        fast_prev_rx = rx64;

        /* ========== PID CONTROL (every 100ms) ========== */
        uint32_t target_bw;
        if (g_cold_start && g_uptime < AIQOS_COLD_START_S) {
            target_bw = (uint32_t)cold_start_bw(tele.sinr);
            if (target_bw < g_active_cfg->min_bw_kbit)
                target_bw = g_active_cfg->min_bw_kbit;
        } else {
            if (g_cold_start) { g_cold_start = 0; pid_reset(&g_pid); }
            pid_update(&g_pid, g_kalman.x, fast_rx_kbps, tele.cqi, dt, g_active_cfg, tele.ambr_dl_kbps);
            target_bw = pid_get_target(&g_pid, g_active_cfg, tele.ambr_dl_kbps);
        }

        /* Handover freeze */
        if (g_active_cfg->freeze_on_handover && tele.handover_count > 0
            && g_prev_pci >= 0 && tele.pci != g_prev_pci) {
            pid_freeze(&g_pid, 3);
            target_bw = target_bw * 50 / 100;
        }
        g_prev_pci = tele.pci;

        /* ========== TC DISPATCH (2% change threshold) ========== */
        int dl_diff = (int)target_bw - (int)last_applied_dl;
        int threshold = (int)(last_applied_dl * AIQOSD_TC_CHANGE_PCT / 100);
        if (threshold < 1000) threshold = 1000;  /* minimum 1Mbps threshold */

        if (last_applied_dl == 0 || dl_diff > threshold || dl_diff < -threshold) {
            uint32_t ul_bw = target_bw * 15 / 100;
            if (tele.ambr_ul_kbps > 0 && ul_bw > tele.ambr_ul_kbps)
                ul_bw = tele.ambr_ul_kbps;
            if (ul_bw < 5000) ul_bw = 5000;

            nlctrl_set_cake_dual(target_bw * 1000, ul_bw * 1000);
            last_applied_dl = target_bw;
        }

        /* Status files (update every ~1s) */
        if (++log_cnt >= 10) {
            log_cnt = 0;
            write_status_files(&tele, target_bw);
            fprintf(stderr, "aiqosd: [%s] SINR=%.1f CQI=%d dl=%ukbps fast=%ukbps dt=%.3fs\n",
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

    fprintf(stderr, "aiqosd v%s starting (P1 dual-track)\n", AIQOSD_VERSION);

    scenario_init();
    kalman_init(&g_kalman, 0.15f, 1.0f);
    g_active_cfg = scenario_get_config(SCENARIO_URBAN);
    pid_init(&g_pid, g_active_cfg);
    apply_uci_scenario();

    nlctrl_init();
    shm_init();

    main_loop();

    shm_close();
    nlctrl_close();
    fprintf(stderr, "aiqosd: stopped\n");
    return 0;
}