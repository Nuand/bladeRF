/*
 * @file common.h
 *
 * @brief Common CLI routines
 *
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2013 Nuand LLC
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
#ifndef COMMON_H__
#define COMMON_H__
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <libbladeRF.h>
#include "host_config.h"

/**
 * Differentiates error code types
 */
enum error_type {
    ETYPE_BUG = -1, /**< Invalid value that should never occur; we
                     *   don't have a better classification and the
                     *   condition should not have occurred. */
    ETYPE_CLI,      /**< CMD_RET cli error code */
    ETYPE_BLADERF,  /**< libbladeRF error code */
    ETYPE_ERRNO,    /**< errno value */
};

/**
 * Information about the last error encountered
 */
struct cli_error {
    int value;              /**< Last command error */
    enum error_type type;   /**< Type/source of last error */
    pthread_mutex_t lock;
};

/**
 * Application state
 */
struct cli_state {
    struct bladerf *dev;            /**< Device currently in use */
    pthread_mutex_t dev_lock;       /**< Should be held when performing
                                     *   any "device conrol" calls */

    int last_lib_error;             /**< Last libbladeRF error */

    struct script *scripts;         /**< Open script files */

    struct rxtx_data *rx;           /**< Data for sample reception */
    struct rxtx_data *tx;           /**< Data for sample transmission */
};

/**
 * Allocate and initialize state object
 *
 * @return state structure on success, NULL on failure
 */
struct cli_state *cli_state_create();

/**
 * Deallocate and deinitlize state object
 */
void cli_state_destroy(struct cli_state *s);

/**
 * Query whether a device is currently opend
 *
 * @return true if device is opened, false otherwise
 */
bool cli_device_is_opened(struct cli_state *s);

/**
 * Query whether the device is currently running RX or TX streams
 *
 * @return true if device is in use, false otherwise
 */
bool cli_device_is_streaming(struct cli_state *s);

/**
 * Print an error message, with a line number, if running from a script.
 *
 *
 * @param   s       CLI state.
 * @param   pfx     Error prefix. "Error: " is used if this is null
 * @param   format  Printf-style format string, followed by args
 *
 */
void cli_err(struct cli_state *s, const char *pfx, const char *format, ...);

/**
 * Intialize error info. Defaults to "no error"
 *
 */
void cli_error_init(struct cli_error *e);

/**
 * Set the "last encountered error" info
 *
 * Always use this routine for thread safety - do not access these fields
 * directly.
 *
 * @pre e has been initialized vie cli_error_init()
 *
 * @param   e       Error structure
 * @param   type    Type of error (i.e. who's error code is it?)
 * @param   error   Error code
 */
void set_last_error(struct cli_error *e, enum error_type type, int error);

/**
 * Fetch the "last encountered error" info
 *
 * @pre e has been initialized vie cli_error_init()
 *
 * @param[in]       e       Error structure
 * @param[out]      type    Type of error (i.e. who's error code is it?)
 * @param[out]      error   Error code
 */
void get_last_error(struct cli_error *e, enum error_type *type, int *error);

/**
 * Allocate and return a file name (including path) for a given FILE *
 *
 * The caller is responsible for freeing this string
 *
 * @return path string on success, NULL on failure
 */
char *to_path(FILE *f);

/**
 * Open the file, expanding the path first, if possible.
 *
 * This is a wrapper around fopen() and interactive_expand_path()
 *
 * @note The value of errno IS NOT guarenteed to be assoicated with fopen()
 *       failures when this function returns.
 *
 * @param   filename    Filename to expand and open
 * @param   mode        fopen() access mode string
 *
 * @return  File handle on success, NULL on failure.
 */
FILE *expand_and_open(const char *filename, const char *mode);

#endif
