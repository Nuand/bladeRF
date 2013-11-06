/**
 * @file rxtx.h
 *
 * @brief Common RX and TX functionality
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
#include <stdbool.h>
#include <libbladeRF.h>

#include "common.h"

struct rxtx_data;

/**
 * Allocate and initialize data for handling sample reception or transmission
 *
 * @param   module  Used to configure this handle for use with RX or TX commands
 *
 * @return  allocated data on succss, NULL on failure
 */
struct rxtx_data *rxtx_data_alloc(bladerf_module module);

/**
 * Startup the RX or TX task.
 * The task will enter and wait in an idle state on success.
 *
 * @param   s   CLI state handle with a valid rx_data handle
 * @param   m   Module (RX or  TX) to start
 *
 * @pre     The cli_state is assumed to have had its rx_data or tx_data field
 *          populated (whichever is relevant for the provided module)
 *
 * @return  0 on success, nonzero on failure
 */
int rxtx_startup(struct cli_state *s, bladerf_module m);

/**
 * Test whether RX or TX task is running (as opposed to idle or shutting down)
 *
 * @param rxtx  Module handle
 *
 * @return true if the task is currently running,
 *         false if it is any other state
 */
bool rxtx_task_running(struct rxtx_data *rxtx);

/**
 * Request RX/TX task to shut down and exit. Blocks until task has shut down.
 *
 * @param   rxtx      RX/TX data handle
 */
void rxtx_shutdown(struct rxtx_data *rxtx);

/**
 * Release any rx/tx wait commands
 *
 * @param   rxtx    RX/TX data handle
 *
 * @return True if a wait command was released
 */
bool rxtx_release_wait(struct rxtx_data *rxtx);

/**
 * Free data allocated with rxtx_data_alloc()
 *
 * @pre     Must be preceeded by a call to rxtx_shutdown()
 *
 * @param   rxtx        RX/TX data handle
 */
void rxtx_data_free(struct rxtx_data *rxtx);

