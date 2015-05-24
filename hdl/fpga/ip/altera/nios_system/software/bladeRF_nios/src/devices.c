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

#ifndef BLADERF_NIOS_PC_SIMULATION

#include <stdbool.h>
#include <stdint.h>
#include "devices.h"
#include "debug.h"
#include "fpga_version.h"

#if 0   /* Removed until we plan to use this at a later time */

static volatile bool fx3_uart_overrun = false;
volatile uint8_t buf[16];
volatile uint8_t buf_count = 0;

static inline void fx3_uart_enable_rx_isr(bool enable)
{
    uint32_t control = IORD_ALTERA_AVALON_UART_CONTROL(FX3_UART);
    if (enable) {
        control |= ALTERA_AVALON_UART_CONTROL_RRDY_MSK;
    } else {
        control &= ~ALTERA_AVALON_UART_CONTROL_RRDY_MSK;
    }
    IOWR_ALTERA_AVALON_UART_CONTROL(FX3_UART, control);
}

static inline void fx3_uart_rx_isr(uint32_t status)
{
    uint8_t data;

    const uint32_t error_mask =
        ALTERA_AVALON_UART_STATUS_PE_MSK |
        ALTERA_AVALON_UART_STATUS_FE_MSK;

    /* Throw away data if error flags are set */
    if (status & error_mask) {
        return;
    }

    data = IORD_ALTERA_AVALON_UART_RXDATA(FX3_UART);

    if (buf_count < sizeof(buf)) {
        buf[buf_count++] = data;
    }
}

static void fx3_uart_isr(void *context)
{
    /* Fetch UART state */
    const uint32_t status = IORD_ALTERA_AVALON_UART_STATUS(FX3_UART);
    static volatile uint32_t debug = 0;

    /* Clear error flags */
    IOWR_ALTERA_AVALON_UART_STATUS(FX3_UART, 0);

    /* Dummmy readback to ensure IRQ is negated before ISR returns */
    IORD_ALTERA_AVALON_UART_STATUS(FX3_UART);

    if (status & ALTERA_AVALON_UART_STATUS_RRDY_MSK) {
        fx3_uart_rx_isr(status);
    } else {
        debug++;
    }
}
#endif

static void command_uart_enable_isr(bool enable) {
    uint32_t val = enable ? 1 : 0 ;
    IOWR_32DIRECT(COMMAND_UART_0_BASE, 16, val) ;
    return ;
}

static void command_uart_isr(void *context) {
    struct pkt_buf *pkt = (struct pkt_buf *)context ;

    /* Reading the request should clear the interrupt */
    command_uart_read_request((uint8_t *)pkt->req) ;

    /* Tell the main loop that there is a request pending */
    pkt->ready = true ;

    return ;
}

void bladerf_nios_init(struct pkt_buf *pkt) {
    /* Set the prescaler for 400kHz with an 80MHz clock:
     *      (prescaler = clock / (5*desired) - 1)
     */
    IOWR_16DIRECT(I2C, OC_I2C_PRESCALER, 39);
    IOWR_8DIRECT(I2C, OC_I2C_CTRL, OC_I2C_ENABLE);

    /* Set the FX3 UART connection divisor to 14 to get 4000000bps UART:
     *      (baud rate = clock/(divisor + 1))
     */
    IOWR_ALTERA_AVALON_UART_DIVISOR(FX3_UART, 19);

    /* Set the IQ Correction parameters to 0 */
    IOWR_ALTERA_AVALON_PIO_DATA(IQ_CORR_RX_PHASE_GAIN_BASE, DEFAULT_CORRECTION);
    IOWR_ALTERA_AVALON_PIO_DATA(IQ_CORR_TX_PHASE_GAIN_BASE, DEFAULT_CORRECTION);

    /* Register Command UART ISR */
    alt_ic_isr_register(
        COMMAND_UART_0_IRQ_INTERRUPT_CONTROLLER_ID,
        COMMAND_UART_0_IRQ,
        command_uart_isr,
        pkt,
        NULL
    ) ;

    /* Enable interrupts */
    command_uart_enable_isr(true) ;
    alt_ic_irq_enable(COMMAND_UART_0_IRQ_INTERRUPT_CONTROLLER_ID, COMMAND_UART_0_IRQ);

#if 0
    /* Register FX3 UART ISR */
    alt_ic_isr_register(UART_0_IRQ_INTERRUPT_CONTROLLER_ID, UART_0_IRQ,
                        fx3_uart_isr, NULL, NULL);

    /* Disable all UART interrupts so that we can enable only those we want */
    IOWR_ALTERA_AVALON_UART_CONTROL(FX3_UART, 0x00);

    /* Enable interrupts */
    fx3_uart_enable_rx_isr(true);
    alt_ic_irq_enable(UART_0_IRQ_INTERRUPT_CONTROLLER_ID, UART_0_IRQ);
#endif
}

