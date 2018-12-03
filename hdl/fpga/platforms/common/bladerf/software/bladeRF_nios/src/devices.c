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

#include "devices.h"
#include "debug.h"
#include "devices_inline.h"
#include "fpga_version.h"
#include <alt_types.h>
#include <stdbool.h>
#include <stdint.h>

/* Define a global variable containing the current VCTCXO DAC setting.
 * This is a 'cached' value of what is written to the DAC and is used
 * by the VCTCXO calibration algorithm to avoid constant read requests
 * going out to the DAC. Initial power-up state of the DAC is mid-scale.
 */
uint16_t vctcxo_trim_dac_value = 0x7FFF;

/* Define a cached version of the VCTCXO tamer control register */
uint8_t vctcxo_tamer_ctrl_reg = 0x00;

#ifdef BOARD_BLADERF_MICRO
/* Common bladeRF2 header */
#include "bladerf2_common.h"

/* Cached version of ADF400x registers */
uint32_t adf400x_reg[4] = { 0 };

/* Define the fast lock profile storage arrays */
fastlock_profile fastlocks_rx[NUM_BBP_FASTLOCK_PROFILES];
fastlock_profile fastlocks_tx[NUM_BBP_FASTLOCK_PROFILES];
#endif  // BOARD_BLADERF_MICRO

static void command_uart_enable_isr(bool enable)
{
    uint32_t val = enable ? 1 : 0;
    IOWR_32DIRECT(COMMAND_UART_BASE, 16, val);
    return;
}

static void command_uart_isr(void *context)
{
    struct pkt_buf *pkt = (struct pkt_buf *)context;

    /* Reading the request should clear the interrupt */
    command_uart_read_request((uint8_t *)pkt->req);

    /* Tell the main loop that there is a request pending */
    pkt->ready = true;

    return;
}

static void vctcxo_tamer_isr(void *context)
{
    struct vctcxo_tamer_pkt_buf *pkt = (struct vctcxo_tamer_pkt_buf *)context;
    uint8_t error_status             = 0x00;

    /* Disable interrupts */
    vctcxo_tamer_enable_isr(false);

    /* Reset (stop) the counters */
    vctcxo_tamer_reset_counters(true);

    /* Read the current count values */
    pkt->pps_1s_error   = vctcxo_tamer_read_count(VT_ERR_1S_ADDR);
    pkt->pps_10s_error  = vctcxo_tamer_read_count(VT_ERR_10S_ADDR);
    pkt->pps_100s_error = vctcxo_tamer_read_count(VT_ERR_100S_ADDR);

    /* Read the error status register */
    error_status = vctcxo_tamer_read(VT_STAT_ADDR);

    /* Set the appropriate flags in the packet buffer */
    pkt->pps_1s_error_flag   = (error_status & VT_STAT_ERR_1S) ? true : false;
    pkt->pps_10s_error_flag  = (error_status & VT_STAT_ERR_10S) ? true : false;
    pkt->pps_100s_error_flag = (error_status & VT_STAT_ERR_100S) ? true : false;

    /* Clear interrupt */
    vctcxo_tamer_clear_isr();

    /* Tell the main loop that there is a request pending */
    pkt->ready = true;

    return;
}

void vctcxo_tamer_enable_isr(bool enable)
{
    if (enable) {
        vctcxo_tamer_ctrl_reg |= VT_CTRL_IRQ_EN;
    } else {
        vctcxo_tamer_ctrl_reg &= ~VT_CTRL_IRQ_EN;
    }

    vctcxo_tamer_write(VT_CTRL_ADDR, vctcxo_tamer_ctrl_reg);
    return;
}

void vctcxo_tamer_clear_isr()
{
    vctcxo_tamer_write(VT_CTRL_ADDR, vctcxo_tamer_ctrl_reg | VT_CTRL_IRQ_CLR);
    return;
}

void vctcxo_tamer_reset_counters(bool reset)
{
    if (reset) {
        vctcxo_tamer_ctrl_reg |= VT_CTRL_RESET;
    } else {
        vctcxo_tamer_ctrl_reg &= ~VT_CTRL_RESET;
    }

    vctcxo_tamer_write(VT_CTRL_ADDR, vctcxo_tamer_ctrl_reg);
    return;
}

