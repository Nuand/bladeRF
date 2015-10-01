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

static void command_uart_enable_isr(bool enable) {
    uint32_t val = enable ? 1 : 0 ;
    IOWR_32DIRECT(COMMAND_UART_BASE, 16, val) ;
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

static void ppscal_enable_isr(bool enable) {
    uint8_t val = enable ? 1 : 0;
    //    IOWR_8DIRECT(PPSCAL_0_BASE, 0x28, val);
    ppscal_write(0x28, val);
    return;
}

static void ppscal_isr(void *context) {
    struct ppscal_pkt_buf *pkt = (struct ppscal_pkt_buf *)context;

    /* Clear interrupt, keeping interrupts enabled */
    ppscal_write(0x28, 0x11);

    /* Read the current count values */
    pkt->pps_1s_count   = ppscal_read(0x00);
    pkt->pps_10s_count  = ppscal_read(0x08);
    pkt->pps_100s_count = ppscal_read(0x10);

    /* Tell the main loop that there is a request pending */
    pkt->ready = true;

    return;
}

void tamer_schedule(bladerf_module m, uint64_t time) {
    uint32_t base = (m == BLADERF_MODULE_RX) ? RX_TAMER_BASE : TX_TAMER_BASE ;

    /* Set the holding time */
    IOWR_8DIRECT(base, 0, (time>> 0)&0xff) ;
    IOWR_8DIRECT(base, 1, (time>> 8)&0xff) ;
    IOWR_8DIRECT(base, 2, (time>>16)&0xff) ;
    IOWR_8DIRECT(base, 3, (time>>24)&0xff) ;
    IOWR_8DIRECT(base, 4, (time>>32)&0xff) ;
    IOWR_8DIRECT(base, 5, (time>>40)&0xff) ;
    IOWR_8DIRECT(base, 6, (time>>48)&0xff) ;
    IOWR_8DIRECT(base, 7, (time>>54)&0xff) ;

    /* Commit it and arm the comparison */
    IOWR_8DIRECT(base, 8, 0) ;

    return ;
}

void bladerf_nios_init(struct pkt_buf *pkt, struct ppscal_pkt_buf *ppscal_pkt) {
    /* Set the prescaler for 400kHz with an 80MHz clock:
     *      (prescaler = clock / (5*desired) - 1)
     */
    IOWR_16DIRECT(I2C, OC_I2C_PRESCALER, 39);
    IOWR_8DIRECT(I2C, OC_I2C_CTRL, OC_I2C_ENABLE);

    /* Set the IQ Correction parameters to 0 */
    IOWR_ALTERA_AVALON_PIO_DATA(IQ_CORR_RX_PHASE_GAIN_BASE, DEFAULT_CORRECTION);
    IOWR_ALTERA_AVALON_PIO_DATA(IQ_CORR_TX_PHASE_GAIN_BASE, DEFAULT_CORRECTION);

    /* Register Command UART ISR */
    alt_ic_isr_register(
        COMMAND_UART_IRQ_INTERRUPT_CONTROLLER_ID,
        COMMAND_UART_IRQ,
        command_uart_isr,
        pkt,
        NULL
    ) ;

    /* Register ppscal ISR */
    alt_ic_isr_register(
        PPSCAL_0_IRQ_INTERRUPT_CONTROLLER_ID,
        PPSCAL_0_IRQ,
        ppscal_isr,
        ppscal_pkt,
        NULL
    );

    /* Enable interrupts */
    command_uart_enable_isr(true) ;
    alt_ic_irq_enable(COMMAND_UART_IRQ_INTERRUPT_CONTROLLER_ID, COMMAND_UART_IRQ);
    ppscal_enable_isr(true);
    alt_ic_irq_enable(PPSCAL_0_IRQ_INTERRUPT_CONTROLLER_ID, PPSCAL_0_IRQ);

    /* Enable the PPSCAL TCXO Counters */
    ppscal_write(0x20, 0x00);
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

    data = IORD_8DIRECT(LMS_SPI_BASE, addr);

    return data;
}

void lms6_write(uint8_t addr, uint8_t data)
{
    IOWR_8DIRECT(LMS_SPI_BASE, addr, data);
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

void vctcxo_trim_dac_write(uint8_t cmd, uint16_t val)
{
    uint8_t data[3] = {
        cmd,
        (val >> 8) & 0xff,
        val & 0xff,
    };

    alt_avalon_spi_command(PERIPHERAL_SPI_BASE, 0, 3, data, 0, 0, 0) ;
}

void vctcxo_trim_dac_read(uint8_t cmd, uint16_t *val)
{
    uint8_t data[2];

    alt_avalon_spi_command(PERIPHERAL_SPI_BASE, 0, 1, &cmd, 0, 0, ALT_AVALON_SPI_COMMAND_MERGE);
    alt_avalon_spi_command(PERIPHERAL_SPI_BASE, 0, 0, 0, 2, &data, 0);

    *val = (data[0] << 8) | data[1];
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

    alt_avalon_spi_command(PERIPHERAL_SPI_BASE, 1, 4, (uint8_t*)&sval.val, 0, 0, 0);
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
    uint32_t base = (m == BLADERF_MODULE_RX) ? RX_TAMER_BASE : TX_TAMER_BASE ;
    uint8_t offset = 0;
    uint64_t value = 0;

    value  = IORD_8DIRECT(base, offset++);
    value |= ((uint64_t) IORD_8DIRECT(base, offset++)) << 8;
    value |= ((uint64_t) IORD_8DIRECT(base, offset++)) << 16;
    value |= ((uint64_t) IORD_8DIRECT(base, offset++)) << 24;
    value |= ((uint64_t) IORD_8DIRECT(base, offset++)) << 32;
    value |= ((uint64_t) IORD_8DIRECT(base, offset++)) << 40;
    value |= ((uint64_t) IORD_8DIRECT(base, offset++)) << 48;
    value |= ((uint64_t) IORD_8DIRECT(base, offset++)) << 56;

    return value;
}

uint64_t ppscal_read(uint8_t addr)
{
    uint32_t base = PPSCAL_0_BASE;
    uint8_t offset = 0;
    uint64_t value = 0;

    value  = IORD_8DIRECT(base, offset++);
    value |= ((uint64_t) IORD_8DIRECT(base, offset++)) << 8;
    value |= ((uint64_t) IORD_8DIRECT(base, offset++)) << 16;
    value |= ((uint64_t) IORD_8DIRECT(base, offset++)) << 24;
    value |= ((uint64_t) IORD_8DIRECT(base, offset++)) << 32;
    value |= ((uint64_t) IORD_8DIRECT(base, offset++)) << 40;
    value |= ((uint64_t) IORD_8DIRECT(base, offset++)) << 48;
    value |= ((uint64_t) IORD_8DIRECT(base, offset++)) << 56;

    return value;
}

void ppscal_write(uint8_t addr, uint8_t data)
{
    IOWR_8DIRECT(PPSCAL_0_BASE, addr, data);
}

#endif
