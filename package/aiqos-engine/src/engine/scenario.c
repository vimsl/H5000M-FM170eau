/**
 * AiQoS v4.3 - 6 场景自动识别引擎
 * 4 主场景 + 2 暗场景，3 周期迟滞窗口防震荡
 */
#include "scenario.h"
#include <math.h>

static const ScenarioConfig g_configs[SCENARIO_COUNT] = {
    [SCENARIO_URBAN] = {
        .Q=0.15f, .R=1.00f,
        .Kp=1.20f, .Ki=0.08f, .Kd=0.25f,
        .min_bw_kbit=50000, .max_bw_kbit=1000000, .cake_rtt_ms=30,
        .cqi_drop_alpha=0.30f, .soft_cap_init=1000000.0f, .soft_cap_decay=0.98f,
        .freeze_on_handover=false, .freeze_duration_ms=0,
        .name="urban", .description="Urban Dense"
    },
    [SCENARIO_RURAL] = {
        .Q=0.05f, .R=2.50f,
        .Kp=0.80f, .Ki=0.03f, .Kd=0.15f,
        .min_bw_kbit=10000, .max_bw_kbit=500000, .cake_rtt_ms=65,
        .cqi_drop_alpha=0.20f, .soft_cap_init=500000.0f, .soft_cap_decay=0.99f,
        .freeze_on_handover=false, .freeze_duration_ms=0,
        .name="rural", .description="Rural Distance"
    },
    [SCENARIO_WILDERNESS] = {
        .Q=0.02f, .R=5.00f,
        .Kp=0.30f, .Ki=0.00f, .Kd=0.05f,
        .min_bw_kbit=512, .max_bw_kbit=200000, .cake_rtt_ms=100,
        .cqi_drop_alpha=0.0f, .soft_cap_init=200000.0f, .soft_cap_decay=1.0f,
        .freeze_on_handover=false, .freeze_duration_ms=0,
        .name="wilderness", .description="Wilderness Emergency"
    },
    [SCENARIO_HIGH_SPEED_RAIL] = {
        .Q=1.50f, .R=0.50f,
        .Kp=2.50f, .Ki=0.01f, .Kd=0.80f,
        .min_bw_kbit=5000, .max_bw_kbit=800000, .cake_rtt_ms=100,
        .cqi_drop_alpha=0.40f, .soft_cap_init=800000.0f, .soft_cap_decay=0.95f,
        .freeze_on_handover=true, .freeze_duration_ms=1500,
        .name="high_speed_rail", .description="High-Speed Rail"
    },
    [SCENARIO_DARK_CCI] = {
        .Q=0.02f, .R=5.00f,
        .Kp=0.50f, .Ki=0.00f, .Kd=0.10f,
        .min_bw_kbit=5000, .max_bw_kbit=300000, .cake_rtt_ms=65,
        .cqi_drop_alpha=0.0f, .soft_cap_init=300000.0f, .soft_cap_decay=1.0f,
        .freeze_on_handover=false, .freeze_duration_ms=0,
        .name="dark_cci", .description="Dark: Co-Channel Interference"
    },
    [SCENARIO_DARK_THROTTLE] = {
        .Q=0.10f, .R=1.50f,
        .Kp=0.80f, .Ki=0.02f, .Kd=0.20f,
        .min_bw_kbit=5000, .max_bw_kbit=500000, .cake_rtt_ms=100,
        .cqi_drop_alpha=0.10f, .soft_cap_init=500000.0f, .soft_cap_decay=0.995f,
        .freeze_on_handover=false, .freeze_duration_ms=0,
        .name="dark_throttle", .description="Dark: Core Network Throttle"
    }
};

static uint32_t g_measured_rx_kbps = 0;  /* measured throughput from main loop */

void scenario_set_measured_rate(uint32_t rx_kbps) {
    g_measured_rx_kbps = rx_kbps;
}