void vctcxo_tamer_set_tune_mode(bladerf_vctcxo_tamer_mode mode)
{
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
    vctcxo_tamer_ctrl_reg |= (((uint8_t)mode) << 6);
    vctcxo_tamer_write(VT_CTRL_ADDR, vctcxo_tamer_ctrl_reg);

    /* Reset the counters */
    vctcxo_tamer_reset_counters(true);

    /* Take counters out of reset if tuning mode is not DISABLED */
    if (mode != 0x00) {
        vctcxo_tamer_reset_counters(false);
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
    tmp         = (tmp & VT_CTRL_TUNE_MODE) >> 6;

    switch (tmp) {
        case BLADERF_VCTCXO_TAMER_DISABLED:
        case BLADERF_VCTCXO_TAMER_1_PPS:
        case BLADERF_VCTCXO_TAMER_10_MHZ:
            return (bladerf_vctcxo_tamer_mode)tmp;

        default:
            return BLADERF_VCTCXO_TAMER_INVALID;
    }
}

int32_t vctcxo_tamer_read_count(uint8_t addr)
{
    uint32_t base  = VCTCXO_TAMER_0_BASE;
    uint8_t offset = addr;
    int32_t value  = 0;

    value = IORD_8DIRECT(base, offset++);
    value |= ((int32_t)IORD_8DIRECT(base, offset++)) << 8;
    value |= ((int32_t)IORD_8DIRECT(base, offset++)) << 16;
    value |= ((int32_t)IORD_8DIRECT(base, offset++)) << 24;

    return value;
}

uint8_t vctcxo_tamer_read(uint8_t addr)
{
    return (uint8_t)IORD_8DIRECT(VCTCXO_TAMER_0_BASE, addr);
}

void vctcxo_tamer_write(uint8_t addr, uint8_t data)
{
    IOWR_8DIRECT(VCTCXO_TAMER_0_BASE, addr, data);
}

void tamer_schedule(bladerf_module m, uint64_t time)
{
    uint32_t base = (m == BLADERF_MODULE_RX) ? RX_TAMER_BASE : TX_TAMER_BASE;

    /* Set the holding time */
    IOWR_8DIRECT(base, 0, (time >> 0) & 0xff);
    IOWR_8DIRECT(base, 1, (time >> 8) & 0xff);
    IOWR_8DIRECT(base, 2, (time >> 16) & 0xff);
    IOWR_8DIRECT(base, 3, (time >> 24) & 0xff);
    IOWR_8DIRECT(base, 4, (time >> 32) & 0xff);
    IOWR_8DIRECT(base, 5, (time >> 40) & 0xff);
    IOWR_8DIRECT(base, 6, (time >> 48) & 0xff);
    IOWR_8DIRECT(base, 7, (time >> 54) & 0xff);

    /* Commit it and arm the comparison */
    IOWR_8DIRECT(base, 8, 0);

    return;
}

void bladerf_nios_init(struct pkt_buf *pkt,
                       struct vctcxo_tamer_pkt_buf *vctcxo_tamer_pkt)
{
    /* Set the prescaler for 400kHz with an 80MHz clock:
     *      (prescaler = clock / (5*desired) - 1)
     */
    IOWR_16DIRECT(I2C, OC_I2C_PRESCALER, 39);
    IOWR_8DIRECT(I2C, OC_I2C_CTRL, OC_I2C_ENABLE);

#ifdef IQ_CORR_RX_PHASE_GAIN_BASE
#ifdef IQ_CORR_TX_PHASE_GAIN_BASE
    /* Set the IQ Correction parameters to 0 */
    IOWR_ALTERA_AVALON_PIO_DATA(IQ_CORR_RX_PHASE_GAIN_BASE, DEFAULT_CORRECTION);
    IOWR_ALTERA_AVALON_PIO_DATA(IQ_CORR_TX_PHASE_GAIN_BASE, DEFAULT_CORRECTION);
#endif
#endif

    /* Disable all triggering */
    IOWR_ALTERA_AVALON_PIO_DATA(TX_TRIGGER_CTL_BASE, 0x00);
    IOWR_ALTERA_AVALON_PIO_DATA(RX_TRIGGER_CTL_BASE, 0x00);

    /* Register Command UART ISR */
    alt_ic_isr_register(COMMAND_UART_IRQ_INTERRUPT_CONTROLLER_ID,
                        COMMAND_UART_IRQ, command_uart_isr, pkt, NULL);

    /* Register the VCTCXO Tamer ISR */
    alt_ic_isr_register(VCTCXO_TAMER_0_IRQ_INTERRUPT_CONTROLLER_ID,
                        VCTCXO_TAMER_0_IRQ, vctcxo_tamer_isr, vctcxo_tamer_pkt,
                        NULL);

    /* Default VCTCXO Tamer and its interrupts to be disabled. */
    vctcxo_tamer_set_tune_mode(BLADERF_VCTCXO_TAMER_DISABLED);

    /* Enable interrupts */
    command_uart_enable_isr(true);
    alt_ic_irq_enable(COMMAND_UART_IRQ_INTERRUPT_CONTROLLER_ID,
                      COMMAND_UART_IRQ);
    alt_ic_irq_enable(VCTCXO_TAMER_0_IRQ_INTERRUPT_CONTROLLER_ID,
                      VCTCXO_TAMER_0_IRQ);
}

static void i2c_complete_transfer(uint8_t check_rxack)
{
    if ((IORD_8DIRECT(I2C, OC_I2C_CMD_STATUS) & OC_I2C_TIP) == 0) {
        while ((IORD_8DIRECT(I2C, OC_I2C_CMD_STATUS) & OC_I2C_TIP) == 0)
            ;
    }

    while (IORD_8DIRECT(I2C, OC_I2C_CMD_STATUS) & OC_I2C_TIP)
        ;

    while (check_rxack && (IORD_8DIRECT(I2C, OC_I2C_CMD_STATUS) & OC_I2C_RXACK))
        ;
}

void spi_arbiter_lock()
{
// Applies only to bladeRF1
#ifdef ARBITER_0_BASE
    uint8_t data;

    IOWR_32DIRECT(ARBITER_0_BASE, 0, 1);

    do {
        data = IORD_8DIRECT(ARBITER_0_BASE, 1);
    } while ((data & 1) == 0);
#endif  // ARBITER_0_BASE
}

void spi_arbiter_unlock()
{
// Applies only to bladeRF1
#ifdef ARBITER_0_BASE
    IOWR_32DIRECT(ARBITER_0_BASE, 0, 2);
#endif  // ARBITER_0_BASE
}

uint8_t lms6_read(uint8_t addr)
{
    uint8_t data;

    spi_arbiter_lock();
    data = IORD_8DIRECT(RFFE_SPI_BASE, addr);
    spi_arbiter_unlock();

    return data;
}

void lms6_write(uint8_t addr, uint8_t data)
{
    spi_arbiter_lock();
    IOWR_8DIRECT(RFFE_SPI_BASE, addr, data);
    spi_arbiter_unlock();
}

#ifdef BOARD_BLADERF_MICRO
uint64_t adi_spi_read(uint16_t addr)
{
    alt_u8 addr8[2];
    alt_u8 data8[8];
    alt_u8 bytes;
    uint8_t i;
    uint64_t rv;

    // The alt_avalon_spi_command expects parameters to be arrays of bytes

    // Convert the uint16_t address into array of 2 uint8_t
    addr8[0] = (addr >> 8);
    addr8[1] = (addr & 0xff);

    // Calculate number of data bytes in this command
    bytes = (((addr >> 12) & 0x7) + 1);

    // Send down the command, read the response into data8
    alt_avalon_spi_command(RFFE_SPI_BASE, 0, 2, &addr8[0], bytes, &data8[0], 0);

    // Build the uint64_t return value
    rv = UINT64_C(0x0);
    for (i = 0; i < 8; i++) {
        rv |= (((uint64_t)data8[i]) << 8 * (7 - i));
    }

    return rv;
}
#endif  // BOARD_BLADERF_MICRO

#ifdef BOARD_BLADERF_MICRO
void adi_spi_write(uint16_t addr, uint64_t data)
{
    alt_u8 data8[10];
    alt_u8 bytes;
    uint8_t i;

    // The alt_avalon_spi_command expects parameters to be arrays of bytes

    // Convert the uint16_t address into array of 2 uint8_t
    data8[0] = (addr >> 8) | 0x80; /* Make sure write bit is set */
    data8[1] = (addr & 0xff);

    // Convert the uint64_t data into an array of 8 uint8_t
    for (i = 0; i < 8; i++) {
        data8[i + 2] = (data >> 8 * (7 - i)) & 0xff;
    }

    // Calculate number of command and data bytes in this command
    bytes = (((addr >> 12) & 0x7) + 1) + 2;

    // Send down the command and the data
    alt_avalon_spi_command(RFFE_SPI_BASE, 0, bytes, &data8[0], 0, 0, 0);
}
#endif  // BOARD_BLADERF_MICRO

#ifdef BOARD_BLADERF_MICRO
uint32_t adi_axi_read(uint16_t addr)
{
    uint32_t data = 0;
#ifdef AXI_AD9361_0_BASE  // Temporary hack for bladeRF1 compat
    data = IORD_32DIRECT(AXI_AD9361_0_BASE, addr);
#endif
    return data;
}
#endif  // BOARD_BLADERF_MICRO

#ifdef BOARD_BLADERF_MICRO
void adi_axi_write(uint16_t addr, uint32_t data)
{
#ifdef AXI_AD9361_0_BASE  // Temporary hack for bladeRF1 compat
    IOWR_32DIRECT(AXI_AD9361_0_BASE, addr, data);
#endif
}
#endif  // BOARD_BLADERF_MICRO

#ifdef BOARD_BLADERF_MICRO
void adi_fastlock_save(bool is_tx, uint8_t rffe_profile, uint16_t nios_profile)
{
    uint16_t fl_prog_addr_reg;
    uint16_t fl_prog_rddata_reg;
    uint16_t addr;
    uint64_t data;
    uint32_t i;
    fastlock_profile *fastlocks;

    if (is_tx) {
        fl_prog_addr_reg   = 0x29c;
        fl_prog_rddata_reg = 0x29e;
        fastlocks          = fastlocks_tx;
    } else {
        fl_prog_addr_reg   = 0x25c;
        fl_prog_rddata_reg = 0x25e;
        fastlocks          = fastlocks_rx;
    }

    /* Read out the profile data and save to Nios memory */
    for (i = 0; i < 16; i++) {
        addr = (0x1 << 15) | (0x0 << 12) | (fl_prog_addr_reg & 0x3ff);
        data = (uint64_t)(((rffe_profile & 0x7) << 4) | (i & 0xf)) << 8 * 7;
        adi_spi_write(addr, data);

        addr = (0x0 << 15) | (0x0 << 12) | (fl_prog_rddata_reg & 0x3ff);
        fastlocks[nios_profile].profile_data[i] = adi_spi_read(addr) >> 56;
    }

    /* Kick out any other profile stored in the Nios that was in this slot */
    for (i = 0; i < NUM_BBP_FASTLOCK_PROFILES; i++) {
        if ((fastlocks[i].profile_num == rffe_profile) &&
            (fastlocks[i].state == FASTLOCK_STATE_BBP_RFFE)) {
            fastlocks[i].state = FASTLOCK_STATE_BBP;
        }
    }

    /* Update profile state */
    fastlocks[nios_profile].state = FASTLOCK_STATE_BBP_RFFE;
}
#endif  // BOARD_BLADERF_MICRO

#ifdef BOARD_BLADERF_MICRO
void adi_fastlock_load(bladerf_module m, fastlock_profile *p)
{
    static const uint8_t fl_prog_write = 1 << 1;
    static const uint8_t fl_prog_clken = 1 << 0;
    static const uint8_t fl_prog_bytes = 16;
    uint16_t fl_prog_data_reg;
    uint16_t fl_prog_ctrl_reg;
    uint32_t i;
    uint16_t addr;
    uint64_t data;
    fastlock_profile *fastlocks;

    if (BLADERF_CHANNEL_IS_TX(m)) {
        fl_prog_data_reg = 0x29d;
        fl_prog_ctrl_reg = 0x29f;
        fastlocks        = fastlocks_tx;
    } else {
        fl_prog_data_reg = 0x25d;
        fl_prog_ctrl_reg = 0x25f;
        fastlocks        = fastlocks_rx;
    }

    if ((p->state == FASTLOCK_STATE_RFFE) ||
        (p->state == FASTLOCK_STATE_BBP_RFFE)) {
        /* Already loaded! */
        return;
    } else {
        /* Kick out any other loaded profile that's in this slot */
        for (i = 0; i < NUM_BBP_FASTLOCK_PROFILES; i++) {
            if ((fastlocks[i].profile_num == p->profile_num) &&
                (fastlocks[i].state == FASTLOCK_STATE_BBP_RFFE)) {
                fastlocks[i].state = FASTLOCK_STATE_BBP;
            }
        }

        /* Write 2 bytes to fast lock program data register */
        addr = (0x1 << 15) | (0x1 << 12) | (fl_prog_data_reg & 0x3ff);
        data = (uint64_t)(p->profile_data[0]) << 8 * 7;
        data |= (uint64_t)(((p->profile_num & 0x7) << 4) | (i & 0xf)) << 8 * 6;
        adi_spi_write(addr, data);

        for (i = 1; i < fl_prog_bytes; i++) {
            /* Write 4 bytes to fast lock program control register */
            addr = (0x1 << 15) | (0x3 << 12) | (fl_prog_ctrl_reg & 0x3ff);
            data = (uint64_t)(fl_prog_write | fl_prog_clken) << 8 * 7;
            data |= UINT64_C(0x0) << 8 * 6;
            data |= (uint64_t)(p->profile_data[i]) << 8 * 5;
            data |= ((uint64_t)(((p->profile_num & 0x7) << 4) | (i & 0xf))
                     << 8 * 4);
            adi_spi_write(addr, data);
        }

        /* Write 1 byte to fast lock program control register */
        addr = (0x1 << 15) | (0x0 << 12) | (fl_prog_ctrl_reg & 0x3ff);
        data = (uint64_t)(fl_prog_write | fl_prog_clken) << 8 * 7;
        adi_spi_write(addr, data);

        /* Write 1 byte to fast lock program control register */
        addr = (0x1 << 15) | (0x0 << 12) | (fl_prog_ctrl_reg & 0x3ff);
        adi_spi_write(addr, 0);

        /* Update profile state */
        p->state = FASTLOCK_STATE_BBP_RFFE;
    }
}
#endif  // BOARD_BLADERF_MICRO

#ifdef BOARD_BLADERF_MICRO
void adi_fastlock_recall(bladerf_module m, fastlock_profile *p)
{
    uint16_t fl_setup_reg = BLADERF_CHANNEL_IS_TX(m) ? 0x29a : 0x25a;
    uint16_t addr;
    uint64_t data = 0;

    addr = (0x1 << 15) | (0x0 << 12) | (fl_setup_reg & 0x3ff);
    data = (uint64_t)(((p->profile_num & 0x7) << 5) | (0x1)) << 8 * 7;

    adi_spi_write(addr, data);
}
#endif  // BOARD_BLADERF_MICRO

#ifdef BOARD_BLADERF_MICRO
void adi_rfport_select(fastlock_profile *p)
{
    static const uint16_t input_sel_reg = 0x4;
    static const uint8_t rx_port_mask   = 0x3f;
    static const uint8_t tx_port_mask   = 0x40;
    uint16_t addr;
    uint64_t data;

    /* Get current port selection */
    addr = (0x0 << 15) | (0x0 << 12) | (input_sel_reg & 0x3ff);
    data = adi_spi_read(addr) >> 56;

    if (p->port >> 7) {
        /* RX bit is set, only modify RX port selection */
        data = (data & ~rx_port_mask) | (p->port & rx_port_mask);
    } else {
        /* RX bit is clear, only modify TX port selection */
        data = (data & ~tx_port_mask) | (p->port & tx_port_mask);
    }

    /* Write the new port selection to AD9361 */
    addr = (0x1 << 15) | (0x0 << 12) | (input_sel_reg & 0x3ff);
    data = data << 56;
    adi_spi_write(addr, data);
}
#endif  // BOARD_BLADERF_MICRO

#ifdef BOARD_BLADERF_MICRO
void adi_rfspdt_select(bladerf_module m, fastlock_profile *p)
{
    static const uint32_t tx_spdt_shift = RFFE_CONTROL_TX_SPDT_1;
    static const uint32_t tx_spdt_mask  = (0xF << tx_spdt_shift);
    static const uint32_t rx_spdt_shift = RFFE_CONTROL_RX_SPDT_1;
    static const uint32_t rx_spdt_mask  = (0xF << rx_spdt_shift);

    uint32_t rffe_gpio;

    rffe_gpio = rffe_csr_read();

    if (BLADERF_CHANNEL_IS_TX(m)) {
        rffe_gpio &= (~tx_spdt_mask);
        rffe_gpio |= ((p->spdt >> 4) << tx_spdt_shift);
    } else {
        rffe_gpio &= (~rx_spdt_mask);
        rffe_gpio |= ((p->spdt & 0xf) << rx_spdt_shift);
    }

    rffe_csr_write(rffe_gpio);
}
#endif  // BOARD_BLADERF_MICRO

uint8_t si5338_read(uint8_t addr)
{
    uint8_t data;

    /* Set the address to the Si5338 */
    IOWR_8DIRECT(I2C, OC_I2C_DATA, SI5338_I2C);
    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_STA | OC_I2C_WR);
    i2c_complete_transfer(1);

    IOWR_8DIRECT(I2C, OC_I2C_DATA, addr);
    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_WR | OC_I2C_STO);
    i2c_complete_transfer(1);

    /* Next transfer is a read operation, so '1' in the read/write bit */
    IOWR_8DIRECT(I2C, OC_I2C_DATA, SI5338_I2C | 1);
    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_STA | OC_I2C_WR);
    i2c_complete_transfer(1);

    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_RD | OC_I2C_NACK | OC_I2C_STO);
    i2c_complete_transfer(0);

    data = IORD_8DIRECT(I2C, OC_I2C_DATA);
    return data;
}

