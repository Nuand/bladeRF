#ifndef BACKEND_H__
#define BACKEND_H__

#include "bladerf_priv.h"

/**
 * Open the device using the backend specified the provided info
 *
 * @param   info    Filled-in device info
 * @return  Opened device handle on success, NULL on failure
 */
struct bladerf * backend_open(struct bladerf_devinfo *info);

// XXX TBD
int backend_probe();

#endif