static bool g_locked = false;
static ScenarioType g_locked_scenario = SCENARIO_URBAN;
static ScenarioType g_current = SCENARIO_URBAN;
static ScenarioType g_candidate = SCENARIO_URBAN;
static int g_consecutive = 0;
#define HYSTERESIS_WINDOW 3

void scenario_init(void) {
    g_locked = false;
    g_current = SCENARIO_URBAN;
    g_candidate = SCENARIO_URBAN;
    g_consecutive = 0;
}

static ScenarioType detect_raw(const ShmTelemetry *tele) {
    float rsrp_max = tele->rsrp[0];
    for (int i = 1; i < 4; i++) {
        if (tele->rsrp[i] > rsrp_max) rsrp_max = tele->rsrp[i];
    }

    /* 暗场景2: 核心网限速 - 实测速率达到 AMBR 95% 以上 */
    if (tele->ambr_dl_kbps > 0 && g_measured_rx_kbps > 0) {
        if (g_measured_rx_kbps >= tele->ambr_dl_kbps * 95 / 100) {
            return SCENARIO_DARK_THROTTLE;
        }
    }

    /* 暗场景1: 强同频干扰 - RSRP 好但 SINR 差 */
    if (rsrp_max > -80.0f && tele->sinr < 3.0f) {
        return SCENARIO_DARK_CCI;
    }

    /* 高铁: 切换频繁或多普勒大 */
    if (tele->handover_count >= 4 || tele->doppler_hz > 400.0f) {
        return SCENARIO_HIGH_SPEED_RAIL;
    }

    /* 野外: 极弱信号或模组过热 */
    if (rsrp_max < -115.0f || tele->modem_temp > 75.0f) {
        return SCENARIO_WILDERNESS;
    }

    /* 乡村: 弱信号 + 低信噪比 (部署报告 4.2 设计: rural = RSRP<-95, SINR 5-15) */
    if (rsrp_max < -95.0f) {
        return SCENARIO_RURAL;
    }
    /* 中间区间 -80 ~ -95 dBm: SINR 良好(>=8dB)时信道容量仍可用, 归 urban 不限速;
     * 仅当 SINR 也差(<8dB)才判 rural, 避免把中等信号误判为弱网限速 */
    if (rsrp_max < -80.0f && tele->sinr < 8.0f) {
        return SCENARIO_RURAL;
    }

    return SCENARIO_URBAN;
}

ScenarioType scenario_detect(const ShmTelemetry *tele) {
    if (g_locked) return g_locked_scenario;

    ScenarioType detected = detect_raw(tele);

    if (detected == g_candidate) {
        g_consecutive++;
        if (g_consecutive >= HYSTERESIS_WINDOW) {
            g_current = detected;
        }
    } else {
        g_candidate = detected;
        g_consecutive = 1;
    }

    return g_current;
}

uint32_t scenario_get_target_rtt(const ShmTelemetry *tele) {
    float rsrp_max = tele->rsrp[0];
    for (int i = 1; i < 4; i++) {
        if (tele->rsrp[i] > rsrp_max) rsrp_max = tele->rsrp[i];
    }

    if (tele->handover_count >= 4 || rsrp_max < -110.0f) return 100000;
    if (tele->ri <= 1 || tele->sinr < 4.0f) return 65000;
    if (tele->sinr >= 15.0f && tele->ri >= 2) return 30000;
    return g_configs[g_current].cake_rtt_ms * 1000U;
}

const ScenarioConfig *scenario_get_config(ScenarioType s) {
    if (s < 0 || s >= SCENARIO_COUNT) s = SCENARIO_URBAN;
    return &g_configs[s];
}

const char *scenario_name(ScenarioType s) {
    if (s < 0 || s >= SCENARIO_COUNT) return "unknown";
    return g_configs[s].name;
}

void scenario_lock(ScenarioType s) {
    if (s >= 0 && s < SCENARIO_COUNT) {
        g_locked = true;
        g_locked_scenario = s;
        g_current = s;
    }
}

void scenario_unlock(void) { g_locked = false; }
bool scenario_is_locked(void) { return g_locked; }