void si5338_write(uint8_t addr, uint8_t data)
{
    /* Set the address to the Si5338 */
    IOWR_8DIRECT(I2C, OC_I2C_DATA, SI5338_I2C);
    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_STA | OC_I2C_WR);
    i2c_complete_transfer(1);

    IOWR_8DIRECT(I2C, OC_I2C_DATA, addr);
    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_WR);
    i2c_complete_transfer(1);

    IOWR_8DIRECT(I2C, OC_I2C_DATA, data);
    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_WR | OC_I2C_STO);
    i2c_complete_transfer(0);
}

#ifdef BOARD_BLADERF_MICRO
uint16_t ina219_read(uint8_t addr)
{
    uint16_t data;

    /* Set the address to the INA219 */
    IOWR_8DIRECT(I2C, OC_I2C_DATA, INA219_I2C);
    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_STA | OC_I2C_WR);
    i2c_complete_transfer(1);

    IOWR_8DIRECT(I2C, OC_I2C_DATA, addr);
    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_WR | OC_I2C_STO);
    i2c_complete_transfer(1);

    /* Next transfer is a read operation, so '1' in the read/write bit */
    IOWR_8DIRECT(I2C, OC_I2C_DATA, INA219_I2C | 1);
    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_STA | OC_I2C_WR);
    i2c_complete_transfer(1);

    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_RD);
    i2c_complete_transfer(1);
    data = IORD_8DIRECT(I2C, OC_I2C_DATA) << 8;

    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_RD | OC_I2C_NACK | OC_I2C_STO);
    i2c_complete_transfer(0);

    data |= IORD_8DIRECT(I2C, OC_I2C_DATA);
    return data;
}
#endif  // BOARD_BLADERF_MICRO

