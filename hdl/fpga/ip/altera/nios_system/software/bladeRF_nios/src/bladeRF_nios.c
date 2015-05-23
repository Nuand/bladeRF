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

/* Define this to run this code on a PC instead of the NIOS II */
//#define BLADERF_NULL_HARDWARE

/* Will send debug alt_printf info to JTAG console while running on NIOS.
 * This can slow performance and cause timing issues... be careful. */
//#define BLADERF_NIOS_DEBUG

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "devices.h"
#include "pkt_handler.h"
#include "pkt_8x8.h"
#include "pkt_8x16.h"
#include "pkt_8x32.h"
#include "pkt_8x64.h"
#include "pkt_retune.h"
#include "pkt_legacy.h"
#include "debug.h"

#ifdef BLADERF_NIOS_PC_SIMULATION
    extern bool run_nios;
#else
#   define run_nios true
#endif

static const struct pkt_handler pkt_handlers[] = {
    PKT_8x8,
    PKT_8x16,
    PKT_8x32,
    PKT_8x64,
    PKT_RETUNE,
    PKT_LEGACY,
};

/* The request buffer is const to help avoid bugs in the handlers
 * However, we have to assign it, so we cast away the const here... */
static inline void set_request(struct pkt_buf *b, uint8_t idx, uint8_t val)
{
    uint8_t *req = (uint8_t *) &b->req[idx];
    *req = val;
}

int main(void)
{
    uint8_t i, j;
    uint8_t count;

    /* Pointer to currently active packet handler */
    const struct pkt_handler *handler;

    struct pkt_buf pkt;

    /* Sanity check */
    ASSERT(PKT_MAGIC_IDX == 0);

    memset(&pkt, 0, sizeof(pkt));

    bladerf_nios_init();

    /* Initialize packet handlers */
    for (i = 0; i < ARRAY_SIZE(pkt_handlers); i++) {
        if (pkt_handlers[i].init != NULL) {
            pkt_handlers[i].init();
        }
    }

    while (run_nios) {

        handler = NULL;
        set_request(&pkt, PKT_MAGIC_IDX, 0x00);
        count = 1;

        /* Wait until we've seen a request magic value. We expect to see
         * garbage 0x00 values when the FX3 switches between its UART and
         * SPI interfaces (which are muxed on the same pins) */
        while(pkt.req[PKT_MAGIC_IDX] == 0x00) {
            while (!fx3_uart_has_data());
            set_request(&pkt, PKT_MAGIC_IDX, fx3_uart_read());
        }

        /* Receive the rest of the message */
        while (count < NIOS_PKT_LEN) {
            while (!fx3_uart_has_data());
            set_request(&pkt, count++, fx3_uart_read());
        }

        /* Determine which packet handler should receive this message */
        for (i = 0; i < ARRAY_SIZE(pkt_handlers); i++) {
            if (pkt_handlers[i].magic == pkt.req[PKT_MAGIC_IDX]) {
                handler = &pkt_handlers[i];
                //bytes_required = sm_curr->bytes_required;
            }
        }

        if (handler == NULL) {
            /* We somehow got out of sync. Throw away request data until
             * we hit a magic value */
            DBG("Got invalid -magic value: 0x%02x\n", pkt.req[PKT_MAGIC_IDX]);
            continue;
        }

        print_bytes("Request data:", pkt.req, count);

        /* Reset response buffer contents to ensure unused
         * values are known values */
        reset_response_buf(&pkt);

        /* Process data and execute requested actions */
        handler->exec(&pkt);

        /* Write response to host */
        for (j = 0; j < sizeof(pkt.resp); j++) {
            fx3_uart_write(pkt.resp[j]);
        }

        /* Flush simulated UART buffer */
        SIMULATION_FLUSH_UART();
    }

  return 0;
}
