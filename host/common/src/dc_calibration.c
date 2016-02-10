/**
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (c) 2015 Nuand LLC
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

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <limits.h>

#define _USE_MATH_DEFINES /* Required for MSVC */
#include <math.h>

#include <libbladeRF.h>

#include "dc_calibration.h"
#include "conversions.h"

struct complexf {
    float i;
    float q;
};

/*******************************************************************************
 * Debug items
 ******************************************************************************/

/* Enable this to print diagnostic and debug information */
//#define ENABLE_DC_CALIBRATION_DEBUG
//#define ENABLE_DC_CALIBRATION_VERBOSE

#ifndef PR_DBG
#   ifdef ENABLE_DC_CALIBRATION_DEBUG
#      define PR_DBG(...) fprintf(stderr, "  " __VA_ARGS__)
#   else
#      define PR_DBG(...) do {} while (0)
#   endif
#endif

#ifndef PR_VERBOSE
#   ifdef ENABLE_DC_CALIBRATION_VERBOSE
#       define PR_VERBOSE(...) fprintf(stderr, "  " __VA_ARGS__)
#   else
#       define PR_VERBOSE(...) do {} while (0)
#   endif
#endif

/*******************************************************************************
 * Debug routines for saving samples
 ******************************************************************************/

//#define ENABLE_SAVE_SC16Q11
#ifdef ENABLE_SAVE_SC16Q11
static void save_sc16q11(const char *name, int16_t *samples, unsigned int count)
{
    FILE *out = fopen(name, "wb");
    if (!out) {
        return;
    }

    fwrite(samples, 2 * sizeof(samples[0]), count, out);
    fclose(out);
}
#else
#   define save_sc16q11(name, samples, count) do {} while (0)
#endif

//#define ENABLE_SAVE_COMPLEXF
#ifdef ENABLE_SAVE_COMPLEXF
static void save_complexf(const char *name, struct complexf *samples,
                          unsigned int count)
{
    unsigned int n;
    FILE *out = fopen(name, "wb");
    if (!out) {
        return;
    }

    for (n = 0; n < count; n++) {
        fwrite(&samples[n].i, sizeof(samples[n].i), 1, out);
        fwrite(&samples[n].q, sizeof(samples[n].q), 1, out);
    }

    fclose(out);
}
#else
#   define save_complexf(name, samples, count) do {} while (0)
#endif


/*******************************************************************************
 * LMS6002D DC offset calibration
 ******************************************************************************/

/* We've found that running samples through the LMS6 tends to be required
 * for the TX LPF calibration to converge */
static inline int tx_lpf_dummy_tx(struct bladerf *dev)
{
    int status;
    int retval = 0;
    struct bladerf_metadata meta;
    int16_t zero_sample[] = { 0, 0 };

    bladerf_loopback loopback_backup;
    struct bladerf_rational_rate sample_rate_backup;

    memset(&meta, 0, sizeof(meta));

    status = bladerf_get_loopback(dev, &loopback_backup);
    if (status != 0) {
        return status;
    }

    status = bladerf_get_rational_sample_rate(dev, BLADERF_MODULE_TX,
                                              &sample_rate_backup);
    if (status != 0) {
        return status;
    }

    status = bladerf_set_loopback(dev, BLADERF_LB_BB_TXVGA1_RXVGA2);
    if (status != 0) {
        goto out;
    }

    status = bladerf_set_sample_rate(dev, BLADERF_MODULE_TX, 3000000, NULL);
    if (status != 0) {
        goto out;
    }

    status = bladerf_sync_config(dev, BLADERF_MODULE_TX,
                                 BLADERF_FORMAT_SC16_Q11_META,
                                 64, 16384, 16, 1000);
    if (status != 0) {
        goto out;
    }

    status = bladerf_enable_module(dev, BLADERF_MODULE_TX, true);
    if (status != 0) {
        goto out;
    }

    meta.flags = BLADERF_META_FLAG_TX_BURST_START |
                 BLADERF_META_FLAG_TX_BURST_END   |
                 BLADERF_META_FLAG_TX_NOW;

    status = bladerf_sync_tx(dev, zero_sample, 1, &meta, 2000);
    if (status != 0) {
        goto out;
    }

out:
    status = bladerf_enable_module(dev, BLADERF_MODULE_TX, false);
    if (status != 0 && retval == 0) {
        retval = status;
    }

    status = bladerf_set_rational_sample_rate(dev, BLADERF_MODULE_TX,
                                              &sample_rate_backup, NULL);
    if (status != 0 && retval == 0) {
        retval = status;
    }

    status = bladerf_set_loopback(dev, loopback_backup);
    if (status != 0 && retval == 0) {
        retval = status;
    }

    return retval;
}

static int cal_tx_lpf(struct bladerf *dev)
{
    int status;

    status = tx_lpf_dummy_tx(dev);
    if (status == 0) {
        status = bladerf_calibrate_dc(dev, BLADERF_DC_CAL_TX_LPF);
    }

    return status;
}

int dc_calibration_lms6(struct bladerf *dev, const char *module_str)
{
    int status;
    bladerf_cal_module module;

    if (!strcasecmp(module_str, "all")) {
        status = bladerf_calibrate_dc(dev, BLADERF_DC_CAL_LPF_TUNING);
        if (status != 0) {
            return status;
        }

        status = cal_tx_lpf(dev);
        if (status != 0) {
            return status;
        }

        status = bladerf_calibrate_dc(dev, BLADERF_DC_CAL_RX_LPF);
        if (status != 0) {
            return status;
        }

        status = bladerf_calibrate_dc(dev, BLADERF_DC_CAL_RXVGA2);
    } else {
        module = str_to_bladerf_cal_module(module_str);
        if (module == BLADERF_DC_CAL_INVALID) {
            return BLADERF_ERR_INVAL;
        }

        if (module == BLADERF_DC_CAL_TX_LPF) {
            status = cal_tx_lpf(dev);
        } else {
            status = bladerf_calibrate_dc(dev, module);
        }
    }

    return status;
}



