/**
 * @file interleave.h
 *
 * This file is not part of the API and may be changed at any time.
 * If you're interfacing with libbladeRF, DO NOT use this file.
 *
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2017 Nuand LLC
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

#ifndef HELPERS_INTERLEAVE_H_
#define HELPERS_INTERLEAVE_H_

size_t _interleave_calc_bytes_per_sample(bladerf_format format);
size_t _interleave_calc_metadata_bytes(bladerf_format format);
size_t _interleave_calc_num_channels(bladerf_channel_layout layout);

int _interleave_interleave_buf(bladerf_channel_layout layout,
                               bladerf_format format,
                               unsigned int buffer_size,
                               void *samples);

int _interleave_deinterleave_buf(bladerf_channel_layout layout,
                                 bladerf_format format,
                                 unsigned int buffer_size,
                                 void *samples);

#endif
