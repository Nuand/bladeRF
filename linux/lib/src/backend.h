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

/**
 * Probe for devices, filling in the provided devinfo list and size of
 * the list that gets populated
 *
 * @param[out]  devinfo_items
 * @param[out]  num_items
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int backend_probe(struct bladerf_devinfo **devinfo_items, size_t *num_items);

#endif

