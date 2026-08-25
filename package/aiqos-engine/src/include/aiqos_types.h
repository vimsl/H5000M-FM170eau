#ifndef AIQOS_TYPES_H
#define AIQOS_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * AiQoS shared type definitions v4.3
 *
 * Design constraints:
 *   1. SHM struct must be __packed__ - Go binary.Write has no padding
 *   2. Fields ordered 8->4->2->1 byte descending for natural alignment
 *   3. SeqLock protocol: odd=writing, even=data stable
 *   4. All integers use stdint fixed-width for cross-language offset determinism
 * ============================================================ */

/* ---- Path and protocol constants ---- */
#define AIQOS_SHM_PATH       "/dev/shm/aiqos_state"
#define AIQOS_SHM_MAGIC      0x41495153    /* ASCII: "AIQS" */
#define AIQOS_COLLECTOR_MS   2000          /* Collector poll interval (AT ~2s) */
#define AIQOS_COLD_START_S   1.5f
#define AIQOS_WATCHDOG_S     15
#define AIQOS_DEFAULT_IFACE  "wwan0"       /* qmi_wwan_f driver data interface */

#define AIQOS_SHM_SIZE       160           /* sizeof(ShmTelemetry) packed */

#define CLAMP(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))

/* ---- SHM Telemetry struct (SeqLock + packed, byte-exact with Go) ----
 *
 * Memory layout (packed, no compiler padding):
 *   offset   0: magic          (4)
 *   offset   4: seq            (4)  SeqLock: odd=writing, even=stable
 *   offset   8: timestamp_us   (8)
 *   offset  16: rx_bytes       (4)
 *   offset  20: tx_bytes       (4)
 *   offset  24: ambr_dl_kbps   (4)
 *   offset  28: ambr_ul_kbps   (4)
 *   offset  32: rsrp[4]        (16)
 *   offset  48: sinr           (4)
 *   offset  52: rsrq           (4)
 *   offset  56: doppler_hz     (4)
 *   offset  60: modem_temp     (4)
 *   offset  64: pci            (4)
 *   offset  68: arfcn          (4)
 *   offset  72: band           (4)
 *   offset  76: qci            (4)
 *   offset  80: ri             (4)
 *   offset  84: cqi            (4)
 *   offset  88: handover_count (4)
 *   offset  92: pci_switch_rate(4)
 *   offset  96: cell_id[32]    (32)
 *   offset 128: operator[32]   (32)
 *   Total: 160 bytes
 */
typedef struct __attribute__((packed)) {
    /* SeqLock header */
    uint32_t magic;            /* offset 0:  must == AIQOS_SHM_MAGIC */
    uint32_t seq;              /* offset 4:  +1 each write; odd=writing, even=stable */

    /* 64-bit timestamp (packed: offset 8, naturally 8-byte aligned) */
    uint64_t timestamp_us;     /* offset 8:  last write microsecond timestamp */

    /* 32-bit traffic counters */
    uint32_t rx_bytes;         /* offset 16 */
    uint32_t tx_bytes;         /* offset 20 */
    uint32_t ambr_dl_kbps;    /* offset 24: subscribed downlink AMBR */
    uint32_t ambr_ul_kbps;    /* offset 28: subscribed uplink AMBR */

    /* 32-bit float RF parameters */
    float    rsrp[4];          /* offset 32: 4-antenna RSRP dBm */
    float    sinr;             /* offset 48: primary SINR dB */
    float    rsrq;             /* offset 52: RSRQ dB */
    float    doppler_hz;       /* offset 56: Doppler shift Hz */
    float    modem_temp;       /* offset 60: modem temperature C */

    /* 32-bit cell/channel integers */
    int32_t  pci;              /* offset 64: Physical Cell ID */
    int32_t  arfcn;            /* offset 68: Absolute ARFCN */
    int32_t  band;             /* offset 72: NR band number */
    int32_t  qci;              /* offset 76: QCI/5QI bearer class */
    int32_t  ri;               /* offset 80: MIMO Rank 1~4 */
    int32_t  cqi;              /* offset 84: CQI 0~15 */
    int32_t  handover_count;   /* offset 88: handover count */
    int32_t  pci_switch_rate;  /* offset 92: PCI switch rate */

    /* Fixed-length strings (null-padded) */
    char     cell_id[32];      /* offset  96: global cell identity */
    char     operator[32];     /* offset 128: operator name */
} ShmTelemetry;
/* sizeof(ShmTelemetry) = 160 (__packed__) */

/* ---- Scenario enumeration ---- */
typedef enum {
    SCENARIO_URBAN = 0,
    SCENARIO_RURAL,
    SCENARIO_WILDERNESS,
    SCENARIO_HIGH_SPEED_RAIL,
    SCENARIO_DARK_CCI,
    SCENARIO_DARK_THROTTLE,
    SCENARIO_COUNT
} ScenarioType;

/* ---- Scenario configuration ---- */
typedef struct {
    float    Q;                /* Kalman process noise */
    float    R;                /* Kalman measurement noise */
    float    Kp, Ki, Kd;       /* PID gains */
    uint32_t min_bw_kbit;      /* minimum guaranteed bandwidth */
    uint32_t max_bw_kbit;      /* maximum bandwidth cap */
    float    cqi_drop_alpha;   /* CQI derivative lookahead coefficient */
    float    soft_cap_init;    /* soft_cap initial value */
    float    soft_cap_decay;   /* soft_cap exponential decay factor */
    bool     freeze_on_handover;
    int32_t  freeze_duration_ms;
    const char *name;
    const char *description;
} ScenarioConfig;

/* ---- 3D spatial guidance result ---- */
typedef struct {
    int32_t  score;            /* 3D-SRQI 0~100 */
    float    azimuth_offset_deg;
    float    elevation_offset_deg;
    float    delta_v_h;
    float    delta_h_v;
    int32_t  rank;
    char     advice[256];
} SpatialGuidanceResult;

/* ---- Antenna raw telemetry ---- */
typedef struct {
    float   rsrp[4];
    float   sinr;
    int32_t ri;
    int32_t pci_switch_rate;
    int32_t feeder_length_m;
} AntennaRawTelemetry;

#ifdef __cplusplus
}
#endif

#endif /* AIQOS_TYPES_H */