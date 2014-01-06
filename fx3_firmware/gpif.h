/*
 * Copyright (c) 2013 Nuand LLC
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
#ifndef GPIF_H_
#define GPIF_H_

/**
 * Supported GPIF configurations
 */
typedef enum {
    GPIF_CONFIG_DISABLED,   /**< GPIF is disabled */
    GPIF_CONFIG_RF_LINK,    /**< GPIF configured for streaming RF samples */
    GPIF_CONFIG_FPGA_LOAD,  /**< GPIF configured for FPGA loading */
} NuandGpifConfig;

/**
 * Reconfigure the GPIF (and PIB) for the specified configuration
 *
 * @param   config      Desired configuration
 *
 * @return CY_U3P_* return code
 */
CyU3PReturnStatus_t NuandConfigureGpif(NuandGpifConfig config);

#endif

