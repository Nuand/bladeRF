#ifndef BACKEND_H__
#define BACKEND_H__

#include "bladerf_priv.h"

/**
 * Open the device using the backend specified the provided info
 *
 * @param[out]  device  On success, updated with device handle
 * @param[in]   info    Filled-in device info
 *
 * @return  0 on success, BLADERF_ERR_* code on failure
 */
int backend_open(struct bladerf **device,  struct bladerf_devinfo *info);

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

