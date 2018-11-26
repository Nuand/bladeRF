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
#include <unistd.h>

#include "devices.h"
#include "pkt_handler.h"
#include "pkt_8x8.h"
#include "pkt_8x16.h"
#include "pkt_8x32.h"
#include "pkt_8x64.h"
#include "pkt_16x64.h"
#include "pkt_32x32.h"
#include "pkt_retune2.h"
#include "pkt_legacy.h"
#include "debug.h"

#define BLADERF_DEVICE_NAME "Nuand bladeRF 2.0 Micro"

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
    PKT_RETUNE2,
    PKT_8x8,
    PKT_8x16,
    PKT_8x32,
    PKT_8x64,
    PKT_16x64,
    PKT_32x32,
    PKT_LEGACY,
};

/* A structure that represents a point on a line. Used for calibrating
 * the VCTCXO */
typedef struct point {
    int32_t  x; // Error counts
    uint16_t y; // DAC count
} point_t;

typedef struct line {
    point_t  point[2];
    int32_t  slope;
    uint16_t y_intercept; // in DAC counts
} line_t;

/* State machine for VCTCXO tuning */
typedef enum state {
    COARSE_TUNE_MIN,
    COARSE_TUNE_MAX,
    COARSE_TUNE_DONE,
    FINE_TUNE,
    DO_NOTHING
} state_t;