#ifdef BOARD_BLADERF_MICRO
void ina219_write(uint8_t addr, uint16_t data)
{
    /* Set the address to the INA219 */
    IOWR_8DIRECT(I2C, OC_I2C_DATA, INA219_I2C);
    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_STA | OC_I2C_WR);
    i2c_complete_transfer(1);

    IOWR_8DIRECT(I2C, OC_I2C_DATA, addr);
    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_CMD_STATUS | OC_I2C_WR);
    i2c_complete_transfer(1);

    IOWR_8DIRECT(I2C, OC_I2C_DATA, data >> 8);
    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_WR);
    i2c_complete_transfer(1);

    IOWR_8DIRECT(I2C, OC_I2C_DATA, data);
    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_WR | OC_I2C_STO);
    i2c_complete_transfer(0);
}
#endif  // BOARD_BLADERF_MICRO

void vctcxo_trim_dac_write(uint8_t cmd, uint16_t val)
{
    uint8_t data[3] = {
        cmd,
        (val >> 8) & 0xff,
        val & 0xff,
    };

    /* Update cached value of trim DAC setting */
    vctcxo_trim_dac_value = val;

    alt_avalon_spi_command(PERIPHERAL_SPI_BASE, 0, 3, data, 0, 0, 0);
}

void vctcxo_trim_dac_read(uint8_t cmd, uint16_t *val)
{
    alt_u8 data[2];

    alt_avalon_spi_command(PERIPHERAL_SPI_BASE, 0, 1, &cmd, 0, 0,
                           ALT_AVALON_SPI_COMMAND_MERGE);
    alt_avalon_spi_command(PERIPHERAL_SPI_BASE, 0, 0, 0, 2, &data[0], 0);

    *val = ((uint8_t)data[0] << 8) | (uint8_t)data[1];
}