/*******************************************************************************
 * Shared utility routines
 ******************************************************************************/

/* Convert ms to samples */
#define MS_TO_SAMPLES(ms_, rate_) (\
    (unsigned int) (ms_ * ((uint64_t) rate_) / 1000) \
)

/* RX samples, retrying if the machine is struggling to keep up. */
static int rx_samples(struct bladerf *dev, int16_t *samples,
                      unsigned int count, uint64_t *ts, uint64_t ts_inc)
{
    int status = 0;
    struct bladerf_metadata meta;
    int retry = 0;
    const int max_retries = 10;
    bool overrun = true;

    memset(&meta, 0, sizeof(meta));
    meta.timestamp = *ts;

    while (status == 0 && overrun && retry < max_retries) {
        meta.timestamp = *ts;
        status = bladerf_sync_rx(dev, samples, count, &meta, 2000);

        if (status == BLADERF_ERR_TIME_PAST) {
            status = bladerf_get_timestamp(dev, BLADERF_MODULE_RX, ts);
            if (status != 0) {
                return status;
            } else {
                *ts += 20 * ts_inc;
                retry++;
                status = 0;
            }
        } else if (status == 0) {
            overrun = (meta.flags & BLADERF_META_STATUS_OVERRUN) != 0;
            if (overrun) {
                *ts += count + ts_inc;
                retry++;
            }
        } else {
            return status;
        }
    }

    if (retry >= max_retries) {
        status = BLADERF_ERR_IO;
    } else if (status == 0) {
        *ts += count + ts_inc;
    }

    return status;
}



/*******************************************************************************
 * RX DC offset calibration
 ******************************************************************************/

#define RX_CAL_RATE             (3000000)
#define RX_CAL_BW               (1500000)
#define RX_CAL_TS_INC           (MS_TO_SAMPLES(15, RX_CAL_RATE))
#define RX_CAL_COUNT            (MS_TO_SAMPLES(5,  RX_CAL_RATE))

#define RX_CAL_MAX_SWEEP_LEN    (2 * 2048 / 32) /* -2048 : 32 : 2048 */

struct rx_cal {
    struct bladerf *dev;

    int16_t *samples;
    unsigned int num_samples;

    int16_t *corr_sweep;

    uint64_t ts;

    unsigned int tx_freq;
};

struct rx_cal_backup {
    struct bladerf_rational_rate rational_sample_rate;
    unsigned int bandwidth;
    unsigned int tx_freq;
};

static int get_rx_cal_backup(struct bladerf *dev, struct rx_cal_backup *b)
{
    int status;

    status = bladerf_get_rational_sample_rate(dev, BLADERF_MODULE_RX,
                                              &b->rational_sample_rate);
    if (status != 0) {
        return status;
    }

    status = bladerf_get_bandwidth(dev, BLADERF_MODULE_RX, &b->bandwidth);
    if (status != 0) {
        return status;
    }

    status = bladerf_get_frequency(dev, BLADERF_MODULE_TX, &b->tx_freq);
    if (status != 0) {
        return status;
    }

    return status;
}

static int set_rx_cal_backup(struct bladerf *dev, struct rx_cal_backup *b)
{
    int status;
    int retval = 0;

    status = bladerf_set_rational_sample_rate(dev, BLADERF_MODULE_RX,
                                              &b->rational_sample_rate, NULL);
    if (status != 0 && retval == 0) {
        retval = status;
    }

    status = bladerf_set_bandwidth(dev, BLADERF_MODULE_RX, b->bandwidth, NULL);
    if (status != 0 && retval == 0) {
        retval = status;
    }

    status = bladerf_set_frequency(dev, BLADERF_MODULE_TX, b->tx_freq);
    if (status != 0 && retval == 0) {
        retval = status;
    }

    return retval;
}

/* Ensure TX >= 1 MHz away from the RX frequency to avoid any potential
 * artifacts from the PLLs interfering with one another */
static int rx_cal_update_frequency(struct rx_cal *cal, unsigned int rx_freq)
{
    int status = 0;
    unsigned int f_diff;

    if (rx_freq < cal->tx_freq) {
        f_diff = cal->tx_freq - rx_freq;
    } else {
        f_diff = rx_freq - cal->tx_freq;
    }

    PR_DBG("Set F_RX = %u\n", rx_freq);
    PR_DBG("F_diff(RX, TX) = %u\n", f_diff);

    if (f_diff < 1000000) {
        if (rx_freq >= (BLADERF_FREQUENCY_MIN + 1000000)) {
            cal->tx_freq = rx_freq - 1000000;
        } else {
            cal->tx_freq = rx_freq + 1000000;
        }

        status = bladerf_set_frequency(cal->dev, BLADERF_MODULE_TX,
                                       cal->tx_freq);
        if (status != 0) {
            return status;
        }

        PR_DBG("Adjusted TX frequency: %u\n", cal->tx_freq);
    }

    status = bladerf_set_frequency(cal->dev, BLADERF_MODULE_RX, rx_freq);
    if (status != 0) {
        return status;
    }

    cal->ts += RX_CAL_TS_INC;

    return status;
}

static inline void sample_mean(int16_t *samples, size_t count,
                               float *mean_i, float *mean_q)
{
    int64_t accum_i = 0;
    int64_t accum_q = 0;

    size_t n;


    if (count == 0) {
        assert(!"Invalid count (0) provided to sample_mean()");
        *mean_i = 0;
        *mean_q = 0;
        return;
    }

    for (n = 0; n < (2 * count); n += 2) {
        accum_i += samples[n];
        accum_q += samples[n + 1];
    }

    *mean_i = ((float) accum_i) / count;
    *mean_q = ((float) accum_q) / count;
}

