#ifndef AIQOS_SHM_READER_H
#define AIQOS_SHM_READER_H

#include "aiqos_types.h"

int shm_init(void);
int shm_read(ShmTelemetry *out);
void shm_close(void);

#endif