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
#include <pthread.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#include "calibrate.h"
#include "common.h"
#include "rel_assert.h"
#include "thread.h"

#define CAL_SAMPLERATE  3000000u
#define CAL_BANDWIDTH   BLADERF_BANDWIDTH_MIN

#define CAL_NUM_BUFS    2
#define CAL_NUM_XFERS   1
#define CAL_BUF_LEN     (16 * 1024) /* In samples (where a sample is (I, Q) */
#define CAL_TIMEOUT     1000

#define CAL_DC_MIN      -2048
#define CAL_DC_MAX      2048

#define UPPER_BAND      15000000u

struct cal_tx_task {
    struct cli_state *s;
    int16_t *samples;
    bool started;

    pthread_t thread;
    MUTEX lock;
    int status;
    bool run;
};

struct point {
    float x, y;
};

/* Settings that need to be backed up and restored during DC cal */
struct settings {
    unsigned int bandwidth;
    unsigned int frequency;
    struct bladerf_rational_rate samplerate;
    bladerf_loopback loopback;
};

/* Return "first" error, with precedence on error 1 */
static inline int first_error(int error1, int error2) {
    return error1 == 0 ? error2 : error1;
}

static inline int set_dc(struct bladerf *dev, bladerf_module module,
                         int16_t dc_i, int16_t dc_q)
{
    int status;

    status = bladerf_set_correction(dev, module,
                                    BLADERF_CORR_LMS_DCOFF_I, dc_i);
    if (status != 0) {
        return status;
    }

    status = bladerf_set_correction(dev, module,
                                    BLADERF_CORR_LMS_DCOFF_Q, dc_q);
    if (status != 0) {
        return status;
    }

    return 0;
}

static inline int set_rx_dc(struct bladerf *dev, int16_t dc_i, int16_t dc_q)
{
    return set_dc(dev, BLADERF_MODULE_RX, dc_i, dc_q);
}

static inline int set_tx_dc(struct bladerf *dev, int16_t dc_i, int16_t dc_q)
{
    return set_dc(dev, BLADERF_MODULE_TX, dc_i, dc_q);
}

/* Interpolate to find the point (x_result, 0) */
static inline int interpolate(int16_t x0, int16_t x1,
                              int16_t y0, int16_t y1,
                              int16_t *x_result)
{
    const int32_t denom = y1 - y0;
    const int32_t num   = (int32_t)y0 * (x1 - x0);

    if (denom == 0) {
        return BLADERF_ERR_UNEXPECTED;
    }

    *x_result = x0 - (int16_t)((num + (denom / 2)) / denom);
    return 0;
}

/* Find the intersection point the lines p0p1 and p2p3 */
static inline int intersection(struct cli_state *s,
                               struct point *p0, struct point *p1,
                               struct point *p2, struct point *p3,
                               struct point *result)
{
    const float m01 = (p0->y - p1->y) / (p0->x - p1->x);
    const float m23 = (p2->y - p3->y) / (p2->x - p3->x);
    const float b01 = p0->y - (m01 * p0->x);
    const float b23 = p2->y - (m23 * p2->x);

    if (m01 >= 0 || m23 <= 0) {
        cli_err(s, "Error",
                "Invalid m01 (%f) or m23 (%f) encountered.\n", m01, m23);
        return BLADERF_ERR_UNEXPECTED;
    }

    result->x = (b23 - b01) / (m01 - m23);
    result->y = m01 * result->x + b01;

    return 0;
}

/* Compute variance via incremental Online algorithm */
static void variance(int16_t *samples, float *var_i, float *var_q)
{
    unsigned int i;

    float n;
    float mean_i, delta_i, m2_i;
    float mean_q, delta_q, m2_q;

    n = mean_i = mean_q = delta_i = delta_q = m2_i = m2_q = 0.0f;

    for (i = 0; i < 2 * CAL_BUF_LEN; i += 2) {
        n += 1;

        delta_i = samples[i] - mean_i;
        mean_i = mean_i + delta_i / n;
        m2_i = m2_i + delta_i * (samples[i] - mean_i);

        delta_q = samples[i + 1] - mean_q;
        mean_q = mean_q + delta_q / n;
        m2_q = m2_q + delta_q * (samples[i + 1] - mean_q);
    }

    *var_i = m2_i / (n - 1);
    *var_q = m2_q / (n - 1);
}

