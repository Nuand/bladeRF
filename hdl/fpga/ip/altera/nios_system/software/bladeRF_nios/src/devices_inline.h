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

#ifndef DEVICES_INLINE_H_
#define DEVICES_INLINE_H_

#include <stdint.h>
#include <stdbool.h>

#include "system.h"
#include "altera_avalon_spi.h"
#include "altera_avalon_jtag_uart_regs.h"
#include "altera_avalon_pio_regs.h"

#include <stdint.h>
#include <stdbool.h>


static inline uint32_t control_reg_read(void)
{
    return IORD_ALTERA_AVALON_PIO_DATA(CONTROL_BASE);
}

static inline void control_reg_write(uint32_t value)
{
    IOWR_ALTERA_AVALON_PIO_DATA(CONTROL_BASE, value);
}

static inline uint32_t expansion_port_read(void)
{
    return IORD_ALTERA_AVALON_PIO_DATA(XB_GPIO_BASE);
}

INLINE void expansion_port_write(uint32_t value)
{
    IOWR_ALTERA_AVALON_PIO_DATA(XB_GPIO_BASE, value);
}

INLINE uint32_t expansion_port_get_direction()
{
    return IORD_ALTERA_AVALON_PIO_DATA(XB_GPIO_DIR_BASE);
}

INLINE void expansion_port_set_direction(uint32_t dir)
{
    IOWR_ALTERA_AVALON_PIO_DATA(XB_GPIO_DIR_BASE, dir);
}

INLINE void time_tamer_reset(bladerf_module m)
{
    /* A single write is sufficient to clear the timestamp counter */
    if (m == BLADERF_MODULE_RX) {
        IOWR_8DIRECT(RX_TAMER_BASE, 0, 0);
    } else {
        IOWR_8DIRECT(TX_TAMER_BASE, 0, 0);
    }
}

INLINE void timer_tamer_clear_interrupt(bladerf_module m)
{
    if (m == BLADERF_MODULE_RX) {
        IOWR_8DIRECT(RX_TAMER_BASE, 8, 1) ;
    } else {
        IOWR_8DIRECT(TX_TAMER_BASE, 8, 1) ;
    }
}

INLINE void command_uart_read_request(uint8_t *req) {
    int i, x ;
    uint32_t val ;
    for( x = 0 ; x < 16 ; x+=4 ) {
        val = IORD_32DIRECT(COMMAND_UART_BASE, x) ;
        for( i = 0 ; i < 4 ; i++ ) {
            req[x+i] = val&0xff ;
            val >>= 8 ;
        }
    }
    return ;
}

INLINE void command_uart_write_response(uint8_t *resp) {
    int i ;
    uint32_t val ;
    for( i = 0 ; i < 16 ; i+=4 ) {
        val = ((uint32_t)resp[i+0]) | (((uint32_t)resp[i+1])<<8) | (((uint32_t)resp[i+2])<<16) | (((uint32_t)resp[i+3])<<24) ;
        IOWR_32DIRECT(COMMAND_UART_BASE, i, val) ;
    }
    return ;
}

#endif
