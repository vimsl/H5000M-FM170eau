#include "aiqos_helpers.h"
#include "aiqosd.h"

float aiqos_cold_start_bw(float sinr) {
    if (sinr >= 20.0f) return 200000.0f;
    if (sinr >= 15.0f) return 100000.0f;
    if (sinr >= 10.0f) return 50000.0f;
    if (sinr >=  5.0f) return 25000.0f;
    if (sinr >=  0.0f) return 10000.0f;
    return 5000.0f;
}

int aiqos_estimate_cqi(int cqi, float sinr) {
    if (cqi != 0) return cqi;
    if (sinr <= -10.0f) return 0;
    int est = (int)(sinr / 2.0f + 5.0f);
    if (est < 1) est = 1;
    if (est > 15) est = 15;
    return est;
}

float aiqos_qci_weight(int qci) {
    if (qci == 1) return 1.05f;
    if (qci <= 5) return 1.0f;
    return 0.92f;
}

bool aiqos_should_dispatch(uint32_t last_applied_kbps, uint32_t target_kbps, bool force) {
    if (force) return true;
    if (last_applied_kbps == 0) return true;
    int32_t diff = (int32_t)target_kbps - (int32_t)last_applied_kbps;
    int32_t threshold = (int32_t)(last_applied_kbps * AIQOSD_TC_CHANGE_PCT / 100);
    if (threshold < 1000) threshold = 1000;
    return diff > threshold || diff < -threshold;
}

uint32_t aiqos_calc_ul_bw(uint32_t target_dl_kbps, uint32_t ambr_ul_kbps) {
    uint32_t ul = target_dl_kbps / 4;
    if (ambr_ul_kbps > 0 && ul > ambr_ul_kbps) ul = ambr_ul_kbps;
    if (ul < 5000) ul = 5000;
    return ul;
}

bool aiqos_should_freeze(bool cfg_freeze, int prev_pci, int cur_pci, uint32_t handover_count) {
    return cfg_freeze && handover_count > 0 && prev_pci >= 0 && cur_pci != prev_pci;
}

void aiqos_rtt_init(AiqosRttTracker *t, uint32_t initial_rtt_ms) {
    if (!t) return;
    t->active_rtt_ms = initial_rtt_ms;
    t->last_target_ms = initial_rtt_ms;
    t->stable_ticks = 0;
    t->pending = false;
}

bool aiqos_rtt_update(AiqosRttTracker *t, uint32_t target_rtt_us) {
    if (!t) return false;
    uint32_t target_ms = target_rtt_us / 1000U;
    if (target_ms == t->active_rtt_ms) {
        t->stable_ticks = 0;
        return false;
    }
    if (target_ms != t->last_target_ms) {
        /* New target appeared, reset stability counter */
        t->stable_ticks = 1;
        t->last_target_ms = target_ms;
        return false;
    }
    t->stable_ticks++;
    if (t->stable_ticks >= 3) {
        t->active_rtt_ms = target_ms;
        t->stable_ticks = 0;
        t->pending = true;
        return true;
    }
    return false;
}

uint32_t aiqos_stale_cap(uint32_t target_kbps, bool stale) {
    if (stale && target_kbps > 30000U) return 30000U;
    return target_kbps;
}