#ifdef BOARD_BLADERF_MICRO
void ad56x1_vctcxo_trim_dac_write(uint16_t val)
{
    uint8_t data[2] = {
        (val >> 8) & 0xff,
        val & 0xff,
    };

    /* Update cached value of trim DAC setting */
    vctcxo_trim_dac_value = val;

    alt_avalon_spi_command(PERIPHERAL_SPI_BASE, 0, 2, data, 0, 0, 0);
}
#endif  // BOARD_BLADERF_MICRO

#ifdef BOARD_BLADERF_MICRO
void ad56x1_vctcxo_trim_dac_read(uint16_t *val)
{
    /* The AD56x1 DAC does not have readback functionality.
     * Return the cached DAC value */
    *val = vctcxo_trim_dac_value;
}
#endif  // BOARD_BLADERF_MICRO

#ifdef BOARD_BLADERF_MICRO
void adf400x_spi_write(uint32_t val)
{
    uint8_t data[3] = { (val >> 16) & 0xff, (val >> 8) & 0xff,
                        (val >> 0) & 0xff };

    /* Update cached value of ADF400x setting */
    adf400x_reg[val & 0x3] = val;

    alt_avalon_spi_command(PERIPHERAL_SPI_BASE, 1, 3, data, 0, 0, 0);
}
#endif  // BOARD_BLADERF_MICRO

