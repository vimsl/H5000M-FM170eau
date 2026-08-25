/**
 * AiQoS - aiqosd core scheduling engine header
 *
 * P1 Dual-Track Control Architecture:
 *   Fast track (100ms): sysfs rx/tx bytes -> PID rate control -> TC dispatch
 *   Slow track (~2s):   AT/QMI via SHM -> Kalman channel estimation -> scenario
 *
 * Control loop:
 *   1. Fast-track sysfs read (every 100ms, <50us)
 *   2. Slow-track SHM poll (only when timestamp changes)
 *   3. Scenario state machine (3-sample hysteresis)
 *   4. QCI-weighted Kalman filter channel estimation
 *   5. dCQI/dt slope lookahead + A3 handover freeze
 *   6. APN-AMBR anchored soft_cap exponential probe
 *   7. Anti-windup dual-loop PID target bandwidth
 *   8. Netlink CAKE TC dispatch (ifb0 + wwan0, 2% change threshold)
 *   9. 1.5s cold-start privileged state
 *  10. 4-level self-healing watchdog
 */

#ifndef AIQOSD_H
#define AIQOSD_H

#include "aiqos_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Core timing constants ---- */
#define AIQOSD_VERSION           "4.4.0"
#define AIQOSD_CONTROL_MS        100      /* P1: Fast-track control period: 100ms (10Hz) */
#define AIQOSD_COLD_START_S      1.5f
#define AIQOSD_WATCHDOG_S        15
#define AIQOSD_AMBR_PROBE_MIN    10

/* ---- Bandwidth and interface constraints ---- */
#define AIQOSD_MIN_BW_KBPS       512
#define AIQOSD_MAX_BW_KBPS       1200000
#define AIQOSD_DEFAULT_IFACE     "wwan0"
#define AIQOSD_DEFAULT_IFB_IF    "ifb0"

/* ---- TC dispatch hysteresis ---- */
#define AIQOSD_TC_CHANGE_PCT     2        /* Only dispatch TC when bw changes > 2% */

/* ---- UCI config and system paths ---- */
#define AIQOSD_UCI_CONFIG        "aiqos"
#define AIQOSD_PID_FILE          "/var/run/aiqosd.pid"
#define AIQOSD_LOG_IDENT         "aiqosd"

/* ---- Fast-track sysfs reader ---- */
#include <stdio.h>
#include <stdint.h>

/*
 * Read cumulative rx/tx bytes from sysfs (< 50us per call).
 * Returns 0 on success, -1 on read failure.
 */
static inline int sysfs_read_iface_bytes(const char *ifname,
                                         uint64_t *rx_bytes,
                                         uint64_t *tx_bytes) {
    char path[128];
    FILE *fp;
    int ret = 0;

    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/rx_bytes", ifname);
    fp = fopen(path, "r");
    if (fp) {
        if (fscanf(fp, "%lu", rx_bytes) != 1) { *rx_bytes = 0; ret = -1; }
        fclose(fp);
    } else { *rx_bytes = 0; ret = -1; }

    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/tx_bytes", ifname);
    fp = fopen(path, "r");
    if (fp) {
        if (fscanf(fp, "%lu", tx_bytes) != 1) { *tx_bytes = 0; ret = -1; }
        fclose(fp);
    } else { *tx_bytes = 0; ret = -1; }

    return ret;
}

/* ---- Engine runtime context ---- */
typedef struct {
    /* Scenario state machine */
    ScenarioType current_scenario;
    ScenarioType candidate_scenario;
    int32_t      consecutive_hits;
    bool         is_paused;
    bool         in_cold_start;

    /* Kalman filter state */
    float        kalman_x;
    float        kalman_p;

    /* PID controller state */
    float        pid_integral;
    float        prev_error;
    uint8_t      prev_cqi;
    uint32_t     prev_rx_bytes;
    uint64_t     last_telemetry_us;

    /* Soft-cap probe state */
    uint32_t     dynamic_soft_cap;
    uint32_t     last_probe_timestamp;

    /* P1: Fast-track state */
    uint64_t     prev_cycle_time_us;     /* monotonic timestamp of previous cycle */
    uint64_t     last_shm_timestamp_us;  /* track slow-track SHM updates */
    uint64_t     fast_prev_rx_bytes;     /* sysfs rx_bytes from previous fast cycle */
    uint32_t     last_applied_dl_kbit;   /* TC dispatch hysteresis */

    /* Interface config */
    char         wan_ifname[16];
    char         ifb_ifname[16];
} AiqosContext;

/* ---- Core function declarations ---- */
int  aiqos_init_context(AiqosContext *ctx);
void aiqos_process_cycle(AiqosContext *ctx, const ShmTelemetry *tele);
int  aiqos_apply_netlink_tc(const char *dev, uint32_t bw_kbit, bool is_ingress);
void aiqos_destroy_context(AiqosContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* AIQOSD_H */