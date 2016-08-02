/**
 * @file
 * @brief   Cross correlates the input waveform with the preamble waveform and
 *          detects peaks
 *
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2016 Nuand LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef CORRELATOR_H_
#define CORRELATOR_H_

#include <stdint.h>
#include "common.h"
#include "fsk.h"

struct correlator;

#define CORRELATOR_NO_RESULT   (UINT64_MAX)
//Amount to decimate by. If 1, no decimation will occur. If 2, the correlator will
//use every other sample. If 3 the correlator will use every third sample. And so on.
#define DECIMATION_FACTOR 2

/**
 * Create a correlator. This is currently limited to symbol lengths that are
 * a multiple of 8 (a byte).
 *
 * @param   syms                Symbols to correlate against
 * @param   n                   Number of symbols in `syms`. Must be a multiple of 8.
 * @param   sps                 Samples per symbol for modulation.
 *
 * @return correlator handle on success, NULL on failure
 */
struct correlator *corr_init(uint8_t *syms, size_t n, unsigned int sps);

/**
 * Deinitialize and deallocate the provided correlator
 */
void corr_deinit(struct correlator *corr);

/**
 * Reset the state of the corrlator. This should be done after a corr_process()
 * returns a result.
 */
void corr_reset(struct correlator *corr);

/**
 * Process samples and return timestamp of when correlation occurred
 *
 * @param   samples     Samples to process
 * @param   n           Number of provided samples
 * @param   timestamp   Timestamp of first samples in `samples`
 *
 * @return The timestamp at which the correlation was found to occur,
 *         or CORRELATOR_NO_RESULT if no match was detected.
 */
uint64_t corr_process(struct correlator *corr,
                      const struct complex_sample *samples, size_t n,
                      uint64_t timestamp);


#endif