#ifdef BOARD_BLADERF_MICRO
uint32_t adf400x_spi_read(uint8_t addr)
{
    /* This assumes the AD400x MUXOUT is set to SDO/MISO,
     * but the behavior of this output is unclear. */
    /* uint8_t sdo_data[3] = { 0 };
    uint8_t sdi_data[3] = {
        (adf400x_reg[addr & 0x3] >> 16) & 0xff,
        (adf400x_reg[addr & 0x3] >> 8)  & 0xff,
        (adf400x_reg[addr & 0x3] >> 0)  & 0xff
    }; */

    /* Perform the SPI operation so we can take a look on SignalTap */
    // alt_avalon_spi_command(PERIPHERAL_SPI_BASE, 1, 3, sdi_data, 3, sdo_data,
    // 0);

    /* Just return the cached value */
    return adf400x_reg[addr & 0x3];
}
#endif  // BOARD_BLADERF_MICRO

void adf4351_write(uint32_t val)
{
    union {
        uint32_t val;
        uint8_t byte[4];
    } sval;

    uint8_t t;
    sval.val = val;

    t            = sval.byte[0];
    sval.byte[0] = sval.byte[3];
    sval.byte[3] = t;

    t            = sval.byte[1];
    sval.byte[1] = sval.byte[2];
    sval.byte[2] = t;

    alt_avalon_spi_command(PERIPHERAL_SPI_BASE, 1, 4, (uint8_t *)&sval.val, 0,
                           0, 0);
}