static inline int set_rx_dc_corr(struct bladerf *dev, int16_t i, int16_t q)
{
    int status;

    status = bladerf_set_correction(dev, BLADERF_MODULE_RX,
                                    BLADERF_CORR_LMS_DCOFF_I, i);
    if (status != 0) {
        return status;
    }

    status = bladerf_set_correction(dev, BLADERF_MODULE_RX,
                                    BLADERF_CORR_LMS_DCOFF_Q, q);
    return status;
}

/* Get the mean for one of the coarse estimate points. If it seems that this
 * value might be (or close) causing us to clamp, adjust it and retry */
static int rx_cal_coarse_means(struct rx_cal *cal, int16_t *corr_value,
                               float *mean_i, float *mean_q)
{
    int status;
    const int16_t mean_limit_high = 2000;
    const int16_t mean_limit_low  = -mean_limit_high;
    const int16_t corr_limit = 128;
    bool retry = false;

    do {
        status = set_rx_dc_corr(cal->dev, *corr_value, *corr_value);
        if (status != 0) {
            return status;
        }

        status = rx_samples(cal->dev, cal->samples, cal->num_samples,
                            &cal->ts, RX_CAL_TS_INC);
        if (status != 0) {
            return status;
        }

        sample_mean(cal->samples, cal->num_samples, mean_i, mean_q);

        if (*mean_i > mean_limit_high || *mean_q > mean_limit_high ||
            *mean_i < mean_limit_low  || *mean_q < mean_limit_low    ) {

            if (*corr_value < 0) {
                retry = (*corr_value <= -corr_limit);
            } else {
                retry = (*corr_value >= corr_limit);
            }

            if (retry) {
                PR_DBG("Coarse estimate point Corr=%4d yields extreme means: "
                       "(%4f, %4f). Retrying...\n",
                       *corr_value, *mean_i, *mean_q);

                *corr_value = *corr_value / 2;
            }
        } else {
            retry = false;
        }
    } while (retry);

    if (retry) {
        PR_DBG("Non-ideal values are being used.\n");
    }

    return 0;
}

/* Estimate the DC correction values that yield zero DC offset via a linear
 * approximation */
static int rx_cal_coarse_estimate(struct rx_cal *cal,
                                  int16_t *i_est, int16_t *q_est)
{
    int status;
    int16_t x1 = -2048;
    int16_t x2 = 2048;
    float y1i, y1q, y2i, y2q;
    float mi, mq;
    float bi, bq;
    float i_guess, q_guess;

    status = rx_cal_coarse_means(cal, &x1, &y1i, &y1q);
    if (status != 0) {
        *i_est = 0;
        *q_est = 0;
        return status;
    }

    PR_VERBOSE("Means for x1=%d: y1i=%f, y1q=%f\n", x1, y1i, y1q);

    status = rx_cal_coarse_means(cal, &x2, &y2i, &y2q);
    if (status != 0) {
        *i_est = 0;
        *q_est = 0;
        return status;
    }

    PR_VERBOSE("Means for x2: y2i=%f, y2q=%f\n", y2i, y2q);

    mi = (y2i - y1i) / (x2 - x1);
    mq = (y2q - y1q) / (x2 - x1);

    bi = y1i - mi * x1;
    bq = y1q - mq * x1;

    PR_VERBOSE("mi=%f, bi=%f, mq=%f, bq=%f\n", mi, bi, mq, bq);

    i_guess = -bi/mi + 0.5f;
    if (i_guess < -2048) {
        i_guess = -2048;
    } else if (i_guess > 2048) {
        i_guess = 2048;
    }

    q_guess = -bq/mq + 0.5f;
    if (q_guess < -2048) {
        q_guess = -2048;
    } else if (q_guess > 2048) {
        q_guess = 2048;
    }

    *i_est = (int16_t) i_guess;
    *q_est = (int16_t) q_guess;

    PR_DBG("Coarse estimate: I=%d, Q=%d\n", *i_est, *q_est);

    return 0;
}

static void init_rx_cal_sweep(int16_t *corr, unsigned int *sweep_len,
                              int16_t i_est, int16_t q_est)
{
    unsigned int actual_len = 0;
    unsigned int i;

    int16_t sweep_min, sweep_max, sweep_val;

    /* LMS6002D RX DC calibrations have a limited range. libbladeRF throws away
     * the lower 5 bits. */
    const int16_t sweep_inc = 32;

    const int16_t min_est = (i_est < q_est) ? i_est : q_est;
    const int16_t max_est = (i_est > q_est) ? i_est : q_est;

    sweep_min = min_est - 12 * 32;
    if (sweep_min < -2048) {
        sweep_min = -2048;
    }

    sweep_max = max_est + 12 * 32;
    if (sweep_max > 2048) {
        sweep_max = 2048;
    }

    /* Given that these lower bits are thrown away, it can be confusing to
     * see that values change in their LSBs that don't matter. Therefore,
     * we'll adjust to muliples of sweep_inc */
    sweep_min = (sweep_min / 32) * 32;
    sweep_max = (sweep_max / 32) * 32;


    PR_DBG("Sweeping [%d : %d : %d]\n", sweep_min, sweep_inc, sweep_max);

    sweep_val = sweep_min;
    for (i = 0; sweep_val < sweep_max && i < RX_CAL_MAX_SWEEP_LEN; i++) {
        corr[i] = sweep_val;
        sweep_val += sweep_inc;
        actual_len++;
    }

    *sweep_len = actual_len;
}

