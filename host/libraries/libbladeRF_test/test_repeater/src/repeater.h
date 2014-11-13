/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2013 Nuand LLC
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
#ifndef REPEATER_H__
#define REPEATER_H__

#define DEFAULT_NUM_BUFFERS         32
#define DEFAULT_NUM_TRANSFERS       16
#define DEFAULT_SAMPLES_PER_BUFFER  8192
#define DEFAULT_SAMPLE_RATE         1000000
#define DEFAULT_FREQUENCY           1000000000
#define DEFAULT_BANDWIDTH           DEFAULT_SAMPLE_RATE

/**
 * Application configuration
 */
struct repeater_config
{
    char *device_str;           /**< Device to use */

    int tx_freq;                /**< TX frequency */
    int rx_freq;                /**< RX frequency */
    int sample_rate;            /**< Sampling rate */
    int bandwidth;              /**< Bandwidth */

    int num_buffers;            /**< Number of buffers to allocate and use */
    int num_transfers;          /**< Number of transfers to allocate and use */
    int samples_per_buffer;     /**< Number of SC16Q11 samples per buffer */


    bladerf_log_level verbosity;    /** Library verbosity */
};

/**
 * Initialize the provided application config to defaults
 *
 * @param   c   Configuration to initialize
 */
void repeater_config_init(struct repeater_config *c);

/**
 * Deinitialize configuration and free any heap-allocated items
 *
 * @param   c   Configuration to deinitialize
 */
void repeater_config_deinit(struct repeater_config *c);

/**
 * Kick off repeater test
 *
 * @param   c   Repeater configuration
 *
 * @return  0 on succes, non-zero on failure
 */
int repeater_run(struct repeater_config *c);

#endif
