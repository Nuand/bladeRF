/**
 * @file
 * @brief   Link layer code for modem
 *
 * This file handles framing, error detection, and guaranteed delivery of frames.
 * On the sender side, this file formats payload data into packets to transmit
 * using phy.c, and waits for acknowledgements. A retransmission is automatically
 * sent if no acknowledgement is received within the timeout period. On the receiver
 * side, this file extracts payload data from a packet received using phy.c, and sends
 * acknowledgements.
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

#ifndef LINK_H
#define LINK_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <libbladeRF.h>
#include "host_config.h"

#include "common.h"

#define PAYLOAD_LENGTH 1000     //If you change this, DATA_FRAME_LENGTH
                                //ACK_FRAME_LENGTH must also be changed in phy.h
#define ACK_TIMEOUT_MS 500      //Timeout to wait for an acknowledgement
#define LINK_MAX_TRIES 3        //Maximum number of frame retransmissions before the
                                //transmitter gives up

/** Opaque handle to link data structure */
struct link_handle;

/**
 * Send data of arbitrary length. Breaks data up into packets (if needed) and sends each
 * packet one-by-one.
 *
 * @param[in]   link            pointer to link handle
 * @param[in]   data            Data to send
 * @param[in]   data_length     Length of data in bytes
 *
 * @return      0 on success, -1 on error, -2 on no connection
 */
int link_send_data(struct link_handle *link, uint8_t *data, unsigned int data_length);

/**
 * Attempts to receive the number of bytes specified by size and places them into
 * data_buf.
 *
 * @param[in]   link            pointer to link handle
 * @param[in]   size            number of bytes to attempt to receive
 * @param[in]   max_timeouts    max number of 0.5 second timeouts before the function
 *                              returns
 * @param[out]  data_buf        buffer to place received bytes in
 *
 * @return      number of bytes received on success, -1 on error. In the case of a
 *              timeout (i.e. when 'max_timeouts' have occured), this return value
 *              will be less than 'size'
 */
int link_receive_data(struct link_handle *link, int size, int max_timeouts,
                        uint8_t *data_buf);

/**
 * Initializes/allocates a link handle data structure and starts all threads
 *
 * @param[in]   dev         pointer to opened bladeRF device handle
 * @param[in]   params      radio parameters to use when configuring bladeRF device
 *
 * @return      pointer to allocated link_handle struct on success, NULL on error
 */
struct link_handle *link_init(struct bladerf *dev, struct radio_params *params);

/**
 * Deinitializes/closes/frees a link_handle struct. Does nothing if link is NULL
 *
 * @param[in]   link        pointer to link handle
 */
void link_close(struct link_handle *link);

#endif
