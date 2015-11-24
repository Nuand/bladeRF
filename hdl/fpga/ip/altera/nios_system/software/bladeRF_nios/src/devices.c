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

/* Define a global variable containing the current VCTCXO DAC setting.
 * This is a 'cached' value of what is written to the DAC and is used
 * by the VCTCXO calibration algorithm to avoid constant read requests
 * going out to the DAC. Initial power-up state of the DAC is mid-scale.
 */
uint16_t vctcxo_trim_dac_value = 0x7FFF;

/* Define a cached version of the VCTCXO tamer control register */
uint8_t vctcxo_tamer_ctrl_reg = 0x00;

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

static void vctcxo_tamer_isr(void *context) {
    struct vctcxo_tamer_pkt_buf *pkt = (struct vctcxo_tamer_pkt_buf *)context;
    uint8_t error_status = 0x00;

    /* Disable interrupts */
    vctcxo_tamer_enable_isr( false );

    /* Reset (stop) the counters */
    vctcxo_tamer_reset_counters( true );

    /* Read the current count values */
    pkt->pps_1s_error   = vctcxo_tamer_read_count(VT_ERR_1S_ADDR);
    pkt->pps_10s_error  = vctcxo_tamer_read_count(VT_ERR_10S_ADDR);
    pkt->pps_100s_error = vctcxo_tamer_read_count(VT_ERR_100S_ADDR);

    /* Read the error status register */
    error_status = vctcxo_tamer_read(VT_STAT_ADDR);

    /* Set the appropriate flags in the packet buffer */
    pkt->pps_1s_error_flag   = (error_status & VT_STAT_ERR_1S)   ? true : false;
    pkt->pps_10s_error_flag  = (error_status & VT_STAT_ERR_10S)  ? true : false;
    pkt->pps_100s_error_flag = (error_status & VT_STAT_ERR_100S) ? true : false;

    /* Clear interrupt */
    vctcxo_tamer_clear_isr();

    /* Tell the main loop that there is a request pending */
    pkt->ready = true;

    return;
}

void vctcxo_tamer_enable_isr(bool enable) {
    if( enable ) {
        vctcxo_tamer_ctrl_reg |= VT_CTRL_IRQ_EN;
    } else {
        vctcxo_tamer_ctrl_reg &= ~VT_CTRL_IRQ_EN;
    }

    vctcxo_tamer_write(VT_CTRL_ADDR, vctcxo_tamer_ctrl_reg);
    return;
}

void vctcxo_tamer_clear_isr() {
    vctcxo_tamer_write(VT_CTRL_ADDR, vctcxo_tamer_ctrl_reg | VT_CTRL_IRQ_CLR);
    return;
}

void vctcxo_tamer_reset_counters(bool reset) {
    if( reset ) {
        vctcxo_tamer_ctrl_reg |= VT_CTRL_RESET;
    } else {
        vctcxo_tamer_ctrl_reg &= ~VT_CTRL_RESET;
    }

    vctcxo_tamer_write(VT_CTRL_ADDR, vctcxo_tamer_ctrl_reg);
    return;
}

void vctcxo_tamer_set_tune_mode(bladerf_vctcxo_tamer_mode mode) {

    switch (mode) {
        case BLADERF_VCTCXO_TAMER_DISABLED:
        case BLADERF_VCTCXO_TAMER_1_PPS:
        case BLADERF_VCTCXO_TAMER_10_MHZ:
            vctcxo_tamer_enable_isr(false);
            break;

        default:
            /* Erroneous value */
            return;
    }

    /* Set tuning mode */
    vctcxo_tamer_ctrl_reg &= ~VT_CTRL_TUNE_MODE;
    vctcxo_tamer_ctrl_reg |= (((uint8_t) mode) << 6);
    vctcxo_tamer_write(VT_CTRL_ADDR, vctcxo_tamer_ctrl_reg);

    /* Reset the counters */
    vctcxo_tamer_reset_counters( true );

    /* Take counters out of reset if tuning mode is not DISABLED */
    if( mode != 0x00 ) {
        vctcxo_tamer_reset_counters( false );
    }

    switch (mode) {
        case BLADERF_VCTCXO_TAMER_1_PPS:
        case BLADERF_VCTCXO_TAMER_10_MHZ:
            vctcxo_tamer_enable_isr(true);
            break;

        default:
            /* Leave ISR disabled otherwise */
            break;
    }

    return;
}

bladerf_vctcxo_tamer_mode vctcxo_tamer_get_tune_mode()
{
    uint8_t tmp = vctcxo_tamer_read(VT_CTRL_ADDR);
    tmp = (tmp & VT_CTRL_TUNE_MODE) >> 6;

    switch (tmp) {
        case BLADERF_VCTCXO_TAMER_DISABLED:
        case BLADERF_VCTCXO_TAMER_1_PPS:
        case BLADERF_VCTCXO_TAMER_10_MHZ:
            return (bladerf_vctcxo_tamer_mode) tmp;

        default:
            return BLADERF_VCTCXO_TAMER_INVALID;
    }
}

int32_t vctcxo_tamer_read_count(uint8_t addr) {
    uint32_t base = VCTCXO_TAMER_0_BASE;
    uint8_t offset = addr;
    int32_t value = 0;

    value  = IORD_8DIRECT(base, offset++);
    value |= ((int32_t) IORD_8DIRECT(base, offset++)) << 8;
    value |= ((int32_t) IORD_8DIRECT(base, offset++)) << 16;
    value |= ((int32_t) IORD_8DIRECT(base, offset++)) << 24;

    return value;
}

uint8_t vctcxo_tamer_read(uint8_t addr) {
    return (uint8_t)IORD_8DIRECT(VCTCXO_TAMER_0_BASE, addr);
}

void vctcxo_tamer_write(uint8_t addr, uint8_t data) {
    IOWR_8DIRECT(VCTCXO_TAMER_0_BASE, addr, data);
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

void bladerf_nios_init(struct pkt_buf *pkt, struct vctcxo_tamer_pkt_buf *vctcxo_tamer_pkt) {
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

    /* Register the VCTCXO Tamer ISR */
    alt_ic_isr_register(
        VCTCXO_TAMER_0_IRQ_INTERRUPT_CONTROLLER_ID,
        VCTCXO_TAMER_0_IRQ,
        vctcxo_tamer_isr,
        vctcxo_tamer_pkt,
        NULL
    );

    /* Default VCTCXO Tamer and its interrupts to be disabled. */
    vctcxo_tamer_set_tune_mode(BLADERF_VCTCXO_TAMER_DISABLED);

    /* Enable interrupts */
    command_uart_enable_isr(true) ;
    alt_ic_irq_enable(COMMAND_UART_IRQ_INTERRUPT_CONTROLLER_ID, COMMAND_UART_IRQ);
    alt_ic_irq_enable(VCTCXO_TAMER_0_IRQ_INTERRUPT_CONTROLLER_ID, VCTCXO_TAMER_0_IRQ);
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

    /* Update cached value of trim DAC setting */
    vctcxo_trim_dac_value = val;

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

#endif
