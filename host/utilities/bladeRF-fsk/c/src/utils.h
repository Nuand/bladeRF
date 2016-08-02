/**
 * @file
 * @brief   Utility functions: loading/saving samples from/to a file,
 *          conversions, etc.
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

#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include "host_config.h"

#if BLADERF_OS_WINDOWS || BLADERF_OS_OSX
    #include "clock_gettime.h"
#else
    #include <time.h>
#endif

#include "common.h"

/**
 * Write samples to csv file
 *
 * @param[in]   filename        name of csv file to write
 * @param[in]   samples         buffer containing IQ samples to write
 * @param[in]   num_samples     number of IQ samples to write to the file
 *
 * @return      0 on success, -1 on failure
 */
int write_samples_to_csv_file(char *filename, int16_t *samples, int num_samples);

/**
 * Load samples from csv file into heap memory buffer. Caller is responsible for
 * freeing the the buffer when done using it.
 *
 * @param[in]   filename        file name to load samples from
 * @param[in]   pad_zeros       If true, the function will pad zeros onto the end until
 *                              the samples fill to a multiple of 'buffer_size' samples
 * @param[in]   buffer_size     buffer size to fill multiples of, if pad_zeros is true.
 *                              Parameter ignored if pad_zeros is false.
 * @param[out]  samples         pointer to the loaded sample buffer. Buffer set to NULL
 *                              on failure. If successful, the buffer must be freed when
 *                              done using it.
 * @return      number of samples loaded on success; -1 on failure
 */
int load_samples_from_csv_file(char *filename, bool pad_zeros, int buffer_size,
                                int16_t **samples);

/**
 * Write 'struct complex_sample' samples to csv file
 *
 * @param[in]   filename        name of csv file to write
 * @param[in]   samples         buffer containing IQ samples to write
 * @param[in]   num_samples     number of IQ samples to write to the file
 *
 * @return      0 on success, -1 on failure
 */
int write_struct_samples_to_csv_file(char *filename, struct complex_sample *samples,
                                        int num_samples);

/**
 * Load 'struct complex_sample' samples from csv file into heap memory buffer.
 * Caller is responsible for freeing the the buffer when done using it.
 *
 * @param[in]   filename        file name to load samples from
 * @param[in]   pad_zeros       If true, the function will pad zeros onto the end until
 *                              the samples fill to a multiple of 'buffer_size' samples
 * @param[in]   buffer_size     buffer size to fill multiples of, if pad_zeros is true.
 *                              Parameter ignored if pad_zeros is false.
 * @param[out]  samples         pointer to the loaded sample buffer. Buffer set to NULL
 *                              on failure. If successful, the buffer must be freed when
 *                              done using it.
 * @return      number of samples loaded on success; -1 on failure
 */
int load_struct_samples_from_csv_file(char *filename, bool pad_zeros, int buffer_size,
                                        struct complex_sample **samples);

void print_chars(uint8_t *data, int num_bytes);
void print_hex_contents(uint8_t *data, int num_bytes);

/**
 * Converts a relative timeout to an absolute time (i.e. 5000 ms to 07/02/2016 20:32:10)
 *
 * @param[in]   timeout_ms      the timeout to create in ms
 * @param[out]  timeout_abs     pointer to timespec struct to fill
 *
 * @return      0 on success, -1 on failure
 */
int create_timeout_abs(unsigned int timeout_ms, struct timespec *timeout_abs);

/**
 * Converts an array of int16_t IQ samples to an array of 'complex_sample' structs.
 * Caller is responsible for memory allocation.
 * 
 * @param[in]   samples         array of IQ samples to convert
 * @param[in]   num_samples     number of samples to convert
 * @param[out]  struct_samples  buffer to place converted samples in
 */
void conv_samples_to_struct(int16_t *samples, unsigned int num_samples,
                            struct complex_sample *struct_samples);

/**
 * Converts an array of complex_sample structs into an array of int16_t
 * samples. Caller is responsible for memory allocation.
 * 
 * @param[in]   struct_samples  complex_sample array to convert
 * @param[in]   num_samples     number of samples to convert
 * @param[out]  samples         buffer to place converted samples in
 */
void conv_struct_to_samples(struct complex_sample *struct_samples, unsigned int num_samples,
                            int16_t *samples);

#endif
