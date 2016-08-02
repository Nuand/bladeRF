/**
 * @file
 * @brief   Physical layer code for modem
 *
 * This file handles transmission/reception of data frames over the air. It uses fsk.c to
 * perform baseband IQ modulation and demodulation, and libbladeRF to transmit/receive
 * samples using the bladeRF device. A different modulator could be used by swapping
 * fsk.c with a file that implements a different modulator. On the receive side the file
 * uses fir_filter.c to low-pass filter the received signal, pnorm.c to power normalize
 * the input signal, and correlator.c to correlate the received signal with a preamble.
 * waveform.
 *
 * The structure of a physical layer transmission is as follows:
 * ```
 *    / ramp up | training sequence | preamble | link layer frame | ramp down \
 * ```
 * The ramps at the beginning/end are SAMP_PER_SYMB samples long
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

#ifndef PHY_H
#define PHY_H

#include <stdbool.h>
#include <libbladeRF.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <math.h>

#include "host_config.h"
#include "common.h"
#include "utils.h"

//Training sequence which goes at the start of every frame
//Note: In order for the preamble waveform not to be messed up, the last
//sample of the modulated training sequence MUST be 1 + 0j (2047 + 0j)
#define TRAINING_SEQ {0xAA, 0xAA, 0xAA, 0xAA}
//Length of training sequence in bytes
#define TRAINING_SEQ_LENGTH 4
//preamble which goes after the training sequence
#define PREAMBLE {0x2E, 0x69, 0x2C, 0xF0}
//Length of preamble in bytes
#define PREAMBLE_LENGTH 4
//Byte codes for data/ack frame
#define DATA_FRAME_CODE 0x00
#define ACK_FRAME_CODE 0xFF
//Frame lengths
#define DATA_FRAME_LENGTH 1009
#define ACK_FRAME_LENGTH 7
//Maximum frame size in bytes
#define MAX_LINK_FRAME_SIZE DATA_FRAME_LENGTH
//Seed for pseudorandom number sequence generator
#define PRNG_SEED 0x0109BBA53CFFD081
//Length (in samples) of ramp up/ramp down
#define RAMP_LENGTH SAMP_PER_SYMB
//Number of samples to receive at a time from bladeRF
#define NUM_SAMPLES_RX SYNC_BUFFER_SIZE
//Correlator countdown size
#define CORR_COUNTDOWN SAMP_PER_SYMB

struct phy_handle;

//----------------------Transmitter functions---------------------------
/**
 * Start the PHY transmitter thread
 * 
 * @param[in]   phy     pointer to phy_handle struct
 *
 * @return      0 on success, -1 on failure
 */
int phy_start_transmitter(struct phy_handle *phy);

/**
 * Stop the PHY transmitter thread
 * 
 * @param[in]   phy     pointer to phy_handle struct
 *
 * @return      0 on success, -1 on failure
 */
int phy_stop_transmitter(struct phy_handle *phy);

/**
 * Fill the tx buffer so its data will be transmitted using phy_transmit_frames()
 *
 * @param[in]   phy         pointer to phy handle structure
 * @param[in]   data_buf    bytes to transmit
 * @param[in]   length      length of data buf
 *
 * @return      0 on success, -1 on failure
 */
int phy_fill_tx_buf(struct phy_handle *phy, uint8_t *data_buf, unsigned int length);

//------------------------Receiver functions---------------------------
/**
 * Start the PHY receiver  thread
 * 
 * @param[in]   phy     pointer to phy_handle struct
 *
 * @return      0 on success, -1 on failure
 */
int phy_start_receiver(struct phy_handle *phy);

/**
 * Stop the PHY receiver thread
 * 
 * @param[in]   phy     pointer to phy_handle struct
 *
 * @return      0 on success, -1 on failure
 */
int phy_stop_receiver(struct phy_handle *phy);

/**
 * Request a received frame from phy_receive_frames(). Caller should call
 * phy_release_rx_buf() when done with the received frame so that 
 * phy_receive_frames() does not drop any frames.
 *
 * @param[in]   phy             pointer to phy_handle struct
 * @param[in]   timeout_ms      amount of time to wait for a buffer from the PHY
 *
 * @return      pointer to filled buffer with received frame inside
 */
uint8_t *phy_request_rx_buf(struct phy_handle *phy, unsigned int timeout_ms);

/**
 * Release RX buffer so that phy_receive_frames() can copy new frames into the buffer
 *
 * @param[in]   phy     pointer to phy_handle struct
 */
void phy_release_rx_buf(struct phy_handle *phy);

//-----------------------Init/Deinit functions-------------------------
/**
 * Open/Initialize a phy_handle
 * 
 * @param[in]   dev     pointer to opened bladeRF device handle
 * @param[in]   params  pointer to radio parameters struct
 *
 * @return      allocated phy_handle on success, NULL on failure
 */
struct phy_handle *phy_init(struct bladerf *dev, struct radio_params *params);

/**
 * Close a phy handle. Does nothing if handle is NULL
 *
 * @param[in]   phy_handle to close
 */
void phy_close(struct phy_handle *phy);

#endif
