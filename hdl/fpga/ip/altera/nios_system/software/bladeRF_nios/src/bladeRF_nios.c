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
#include "pkt_32x32.h"
#include "pkt_retune.h"
#include "pkt_legacy.h"
#include "debug.h"

#ifdef BLADERF_NIOS_PC_SIMULATION
    extern bool run_nios;
#   define HAVE_REQUEST() ({ \
        have_request = !have_request; \
        if (have_request) { \
            command_uart_read_request( (uint8_t*) pkt.req); \
        } \
        have_request; \
    })

    /* We need to reset the response buffer to known values so we can
     * compare against expected test case responses */
#   define RESET_RESPONSE_BUF

#else
#   define run_nios true
#   define HAVE_REQUEST() (pkt.ready == true)
#endif

#ifdef RESET_RESPONSE_BUF
#   undef RESET_RESPONSE_BUF
#   define RESET_RESPONSE_BUF() do { \
        memset(pkt.resp, 0xff, NIOS_PKT_LEN); \
    } while (0)

#else
#   define RESET_RESPONSE_BUF() do {} while (0)
#endif

/* When adding packet handlers here, you must also ensure that you update
 * the bladeRF/hdl/fpga/ip/nuand/command_uart/vhdl/command_uart.vhd
 * to include the magic header byte value in the `magics` array.
 */
static const struct pkt_handler pkt_handlers[] = {
    PKT_RETUNE,
    PKT_8x8,
    PKT_8x16,
    PKT_8x32,
    PKT_8x64,
    PKT_32x32,
    PKT_LEGACY,
};

int main(void)
{
    uint8_t i;

    /* Pointer to currently active packet handler */
    const struct pkt_handler *handler;

    struct pkt_buf pkt;
    struct ppscal_pkt_buf ppscal_pkt;

    /* Marked volatile to ensure we actually read the byte populated by
     * the UART ISR */
    const volatile uint8_t *magic = &pkt.req[PKT_MAGIC_IDX];

    volatile bool have_request = false;

    // PPS Calibration variables
    const uint64_t pps_1s_count_exp   = 1*38.4e6;
    const uint64_t pps_10s_count_exp  = 10*38.4e6;
    const uint64_t pps_100s_count_exp = 100*38.4e6;
    uint16_t trim_dac_value;
    //double   trim_dac_voltage;
    uint16_t trim_dac_next_value;
    //double   trim_dac_next_voltage;

    /* Sanity check */
    ASSERT(PKT_MAGIC_IDX == 0);

    memset(&pkt, 0, sizeof(pkt));
    pkt.ready = false;
    bladerf_nios_init(&pkt, &ppscal_pkt);

    /* Initialize packet handlers */
    for (i = 0; i < ARRAY_SIZE(pkt_handlers); i++) {
        if (pkt_handlers[i].init != NULL) {
            pkt_handlers[i].init();
        }
    }

    while (run_nios) {
        have_request = HAVE_REQUEST();

        /* We have a command in the UART */
        if (have_request) {
            pkt.ready = false;
            handler = NULL;

            /* Determine which packet handler should receive this message */
            for (i = 0; i < ARRAY_SIZE(pkt_handlers); i++) {
                if (pkt_handlers[i].magic == *magic) {
                    handler = &pkt_handlers[i];
                }
            }

            if (handler == NULL) {
                /* We somehow got out of sync. Throw away request data until
                 * we hit a magic value */
                DBG("Got invalid magic value: 0x%02x\n", pkt.req[PKT_MAGIC_IDX]);
                continue;
            }

            print_bytes("Request data:", pkt.req, NIOS_PKT_LEN);

            /* If building with RESET_RESPONSE_BUF defined, reset response buffer
             * contents to ensure unused values are known values. */
            RESET_RESPONSE_BUF();

            /* Process data and execute requested actions */
            handler->exec(&pkt);

            /* Write response to host */
            command_uart_write_response(pkt.resp);
        } else {

            // Temporarily putting the PPS Calibration stuff here for testing purposes.
            // TODO: figure out a better (more fair) way of handling interrupts.
            if( ppscal_pkt.ready ) {

                // Get the current VCTCXO trim DAC value
                //    void vctcxo_trim_dac_write(uint8_t cmd, uint16_t val)
                //    void vctcxo_trim_dac_read(uint8_t cmd, uint16_t *val)
                vctcxo_trim_dac_read( 0x88 , &trim_dac_value);

                // New DAC value
                trim_dac_value += trim_dac_value + 0x10;
                if( trim_dac_value < 0xF000 ) {
                    vctcxo_trim_dac_write( 0x08, &trim_dac_value );
                }

                // compute deltas from target
                // pick a value higher or lower than current dac value
                // get next delta
                // worse? go in the opposite direction
                // better? keep going...
                // repeat


                // probably don't need to compue the actual voltage

                // Compute the voltage that the DAC is outputting
                //trim_dac_voltage = ((3.3-0.0)/(uint16t)(0xFFFF))*trim_dac_value;

                // Enable interrupts
                //                ppscal_enable_isr(true);
            }

            for (i = 0; i < ARRAY_SIZE(pkt_handlers); i++) {
                if (pkt_handlers[i].do_work != NULL) {
                    pkt_handlers[i].do_work();
                }
            }
        }
    }

    return 0;
}