static int rx_cal_sweep(struct rx_cal *cal,
                        int16_t *corr, unsigned int sweep_len,
                        int16_t *result_i, int16_t *result_q,
                        float *error_i,  float *error_q)
{
    int status = BLADERF_ERR_UNEXPECTED;
    unsigned int n;

    int16_t min_corr_i = 0;
    int16_t min_corr_q = 0;

    float mean_i, mean_q;
    float min_val_i, min_val_q;

    min_val_i = min_val_q = 2048;

    for (n = 0; n < sweep_len; n++) {
        status = set_rx_dc_corr(cal->dev, corr[n], corr[n]);
        if (status != 0) {
            return status;
        }

        status = rx_samples(cal->dev, cal->samples, cal->num_samples,
                            &cal->ts, RX_CAL_TS_INC);
        if (status != 0) {
            return status;
        }

        sample_mean(cal->samples, cal->num_samples, &mean_i, &mean_q);

        PR_VERBOSE("  Corr=%4d, Mean_I=%4.2f, Mean_Q=%4.2f\n",
                   corr[n], mean_i, mean_q);

        /* Not using fabs() to avoid adding a -lm dependency */
        if (mean_i < 0) {
            mean_i = -mean_i;
        }

        if (mean_q < 0) {
            mean_q = -mean_q;
        }

        if (mean_i < min_val_i) {
            min_val_i  = mean_i;
            min_corr_i = corr[n];
        }

        if (mean_q < min_val_q) {
            min_val_q  = mean_q;
            min_corr_q = corr[n];
        }
    }

    *result_i = min_corr_i;
    *result_q = min_corr_q;
    *error_i  = min_val_i;
    *error_q  = min_val_q;

    return 0;
}

static int perform_rx_cal(struct rx_cal *cal, struct dc_calibration_params *p)
{
    int status;
    int16_t i_est, q_est;
    unsigned int sweep_len = RX_CAL_MAX_SWEEP_LEN;

    status = rx_cal_update_frequency(cal, p->frequency);
    if (status != 0) {
        return status;
    }

    /* Get an initial guess at our correction values */
    status = rx_cal_coarse_estimate(cal, &i_est, &q_est);
    if (status != 0) {
        return status;
    }

    /* Perform a finer sweep of correction values */
    init_rx_cal_sweep(cal->corr_sweep, &sweep_len, i_est, q_est);

    /* Advance our timestmap just to account for any time we may have lost */
    cal->ts += RX_CAL_TS_INC;

    status = rx_cal_sweep(cal, cal->corr_sweep, sweep_len,
                          &p->corr_i, &p->corr_q,
                          &p->error_i, &p->error_q);

    if (status != 0) {
        return status;
    }

    /* Apply the nominal correction values */
    status = set_rx_dc_corr(cal->dev, p->corr_i, p->corr_q);

    return status;
}

static int rx_cal_init_state(struct bladerf *dev,
                             const struct rx_cal_backup *backup,
                             struct rx_cal *state)
{
    int status;

    state->dev = dev;

    state->num_samples  = RX_CAL_COUNT;

    state->samples = malloc(2 * sizeof(state->samples[0]) * RX_CAL_COUNT);
    if (state->samples == NULL) {
        return BLADERF_ERR_MEM;
    }

    state->corr_sweep = malloc(sizeof(state->corr_sweep[0]) * RX_CAL_MAX_SWEEP_LEN);
    if (state->corr_sweep == NULL) {
        return BLADERF_ERR_MEM;
    }

    state->tx_freq = backup->tx_freq;

    status = bladerf_get_timestamp(dev, BLADERF_MODULE_RX, &state->ts);
    if (status != 0) {
        return status;
    }

    /* Schedule first RX well into the future */
    state->ts += 20 * RX_CAL_TS_INC;

    return status;
}

static int rx_cal_init(struct bladerf *dev)
{
    int status;

    status = bladerf_set_sample_rate(dev, BLADERF_MODULE_RX, RX_CAL_RATE, NULL);
    if (status != 0) {
        return status;
    }

    status = bladerf_set_bandwidth(dev, BLADERF_MODULE_RX, RX_CAL_BW, NULL);
    if (status != 0) {
        return status;
    }

    status = bladerf_sync_config(dev, BLADERF_MODULE_RX,
                                 BLADERF_FORMAT_SC16_Q11_META,
                                 64, 16384, 16, 1000);
    if (status != 0) {
        return status;
    }

    status = bladerf_enable_module(dev, BLADERF_MODULE_RX, true);
    if (status != 0) {
        return status;
    }

    return status;
}

int dc_calibration_rx(struct bladerf *dev,
                      struct dc_calibration_params *params,
                      size_t params_count, bool print_status)
{
    int status = 0;
    int retval = 0;
    struct rx_cal state;
    struct rx_cal_backup backup;
    size_t i;

    memset(&state, 0, sizeof(state));

    status = get_rx_cal_backup(dev, &backup);
    if (status != 0) {
        return status;
    }

    status = rx_cal_init(dev);
    if (status != 0) {
        goto out;
    }

    status = rx_cal_init_state(dev, &backup, &state);
    if (status != 0) {
        goto out;
    }

    for (i = 0; i < params_count && status == 0; i++) {
        status = perform_rx_cal(&state, &params[i]);

        if (status == 0 && print_status) {
#           ifdef DEBUG_DC_CALIBRATION
            const char sol = '\n';
            const char eol = '\n';
#           else
            const char sol = '\r';
            const char eol = '\0';
#           endif
            printf("%cCalibrated @ %10u Hz: I=%4d (Error: %4.2f), "
                   "Q=%4d (Error: %4.2f)      %c",
                   sol,
                   params[i].frequency,
                   params[i].corr_i, params[i].error_i,
                   params[i].corr_q, params[i].error_q,
                   eol);
            fflush(stdout);
        }
    }

    if (print_status) {
        putchar('\n');
    }

out:
    free(state.samples);
    free(state.corr_sweep);

    retval = status;

    status = bladerf_enable_module(dev, BLADERF_MODULE_RX, false);
    if (status != 0 && retval == 0) {
        retval = status;
    }

    status = set_rx_cal_backup(dev, &backup);
    if (status != 0 && retval == 0) {
        retval = status;
    }

    return retval;
}



