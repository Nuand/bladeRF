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

/* This file provides DC calibration routines that utilize libbladeRF */

#ifndef DC_CALIBRATION_H_
#define DC_CALIBRATION_H_

#include <libbladeRF.h>

#ifdef __cplusplus
extern "C" {
#endif

struct dc_calibration_params {
    unsigned int frequency;
    int16_t corr_i;
    int16_t corr_q;
    float error_i;
    float error_q;
};

/**
 * Calibrate the specified LMS6002D module.
 *
 * Generally, one should run all of these calibrations prior to executing the RX
 * and TX auto calibration routines.
 *
 * Streams should not be running when attempting to call this function.
 *
 * Below are the case-insensitive options for the LMS6002D `module` string:
 *  - LPF Tuning: "lpf_tuning", "lpftuning", "tuning"
 *  - TX LPF: "tx_lpf", "txlpf"
 *  - RX LPF: "rx_lpf", "rxlpf"
 *  - RX VGA2 "rx_vga2", "rxvga2"
 *  - All of the above: "all"
 *
 * @param   dev     bladeRF device handle
 * @param   module  LMS6002D module to calibrate. Passing NULL is synonymous
 *                  with specifying "ALL".
 *
 * @return 0 on success or libbladeRF return value on failure.
 *         BLADERF_ERR_INVAL is returned if `module` is invalid.
 */
int dc_calibration_lms6(struct bladerf *dev, const char *module);

/**
 * Perform DC offset calbration of the RX path. Neither RX or TX should be
 * used during this procedure, as the TX frequency may be adjusted to ensure
 * good results are obtained.
 *
 * The RX input should be disconected from any input sources or antennas when
 * performing this calibration.
 *
 * @pre dc_calibration_lms6() should have been called for all modules prior to
 *      using this function.
 *
 * @param[in]       dev         bladeRF device handle
 *
 * @param[inout]    params      DC calibration input and output parameters. This
 *                              list should be populated to describe the
 *                              frequencies over which to calibrate. The
 *                              `corr_i` and `corr_q` fields will be filled in
 *                              for each entry.
 *
 * @param[in]       num_params  Number of entries in the `params` list.
 *
 * @param[in]       show_status Print status information to stdout
 *
 * @return 0 on success or libbladeRF return value on failure.
 */
int dc_calibration_rx(struct bladerf *dev,
                      struct dc_calibration_params *params,
                      size_t num_params, bool show_status);

/**
 * Perform DC offset calbration of the TX path.
 *
 * This requires use of both RX and TX modules in an internal loopback mode;
 * neither should be in use when this function is called. It should not
 * be neccessary to disconnect the RX and TX ports, but it is still recommended,
 * just to err on the side of caution.
 *
 * @pre dc_calibration_lms6() should have been called for all modules prior to
 *      using this function.
 *
 * @param   dev     bladeRF device handle
 *
 * @return 0 on success or libbladeRF return value on failure.
 */
int dc_calibration_tx(struct bladerf *dev,
                      struct dc_calibration_params *params,
                      size_t num_params, bool show_status);

/**
 * Convenience wrapper around dc_cal_rx() and dc_cal_tx()
 *
 * @pre dc_calibration_lms6() should have been called for all modules prior to
 *      using this function.
 *
 * @param[in]       dev         Device handle
 * @param[in]       module      BLADERF_MODULE_RX or BLADERF_MODULE_TX
 * @param[inout]    params      DC calibration input and output parameters. This
 *                              list should be populated to describe the
 *                              frequencies over which to calibrate. The
 *                              `corr_i` and `corr_q` fields will be filled in
 *                              for each entry.
 *
 * @param[in]       num_params  Number of entries in the `params` list.
 * @param[in]       show_status Print status information to stdout
 *
 * @return 0 on success or libbladeRF return value on failure.
 */
int dc_calibration(struct bladerf *dev, bladerf_module module,
                   struct dc_calibration_params *params,
                   size_t num_params, bool show_status);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
