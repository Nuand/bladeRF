/**
 * @file
 * @brief   FSK modulator/demodulator
 *
 * This file handles raw modulation and demodulation of bits using continuous-phase
 * frequency shift keying (CPFSK). fsk_mod() takes in an array of bytes and produces
 * a CPFSK signal in the form of baseband IQ samples. fsk_demod() takes in an array of
 * CPFSK IQ samples and produces an array of bytes.
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
#ifndef FSK_H
#define FSK_H

#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "common.h"

/* 32 points per revolution and 8 samples per symbol means one quarter turn per
   symbol = phase modulation index of pi/2 */

//Sample points per one revolution of the complex IQ unit circle
#define POINTS_PER_REV 32
//Sample points per symbol
#define SAMP_PER_SYMB 8
//NOTE: If you change these^ two macros, you'll need to change the FIR channel
//filter as well if you want the modem to work

struct fsk_handle;

/**
 * Initialize an allocate memory for an fsk handle
 *
 * @return    pointer to allocated/initiailized fsk_handle on success, NULL on failure
 */
struct fsk_handle *fsk_init(void);

/**
 * Deinitialize / deallocate memory for an fsk handle
 *
 * @param   pointer to fsk handle to close
 */
void fsk_close(struct fsk_handle *fsk);

/**
 * Convert an array of bytes to an array of CPFSK modulated IQ samples
 * Bit order: LSb transmitted first, MSb last
 * This function assumes an initial phase of 0 (1 + 0j)
 *
 * @param[in]   fsk             pointer to fsk handle
 * @param[in]   data_buf        bytes to transmit
 * @param[in]   num_bytes       number of bytes to transmit from data_buf
 * @param[out]  samples         buffer to place modulated IQ samples in
 *
 * @return      number of IQ samples modulated
 */
unsigned int fsk_mod(struct fsk_handle *fsk, uint8_t *data_buf, int num_bytes,
                        struct complex_sample *samples);

/**
 * Convert an array of modulated CPFSK IQ samples into an array of bytes.
 * Expected bit order: LSb arrives first, MSb arrives last.
 * This function will still work when the number of samples line up in a way that the
 * last byte is only partially demodulated (e.g. 3 of 8 bits demodulated, 6 of 8 samples
 * processed from the 4th bit). It saves off its state to the fsk_handle struct, so in 
 * the next call to fsk_demod(), it will resume and finish demodulating that byte.
 *
 * @param[in]   fsk             fsk_handle struct
 * @param[in]   samples         Array of baseband IQ samples to demodulate
 * @param[in]   num_samples     Number of samples in samples array
 * @param[in]   new_signal      Is this a new FSK signal or a continuation of a previous one?
 *                              If true, the first sample will define the initial phase. Otherwise,
 *                              the previous phase (from last call to fsk_demod()) will define the
 *                              initial phase.
 * @param[in]   num_bytes       number of bytes to attempt to demodulate. If -1, the function will
 *                              demod as many bytes as 'num_samples' will allow.
 * @param[out]  data_buf        buffer to place demodulated bytes in
 *
 * @return      number of bytes demodulated. This number will be less than the 'num_bytes' parameter
 *              if there weren't enough samples to demod 'num_bytes' bytes
 */
unsigned int fsk_demod(struct fsk_handle *fsk, struct complex_sample *samples,
                    int num_samples, bool new_signal, int num_bytes, uint8_t *data_buf);

#endif
