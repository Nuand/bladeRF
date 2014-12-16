/*
 * This file defines functionality for reading and validating an FX3 firmware
 * image, and providing access to the image contents.
 *
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
#ifndef FX3_FW_H_
#define FX3_FW_H_

#include <stdint.h>
#include <stdbool.h>
#include "host_config.h"

struct fx3_firmware;

/**
 * Open, read, and validate the contents of an FX3 firmware file.
 *
 * @param[in]   file    Path to FX3 firmware image to open
 * @param[out]  fw      Handle to FX3 firmware data
 *
 * @return 0 on success,
 *         BLADERF_ERR_INVAL if image validation fails,
 *         BLADERF_ERR_* values on other errors.
 *
 */
int fx3_fw_read(const char *file, struct fx3_firmware **fw);

/**
 * Deallocation and deinitialize data stored in the provided fx3_firmware
 * structure
 *
 * @param   fw  Structure to deallocate
 */
void fx3_fw_deinit(struct fx3_firmware *fw);

/**
 * This function allows each section to be iterated over by calling it
 * repeatedly, until it returns false.
 *
 * @param[in]   fw              Handle FX3 firmware data
 * @param[out]  section_addr    Target RAM address of the section (on the FX3)
 * @param[out]  section_data    Updated to point to start of next section's data
 * @parma[out]  section_len     Length of the next section
 *
 * @return true if this function returned section data,
 *         false if the end of the FW has been reached and no data is available.
 */
bool fx3_fw_next_section(struct fx3_firmware *fw, uint32_t *section_addr,
                         uint8_t **section_data, uint32_t *section_len);

/**
 * @return The 32-bit little-endian address of the firmware entry point.
 */
uint32_t fx3_fw_entry_point(struct fx3_firmware *fw);

#endif
