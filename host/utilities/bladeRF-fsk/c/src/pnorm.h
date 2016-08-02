/**
 * @file
 * @brief   Normalizes the input IQ signal [-1.0, 1.0] to a power of 1.0
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
#ifndef PNORM_H_
#define PNORM_H_

#include <stdint.h>
#include <stdbool.h>
#include "common.h"    //For struct complex_sample

#define SAMP_MAX_ABS 2048
#define CLAMP_VAL_ABS 3072

struct pnorm_state_t ;

struct pnorm_state_t *pnorm_init(float alpha, float min_gain, float max_gain) ;
void pnorm_reset(struct pnorm_state_t *state) ;
void pnorm_deinit(struct pnorm_state_t *state) ;
void pnorm_hold(struct pnorm_state_t *state, bool val) ;

/**
 * Power normalize a set of samples.
 * The 'ests' and 'gains' output buffers are for extra debug information. If they are NULL,
 * the function will not attempt to place values in them.
 */
void pnorm(struct pnorm_state_t *state, uint16_t length, struct complex_sample *in,
            struct complex_sample *out, float *est, float *gain);

#endif
