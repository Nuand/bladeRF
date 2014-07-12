/**
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014 Nuand LLC
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef BLADERF_FLASH_FIELDS_H_
#define BLADERF_FLASH_FIELDS_H_

#include "libbladeRF.h"

#define OTP_BUFFER_SIZE 256

/**
 * Read data from calibration ("cal") section of flash
 *
 * @param[in]   dev         Device handle
 * @param[in]   field       Cal field
 * @param[out]  data        Populated with retrieved data
 * @param[in]   data_size   Size of the data to read
 *
 * 0 on success, BLADERF_ERR_* on failure
 */
int get_otp_field(struct bladerf *device, char *field,
                  char *data, size_t data_size);

/**
 * Retrieve the device serial from flash and store it in the provided buffer.
 *
 * @pre The provided buffer is BLADERF_SERIAL_LENGTH in size
 *
 * @param[inout]   dev      Device handle. On success, serial field is updated
 *
 * 0 on success, BLADERF_ERR_* on failure
 */
int read_serial(struct bladerf *device, char *serial_buf);

/**
 * Retrieve VCTCXO calibration value from flash and cache it in the
 * provided device structure
 *
 * @param[inout]   dev      Device handle. On success, trim field is updated
 *
 * 0 on success, BLADERF_ERR_* on failure
 */
int get_and_cache_vctcxo_trim(struct bladerf *device);

/**
 * Retrieve FPGA size variant from flash and cache it in the provided
 * device structure
 *
 * @param[inout]   dev      Device handle.
 *                          On success, fpga_size field  is updated
 *
 * 0 on success, BLADERF_ERR_* on failure
 */
int get_and_cache_fpga_size(struct bladerf *device);

/**
 * Create data that can be read by extract_field()
 *
 * @param[in]       ptr     Pointer to data buffer that will contain encoded data
 * @param[in]       len     Length of data buffer that will contain encoded data
 * @param[inout]    idx     Pointer indicating next free byte inside of data
 *                          buffer that will contain encoded data
 * @param[in]       field   Key of value to be stored in encoded data buffer
 * @param[in]       val     Value to be stored in encoded data buffer
 *
 * 0 on success, BLADERF_ERR_* on failure
 */
int encode_field(char *ptr, int len, int *idx, const char *field,
                 const char *val);

/**
 * Add kv association to data region readable by extract_field()
 *
 * @param[in]    buf    Buffer to add field to
 * @param[in]    len    Length of `buf' in bytes
 * @param[in]    field  Key of value to be stored in encoded data buffer
 * @param[in]    val    Value associated with key `field'
 *
 * 0 on success, BLADERF_ERR_* on failure
 */
int add_field(char *buf, int len, const char *field, const char *val);

#endif
