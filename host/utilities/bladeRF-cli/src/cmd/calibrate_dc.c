/*
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2014 Nuand LLC
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

#include "calibrate.h"
#include "common.h"
#include "rel_assert.h"


#define CAL_SAMPLERATE  24e5
#define CAL_BANDWIDTH   15e5

#define CAL_NUM_BUFS    16
#define CAL_NUM_XFERS   4
#define CAL_BUF_LEN     (16 * 1024) /* In samples (where a sample is (I, Q) */
#define CAL_TIMEOUT     1000

#define CAL_DC_MIN      -2048
#define CAL_DC_MAX      2048

/* Return "first" error, with precedence on error 1 */
static inline int first_error(int error1, int error2) {
    return error1 == 0 ? error2 : error1;
}

/* Apply a DC offset correction values */
static inline int set_rx_dc(struct bladerf *dev, int16_t dc_i, int16_t dc_q)
{
    int status;

    status = bladerf_set_correction(dev, BLADERF_MODULE_RX,
                                    BLADERF_CORR_LMS_DCOFF_I, dc_i);
    if (status != 0) {
        return status;
    }

    status = bladerf_set_correction(dev, BLADERF_MODULE_RX,
                                    BLADERF_CORR_LMS_DCOFF_Q, dc_q);
    if (status != 0) {
        return status;
    }

    return 0;
}

static inline int16_t interpolate(int16_t x0, int16_t x1,
                                  int16_t y0, int16_t y1)
{
    const int32_t denom = y1 - y0;
    const int32_t num   = (int32_t)y0 * (x1 - x0);

    return x0 - (int16_t)((num + (denom / 2)) / denom);
}

static int rx_sample_avg(struct bladerf *dev, int16_t *samples,
                         int16_t *avg_i, int16_t *avg_q)
{
    int status;
    int64_t accum_i, accum_q;
    unsigned int i;

    /* Flush out samples and read a buffer's worth of data */
    for (i = 0; i < 2 * CAL_NUM_BUFS + 1; i++) {
        status = bladerf_sync_rx(dev, samples, CAL_BUF_LEN, NULL, CAL_TIMEOUT);
        if (status != 0) {
            return status;
        }
    }

    for (i = 0, accum_i = accum_q = 0; i < 2 * CAL_BUF_LEN; i += 2) {
        accum_i += samples[i];
        accum_q += samples[i + 1];
    }

    accum_i /= CAL_BUF_LEN;
    accum_q /= CAL_BUF_LEN;

    assert(abs(accum_i) < (1 << 12));
    assert(abs(accum_q) < (1 << 12));

    *avg_i = accum_i;
    *avg_q = accum_q;

    return 0;
}

int calibrate_dc_rx(struct bladerf *dev,
                    int16_t *dc_i, int16_t *dc_q,
                    int16_t *avg_i, int16_t *avg_q)
{
    int status;
    int16_t *samples = NULL;
    int16_t dc_i0, dc_q0, dc_i1, dc_q1;
    int16_t avg_i0, avg_q0, avg_i1, avg_q1;

    samples = (int16_t*) malloc(CAL_BUF_LEN * 2 * sizeof(samples[0]));
    if (samples == NULL) {
        status = BLADERF_ERR_MEM;;
        goto out;
    }

    dc_i0 = dc_q0 = -512;
    dc_i1 = dc_q1 = 512;

    /* Get an initial set of sample points */
    status = set_rx_dc(dev, dc_i0, dc_q0);
    if (status != 0) {
        goto out;
    }

    status = rx_sample_avg(dev, samples, &avg_i0, &avg_q0);
    if (status != 0) {
        goto out;
    }

    status = set_rx_dc(dev, dc_i1, dc_q1);
    if (status != 0) {
        goto out;
    }

    status = rx_sample_avg(dev, samples, &avg_i1, &avg_q1);
    if (status != 0) {
        goto out;
    }

    *dc_i = interpolate(dc_i0, dc_i1, avg_i0, avg_i1);
    *dc_q = interpolate(dc_q0, dc_q1, avg_q0, avg_q1);

    if (*dc_i < CAL_DC_MIN) {
        *dc_i = CAL_DC_MIN;
    } else if (*dc_i > CAL_DC_MAX) {
        *dc_i = CAL_DC_MAX;
    }

    if (*dc_q < CAL_DC_MIN) {
        *dc_q = CAL_DC_MIN;
    } else if (*dc_q > CAL_DC_MAX){
        *dc_q = CAL_DC_MAX;
    }

    /* See where we're at now... */
    status = set_rx_dc(dev, *dc_i, *dc_q);
    if (status != 0) {
        goto out;
    }

    status = rx_sample_avg(dev, samples, avg_i, avg_q);
    if (status != 0) {
        goto out;
    }

out:
    free(samples);
    return status;
}

static inline int dummy_rx(struct bladerf *dev)
{
    int status, i;
    const size_t buf_len = CAL_BUF_LEN;
    uint16_t *buf = malloc(2 * sizeof(buf[0]) * buf_len);

    if (buf == NULL) {
        return BLADERF_ERR_MEM;
    }

    status = bladerf_sync_config(dev, BLADERF_MODULE_RX,
                                 BLADERF_FORMAT_SC16_Q11,
                                 CAL_NUM_BUFS, buf_len,
                                 CAL_NUM_XFERS, CAL_TIMEOUT);

    if (status != 0) {
        goto error;
    }


    for (i = 0; i < 4; i++) {
        status = bladerf_sync_rx(dev, buf, buf_len, NULL, CAL_TIMEOUT);
        if (status != 0) {
            goto error;
        }
    }

error:
    free(buf);
    return status;
}

