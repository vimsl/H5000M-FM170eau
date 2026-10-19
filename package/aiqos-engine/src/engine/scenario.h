#ifndef AIQOS_SCENARIO_H
#define AIQOS_SCENARIO_H

#include "aiqos_types.h"

void scenario_init(void);
void scenario_set_measured_rate(uint32_t rx_kbps);
ScenarioType scenario_detect(const ShmTelemetry *tele);
uint32_t scenario_get_target_rtt(const ShmTelemetry *tele);
const ScenarioConfig *scenario_get_config(ScenarioType s);
const char *scenario_name(ScenarioType s);
void scenario_lock(ScenarioType s);
void scenario_unlock(void);
bool scenario_is_locked(void);

#endif