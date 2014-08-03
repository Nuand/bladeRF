/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014 Nuand LLC
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
 */

#include "gain.h"
#include "lms.h"

static inline int set_rx_gain_combo(struct bladerf *dev,
                                     bladerf_lna_gain lnagain,
                                     int rxvga1, int rxvga2)
{
    int status;
    status = lms_lna_set_gain(dev, lnagain);
    if (status < 0) {
        return status;
    }

    status = lms_rxvga1_set_gain(dev, rxvga1);
    if (status < 0) {
        return status;
    }

    return lms_rxvga2_set_gain(dev, rxvga2);
}

static int set_rx_gain(struct bladerf *dev, int gain)
{
    if (gain <= BLADERF_LNA_GAIN_MID_DB) {
        return set_rx_gain_combo(dev,
                                 BLADERF_LNA_GAIN_BYPASS,
                                 BLADERF_RXVGA1_GAIN_MIN,
                                 BLADERF_RXVGA2_GAIN_MIN);
    } else if (gain <= BLADERF_LNA_GAIN_MID_DB + BLADERF_RXVGA1_GAIN_MIN) {
        return set_rx_gain_combo(dev,
                                 BLADERF_LNA_GAIN_MID_DB,
                                 BLADERF_RXVGA1_GAIN_MIN,
                                 BLADERF_RXVGA2_GAIN_MIN);
    } else if (gain <= (BLADERF_LNA_GAIN_MAX_DB + BLADERF_RXVGA1_GAIN_MAX)) {
        return set_rx_gain_combo(dev,
                                 BLADERF_LNA_GAIN_MID,
                                 gain - BLADERF_LNA_GAIN_MID_DB,
                                 BLADERF_RXVGA2_GAIN_MIN);
    } else if (gain < (BLADERF_LNA_GAIN_MAX_DB + BLADERF_RXVGA1_GAIN_MAX + BLADERF_RXVGA2_GAIN_MAX)) {
        return set_rx_gain_combo(dev,
                BLADERF_LNA_GAIN_MAX,
                BLADERF_RXVGA1_GAIN_MAX,
                gain - (BLADERF_LNA_GAIN_MAX_DB + BLADERF_RXVGA1_GAIN_MAX));
    } else {
        return set_rx_gain_combo(dev,
                                 BLADERF_LNA_GAIN_MAX,
                                 BLADERF_RXVGA1_GAIN_MAX,
                                 BLADERF_RXVGA2_GAIN_MAX);
    }
}

static inline int set_tx_gain_combo(struct bladerf *dev, int txvga1, int txvga2)
{
    int status;

    status = lms_txvga1_set_gain(dev, txvga1);
    if (status == 0) {
        status = lms_txvga2_set_gain(dev, txvga2);
    }

    return status;
}

static int set_tx_gain(struct bladerf *dev, int gain)
{
    int status;
    const int max_gain =
        (BLADERF_TXVGA1_GAIN_MAX - BLADERF_TXVGA1_GAIN_MIN)
            + BLADERF_TXVGA2_GAIN_MAX;

    if (gain < 0) {
        gain = 0;
    }

    if (gain <= BLADERF_TXVGA2_GAIN_MAX) {
        status = set_tx_gain_combo(dev, BLADERF_TXVGA1_GAIN_MIN, gain);
    } else if (gain <= max_gain) {
        status = set_tx_gain_combo(dev,
                    BLADERF_TXVGA1_GAIN_MIN + gain - BLADERF_TXVGA2_GAIN_MAX,
                    BLADERF_TXVGA2_GAIN_MAX);
    } else {
        status = set_tx_gain_combo(dev,
                                   BLADERF_TXVGA1_GAIN_MAX,
                                   BLADERF_TXVGA2_GAIN_MAX);
    }

    return status;
}

int gain_set(struct bladerf *dev, bladerf_module module, int gain)
{
    int status;

    if (module == BLADERF_MODULE_TX) {
        status = set_tx_gain(dev, gain);
    } else if (module == BLADERF_MODULE_RX) {
        status = set_rx_gain(dev, gain);
    } else {
        status = BLADERF_ERR_INVAL;
    }

    return status;
}

