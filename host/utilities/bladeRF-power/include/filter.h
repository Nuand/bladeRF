/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2024 Nuand LLC
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
 *
 * This program is intended to verify that C programs build against
 * libbladeRF without any unintended dependencies.
 */
#ifndef FILTER_H_
#define FILTER_H_
#include <stdlib.h>

/**
 * @brief Apply a predefined FIR filter to input samples to flatten the noise
 * figure across the specified frequency range, specifically designed for
 * bladeRF 2.0 devices.
 *
 * This function convolves input samples with a low-pass FIR filter designed to
 * equalize the device's frequency response for power measurement. The filter
 * aims to achieve a uniform gain across the desired frequency spectrum with
 * minimal ripple.
 *
 * @param dev Pointer to the `bladerf` device structure
 * @param samples Pointer to the input sample array, where each element
 * represents a complex sample (interleaved I and Q components). The input array
 * should have a length of `2 * num_samples`, accounting for both I and Q
 * components.
 * @param num_samples The number of complex samples in the `samples` array. This
 * is not the length of the array but the number of complex samples (each
 * consisting of an I and a Q component).
 * @param convolved_samples Pointer to the output sample array where the
 * filtered samples will be stored. This array must be preallocated and have the
 * same length as the `samples` array to hold the convolved (filtered) output.
 *
 * @return BLADERF_ERR_INVAL if any of the input pointers are NULL, indicating
 * an invalid parameter. Returns 0 on successful execution of the function.
 *
 * @note The function performs in-place filtering if the input and output arrays
 * are the same. Care must be taken to ensure adequate memory allocation for the
 * output to avoid memory corruption.
 * @note This function checks the device type and only performs filtering for
 * bladeRF 2.0 devices. For other devices, it copies the input samples to the
 * output buffer directly without filtering.
 * @note The filter coefficients and the filtering process are optimized for
 * specific frequency ranges and may not be suitable for all signal processing
 * needs.
 */
int flatten_noise_figure(struct bladerf *dev,
                         const int16_t *samples,
                         size_t num_samples,
                         int16_t *convolved_samples);

#endif // FILTER_H_