static void si5338_complete_transfer(uint8_t check_rxack)
{
    if ((IORD_8DIRECT(I2C, OC_I2C_CMD_STATUS) & OC_I2C_TIP) == 0) {
        while ( (IORD_8DIRECT(I2C, OC_I2C_CMD_STATUS) & OC_I2C_TIP) == 0);
    }

    while (IORD_8DIRECT(I2C, OC_I2C_CMD_STATUS) & OC_I2C_TIP);

    while (check_rxack && (IORD_8DIRECT(I2C, OC_I2C_CMD_STATUS) & OC_I2C_RXACK));
}

uint8_t lms6_read(uint8_t addr)
{
    uint8_t data;

    alt_avalon_spi_command(SPI_0_BASE, 0, 1, &addr, 0, 0, ALT_AVALON_SPI_COMMAND_MERGE);
    alt_avalon_spi_command(SPI_0_BASE, 0, 0, 0, 1, &data, 0);

    return data;
}

void lms6_write(uint8_t addr, uint8_t data)
{
    uint8_t msg[2] = { addr | LMS6002D_WRITE_BIT, data};
    alt_avalon_spi_command(SPI_0_BASE, 0, 2, msg, 0, 0, 0 ) ;
}

uint8_t si5338_read(uint8_t addr)
{
    uint8_t data;

    /* Set the address to the Si5338 */
    IOWR_8DIRECT(I2C, OC_I2C_DATA, SI5338_I2C);
    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_STA | OC_I2C_WR);
    si5338_complete_transfer(1);

    IOWR_8DIRECT(I2C, OC_I2C_DATA, addr);
    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_WR | OC_I2C_STO);
    si5338_complete_transfer(1) ;

    /* Next transfer is a read operation, so '1' in the read/write bit */
    IOWR_8DIRECT(I2C, OC_I2C_DATA, SI5338_I2C | 1);
    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_STA | OC_I2C_WR);
    si5338_complete_transfer(1) ;

    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_RD | OC_I2C_NACK | OC_I2C_STO);
    si5338_complete_transfer(0);

    data = IORD_8DIRECT(I2C, OC_I2C_DATA);
    return data;
}

void si5338_write(uint8_t addr, uint8_t data)
{
    /* Set the address to the Si5338 */
    IOWR_8DIRECT(I2C, OC_I2C_DATA, SI5338_I2C) ;
    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_STA | OC_I2C_WR);
    si5338_complete_transfer(1);

    IOWR_8DIRECT(I2C, OC_I2C_DATA, addr);
    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_CMD_STATUS | OC_I2C_WR);
    si5338_complete_transfer(1);

    IOWR_8DIRECT(I2C, OC_I2C_DATA, data);
    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_WR | OC_I2C_STO);
    si5338_complete_transfer(0);
}

