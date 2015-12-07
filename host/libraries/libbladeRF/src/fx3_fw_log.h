/*
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

#ifndef FX3_FW_LOG_H_
#define FX3_FW_LOG_H_

#include "host_config.h"

/**
 * Read and parse the contents of the log in the FX3 firmware and dump
 * it to the specified file
 *
 * @param   dev         Device to access
 * @param   filename    File to write to. If NULL, information will be
 *                      printed to stdout.
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int fx3_fw_log_dump(struct bladerf *dev, const char *filename);

#endif