/*******************************************************************************
 * TX DC offset calibration
 ******************************************************************************/

#define TX_CAL_RATE     (4000000)

#define TX_CAL_RX_BW    (3000000)
#define TX_CAL_RX_LNA   (BLADERF_LNA_GAIN_MAX)
#define TX_CAL_RX_VGA1  (25)
#define TX_CAL_RX_VGA2  (0)

#define TX_CAL_TX_BW    (1500000)

#define TX_CAL_TS_INC   (MS_TO_SAMPLES(15, TX_CAL_RATE))
#define TX_CAL_COUNT    (MS_TO_SAMPLES(5,  TX_CAL_RATE))

#define TX_CAL_CORR_SWEEP_LEN (4096 / 16)   /* -2048:16:2048 */

#define TX_CAL_DEFAULT_LB (BLADERF_LB_RF_LNA1)

struct tx_cal_backup {
    unsigned int rx_freq;
    struct bladerf_rational_rate rx_sample_rate;
    unsigned int rx_bandwidth;

    bladerf_lna_gain rx_lna;
    int rx_vga1;
    int rx_vga2;

    struct bladerf_rational_rate tx_sample_rate;
    unsigned int tx_bandwidth;

    bladerf_loopback loopback;
};

struct tx_cal {
    struct bladerf *dev;
    int16_t *samples;           /* Raw samples */
    unsigned int num_samples;   /* Number of raw samples */
    struct complexf *filt;      /* Filter state */
    struct complexf *filt_out;  /* Filter output */
    struct complexf *post_mix;  /* Post-filter, mixed to baseband */
    int16_t *sweep;             /* Correction sweep */
    float   *mag;               /* Magnitude results from sweep */
    uint64_t ts;                /* Timestamp */
    bladerf_loopback loopback;  /* Current loopback mode */
    bool rx_low;                /* RX tuned lower than TX */
};

/* Filter used to isolate contribution of TX LO leakage in received
 * signal. 15th order Equiripple FIR with Fs=4e6, Fpass=1, Fstop=1e6
 */
static const float tx_cal_filt[] = {
    0.000327949366768f, 0.002460188536582f, 0.009842382390924f,
    0.027274728394777f, 0.057835200476419f, 0.098632713294830f,
    0.139062540460741f, 0.164562494987592f, 0.164562494987592f,
    0.139062540460741f, 0.098632713294830f, 0.057835200476419f,
    0.027274728394777f, 0.009842382390924f, 0.002460188536582f,
    0.000327949366768f,
};

static const unsigned int tx_cal_filt_num_taps =
    (sizeof(tx_cal_filt) / sizeof(tx_cal_filt[0]));

static inline int set_tx_dc_corr(struct bladerf *dev, int16_t i, int16_t q)
{
    int status;

    status = bladerf_set_correction(dev, BLADERF_MODULE_TX,
                                    BLADERF_CORR_LMS_DCOFF_I, i);
    if (status != 0) {
        return status;
    }

    status = bladerf_set_correction(dev, BLADERF_MODULE_TX,
                                    BLADERF_CORR_LMS_DCOFF_Q, q);
    return status;
}

static int get_tx_cal_backup(struct bladerf *dev, struct tx_cal_backup *b)
{
    int status;

    status = bladerf_get_frequency(dev, BLADERF_MODULE_RX, &b->rx_freq);
    if (status != 0) {
        return status;
    }

    status = bladerf_get_rational_sample_rate(dev, BLADERF_MODULE_RX,
                                              &b->rx_sample_rate);
    if (status != 0) {
        return status;
    }

    status = bladerf_get_bandwidth(dev, BLADERF_MODULE_RX, &b->rx_bandwidth);
    if (status != 0) {
        return status;
    }

    status = bladerf_get_lna_gain(dev, &b->rx_lna);
    if (status != 0) {
        return status;
    }

    status = bladerf_get_rxvga1(dev, &b->rx_vga1);
    if (status != 0) {
        return status;
    }

    status = bladerf_get_rxvga2(dev, &b->rx_vga2);
    if (status != 0) {
        return status;
    }

    status = bladerf_get_rational_sample_rate(dev, BLADERF_MODULE_TX,
                                              &b->tx_sample_rate);
    if (status != 0) {
        return status;
    }

    status = bladerf_get_loopback(dev, &b->loopback);

    return status;
}

static int set_tx_cal_backup(struct bladerf *dev, struct tx_cal_backup *b)
{
    int status;
    int retval = 0;

    status = bladerf_set_loopback(dev, b->loopback);
    if (status != 0 && retval == 0) {
        retval = status;
    }

    status = bladerf_set_frequency(dev, BLADERF_MODULE_RX, b->rx_freq);
    if (status != 0 && retval == 0) {
        retval = status;
    }

    status = bladerf_set_rational_sample_rate(dev, BLADERF_MODULE_RX,
                                              &b->rx_sample_rate, NULL);
    if (status != 0 && retval == 0) {
        retval = status;
    }

    status = bladerf_set_bandwidth(dev, BLADERF_MODULE_RX,
                                   b->rx_bandwidth, NULL);
    if (status != 0 && retval == 0) {
        retval = status;
    }

    status = bladerf_set_lna_gain(dev, b->rx_lna);
    if (status != 0 && retval == 0) {
        retval = status;
    }

    status = bladerf_set_rxvga1(dev, b->rx_vga1);
    if (status != 0 && retval == 0) {
        retval = status;
    }

    status = bladerf_set_rxvga2(dev, b->rx_vga2);
    if (status != 0 && retval == 0) {
        retval = status;
    }

    status = bladerf_set_rational_sample_rate(dev, BLADERF_MODULE_TX,
                                              &b->tx_sample_rate, NULL);
    if (status != 0 && retval == 0) {
        retval = status;
    }

    return retval;
}

