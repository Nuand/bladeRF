/**
 * @file
 * @brief   Gets configuration options from command line arguments
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

#include <stdlib.h>
#include <stdio.h>
#include <libbladeRF.h>

#include "common.h"

/**
 * Application configuration parameters
 */
struct config {
    struct bladerf *bladerf_dev;    //bladeRF device handle
    struct radio_params params;     //Radio parameters: gains and frequencies
    FILE *rx_output;                //File to write received data to
    FILE *tx_input;                 //File to read transmitted data from
    long int tx_filesize;           //Size of the tx_input file, if it is not stdin
    bool quiet;                     //Option to suppress printing of banner message
};


/**
 * Create and intialize an application configuration from command line arguments
 *
 * @param[in]   argc    argc from main()
 * @param[in]   argv    argv from main()
 * @param[out]  c       Updated to point to configuration struture on success,
 *                      or NULL on non-zero return.
 *
 * @return 0 on success
 *         1 on request for help text
 *        <0 on failure
 */
int config_init_from_cmdline(int argc, char * const argv[], struct config **c);

/**
 * Deinitialize and deallocate a configuration structure and all of its members
 *
 * @param   c       Configuration to deinitialize
 */
void config_deinit(struct config *c);

/**
 * Print help text for options parsed by config_init_from_cmdline()
 */
void config_print_options();
