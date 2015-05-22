/* This file is part of the bladeRF project:
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
#include <stdint.h>
#include <stdbool.h>
#include "pkt_handler.h"
#include "pkt_retune.h"
#include "nios_pkt_retune.h"    /* Packet format definition */
#include "devices.h"
#include "band_select.h"
#include "debug.h"


void pkt_retune(struct pkt_buf *b)
{
    int status;
    bladerf_module module;
    uint8_t flags;
    struct lms_freq f;
    uint64_t timestamp;
    uint64_t retune_start;
    uint64_t retune_end;
    uint64_t retune_duration = 0;

    flags = NIOS_PKT_RETUNERESP_FLAG_SUCCESS;

    nios_pkt_retune_unpack(b->req, &module, &timestamp,
                           &f.nint, &f.nfrac, &f.freqsel, &f.low_band,
                           &f.vcocap_est);

    f.vcocap_result = 0xff;

    if (timestamp == NIOS_PKT_RETUNE_NOW) {
        /* Fire off this retune operation now */

        switch (module) {
            case BLADERF_MODULE_RX:
            case BLADERF_MODULE_TX:
                retune_start = time_tamer_read(module);

                status = lms_set_precalculated_frequency(NULL, module, &f);
                if (status == 0) {
                    flags |= NIOS_PKT_RETUNERESP_FLAG_TSVTUNE_VALID;

                    status = band_select(NULL, module, f.low_band);
                    if (status != 0) {
                        flags &= ~(NIOS_PKT_RETUNERESP_FLAG_SUCCESS);
                    }

                } else {
                    flags &= ~(NIOS_PKT_RETUNERESP_FLAG_SUCCESS);
                }

                retune_end = time_tamer_read(module);
                retune_duration = retune_end - retune_start;
                break;

            default:
                flags &= ~(NIOS_PKT_RETUNERESP_FLAG_SUCCESS);
        }

    } else {
        /* TODO Schedule the retune operation in our queue */
        retune_duration = 0;
    }

    nios_pkt_retune_resp_pack(b->resp, retune_duration, f.vcocap_result, flags);
}