static int tx_cal_update_frequency(struct tx_cal *state, unsigned int freq)
{
    int status;
    bladerf_loopback lb;
    unsigned int rx_freq;

    status = bladerf_set_frequency(state->dev, BLADERF_MODULE_TX, freq);
    if (status != 0) {
        return status;
    }

    rx_freq = freq - 1000000;
    if (rx_freq < BLADERF_FREQUENCY_MIN) {
        rx_freq = freq + 1000000;
        state->rx_low = false;
    } else {
        state->rx_low = true;
    }

    status = bladerf_set_frequency(state->dev, BLADERF_MODULE_RX, rx_freq);
    if (status != 0) {
        return status;
    }

    if (freq < 1500000000) {
        lb = BLADERF_LB_RF_LNA1;
        PR_DBG("Switching to RF LNA1 loopback.\n");
    } else {
        lb = BLADERF_LB_RF_LNA2;
        PR_DBG("Switching to RF LNA2 loopback.\n");
    }

    if (state->loopback != lb) {
        status = bladerf_set_loopback(state->dev, lb);
        if (status == 0) {
            state->loopback = lb;
        }
    }

    return status;
}

static int apply_tx_cal_settings(struct bladerf *dev)
{
    int status;

    status = bladerf_set_sample_rate(dev, BLADERF_MODULE_RX, TX_CAL_RATE, NULL);
    if (status != 0) {
        return status;
    }

    status = bladerf_set_bandwidth(dev, BLADERF_MODULE_RX, TX_CAL_RX_BW, NULL);
    if (status != 0) {
        return status;
    }

    status = bladerf_set_lna_gain(dev, TX_CAL_RX_LNA);
    if (status != 0) {
        return status;
    }

    status = bladerf_set_rxvga1(dev, TX_CAL_RX_VGA1);
    if (status != 0) {
        return status;
    }

    status = bladerf_set_rxvga2(dev, TX_CAL_RX_VGA2);
    if (status != 0) {
        return status;
    }

    status = bladerf_set_sample_rate(dev, BLADERF_MODULE_TX, TX_CAL_RATE, NULL);
    if (status != 0) {
        return status;
    }

    status = bladerf_set_loopback(dev, TX_CAL_DEFAULT_LB);
    if (status != 0) {
        return status;
    }

    return status;
}

/* We just need to flush some zeros through the system to hole the DAC at
 * 0+0j and remain there while letting it underrun. This alleviates the
 * need to worry about continuously TX'ing zeros. */
static int tx_cal_tx_init(struct bladerf *dev)
{
    int status;
    int16_t zero_sample[] = { 0, 0 };
    struct bladerf_metadata meta;

    memset(&meta, 0, sizeof(meta));

    status = bladerf_sync_config(dev, BLADERF_MODULE_TX,
                                 BLADERF_FORMAT_SC16_Q11_META,
                                 4, 16384, 2, 1000);
    if (status != 0) {
        return status;
    }

    status = bladerf_enable_module(dev, BLADERF_MODULE_TX, true);
    if (status != 0) {
        return status;
    }

    meta.flags = BLADERF_META_FLAG_TX_BURST_START |
                 BLADERF_META_FLAG_TX_BURST_END   |
                 BLADERF_META_FLAG_TX_NOW;

    status = bladerf_sync_tx(dev, &zero_sample, 1, &meta, 2000);
    return status;
}

static int tx_cal_rx_init(struct bladerf *dev)
{
    int status;

    status = bladerf_sync_config(dev, BLADERF_MODULE_RX,
                                 BLADERF_FORMAT_SC16_Q11_META,
                                 64, 16384, 32, 1000);
    if (status != 0) {
        return status;
    }

    status = bladerf_enable_module(dev, BLADERF_MODULE_RX, true);
    return status;
}

static void tx_cal_state_deinit(struct tx_cal *cal)
{
    free(cal->sweep);
    free(cal->mag);
    free(cal->samples);
    free(cal->filt);
    free(cal->filt_out);
    free(cal->post_mix);
}

/* This should be called immediately preceding the cal routines */
static int tx_cal_state_init(struct bladerf *dev, struct tx_cal *cal)
{
    int status;

    cal->dev = dev;
    cal->num_samples = TX_CAL_COUNT;
    cal->loopback = TX_CAL_DEFAULT_LB;

    /* Interleaved SC16 Q11 samples */
    cal->samples = malloc(2 * sizeof(cal->samples[0]) * cal->num_samples);
    if (cal->samples == NULL) {
        return BLADERF_ERR_MEM;
    }

    /* Filter state */
    cal->filt = malloc(2 * sizeof(cal->filt[0]) * tx_cal_filt_num_taps);
    if (cal->filt == NULL) {
        return BLADERF_ERR_MEM;
    }

    /* Filter output */
    cal->filt_out = malloc(sizeof(cal->filt_out[0]) * cal->num_samples);
    if (cal->filt_out == NULL) {
        return BLADERF_ERR_MEM;
    }

    /* Post-mix */
    cal->post_mix = malloc(sizeof(cal->post_mix[0]) * cal->num_samples);
    if (cal->post_mix == NULL) {
        return BLADERF_ERR_MEM;
    }

    /* Correction sweep and results */
    cal->sweep = malloc(sizeof(cal->sweep[0]) * TX_CAL_CORR_SWEEP_LEN);
    if (cal->sweep == NULL) {
        return BLADERF_ERR_MEM;
    }

    cal->mag = malloc(sizeof(cal->mag[0]) * TX_CAL_CORR_SWEEP_LEN);
    if (cal->mag == NULL) {
        return BLADERF_ERR_MEM;
    }

    /* Set initial RX in the future */
    status = bladerf_get_timestamp(cal->dev, BLADERF_MODULE_RX, &cal->ts);
    if (status == 0) {
        cal->ts += 20 * TX_CAL_TS_INC;
    }

    return status;
}

