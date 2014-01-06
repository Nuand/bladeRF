/*
 * Copyright (c) 2014 Nuand LLC
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
#include <cyu3error.h>
#include <cyu3gpif.h>
#include <cyu3pib.h>

#include "gpif.h"

/* These generated headers contain declarations, and must be included in
 * only one place */
#include "cyfxgpif_C4loader.h"
#include "cyfxgpif_RFlink.h"

CyU3PReturnStatus_t NuandConfigureGpif(NuandGpifConfig config)
{
    CyU3PReturnStatus_t status;
    CyU3PPibClock_t pibClock;
    const CyU3PGpifConfig_t *gpif_config;
    uint8_t gpif_state_index;
    uint8_t gpif_initial_alpha;

    static CyBool_t gpif_active = CyFalse;
    static CyBool_t pib_active = CyFalse;

    /* Restart thel GPIF and PIB block for safe measure.
     *
     * Previously, a bug had been observed where 16-bit to 32-bit GPIF
     * transitions messes up the first DMA transaction, requiring these
     * resets. */

    if (gpif_active) {
        CyU3PGpifDisable(CyTrue);
        gpif_active = CyFalse;
    }

    if (pib_active) {
        status = CyU3PPibDeInit();
        if (status != CY_U3P_SUCCESS) {
            return status;
        }
        pib_active = CyFalse;
    }

    switch (config) {
        case GPIF_CONFIG_RF_LINK:
            gpif_config = &Rflink_CyFxGpifConfig;
            gpif_state_index = RFLINK_START;
            gpif_initial_alpha = RFLINK_ALPHA_START;
            break;

        case GPIF_CONFIG_FPGA_LOAD:
            gpif_config = &C4loader_CyFxGpifConfig;
            gpif_state_index = C4LOADER_START;
            gpif_initial_alpha = C4LOADER_ALPHA_START;
            break;

        case GPIF_CONFIG_DISABLED:
            /* Nothing left to do here */
            return CY_U3P_SUCCESS;

        default:
            return CY_U3P_ERROR_BAD_ARGUMENT;
    }


    pibClock.clkDiv = 4;
    pibClock.clkSrc = CY_U3P_SYS_CLK;
    pibClock.isHalfDiv = CyFalse;
    pibClock.isDllEnable = CyFalse;

    status = CyU3PPibInit(CyTrue, &pibClock);
    if (status != CY_U3P_SUCCESS) {
        return status;
    }
    pib_active = CyTrue;

    /* Load the GPIF configuration */
    status = CyU3PGpifLoad(gpif_config);
    if (status != CY_U3P_SUCCESS) {
        return status;
    }
    gpif_active = CyTrue;

    /* Start the state machine. */
    status = CyU3PGpifSMStart(gpif_state_index, gpif_initial_alpha);
    if (status != CY_U3P_SUCCESS) {
        return status;
    }

    return CY_U3P_SUCCESS;
}