static int rx_avg(struct bladerf *dev, int16_t *samples,
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

    assert(accum_i < (1 << 12) && accum_i >= (-(1 << 12)) );
    assert(accum_q < (1 << 12) && accum_i >= (-(1 << 12)) );

    *avg_i = (int16_t) accum_i;
    *avg_q = (int16_t) accum_q;

    return 0;
}

int calibrate_dc_rx(struct cli_state *s,
                    int16_t *dc_i, int16_t *dc_q,
                    int16_t *avg_i, int16_t *avg_q)
{
    int status;
    int16_t *samples = NULL;
    int16_t dc_i0, dc_q0, dc_i1, dc_q1;
    int16_t avg_i0, avg_q0, avg_i1, avg_q1;

    int16_t test_i[7], test_q[7];
    int16_t tmp_i, tmp_q, min_i, min_q;
    unsigned int min_i_idx, min_q_idx, n;

    samples = (int16_t*) malloc(CAL_BUF_LEN * 2 * sizeof(samples[0]));
    if (samples == NULL) {
        status = BLADERF_ERR_MEM;;
        goto out;
    }

    /* Ensure old samples are flushed */
    status = bladerf_enable_module(s->dev, BLADERF_MODULE_RX, false);
    if (status != 0) {
        goto out;
    }


    status = bladerf_sync_config(s->dev, BLADERF_MODULE_RX,
                                 BLADERF_FORMAT_SC16_Q11,
                                 CAL_NUM_BUFS, CAL_BUF_LEN,
                                 CAL_NUM_XFERS, CAL_TIMEOUT);
    if (status != 0) {
        goto out;
    }

    status = bladerf_enable_module(s->dev, BLADERF_MODULE_RX, true);
    if (status != 0) {
        goto out;
    }

    dc_i0 = dc_q0 = -512;
    dc_i1 = dc_q1 = 512;

    /* Get an initial set of sample points */
    status = set_rx_dc(s->dev, dc_i0, dc_q0);
    if (status != 0) {
        goto out;
    }

    status = rx_avg(s->dev, samples, &avg_i0, &avg_q0);
    if (status != 0) {
        goto out;
    }

    status = set_rx_dc(s->dev, dc_i1, dc_q1);
    if (status != 0) {
        goto out;
    }

    status = rx_avg(s->dev, samples, &avg_i1, &avg_q1);
    if (status != 0) {
        goto out;
    }

    status = interpolate(dc_i0, dc_i1, avg_i0, avg_i1, dc_i);
    if (status != 0) {
        cli_err(s, "Error", "RX I values appear to be stuck @ %d\n", avg_i0);
        goto out;
    }

    status = interpolate(dc_q0, dc_q1, avg_q0, avg_q1, dc_q);
    if (status != 0) {
        cli_err(s, "Error", "RX Q values appear to be stuck @ %d\n", avg_q0);
        goto out;
    }

    test_i[0] = *dc_i;
    test_i[1] = test_i[0] + 96;
    test_i[2] = test_i[0] + 64;
    test_i[3] = test_i[0] + 32;
    test_i[4] = test_i[0] - 32;
    test_i[5] = test_i[0] - 64;
    test_i[6] = test_i[0] - 96;

    test_q[0] = *dc_q;
    test_q[1] = test_q[0] + 96;
    test_q[2] = test_q[0] + 64;
    test_q[3] = test_q[0] + 32;
    test_q[4] = test_q[0] - 32;
    test_q[5] = test_q[0] - 64;
    test_q[6] = test_q[0] - 96;

    min_i_idx = min_q_idx = 0;
    min_i = min_q = INT16_MAX;

    for (n = 0; n < 7; n++) {

        if (test_i[n] > CAL_DC_MAX) {
            test_i[n] = CAL_DC_MAX;
        } else if (test_i[n] < CAL_DC_MIN) {
            test_i[n] = CAL_DC_MIN;
        }

        if (test_q[n] > CAL_DC_MAX) {
            test_q[n] = CAL_DC_MAX;
        } else if (test_q[n] < CAL_DC_MIN) {
            test_q[n] = CAL_DC_MIN;
        }

        /* See where we're at now... */
        status = set_rx_dc(s->dev, test_i[n], test_q[n]);
        if (status != 0) {
            goto out;
        }

        status = rx_avg(s->dev, samples, &tmp_i, &tmp_q);
        if (status != 0) {
            goto out;
        }

        if (abs(tmp_i) < abs(min_i)) {
            min_i = tmp_i;
            min_i_idx = n;
        }

        if (abs(tmp_q) < abs(min_q)) {
            min_q = tmp_q;
            min_q_idx = n;
        }
    }

    *dc_i = test_i[min_i_idx];
    *dc_q = test_q[min_q_idx];

    if (avg_i) {
        *avg_i = min_i;
    }

    if (avg_q) {
        *avg_q = min_q;
    }

    status = set_rx_dc(s->dev, *dc_i, *dc_q);
    if (status != 0) {
        goto out;
    }

out:
    free(samples);
    return status;
}

