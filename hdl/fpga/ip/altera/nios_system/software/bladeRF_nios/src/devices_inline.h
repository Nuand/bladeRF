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
#include "altera_avalon_uart_regs.h"
#include "altera_avalon_jtag_uart_regs.h"
#include "altera_avalon_pio_regs.h"

#include <stdint.h>
#include <stdbool.h>

static inline bool fx3_uart_has_data(void)
{
    const uint32_t regval = IORD_ALTERA_AVALON_UART_STATUS(FX3_UART);
    return (regval & ALTERA_AVALON_UART_STATUS_RRDY_MSK) != 0;
}

static inline uint8_t fx3_uart_read(void) {
    return IORD_ALTERA_AVALON_UART_RXDATA(FX3_UART);
}

static inline void fx3_uart_write(uint8_t data)
{
	while (!(IORD_ALTERA_AVALON_UART_STATUS(FX3_UART) &
                ALTERA_AVALON_UART_STATUS_TRDY_MSK)
          );

	IOWR_ALTERA_AVALON_UART_TXDATA(FX3_UART, data);
}

static inline uint32_t control_reg_read(void)
{
    return IORD_ALTERA_AVALON_PIO_DATA(PIO_0_BASE);
}

static inline void control_reg_write(uint32_t value)
{
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, value);
}

static inline uint32_t expansion_port_read(void)
{
    return IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE);
}

INLINE void expansion_port_write(uint32_t value)
{
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_1_BASE, value);
}

INLINE uint32_t expansion_port_get_direction()
{
    return IORD_ALTERA_AVALON_PIO_DATA(PIO_2_BASE);
}

INLINE void expansion_port_set_direction(uint32_t dir)
{
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_2_BASE, dir);
}

INLINE void time_tamer_reset(enum bladerf_module m)
{
    /* A single write is sufficient to clear the timestamp counter */
    if (m == BLADERF_MODULE_RX) {
        IOWR_8DIRECT(TIME_TAMER_0_BASE, 0, 0);
    } else {
        IOWR_8DIRECT(TIME_TAMER_0_BASE, 8, 0);
    }
}

#endif