// Temporary for bladeRF2 compat
#ifdef IQ_CORR_RX_PHASE_GAIN_BASE
#ifdef IQ_CORR_TX_PHASE_GAIN_BASE
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
    const uint32_t base   = module_to_iqcorr_base(m);
    const uint32_t regval = IORD_ALTERA_AVALON_PIO_DATA(base);
    return (uint16_t)regval;
}

void iqbal_set_gain(bladerf_module m, uint16_t value)
{
    const uint32_t base = module_to_iqcorr_base(m);
    uint32_t regval     = IORD_ALTERA_AVALON_PIO_DATA(base);

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

    return (uint16_t)(regval >> 16);
}

void iqbal_set_phase(bladerf_module m, uint16_t value)
{
    const uint32_t base = module_to_iqcorr_base(m);
    uint32_t regval     = IORD_ALTERA_AVALON_PIO_DATA(base);

    regval &= 0x0000ffff;
    regval |= ((uint32_t)value) << 16;

    IOWR_ALTERA_AVALON_PIO_DATA(base, regval);
}

#endif
#else
uint16_t iqbal_get_gain(bladerf_module m)
{
    return 0;
}
void iqbal_set_gain(bladerf_module m, uint16_t value) {}
uint16_t iqbal_get_phase(bladerf_module m)
{
    return 0;
}
void iqbal_set_phase(bladerf_module m, uint16_t value) {}
#endif