/* Filter samples
 *  Input:  state->post_mix
 *  Output: state->filt_out
 */
static void tx_cal_filter(struct tx_cal *state)
{
    unsigned int n, m;
    struct complexf *ins1, *ins2;
    struct complexf *curr; /* Current filter state */
    const struct complexf *filt_end = &state->filt[2 * tx_cal_filt_num_taps];

    /* Reset filter state */
    ins1 = &state->filt[0];
    ins2 = &state->filt[tx_cal_filt_num_taps];
    memset(state->filt, 0, 2 * sizeof(state->filt[0]) * tx_cal_filt_num_taps);

    for (n = 0; n < state->num_samples; n++) {
        /* Insert sample */
        *ins1 = *ins2 = state->post_mix[n];

        /* Convolve */
        state->filt_out[n].i = 0;
        state->filt_out[n].q = 0;
        curr = ins2;

        for (m = 0; m < tx_cal_filt_num_taps; m++, curr--) {
            state->filt_out[n].i += tx_cal_filt[m] * curr->i;
            state->filt_out[n].q += tx_cal_filt[m] * curr->q;
        }

        /* Update insertion points */
        ins2++;
        if (ins2 == filt_end) {
            ins1 = &state->filt[0];
            ins2 = &state->filt[tx_cal_filt_num_taps];
        } else {
            ins1++;
        }

    }
}

/* Deinterleave, scale, and mix with an -Fs/4 tone to shift TX DC offset out at
 * Fs/4 to baseband.
 *  Input:  state->samples
 *  Output: state->post_mix
 */
static void tx_cal_mix(struct tx_cal *state)
{
    unsigned int n, m;
    int mix_state;
    float scaled_i, scaled_q;

    /* Mix with -Fs/4 if RX is tuned "lower" than TX, and Fs/4 otherwise */
    const int mix_state_inc = state->rx_low ? 1 : -1;
    mix_state = 0;

    for (n = 0, m = 0; n < (2 * state->num_samples); n += 2, m++) {
        scaled_i = state->samples[n]   / 2048.0f;
        scaled_q = state->samples[n+1] / 2048.0f;

        switch (mix_state) {
            case 0:
                state->post_mix[m].i =  scaled_i;
                state->post_mix[m].q =  scaled_q;
                break;

            case 1:
                state->post_mix[m].i =  scaled_q;
                state->post_mix[m].q = -scaled_i;
                break;

            case 2:
                state->post_mix[m].i = -scaled_i;
                state->post_mix[m].q = -scaled_q;
                break;

            case 3:
                state->post_mix[m].i = -scaled_q;
                state->post_mix[m].q =  scaled_i;
                break;
        }

        mix_state = (mix_state + mix_state_inc) & 0x3;
    }
}

static int tx_cal_avg_magnitude(struct tx_cal *state, float *avg_mag)
{
    int status;
    const unsigned int start = (tx_cal_filt_num_taps + 1) / 2;
    unsigned int n;
    float accum;

    /* Fetch samples at the current settings */
    status = rx_samples(state->dev, state->samples, state->num_samples,
                        &state->ts, TX_CAL_TS_INC);
    if (status != 0) {
        return status;
    }

    /* Deinterleave & mix TX's DC offset contribution to baseband */
    tx_cal_mix(state);

    /* Filter out everything other than the TX DC offset's contribution */
    tx_cal_filter(state);

    /* Compute the power (magnitude^2 to alleviate need for square root).
     * We skip samples here to account for the group delay of the filter;
     * the initial samples will be ramping up. */
    accum = 0;
    for (n = start; n < state->num_samples; n++) {
        const struct complexf *s = &state->filt_out[n];
        const float m = (float) sqrt(s->i * s->i + s->q * s->q);
        accum += m;
    }

    *avg_mag = (accum / (state->num_samples - start));

    /* Scale this back up to DAC/ADC counts, just for convenience */
    *avg_mag *= 2048.0;

    return status;
}

/* Apply the correction value and read the TX DC offset magnitude */
static int tx_cal_measure_correction(struct tx_cal *state,
                                     bladerf_correction c,
                                     int16_t value, float *mag)
{
    int status;

    status = bladerf_set_correction(state->dev, BLADERF_MODULE_TX, c, value);
    if (status != 0) {
        return status;
    }

    state->ts += TX_CAL_TS_INC;

    status = tx_cal_avg_magnitude(state, mag);
    if (status == 0) {
        PR_VERBOSE("  Corr=%5d, Avg_magnitude=%f\n", value, *mag);
    }

    return status;
}