static void * exec_tx_task(void *args)
{
    bool run;
    int status = 0;
    struct cal_tx_task *task = (struct cal_tx_task*) args;

    MUTEX_LOCK(&task->lock);
    run = task->run;
    MUTEX_UNLOCK(&task->lock);

    while (run && status == 0) {
        status = bladerf_sync_tx(task->s->dev, task->samples, CAL_BUF_LEN,
                                 NULL, CAL_TIMEOUT);


        MUTEX_LOCK(&task->lock);
        run = task->run;
        MUTEX_UNLOCK(&task->lock);
    }

    MUTEX_LOCK(&task->lock);
    task->status = status;
    MUTEX_UNLOCK(&task->lock);

    return NULL;
}

static inline int init_tx_task(struct cli_state *s, struct cal_tx_task *task)
{
    memset(task, 0, sizeof(*task));

    task->started = false;
    task->run = true;
    task->s = s;

    MUTEX_INIT(&task->lock);

    /* Transmit the vector 0 + 0j */
    task->samples = (int16_t*) calloc(CAL_BUF_LEN * 2, sizeof(task->samples[0]));
    if (task->samples == NULL) {
        return BLADERF_ERR_MEM;
    }

    return 0;
}

static inline int start_tx_task(struct cal_tx_task *task)
{
    int status;

    status = pthread_create(&task->thread, NULL, exec_tx_task, task);
    if (status != 0) {
        return BLADERF_ERR_UNEXPECTED;
    } else {
        task->started = true;
        return 0;
    }
}

static inline int stop_tx_task(struct cal_tx_task *task)
{
    if (task->started) {
        MUTEX_LOCK(&task->lock);
        task->run = false;
        MUTEX_UNLOCK(&task->lock);

        pthread_join(task->thread, NULL);
    }

    return task->status;
}

int rx_avg_magnitude(struct bladerf *dev, int16_t *samples,
                     int16_t dc_i, int16_t dc_q, float *avg_magnitude)
{
    int status;
    unsigned int i;
    float var_i, var_q;

    status = set_tx_dc(dev, dc_i, dc_q);
    if (status != 0) {
        return status;
    }

    /* Flush out samples and read a buffer's worth of data */
    for (i = 0; i < 2 * CAL_NUM_BUFS + 1; i++) {
        status = bladerf_sync_rx(dev, samples, CAL_BUF_LEN, NULL, CAL_TIMEOUT);
        if (status != 0) {
            return status;
        }
    }

    variance(samples, &var_i, &var_q);
    *avg_magnitude = (float) sqrt(var_i + var_q);
    return status;
}