void tx_trigger_ctl_write(uint8_t data)
{
    IOWR_ALTERA_AVALON_PIO_DATA(TX_TRIGGER_CTL_BASE, data);
}

uint8_t tx_trigger_ctl_read(void)
{
    return IORD_ALTERA_AVALON_PIO_DATA(TX_TRIGGER_CTL_BASE);
}

void rx_trigger_ctl_write(uint8_t data)
{
    IOWR_ALTERA_AVALON_PIO_DATA(RX_TRIGGER_CTL_BASE, data);
}

uint8_t rx_trigger_ctl_read(void)
{
    return IORD_ALTERA_AVALON_PIO_DATA(RX_TRIGGER_CTL_BASE);
}

void agc_dc_corr_write(uint16_t addr, uint16_t value)
{
// Applies only to bladeRF1
#ifdef AGC_DC_I_MIN_BASE
    if (addr == NIOS_PKT_8x16_ADDR_AGC_DC_Q_MAX)
        IOWR_ALTERA_AVALON_PIO_DATA(AGC_DC_Q_MAX_BASE, value);
    else if (addr == NIOS_PKT_8x16_ADDR_AGC_DC_I_MAX)
        IOWR_ALTERA_AVALON_PIO_DATA(AGC_DC_I_MAX_BASE, value);
    else if (addr == NIOS_PKT_8x16_ADDR_AGC_DC_Q_MID)
        IOWR_ALTERA_AVALON_PIO_DATA(AGC_DC_Q_MID_BASE, value);
    else if (addr == NIOS_PKT_8x16_ADDR_AGC_DC_I_MID)
        IOWR_ALTERA_AVALON_PIO_DATA(AGC_DC_I_MID_BASE, value);
    else if (addr == NIOS_PKT_8x16_ADDR_AGC_DC_Q_MIN)
        IOWR_ALTERA_AVALON_PIO_DATA(AGC_DC_Q_MIN_BASE, value);
    else if (addr == NIOS_PKT_8x16_ADDR_AGC_DC_I_MIN)
        IOWR_ALTERA_AVALON_PIO_DATA(AGC_DC_I_MIN_BASE, value);
#endif  // AGC_DC_I_MIN_BASE
}

uint64_t time_tamer_read(bladerf_module m)
{
    uint32_t base  = (m == BLADERF_MODULE_RX) ? RX_TAMER_BASE : TX_TAMER_BASE;
    uint8_t offset = 0;
    uint64_t value = 0;

    value = IORD_8DIRECT(base, offset++);
    value |= ((uint64_t)IORD_8DIRECT(base, offset++)) << 8;
    value |= ((uint64_t)IORD_8DIRECT(base, offset++)) << 16;
    value |= ((uint64_t)IORD_8DIRECT(base, offset++)) << 24;
    value |= ((uint64_t)IORD_8DIRECT(base, offset++)) << 32;
    value |= ((uint64_t)IORD_8DIRECT(base, offset++)) << 40;
    value |= ((uint64_t)IORD_8DIRECT(base, offset++)) << 48;
    value |= ((uint64_t)IORD_8DIRECT(base, offset++)) << 56;

    return value;
}

#endif