static int tx_cal_get_corr(struct tx_cal *state, bool i_ch,
                           int16_t *corr_value, float *error_value)
{
    int status;
    unsigned int n;
    int16_t corr;
    float mag[4];
    float m1, m2, b1, b2;
    int16_t range_min, range_max;
    int16_t min_corr;
    float   min_mag;

    const int16_t x[4] = { -1800, -1000, 1000, 1800 };

    const bladerf_correction corr_module =
        i_ch ? BLADERF_CORR_LMS_DCOFF_I : BLADERF_CORR_LMS_DCOFF_Q;

    PR_DBG("Getting coarse estimate for %c\n", i_ch ? 'I' : 'Q');

    for (n = 0; n < 4; n++) {
        status = tx_cal_measure_correction(state, corr_module, x[n], &mag[n]);
        if (status != 0) {
            return status;
        }

    }

    m1 = (mag[1] - mag[0]) / (x[1] - x[0]);
    b1 = mag[0] - m1 * x[0];

    m2 = (mag[3] - mag[2]) / (x[3] - x[2]);
    b2 = mag[2] - m2 * x[2];

    PR_VERBOSE("  m1=%3.8f, b1=%3.8f, m2=%3.8f, b=%3.8f\n", m1, b1, m2, b2);

    if (m1 < 0 && m2 > 0) {
        const int16_t tmp = (int16_t)((b2 - b1) / (m1 - m2) + 0.5);
        const int16_t corr_est = (tmp / 16) * 16;

        /* Number of points to sweep on either side of our estimate */
        const unsigned int n_sweep = 10;

        PR_VERBOSE("  corr_est=%d\n", corr_est);

        range_min = corr_est - 16 * n_sweep;
        if (range_min < -2048) {
            range_min = -2048;
        }

        range_max = corr_est + 16 * n_sweep;
        if (range_max > 2048) {
            range_max = 2048;
        }

    } else {
        /* The frequency and gain combination have yielded a set of
         * points that do not form intersecting lines. This may be indicative
         * of a case where the LMS6 DC bias settings can't pull the DC offset
         * to a zero-crossing.  We'll just do a slow, full scan to find
         * a minimum */
        PR_VERBOSE("  Could not compute estimate. Performing full sweep.\n");
        range_min = -2048;
        range_max = 2048;
    }


    PR_DBG("Performing correction value sweep: [%-5d : 16 :%5d]\n",
           range_min, range_max);

    min_corr = 0;
    min_mag  = 2048;

    for (n = 0, corr = range_min;
         corr <= range_max && n < TX_CAL_CORR_SWEEP_LEN;
         n++, corr += 16) {

        float tmp;

        status = tx_cal_measure_correction(state, corr_module, corr, &tmp);
        if (status != 0) {
            return status;
        }

        if (tmp < 0) {
            tmp = -tmp;
        }

        if (tmp < min_mag) {
            min_corr = corr;
            min_mag  = tmp;
        }
    }

    /* Leave the device set to the minimum */
    status = bladerf_set_correction(state->dev, BLADERF_MODULE_TX,
                                    corr_module, min_corr);
    if (status == 0) {
        *corr_value  = min_corr;
        *error_value = min_mag;
    }

    return status;
}

static int perform_tx_cal(struct tx_cal *state, struct dc_calibration_params *p)
{
    int status = 0;

    status = tx_cal_update_frequency(state, p->frequency);
    if (status != 0) {
        return status;
    }

    state->ts += TX_CAL_TS_INC;

    /* Perform I calibration */
    status = tx_cal_get_corr(state, true, &p->corr_i, &p->error_i);
    if (status != 0) {
        return status;
    }

    /* Perform Q calibration */
    status = tx_cal_get_corr(state, false, &p->corr_q, &p->error_q);
    if (status != 0) {
        return status;
    }

    /* Re-do I calibration to try to further fine-tune result */
    status = tx_cal_get_corr(state, true, &p->corr_i, &p->error_i);
    if (status != 0) {
        return status;
    }

    /* Apply the resulting nominal values */
    status = set_tx_dc_corr(state->dev, p->corr_i, p->corr_q);

    return status;
}

int dc_calibration_tx(struct bladerf *dev,
                      struct dc_calibration_params *params,
                      size_t num_params, bool print_status)
{
    int status = 0;
    int retval = 0;
    struct tx_cal_backup backup;
    struct tx_cal state;
    size_t i;

    memset(&state, 0, sizeof(state));

    /* Backup the device state prior to making changes */
    status = get_tx_cal_backup(dev, &backup);
    if (status != 0) {
        return status;
    }

    /* Configure the device for our TX cal operation */
    status = apply_tx_cal_settings(dev);
    if (status != 0) {
        goto out;
    }

    /* Enable TX and run zero samples through the device */
    status = tx_cal_tx_init(dev);
    if (status != 0) {
        goto out;
    }

    /* Enable RX */
    status = tx_cal_rx_init(dev);
    if (status != 0) {
        goto out;
    }

    /* Initialize calibration state information and resources */
    status = tx_cal_state_init(dev, &state);
    if (status != 0) {
        goto out;
    }

    for (i = 0; i < num_params && status == 0; i++) {
        status = perform_tx_cal(&state, &params[i]);

        if (status == 0 && print_status) {
#           ifdef DEBUG_DC_CALIBRATION
            const char sol = '\n';
            const char eol = '\n';
#           else
            const char sol = '\r';
            const char eol = '\0';
#           endif
            printf("%cCalibrated @ %10u Hz: "
                   "I=%4d (Error: %4.2f), "
                   "Q=%4d (Error: %4.2f)      %c",
                   sol,
                   params[i].frequency,
                   params[i].corr_i, params[i].error_i,
                   params[i].corr_q, params[i].error_q,
                   eol);
            fflush(stdout);
        }
    }

    if (print_status) {
        putchar('\n');
    }

out:
    retval = status;

    status = bladerf_enable_module(dev, BLADERF_MODULE_RX, false);
    if (status != 0 && retval == 0) {
        retval = status;
    }

    status = bladerf_enable_module(dev, BLADERF_MODULE_TX, false);
    if (status != 0 && retval == 0) {
        retval = status;
    }

    tx_cal_state_deinit(&state);

    status = set_tx_cal_backup(dev, &backup);
    if (status != 0 && retval == 0) {
        retval = status;
    }

    return retval;
}

int dc_calibration(struct bladerf *dev, bladerf_module module,
                   struct dc_calibration_params *params,
                   size_t num_params, bool show_status)
{
    int status;

    switch (module) {
        case BLADERF_MODULE_RX:
            status = dc_calibration_rx(dev, params, num_params, show_status);
            break;

        case BLADERF_MODULE_TX:
            status = dc_calibration_tx(dev, params, num_params, show_status);
            break;

        default:
            status = BLADERF_ERR_INVAL;
    }

    return status;
}