int calibrate_dc_tx(struct cli_state *s,
                    int16_t *dc_i, int16_t *dc_q,
                    float *error_i, float *error_q)
{
    int retval, status;
    unsigned int rx_freq, tx_freq;
    int16_t *rx_samples = NULL;
    struct cal_tx_task tx_task;

    struct point p0, p1, p2, p3;
    struct point result;

    status = bladerf_get_frequency(s->dev, BLADERF_MODULE_RX, &rx_freq);
    if (status != 0) {
        return status;
    }

    status = bladerf_get_frequency(s->dev, BLADERF_MODULE_TX, &tx_freq);
    if (status != 0) {
        return status;
    }

    rx_samples = (int16_t*) malloc(CAL_BUF_LEN * 2 * sizeof(rx_samples[0]));
    if (rx_samples == NULL) {
        return BLADERF_ERR_MEM;
    }

    status = init_tx_task(s, &tx_task);
    if (status != 0) {
        goto out;

    }

    status = bladerf_set_frequency(s->dev, BLADERF_MODULE_TX,
                                   rx_freq + (CAL_SAMPLERATE / 4));
    if (status != 0) {
        goto out;
    }

    if (tx_freq < UPPER_BAND) {
        status = bladerf_set_loopback(s->dev, BLADERF_LB_RF_LNA1);
    } else {
        status = bladerf_set_loopback(s->dev, BLADERF_LB_RF_LNA2);
    }

    if (status != 0) {
        goto out;
    }

    /* Ensure old samples are flushed */
    status = bladerf_enable_module(s->dev, BLADERF_MODULE_RX, false);
    if (status != 0) {
        goto out;
    }

    status = bladerf_enable_module(s->dev, BLADERF_MODULE_TX, false);
    if (status != 0) {
        goto out;
    }

    status = bladerf_sync_config(s->dev, BLADERF_MODULE_RX,
                                 BLADERF_FORMAT_SC16_Q11,
                                 CAL_NUM_BUFS, CAL_BUF_LEN,
                                 CAL_NUM_XFERS, CAL_TIMEOUT);
    if (status != 0) {
        goto out;
    }

    status = bladerf_sync_config(s->dev, BLADERF_MODULE_TX,
                                 BLADERF_FORMAT_SC16_Q11,
                                 CAL_NUM_BUFS, CAL_BUF_LEN,
                                 CAL_NUM_XFERS, CAL_TIMEOUT);
    if (status != 0) {
        goto out;
    }

    status = bladerf_enable_module(s->dev, BLADERF_MODULE_RX, true);
    if (status != 0) {
        goto out;
    }

    status = bladerf_enable_module(s->dev, BLADERF_MODULE_TX, true);
    if (status != 0) {
        goto out;
    }

    status = start_tx_task(&tx_task);
    if (status != 0) {
        goto out;
    }

    /* Sample the results of 4 points, which should yield 2 intersecting lines,
     * for 4 different DC offset settings of the I channel */
    p0.x = -2048;
    p1.x = -512;
    p2.x = 512;
    p3.x = 2048;

    status = rx_avg_magnitude(s->dev, rx_samples, (int16_t) p0.x, 0, &p0.y);
    if (status != 0) {
        goto out;
    }

    status = rx_avg_magnitude(s->dev, rx_samples, (int16_t) p1.x, 0, &p1.y);
    if (status != 0) {
        goto out;
    }

    status = rx_avg_magnitude(s->dev, rx_samples, (int16_t) p2.x, 0, &p2.y);
    if (status != 0) {
        goto out;
    }

    status = rx_avg_magnitude(s->dev, rx_samples, (int16_t) p3.x, 0, &p3.y);
    if (status != 0) {
        goto out;
    }

    status = intersection(s, &p0, &p1, &p2, &p3, &result);
    if (status != 0) {
        goto out;
    }

    if (result.x < CAL_DC_MIN || result.x > CAL_DC_MAX) {
        cli_err(s, "Error", "Obtained out-of-range TX I DC cal value (%f).\n",
                result.x);
        status = BLADERF_ERR_UNEXPECTED;
        goto out;
    }

    *dc_i = (int16_t) (result.x + 0.5);
    *error_i = result.y;

    status = set_tx_dc(s->dev, *dc_i, 0);
    if (status != 0) {
        goto out;
    }

    /* Repeat for the Q channel */
    status = rx_avg_magnitude(s->dev, rx_samples, *dc_i, (int16_t) p0.x, &p0.y);
    if (status != 0) {
        goto out;
    }

    status = rx_avg_magnitude(s->dev, rx_samples, *dc_i, (int16_t) p1.x, &p1.y);
    if (status != 0) {
        goto out;
    }

    status = rx_avg_magnitude(s->dev, rx_samples, *dc_i, (int16_t) p2.x, &p2.y);
    if (status != 0) {
        goto out;
    }

    status = rx_avg_magnitude(s->dev, rx_samples, *dc_i, (int16_t) p3.x, &p3.y);
    if (status != 0) {
        goto out;
    }

    status = intersection(s, &p0, &p1, &p2, &p3, &result);
    if (status != 0) {
        goto out;
    }

    *dc_q = (int16_t) (result.x + 0.5);
    *error_q = result.y;

    status = set_tx_dc(s->dev, *dc_i, *dc_q);

out:
    retval = status;

    status = stop_tx_task(&tx_task);
    retval = first_error(retval, status);

    free(rx_samples);
    free(tx_task.samples);

    status = bladerf_enable_module(s->dev, BLADERF_MODULE_TX, false);
    retval = first_error(retval, status);

    /* Restore RX frequency */
    status = bladerf_set_frequency(s->dev, BLADERF_MODULE_TX, tx_freq);
    retval = first_error(retval, status);

    return retval;
}

