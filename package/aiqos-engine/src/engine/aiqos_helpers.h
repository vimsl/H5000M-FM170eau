#ifndef AIQOS_HELPERS_H
#define AIQOS_HELPERS_H

#include <stdint.h>
#include <stdbool.h>

/* Cold-start bandwidth lookup (kbps) from SINR */
float aiqos_cold_start_bw(float sinr);

/* CQI estimation fallback when modem returns 0: sinr/2+5 clamped [1,15] */
int aiqos_estimate_cqi(int cqi, float sinr);

/* QCI weighting for Kalman capacity estimate */
float aiqos_qci_weight(int qci);

/* TC dispatch hysteresis: >2% change or minimum 1000kbps, or forced */
bool aiqos_should_dispatch(uint32_t last_applied_kbps, uint32_t target_kbps, bool force);

/* Upstream bandwidth: target/4, AMBR-capped, min 5000 kbps */
uint32_t aiqos_calc_ul_bw(uint32_t target_dl_kbps, uint32_t ambr_ul_kbps);

/* Handover freeze: should we cut bandwidth and freeze PID */
bool aiqos_should_freeze(bool cfg_freeze, int prev_pci, int cur_pci, uint32_t handover_count);

/* RTT stability tracker: requires 3 consecutive samples to switch */
typedef struct {
    uint32_t active_rtt_ms;
    uint32_t last_target_ms;
    int      stable_ticks;
    bool     pending;
} AiqosRttTracker;

void aiqos_rtt_init(AiqosRttTracker *t, uint32_t initial_rtt_ms);
bool aiqos_rtt_update(AiqosRttTracker *t, uint32_t target_rtt_us);

/* Stale safety cap: clamp target to 30000 kbps when watchdog stale */
uint32_t aiqos_stale_cap(uint32_t target_kbps, bool stale);

#endif