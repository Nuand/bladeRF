/**
 * Routines for parsing and handling device identifier strings
 */
#ifndef DEVICE_IDENTIFIER_H__
#define DEVICE_IDENTIFIER_H__


/**
 * Fill out a device info structure based upon the provided device indentifer
 * string. If a failure occurrs, the contents of d are undefined.
 *
 * For device identifier format, see the documentation for bladerf_open
 * (in include/libbladeRF.h)
 *
 * @param[in]  device_identifier   Device identifier string
 * @param[out] d                   Device info to fill in
 *
 * @return 0 on success, BLADERF_ERR_* on failure
 */
int str2devinfo(const char *device_identifier, struct bladerf_devinfo *d);

#endif