int main(void)
{
    DBG(BLADERF_DEVICE_NAME " FPGA v%x.%x.%x\n",
        FPGA_VERSION_MAJOR, FPGA_VERSION_MINOR, FPGA_VERSION_PATCH);
    DBG("Built " __DATE__ " " __TIME__ " with love <3\n");
#ifdef BLADERF_NIOS_LIBAD936X
    DBG("libad936x found: This FPGA image has magic transgirl powers\n");
#endif  // BLADERF_NIOS_LIBAD936X

    uint8_t i;

    /* Pointer to currently active packet handler */
    const struct pkt_handler *handler;

    struct pkt_buf pkt;
    struct vctcxo_tamer_pkt_buf vctcxo_tamer_pkt;

    /* Marked volatile to ensure we actually read the byte populated by
     * the UART ISR */
    const volatile uint8_t *magic = &pkt.req[PKT_MAGIC_IDX];

    volatile bool have_request = false;

#ifdef BLADERF_NIOS_DEBUG
    // Twiddler: gratuitous screen placebo
    size_t const TWIDDLE_DELAY_CONSTANT = 100000;
    size_t twiddle_count = 0;
    enum {
        TWIDDLE_STATE_PIPE = '|',
        TWIDDLE_STATE_FWD_SLASH = '/',
        TWIDDLE_STATE_HYPHEN = '-',
        TWIDDLE_STATE_BACKSLASH = '\\',
    } twiddle_state = TWIDDLE_STATE_PIPE;
#endif

    // Trim DAC constants
    const uint16_t trimdac_min       = 0x28F5;
    const uint16_t trimdac_max       = 0xF5C3;

    // Trim DAC calibration line
    line_t trimdac_cal_line;

    // VCTCXO Tune State machine
    state_t tune_state = COARSE_TUNE_MIN;

    // Set the known/default values of the trim DAC cal line
    trimdac_cal_line.point[0].x  = 0;
    trimdac_cal_line.point[0].y  = trimdac_min;
    trimdac_cal_line.point[1].x  = 0;
    trimdac_cal_line.point[1].y  = trimdac_max;
    trimdac_cal_line.slope       = 0;
    trimdac_cal_line.y_intercept = 0;

    /* Sanity check */
    ASSERT(PKT_MAGIC_IDX == 0);

    memset(&pkt, 0, sizeof(pkt));
    pkt.ready = false;
    bladerf_nios_init(&pkt, &vctcxo_tamer_pkt);

    /* Initialize packet handlers */
    for (i = 0; i < ARRAY_SIZE(pkt_handlers); i++) {
        if (pkt_handlers[i].init != NULL) {
            pkt_handlers[i].init();
        }
    }

    /* ====================
     * AD9361 SPI TESTS
     * ==================== */
    #ifdef BLADERF_NIOS_AD9361_SPI_TESTS
        uint16_t adi_spi_addr;
        uint64_t adi_spi_data;

        while ( 1 ) {
            //             W/Rb        | NB2:0       | A[9:0]
            adi_spi_addr = (0x0 << 15) | (0x0 << 12) | (0x000 << 0);
            adi_spi_data = UINT64_C(0x0);

            // Read AD9361 registers 0x045-0x04c
            for( i=0x45; i < 0x4d; i++ ) {
                adi_spi_addr = (adi_spi_addr & 0xfc00) | i;
                adi_spi_data = adi_spi_read(adi_spi_addr);
            }

            // Read 0x028
            adi_spi_addr = (adi_spi_addr & 0xfc00) | 0x28;
            adi_spi_data = adi_spi_read(adi_spi_addr);

            // Write 0x5a to 0x028
            adi_spi_addr = (adi_spi_addr & 0xfc00) | 0x28;
            adi_spi_data = (UINT64_C(0x5a) << (64-8));
            adi_spi_write((adi_spi_addr | 0x8000), adi_spi_data);

            // Read 0x028
            adi_spi_addr = (adi_spi_addr & 0xfc00) | 0x28;
            adi_spi_data = adi_spi_read(adi_spi_addr);

            // Write 0x5a to 0x028
            adi_spi_addr = (adi_spi_addr & 0xfc00) | 0x28;
            adi_spi_data = (UINT64_C(0x0) << (64-8));
            adi_spi_write((adi_spi_addr | 0x8000), adi_spi_data);

            usleep(100);
        }
    #endif


    /* ====================
     * AD5621 SPI TESTS
     * ==================== */
    #ifdef BLADERF_NIOS_AD5621_SPI_TESTS
        uint16_t dac_val;

        // Disable the ADF400x
        control_reg_write( 0x0 );
        usleep(2000000);
        while( 1 ) {
            for( i = 0; i < 6; i++ ) {
                // Calculate DAC value
                dac_val = ((((uint16_t)i) * 0x333) << 2) & 0x3fff;
                // Write DAC value
                ad56x1_vctcxo_trim_dac_write(dac_val);
                usleep(2000000);
                // Tristate the DAC, keeping same DAC value in register
                ad56x1_vctcxo_trim_dac_write(dac_val | 0xc000 );
                usleep(2000000);
            }
        }
    #endif


    /* ====================
     * ADF4001 SPI TESTS
     * ==================== */
    #ifdef BLADERF_NIOS_ADF4001_SPI_TESTS
        // Tristate the DAC
        ad56x1_vctcxo_trim_dac_write( 0xc000 );
        while( 1 ) {

            // Enable the ADF400x
            control_reg_write( 0x1 << 11 );
            usleep(2000000);

            // 0x000003 -- MUXOUT = 'Z'; CP = Normal
            adf400x_spi_write(0x3);
            usleep(2000000);

            // 0x000137 -- MUXOUT = AVDD (3.3 V); CP = 'Z'
            adf400x_spi_write(0x137);
            usleep(2000000);

            // 0x000177 -- MUXOUT = DGND; CP = 'Z'
            adf400x_spi_write(0x177);
            usleep(2000000);

            // 0x000167 -- MUXOUT = SDO; CP = 'Z'
            //adf400x_spi_write(0x167);
            //usleep(2000000);

            for( i = 0; i < 4; i++ ) {
                // Read all the registers
                adf400x_spi_read((uint32_t)i);
                usleep(100);
            }

            // Disable the ADF400x
            control_reg_write( 0x0 );
            usleep(2000000);
        }
    #endif

    DBG("=== System Ready ===\n");

    while (run_nios) {
        have_request = HAVE_REQUEST();

#ifdef BLADERF_NIOS_DEBUG
        if (have_request) {
            twiddle_count = 0;
        } else if (TWIDDLE_DELAY_CONSTANT == ++twiddle_count) {
            DBG("%c\b", twiddle_state);

            switch (twiddle_state) {
                case TWIDDLE_STATE_PIPE:
                    twiddle_state = TWIDDLE_STATE_FWD_SLASH;
                    break;
                case TWIDDLE_STATE_FWD_SLASH:
                    twiddle_state = TWIDDLE_STATE_HYPHEN;
                    break;
                case TWIDDLE_STATE_HYPHEN:
                    twiddle_state = TWIDDLE_STATE_BACKSLASH;
                    break;
                case TWIDDLE_STATE_BACKSLASH:
                    twiddle_state = TWIDDLE_STATE_PIPE;
                    break;
            }

            twiddle_count = 0;
        }
#endif

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
                DBG("Got invalid magic value: 0x%x\n", pkt.req[PKT_MAGIC_IDX]);
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

            /* Temporarily putting the VCTCXO Calibration stuff here. */
            if( vctcxo_tamer_pkt.ready ) {

                vctcxo_tamer_pkt.ready = false;

                switch(tune_state) {

                case COARSE_TUNE_MIN:

                    /* Tune to the minimum DAC value */
                    vctcxo_trim_dac_write( 0x08, trimdac_min );

                    /* State to enter upon the next interrupt */
                    tune_state = COARSE_TUNE_MAX;

                    break;

                case COARSE_TUNE_MAX:

                    /* We have the error from the minimum DAC setting, store it
                     * as the 'x' coordinate for the first point */
                    trimdac_cal_line.point[0].x = vctcxo_tamer_pkt.pps_1s_error;

                    /* Tune to the maximum DAC value */
                    vctcxo_trim_dac_write( 0x08, trimdac_max );

                    /* State to enter upon the next interrupt */
                    tune_state = COARSE_TUNE_DONE;

                    break;

                case COARSE_TUNE_DONE:

                    /* We have the error from the maximum DAC setting, store it
                     * as the 'x' coordinate for the second point */
                    trimdac_cal_line.point[1].x = vctcxo_tamer_pkt.pps_1s_error;

                    /* We now have two points, so we can calculate the equation
                     * for a line plotted with DAC counts on the Y axis and
                     * error on the X axis. We want a PPM of zero, which ideally
                     * corresponds to the y-intercept of the line. */
                    trimdac_cal_line.slope = ( (trimdac_cal_line.point[1].y - trimdac_cal_line.point[0].y) /
                                               (trimdac_cal_line.point[1].x - trimdac_cal_line.point[0].x) );
                    trimdac_cal_line.y_intercept = ( trimdac_cal_line.point[0].y -
                                                     (trimdac_cal_line.slope * trimdac_cal_line.point[0].x) );

                    /* Set the trim DAC count to the y-intercept */
                    vctcxo_trim_dac_write( 0x08, trimdac_cal_line.y_intercept );

                    /* State to enter upon the next interrupt */
                    tune_state = FINE_TUNE;

                    break;

                case FINE_TUNE:

                    /* We should be extremely close to a perfectly tuned
                     * VCTCXO, but some minor adjustments need to be made */

                    /* Check the magnitude of the errors starting with the
                     * one second count. If an error is greater than the maxium
                     * tolerated error, adjust the trim DAC by the error (Hz)
                     * multiplied by the slope (in counts/Hz) and scale the
                     * result by the precision interval (e.g. 1s, 10s, 100s). */
                    if( vctcxo_tamer_pkt.pps_1s_error_flag ) {
                        vctcxo_trim_dac_write( 0x08, (vctcxo_trim_dac_value -
                            ((vctcxo_tamer_pkt.pps_1s_error * trimdac_cal_line.slope)/1)) );
                    } else if( vctcxo_tamer_pkt.pps_10s_error_flag ) {
                        vctcxo_trim_dac_write( 0x08, (vctcxo_trim_dac_value -
                            ((vctcxo_tamer_pkt.pps_10s_error * trimdac_cal_line.slope)/10)) );
                    } else if( vctcxo_tamer_pkt.pps_100s_error_flag ) {
                        vctcxo_trim_dac_write( 0x08, (vctcxo_trim_dac_value -
                            ((vctcxo_tamer_pkt.pps_100s_error * trimdac_cal_line.slope)/100)) );
                    }

                    break;

                default:
                    break;

                } /* switch */

                /* Take PPS counters out of reset */
                vctcxo_tamer_reset_counters( false );

                /* Enable interrupts */
                vctcxo_tamer_enable_isr( true );

            } /* VCTCXO Tamer interrupt */

            for (i = 0; i < ARRAY_SIZE(pkt_handlers); i++) {
                if (pkt_handlers[i].do_work != NULL) {
                    pkt_handlers[i].do_work();
                }
            }
        }
    }

    return 0;
}
