/**
 * @file xb.h
 *
 * @brief XB-200 support
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

#ifndef EXPANSION_XB200_H_
#define EXPANSION_XB200_H_

#include <stdbool.h>

#include <libbladeRF.h>

#include "board/board.h"

int xb200_attach(struct bladerf *dev);
void xb200_detach(struct bladerf *dev);
int xb200_enable(struct bladerf *dev, bool enable);
int xb200_init(struct bladerf *dev);

/**
 * Select an XB-200 filterbank
 *
 * @param       dev     Device handle
 * @param[in]   ch      Channel
 * @param[in]   filter  XB200 filterbank
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int xb200_set_filterbank(struct bladerf *dev,
                         bladerf_channel ch,
                         bladerf_xb200_filter filter);

/**
 * Select an appropriate filterbank, based upon the specified frequency
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[in]   frequency   Frequency
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int xb200_auto_filter_selection(struct bladerf *dev,
                                bladerf_channel ch,
                                uint64_t frequency);

/**
 * Get the current selected XB-200 filterbank
 *
 * @param       dev        Device handle
 * @param[in]   ch         Channel
 * @param[out]  filter     Pointer to filterbank, only updated if return value
 *                          is 0.
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int xb200_get_filterbank(struct bladerf *dev,
                         bladerf_channel ch,
                         bladerf_xb200_filter *filter);
/**
 * Configure the XB-200 signal path
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[in]   path        Desired XB-200 signal path
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int xb200_set_path(struct bladerf *dev,
                   bladerf_channel ch,
                   bladerf_xb200_path path);

/**
 * Get the current XB-200 signal path
 *
 * @param       dev         Device handle
 * @param[in]   ch          Channel
 * @param[out]  path        Pointer to XB200 signal path
 *
 * @return 0 on success, value from \ref RETCODES list on failure
 */
int xb200_get_path(struct bladerf *dev,
                   bladerf_channel ch,
                   bladerf_xb200_path *path);

#endif
