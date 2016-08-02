/**
 * @brief   BladeRF radio configuration - setting of frequencies, gains, etc.
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

#include "radio_config.h"

#ifdef DEBUG_MODE
    #define DEBUG_MSG(...) fprintf(stderr, __VA_ARGS__)
#else
    #define DEBUG_MSG(...)
#endif

//private structs
struct module_config{
    bladerf_module module;
    unsigned int frequency;
    unsigned int bandwidth;
    unsigned int samplerate;
    /* Gains */
    bladerf_lna_gain rx_lna;
    int vga1;
    int vga2;
};
//internal functinos
static int radio_configure_module(struct bladerf *dev, struct module_config *c);
static int radio_init_sync(struct bladerf *dev);

/**
 * Configure RX/TX module
 */
static int radio_configure_module(struct bladerf *dev, struct module_config *c)
{
    int status;
    status = bladerf_set_frequency(dev, c->module, c->frequency);
    if (status != 0) {
        fprintf(stderr, "Failed to set frequency = %u: %s\n",
                c->frequency, bladerf_strerror(status));
        return status;
    }
    status = bladerf_set_sample_rate(dev, c->module, c->samplerate, NULL);
    if (status != 0) {
        fprintf(stderr, "Failed to set samplerate = %u: %s\n",
                c->samplerate, bladerf_strerror(status));
        return status;
    }
    status = bladerf_set_bandwidth(dev, c->module, c->bandwidth, NULL);
    if (status != 0) {
        fprintf(stderr, "Failed to set bandwidth = %u: %s\n",
                c->bandwidth, bladerf_strerror(status));
        return status;
    }
    switch (c->module) {
        case BLADERF_MODULE_RX:
            /* Configure the gains of the RX LNA, RX VGA1, and RX VGA2  */
            status = bladerf_set_lna_gain(dev, c->rx_lna);
            if (status != 0) {
                fprintf(stderr, "Failed to set RX LNA gain: %s\n",
                        bladerf_strerror(status));
                return status;
            }
            status = bladerf_set_rxvga1(dev, c->vga1);
            if (status != 0) {
                fprintf(stderr, "Failed to set RX VGA1 gain: %s\n",
                        bladerf_strerror(status));
                return status;
            }
            status = bladerf_set_rxvga2(dev, c->vga2);
            if (status != 0) {
                fprintf(stderr, "Failed to set RX VGA2 gain: %s\n",
                        bladerf_strerror(status));
                return status;
            }
            break;
        case BLADERF_MODULE_TX:
            /* Configure the TX VGA1 and TX VGA2 gains */
            status = bladerf_set_txvga1(dev, c->vga1);
            if (status != 0) {
                fprintf(stderr, "Failed to set TX VGA1 gain: %s\n",
                        bladerf_strerror(status));
                return status;
            }
            status = bladerf_set_txvga2(dev, c->vga2);
            if (status != 0) {
                fprintf(stderr, "Failed to set TX VGA2 gain: %s\n",
                        bladerf_strerror(status));
                return status;
            }
            break;
        default:
            status = BLADERF_ERR_INVAL;
            fprintf(stderr, "%s: Invalid module specified (%d)\n",
                    __FUNCTION__, c->module);
    }
    return status;
}

/** 
 * Initialize synchronous interface
 */
static int radio_init_sync(struct bladerf *dev)
{
    int status;
    const unsigned int num_buffers   = 64;
    const unsigned int buffer_size   = SYNC_BUFFER_SIZE;  /* Must be a multiple of 1024 */
    const unsigned int num_transfers = 16;
    const unsigned int timeout_ms    = 3500;
    /* Configure both the device's RX and TX modules for use with the synchronous
     * interface. SC16 Q11 samples with metadata are used. */
    status = bladerf_sync_config(dev,
                                 BLADERF_MODULE_RX,
                                 BLADERF_FORMAT_SC16_Q11_META,
                                 num_buffers,
                                 buffer_size,
                                 num_transfers,
                                 timeout_ms);
    if (status != 0) {
        fprintf(stderr, "Failed to configure RX sync interface: %s\n",
                bladerf_strerror(status));
        return status;
    }
    status = bladerf_sync_config(dev,
                                 BLADERF_MODULE_TX,
                                 BLADERF_FORMAT_SC16_Q11_META,
                                 num_buffers,
                                 buffer_size,
                                 num_transfers,
                                 timeout_ms);
    if (status != 0) {
        fprintf(stderr, "Failed to configure TX sync interface: %s\n",
                bladerf_strerror(status));
    }
    return status;
}

int radio_init_and_configure(struct bladerf *dev, struct radio_params *params)
{
    struct module_config config;
    int status;

    #ifdef DEBUG_MODE
        bladerf_log_set_verbosity(BLADERF_LOG_LEVEL_DEBUG);
    #endif

    //Configure TX parameters
    config.module       = BLADERF_MODULE_TX;
    config.frequency    = params->tx_freq;
    config.bandwidth    = BLADERF_BANDWIDTH;
    config.samplerate   = BLADERF_SAMPLE_RATE;
    config.vga1         = params->tx_vga1_gain;
    config.vga2         = params->tx_vga2_gain;
    status = radio_configure_module(dev, &config);
    if (status != 0){
        fprintf(stderr, "Couldn't configure TX module: %s\n", bladerf_strerror(status));
        return status;
    }

    //Configure RX parameters
    config.module       = BLADERF_MODULE_RX;
    config.frequency    = params->rx_freq;
    config.bandwidth    = BLADERF_BANDWIDTH;
    config.samplerate   = BLADERF_SAMPLE_RATE;
    config.rx_lna       = params->rx_lna_gain;
    config.vga1         = params->rx_vga1_gain;
    config.vga2         = params->rx_vga2_gain;
    status = radio_configure_module(dev, &config);
    if (status != 0){
        fprintf(stderr, "Couldn't configure RX module: %s\n", bladerf_strerror(status));
        return status;
    }

    //Unset loopback
    status = bladerf_set_loopback(dev, BLADERF_LB_NONE);
    if (status != 0){
        fprintf(stderr, "Couldn't set loopback: %s", bladerf_strerror(status));
        return status;
    }

    //Initialize synchronous interface
    status = radio_init_sync(dev);
    if (status != 0){
        fprintf(stderr, "Couldn't initialize synchronous interface: %s\n",
                bladerf_strerror(status));
        return status;
    }

    //Enable tx module
    status = bladerf_enable_module(dev, BLADERF_MODULE_TX, true);
    if (status != 0){
        fprintf(stderr, "Couldn't enable TX module: %s\n", bladerf_strerror(status));
        return status;
    }
    //Enable rx module
    status = bladerf_enable_module(dev, BLADERF_MODULE_RX, true);
    if (status != 0){
        fprintf(stderr, "Couldn't enable RX module: %s\n", bladerf_strerror(status));
        return status;
    }
    return 0;
}

void radio_stop(struct bladerf *dev)
{
    int status;

    if (dev == NULL){
        return;
    }
    //Disable tx module
    status = bladerf_enable_module(dev, BLADERF_MODULE_TX, false);
    if (status != 0){
        fprintf(stderr, "Couldn't disable TX module: %s\n", bladerf_strerror(status));
    }
    //Disable rx module
    status = bladerf_enable_module(dev, BLADERF_MODULE_RX, false);
    if (status != 0){
        fprintf(stderr, "Couldn't disable RX module: %s\n", bladerf_strerror(status));
    }
}