static inline int dummy_tx(struct bladerf *dev)
{
    int status;
    const unsigned int buf_len = CAL_BUF_LEN;
    uint16_t *buf = (uint16_t*) calloc(2 * buf_len, sizeof(buf[0]));

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

static inline int backup_and_update_settings(struct bladerf *dev,
                                            bladerf_module module,
                                            struct settings *settings)
{
    int status;

    status = bladerf_get_bandwidth(dev, module, &settings->bandwidth);
    if (status != 0) {
        return status;
    }

    status = bladerf_get_frequency(dev, module, &settings->frequency);
    if (status != 0) {
        return status;
    }

    status = bladerf_get_rational_sample_rate(dev, module,
                                              &settings->samplerate);
    if (status != 0) {
        return status;
    }

    status = bladerf_set_bandwidth(dev, module, CAL_BANDWIDTH, NULL);
    if (status != 0) {
        return status;
    }

    status = bladerf_set_sample_rate(dev, module, CAL_SAMPLERATE, NULL);

    return status;
}

static inline int restore_settings(struct bladerf *dev, bladerf_module module,
                                   struct settings *settings)
{
    int status;

    status = bladerf_set_bandwidth(dev, module, settings->bandwidth, NULL);
    if (status != 0) {
        return status;
    }

    status = bladerf_set_frequency(dev, module, settings->frequency);
    if (status != 0) {
        return status;
    }

    status = bladerf_set_rational_sample_rate(dev, module,
                                              &settings->samplerate, NULL);

    return status;
}

/* See libbladeRF's dc_cal_table.c for the packed table data format */
int calibrate_dc_gen_tbl(struct cli_state *s, bladerf_module module,
                         const char *filename, unsigned int f_low,
                         unsigned f_inc, unsigned int f_high)
{
    int retval, status;
    size_t off;
    struct bladerf_lms_dc_cals lms_dc_cals;
    unsigned int f;
    struct settings settings;
    bladerf_loopback loopback_backup;
    struct bladerf_image *image = NULL;

    const uint16_t magic = HOST_TO_LE16(0x1ab1);
    const uint32_t reserved = HOST_TO_LE32(0x00000000);
    const uint32_t tbl_version = HOST_TO_LE32(0x00000001);

    const size_t lms_data_size = 10; /* 10 uint8_t register values */

    const uint32_t n_frequencies = (f_high - f_low) / f_inc + 1;
    const uint32_t n_frequencies_le = HOST_TO_LE32(n_frequencies);

    const size_t entry_size = sizeof(uint32_t) +   /* Frequency */
                              2 * sizeof(int16_t); /* DC I and Q valus */

    const size_t table_size = n_frequencies * entry_size;

    const size_t data_size = sizeof(magic) + sizeof(reserved) +
                             sizeof(tbl_version) + sizeof(n_frequencies_le) +
                             lms_data_size + table_size;

    assert(data_size <= UINT_MAX);

    status = backup_and_update_settings(s->dev, module, &settings);
    if (status != 0) {
        return status;
    }

    status = bladerf_get_loopback(s->dev, &loopback_backup);
    if (status != 0) {
        return status;
    }

    status = bladerf_lms_get_dc_cals(s->dev, &lms_dc_cals);
    if (status != 0) {
        goto out;
    }

    if (module == BLADERF_MODULE_RX) {
        image = bladerf_alloc_image(BLADERF_IMAGE_TYPE_RX_DC_CAL,
                                    0xffffffff, (unsigned int) data_size);
    } else {
        image = bladerf_alloc_image(BLADERF_IMAGE_TYPE_TX_DC_CAL,
                                    0xffffffff, (unsigned int) data_size);
    }

    if (image == NULL) {
        status = BLADERF_ERR_MEM;
        goto out;
    }

    status = bladerf_get_serial(s->dev, image->serial);
    if (status != 0) {
        goto out;
    }

    if (module == BLADERF_MODULE_RX) {
        status = bladerf_set_loopback(s->dev, BLADERF_LB_NONE);
        if (status != 0) {
            goto out;
        }
    }

    off = 0;

    memcpy(&image->data[off], &magic, sizeof(magic));
    off += sizeof(magic);

    memcpy(&image->data[off], &reserved, sizeof(reserved));
    off += sizeof(reserved);

    memcpy(&image->data[off], &tbl_version, sizeof(tbl_version));
    off += sizeof(tbl_version);

    memcpy(&image->data[off], &n_frequencies_le, sizeof(n_frequencies_le));
    off += sizeof(n_frequencies_le);

    image->data[off++] = (uint8_t)lms_dc_cals.lpf_tuning;
    image->data[off++] = (uint8_t)lms_dc_cals.tx_lpf_i;
    image->data[off++] = (uint8_t)lms_dc_cals.tx_lpf_q;
    image->data[off++] = (uint8_t)lms_dc_cals.rx_lpf_i;
    image->data[off++] = (uint8_t)lms_dc_cals.rx_lpf_q;
    image->data[off++] = (uint8_t)lms_dc_cals.dc_ref;
    image->data[off++] = (uint8_t)lms_dc_cals.rxvga2a_i;
    image->data[off++] = (uint8_t)lms_dc_cals.rxvga2a_q;
    image->data[off++] = (uint8_t)lms_dc_cals.rxvga2b_i;
    image->data[off++] = (uint8_t)lms_dc_cals.rxvga2b_q;

    putchar('\n');

    for (f = f_low; f <= f_high; f += f_inc) {
        const uint32_t frequency = HOST_TO_LE32((uint32_t)f);
        int16_t dc_i, dc_q;

        printf("  Calibrating @ %u Hz...", f);

        status = bladerf_set_frequency(s->dev, module, f);
        if (status != 0) {
            goto out;
        }

        if (module == BLADERF_MODULE_RX) {
            int16_t error_i, error_q;
            status = calibrate_dc_rx(s, &dc_i, &dc_q, &error_i, &error_q);
            printf("    I=%-4d (avg: %-4d), Q=%-4d (avg: %-4d)\r",
                    dc_i, error_i, dc_q, error_q);
        } else {
            float error_i, error_q;
            status = calibrate_dc_tx(s, &dc_i, &dc_q, &error_i, &error_q);
            printf("    I=%-4d (avg: %3.3f), Q=%-4d (avg: %3.3f)\r",
                    dc_i, error_i, dc_q, error_q);
        }

        if (status != 0) {
            goto out;
        }

        fflush(stdout);

        dc_i = HOST_TO_LE16(dc_i);
        dc_q = HOST_TO_LE16(dc_q);

        memcpy(&image->data[off], &frequency, sizeof(frequency));
        off += sizeof(frequency);

        memcpy(&image->data[off], &dc_i, sizeof(dc_i));
        off += sizeof(dc_i);

        memcpy(&image->data[off], &dc_q, sizeof(dc_q));
        off += sizeof(dc_q);
    }

    status = bladerf_image_write(image, filename);

    printf("\n  Done.\n\n");

out:
    retval = status;

    if (module == BLADERF_MODULE_RX) {
        status = bladerf_set_loopback(s->dev, loopback_backup);
        retval = first_error(retval, status);
    }

    status = bladerf_enable_module(s->dev, BLADERF_MODULE_RX, false);
    retval = first_error(retval, status);

    status = restore_settings(s->dev, module, &settings);
    retval = first_error(retval, status);

    bladerf_free_image(image);
    return retval;
}

int calibrate_dc(struct cli_state *s, unsigned int ops)
{
    int retval = 0;
    int status = BLADERF_ERR_UNEXPECTED;
    struct settings rx_settings, tx_settings;
    bladerf_loopback loopback;
    int16_t dc_i, dc_q;

    if (IS_RX_CAL(ops)) {
        status = backup_and_update_settings(s->dev, BLADERF_MODULE_RX,
                                            &rx_settings);
        if (status != 0) {
            s->last_lib_error = status;
            return CLI_RET_LIBBLADERF;
        }
    }

    if (IS_TX_CAL(ops)) {
        status = backup_and_update_settings(s->dev, BLADERF_MODULE_TX,
                                            &tx_settings);
        if (status != 0) {
            s->last_lib_error = status;
            return CLI_RET_LIBBLADERF;
        }
    }

    status = bladerf_get_loopback(s->dev, &loopback);
    if (status != 0) {
        s->last_lib_error = status;
        return CLI_RET_LIBBLADERF;
    }

    if (IS_RX_CAL(ops)) {
        status = set_rx_dc(s->dev, 0, 0);
        if (status != 0) {
            goto error;
        }

        status = bladerf_enable_module(s->dev, BLADERF_MODULE_RX, true);
        if (status != 0) {
            goto error;
        }
    }

    if (IS_TX_CAL(ops)) {
        status = bladerf_enable_module(s->dev, BLADERF_MODULE_RX, true);
        if (status != 0) {
            goto error;
        }

        status = bladerf_set_loopback(s->dev, BLADERF_LB_BB_TXVGA1_RXVGA2);
        if (status != 0) {
            goto error;
        }

        status = bladerf_enable_module(s->dev, BLADERF_MODULE_TX, true);
        if (status != 0) {
            goto error;
        }

        status = dummy_tx(s->dev);
        if (status != 0) {
            goto error;
        }

        status = bladerf_enable_module(s->dev, BLADERF_MODULE_TX, false);
        if (status != 0) {
            goto error;
        }
    }

    status = bladerf_set_loopback(s->dev, BLADERF_LB_NONE);
    if (status != 0) {
        goto error;
    }

    putchar('\n');

    if (IS_CAL(CAL_DC_LMS_TUNING, ops)) {
        printf("  Calibrating LMS LPF tuning module...\n");
        status = bladerf_calibrate_dc(s->dev, BLADERF_DC_CAL_LPF_TUNING);
        if (status != 0) {
            goto error;
        } else {
            struct bladerf_lms_dc_cals dc_cals;
            status = bladerf_lms_get_dc_cals(s->dev, &dc_cals);
            if (status != 0) {
                goto error;
            }

            printf("    LPF tuning module: %d\n\n", dc_cals.lpf_tuning);
        }
    }

    if (IS_CAL(CAL_DC_LMS_TXLPF, ops)) {
        printf("  Calibrating LMS TX LPF modules...\n");
        status = bladerf_calibrate_dc(s->dev, BLADERF_DC_CAL_TX_LPF);
        if (status != 0) {
            goto error;
        } else {
            struct bladerf_lms_dc_cals dc_cals;
            status = bladerf_lms_get_dc_cals(s->dev, &dc_cals);
            if (status != 0) {
                goto error;
            }

            printf("    TX LPF I filter: %d\n", dc_cals.tx_lpf_i);
            printf("    TX LPF Q filter: %d\n\n", dc_cals.tx_lpf_q);
        }
    }

    if (IS_CAL(CAL_DC_LMS_RXLPF, ops)) {
        printf("  Calibrating LMS RX LPF modules...\n");
        status = bladerf_calibrate_dc(s->dev, BLADERF_DC_CAL_RX_LPF);
        if (status != 0) {
            goto error;
        } else {
            struct bladerf_lms_dc_cals dc_cals;
            status = bladerf_lms_get_dc_cals(s->dev, &dc_cals);
            if (status != 0) {
                goto error;
            }

            printf("    RX LPF I filter: %d\n", dc_cals.rx_lpf_i);
            printf("    RX LPF Q filter: %d\n\n", dc_cals.rx_lpf_q);
        }
    }

    if (IS_CAL(CAL_DC_LMS_RXVGA2, ops)) {
        printf("  Calibrating LMS RXVGA2 modules...\n");
        status = bladerf_calibrate_dc(s->dev, BLADERF_DC_CAL_RXVGA2);
        if (status != 0) {
            goto error;
        } else {
            struct bladerf_lms_dc_cals dc_cals;
            status = bladerf_lms_get_dc_cals(s->dev, &dc_cals);
            if (status != 0) {
                goto error;
            }

            printf("    RX VGA2 DC reference module: %d\n", dc_cals.dc_ref);
            printf("    RX VGA2 stage 1, I channel: %d\n", dc_cals.rxvga2a_i);
            printf("    RX VGA2 stage 1, Q channel: %d\n", dc_cals.rxvga2a_q);
            printf("    RX VGA2 stage 2, I channel: %d\n", dc_cals.rxvga2b_i);
            printf("    RX VGA2 stage 2, Q channel: %d\n\n", dc_cals.rxvga2b_q);
        }
    }


    if (IS_CAL(CAL_DC_AUTO_RX, ops)) {
        int16_t avg_i, avg_q;
        status = calibrate_dc_rx(s, &dc_i, &dc_q, &avg_i, &avg_q);
        if (status != 0) {
            goto error;
        } else {
            printf("  RX DC I Setting = %d, error ~= %d\n", dc_i, avg_i);
            printf("  RX DC Q Setting = %d, error ~= %d\n\n", dc_q, avg_q);
        }
    }

    if (IS_CAL(CAL_DC_AUTO_TX, ops)) {
        float error_i, error_q;
        status = calibrate_dc_tx(s, &dc_i, &dc_q, &error_i, &error_q);
        if (status != 0) {
            goto error;
        } else {
            printf("  TX DC I Setting = %d, error ~= %f\n", dc_i, error_i);
            printf("  TX DC Q Setting = %d, error ~= %f\n\n", dc_q, error_q);
        }
    }

error:
    retval = status;

    if (IS_RX_CAL(ops)) {
        status = restore_settings(s->dev, BLADERF_MODULE_RX, &rx_settings);
        retval = first_error(retval, status);
    }


    if (IS_TX_CAL(ops)) {
        status = restore_settings(s->dev, BLADERF_MODULE_TX, &tx_settings);
        retval = first_error(retval, status);
    }

    status = bladerf_enable_module(s->dev, BLADERF_MODULE_RX, false);
    retval = first_error(retval, status);

    status = bladerf_enable_module(s->dev, BLADERF_MODULE_TX, false);
    retval = first_error(retval, status);

    status = bladerf_set_loopback(s->dev, loopback);
    retval = first_error(retval, status);

    if (retval != 0) {
        s->last_lib_error = retval;
        retval = CLI_RET_LIBBLADERF;
    }

    return retval;
}