void vctcxo_trim_dac_write(uint16_t val)
{
    uint8_t data[3];

    data[0] = 0x28;
    data[1] = 0;
    data[2] = 0;
    alt_avalon_spi_command(SPI_1_BASE, 0, 3, data, 0, 0, 0);

    data[0] = 0x08;
    data[1] = (val >> 8) & 0xff;
    data[2] = val & 0xff;
    alt_avalon_spi_command(SPI_1_BASE, 0, 3, data, 0, 0, 0) ;
}

void adf4351_write(uint32_t val)
{
    union {
        uint32_t val;
        uint8_t byte[4];
    } sval;

    uint8_t t;
    sval.val = val;

    t = sval.byte[0];
    sval.byte[0] = sval.byte[3];
    sval.byte[3] = t;

    t = sval.byte[1];
    sval.byte[1] = sval.byte[2];
    sval.byte[2] = t;

    alt_avalon_spi_command(SPI_1_BASE, 1, 4, (uint8_t*)&sval.val, 0, 0, 0);
}

static inline uint32_t module_to_iqcorr_base(bladerf_module m)
{
    if (m == BLADERF_MODULE_RX) {
        return IQ_CORR_RX_PHASE_GAIN_BASE;
    } else {
        return IQ_CORR_TX_PHASE_GAIN_BASE;
    }
}

uint16_t iqbal_get_gain(bladerf_module m)
{
    const uint32_t base = module_to_iqcorr_base(m);
    const uint32_t regval = IORD_ALTERA_AVALON_PIO_DATA(base);
    return (uint16_t) regval;
}

void iqbal_set_gain(bladerf_module m, uint16_t value)
{
    const uint32_t base = module_to_iqcorr_base(m);
    uint32_t regval = IORD_ALTERA_AVALON_PIO_DATA(base);

    regval &= 0xffff0000;
    regval |= value;

    IOWR_ALTERA_AVALON_PIO_DATA(base, regval);
}

uint16_t iqbal_get_phase(bladerf_module m)
{
    uint32_t regval;

    if (m == BLADERF_MODULE_RX) {
        regval = IORD_ALTERA_AVALON_PIO_DATA(IQ_CORR_RX_PHASE_GAIN_BASE);
    } else {
        regval = IORD_ALTERA_AVALON_PIO_DATA(IQ_CORR_TX_PHASE_GAIN_BASE);
    }

    return (uint16_t) (regval >> 16);
}

void iqbal_set_phase(bladerf_module m, uint16_t value)
{
    const uint32_t base = module_to_iqcorr_base(m);
    uint32_t regval = IORD_ALTERA_AVALON_PIO_DATA(base);

    regval &= 0x0000ffff;
    regval |= ((uint32_t) value) << 16;

    IOWR_ALTERA_AVALON_PIO_DATA(base, regval);
}

uint64_t time_tamer_read(bladerf_module m)
{
    uint8_t offset = (m == BLADERF_MODULE_RX) ? 0 : 8;
    uint64_t value = 0;

    value  = IORD_8DIRECT(TIME_TAMER_0_BASE, offset++);
    value |= ((uint64_t) IORD_8DIRECT(TIME_TAMER_0_BASE, offset++)) << 8;
    value |= ((uint64_t) IORD_8DIRECT(TIME_TAMER_0_BASE, offset++)) << 16;
    value |= ((uint64_t) IORD_8DIRECT(TIME_TAMER_0_BASE, offset++)) << 24;
    value |= ((uint64_t) IORD_8DIRECT(TIME_TAMER_0_BASE, offset++)) << 32;
    value |= ((uint64_t) IORD_8DIRECT(TIME_TAMER_0_BASE, offset++)) << 40;
    value |= ((uint64_t) IORD_8DIRECT(TIME_TAMER_0_BASE, offset++)) << 48;
    value |= ((uint64_t) IORD_8DIRECT(TIME_TAMER_0_BASE, offset++)) << 56;

    return value;
}

#endif