static inline int dummy_tx(struct bladerf *dev)
{
    int status;
    const size_t buf_len = CAL_BUF_LEN;
    uint16_t *buf = calloc(2 * buf_len, sizeof(buf[0]));

    if (buf == NULL) {
        return BLADERF_ERR_MEM;
    }

    status = bladerf_sync_config(dev, BLADERF_MODULE_TX,
                                 BLADERF_FORMAT_SC16_Q11,
                                 CAL_NUM_BUFS, buf_len,
                                 CAL_NUM_XFERS, CAL_TIMEOUT);

    if (status != 0) {
        goto error;
    }

    status = bladerf_sync_tx(dev, buf, buf_len, NULL, CAL_TIMEOUT);

error:
    free(buf);
    return status;
}

static inline int backup_settings(struct bladerf *dev, bladerf_module module,
                                  unsigned int *bandwidth,
                                  struct bladerf_rational_rate *samplerate)
{
    int status;

    status = bladerf_get_bandwidth(dev, module, bandwidth);
    if (status != 0) {
        return status;
    } else {
        status = bladerf_get_rational_sample_rate(dev, module, samplerate);
    }

    return status;
}

static inline int restore_settings(struct bladerf *dev, bladerf_module module,
                                   unsigned int bandwidth,
                                   struct bladerf_rational_rate *samplerate)
{
    int status;

    status = bladerf_set_bandwidth(dev, module, bandwidth, NULL);
    if (status != 0) {
        return status;
    } else {
        status = bladerf_set_rational_sample_rate(dev, module,
                                                  samplerate, NULL);
    }

    return status;
}


int calibrate_dc(struct bladerf *dev, unsigned int ops)
{
    int retval = 0;
    int status = BLADERF_ERR_UNEXPECTED;
    unsigned int rx_bandwidth, tx_bandwidth;
    struct bladerf_rational_rate rx_samplerate, tx_samplerate;


    if (IS_RX_CAL(ops)) {
        status = backup_settings(dev, BLADERF_MODULE_RX,
                                 &rx_bandwidth, &rx_samplerate);
        if (status != 0) {
            return status;
        }
    }

    if (IS_TX_CAL(ops)) {
        status = backup_settings(dev, BLADERF_MODULE_TX,
                                 &tx_bandwidth, &tx_samplerate);
        if (status != 0) {
            return status;
        }
    }

    if (IS_RX_CAL(ops)) {
        status = bladerf_enable_module(dev, BLADERF_MODULE_RX, true);
        if (status != 0) {
            goto error;
        }

        status = dummy_rx(dev);
        if (status != 0) {
            goto error;
        }
    }

    if (IS_TX_CAL(ops)) {
        /* TODO disconnect TX from external output */

        status = bladerf_enable_module(dev, BLADERF_MODULE_TX, true);
        if (status != 0) {
            goto error;
        }

        status = dummy_tx(dev);
        if (status != 0) {
            goto error;
        }
    }

    if (IS_CAL(CAL_DC_LMS_TUNING, ops)) {
        printf ("Calibrating LMS bandwidth tuning\n");
        status = bladerf_calibrate_dc(dev, BLADERF_DC_CAL_LPF_TUNING);
        if (status != 0) {
            goto error;
        }
    }

    if (IS_CAL(CAL_DC_LMS_TXLPF, ops)) {
        printf ("Calibrating LMS txlpf\n");
        status = bladerf_calibrate_dc(dev, BLADERF_DC_CAL_TX_LPF);
        if (status != 0) {
            goto error;
        }
    }

    if (IS_CAL(CAL_DC_LMS_RXLPF, ops)) {
        printf ("Calibrating LMS rxlpf\n");
        status = bladerf_calibrate_dc(dev, BLADERF_DC_CAL_RX_LPF);
        if (status != 0) {
            goto error;
        }
    }

    if (IS_CAL(CAL_DC_LMS_RXVGA2, ops)) {
        printf ("Calibrating LMS rxvga2\n");
        status = bladerf_calibrate_dc(dev, BLADERF_DC_CAL_RXVGA2);
        if (status != 0) {
            goto error;
        }
    }

    if (IS_CAL(CAL_DC_AUTO_RX, ops)) {
        int16_t dc_i, dc_q, avg_i, avg_q;
        status = calibrate_dc_rx(dev, &dc_i, &dc_q, &avg_i, &avg_q);
        if (status != 0) {
            goto error;
        } else {
            printf("DC I Setting: %d\n", dc_i);
            printf("DC Q Setting: %d\n", dc_q);
            printf("Average I value after calibration: %d\n", avg_i);
            printf("Average Q value after calibration: %d\n", avg_q);
        }
    }

error:
    retval = status;

    if (IS_RX_CAL(ops)) {
        status = bladerf_enable_module(dev, BLADERF_MODULE_RX, false);
        retval = first_error(retval, status);

        status = restore_settings(dev, BLADERF_MODULE_RX,
                                  rx_bandwidth, &rx_samplerate);
        retval = first_error(retval, status);
    }


    if (IS_TX_CAL(ops)) {
        status = bladerf_enable_module(dev, BLADERF_MODULE_TX, false);
        retval = first_error(retval, status);

        status = restore_settings(dev, BLADERF_MODULE_TX,
                                  tx_bandwidth, &tx_samplerate);
        retval = first_error(retval, status);
    }

    return retval;
}
