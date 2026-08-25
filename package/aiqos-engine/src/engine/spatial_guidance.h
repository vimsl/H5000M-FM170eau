#ifndef AIQOS_SPATIAL_GUIDANCE_H
#define AIQOS_SPATIAL_GUIDANCE_H

#include "aiqos_types.h"

SpatialGuidanceResult calculate_3d_alignment(const AntennaRawTelemetry *tele,
                                              bool is_directional);

void write_spatial_files(const SpatialGuidanceResult *res,
                         const AntennaRawTelemetry *tele,
                         const char *failsafe_status);

#endif