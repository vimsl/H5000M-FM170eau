#ifndef AIQOS_FACTORY_CAL_H
#define AIQOS_FACTORY_CAL_H

#include <stdint.h>

#define FACTORY_MAGIC 0x4149434C  /* "AICL" */
#define FACTORY_MTD   "/dev/mtd3"

struct FactoryCalibrationData {
    uint32_t magic;
    uint16_t version;
    uint8_t  pcba_sn[32];
    float    rf_gain_offset[4];   /* 4 路天线增益偏差 dB */
    float    dc_ref_voltage;      /* 参考电压 */
    uint8_t  reserved[64];
    uint16_t crc16;
} __attribute__((packed));

uint16_t crc16_ccitt(const uint8_t *data, size_t len);
int factory_cal_read(struct FactoryCalibrationData *cal);
float factory_cal_get_gain_offset(int antenna_index);

#endif