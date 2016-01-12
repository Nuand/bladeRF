/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014-2015 Nuand LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef DEVCFG_H_
#define DEVCFG_H_

#include <stdint.h>
#include <libbladeRF.h>

#include "rel_assert.h"
#include "host_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEVCFG_OPTIONS_BASE   "hd:l:v:s:b:f:"

/**
 * Device configuration parameters
 */
struct devcfg {
    char *device_specifier;

    unsigned int tx_frequency;
    unsigned int tx_bandwidth;
    unsigned int tx_samplerate;
    int txvga1;
    int txvga2;

    unsigned int rx_frequency;
    unsigned int rx_bandwidth;
    unsigned int rx_samplerate;
    bladerf_lna_gain lnagain;
    int rxvga1;
    int rxvga2;

    bladerf_loopback loopback;

    bladerf_log_level verbosity;

    bool enable_xb200;

    unsigned int samples_per_buffer;
    unsigned int num_buffers;
    unsigned int num_transfers;
    unsigned int stream_timeout_ms;
    unsigned int sync_timeout_ms;
};

/**
 * Initialize the specified device_config structure to default values
 *
 * @param   c   Configuration to initialize
 */
void devcfg_init(struct devcfg *c);

/**
 * Deinitialize any items allocated in the provided device configuration
 *
 * @param   c   Configuration to deinitialize
 */
void devcfg_deinit(struct devcfg *c);

/**
 * Creates an option array with the app-specific options appended to the common
 * devcfg options. The app_options should be 0-terminated.
 *
 * The caller is responsible for freeing the resturned structure.
 *
 * @return Heap-allocated array on success, NULL on failure
 */
struct option *devcfg_get_long_options(const struct option *app_options);

/**
 * Handle command line arguments and update the specified device configuration
 * accordingly.
 *
 * @param   argc            # of arguments in argv
 * @param   argv            Array of arguments
 *
 * @param   option_str      Options string to pass to getopt_long(). This should
 *                          include DEVCFG_OPTIONS_BASE, with any
 *                          application-handled options appended.
 *
 * @param   long_options    List of long options created by caller
 *
 * @param   c               Device configuration to update. It should have
 *                          already been initialized via devcfg_init()
 *
 * @return 0 on success, -1 on failure, 1 if help is requested
 */
int devcfg_handle_args(int argc, char **argv,
                          const char *options, const struct option *long_options,
                          struct devcfg *c);

/**
 * Wrapper around bladerf_sync_config, using parameters in the specified
 * devcfg. Optionally, enable the associated module.
 */
int devcfg_perform_sync_config(struct bladerf *dev,
                               bladerf_module module,
                               bladerf_format format,
                               const struct devcfg *config,
                               bool enable_module);


/**
 * Apply the provided device_config to the specified device
 *
 * @param   dev     Device to configure
 * @param   c       Configuration parameters
 *
 * @return 0 on succes, -1 on failure
 */
int devcfg_apply(struct bladerf *dev, const struct devcfg *c);

/**
 * Print help info about arguments handled by devcfg_handle_args()
 *
 * @param   title   Optional title to print before usage information
 */
void devcfg_print_common_help(const char *title);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
