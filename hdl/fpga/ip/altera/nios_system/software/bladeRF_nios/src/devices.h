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

#ifndef DEVICES_H_
#define DEVICES_H_

#include <stdint.h>
#include <stdbool.h>
#include "libbladeRF_nios_compat.h"
#include "fpga_version.h"
#include "pkt_handler.h"

/* Detect if we are in NIOS Build tools */
#ifdef BLADERF_NIOS_BUILD
#   ifdef BLADERF_NIOS_PC_SIMULATION
#       error "When compiling for the NIOS II, you cannot define BLADERF_NIOS_PC_SIMULATION."
#   endif
#else
#   ifndef BLADERF_NIOS_PC_SIMULATION
#   error "When compiling for PC simulation, you must define BLADERF_NIOS_PC_SIMULATION."
#   endif
#endif

#ifndef BLADERF_NIOS_PC_SIMULATION
#   include "system.h"
#   include "altera_avalon_spi.h"
#   include "altera_avalon_jtag_uart_regs.h"
#   include "altera_avalon_pio_regs.h"
#   include "sys/alt_irq.h"
#   define INLINE static inline
#   define SIMULATION_FLUSH_UART()

#   define I2C                 OPENCORES_I2C_BASE

/* Time tamer register offsets from the base */
#   define OC_I2C_PRESCALER    0
#   define OC_I2C_CTRL         2
#   define OC_I2C_DATA         3
#   define OC_I2C_CMD_STATUS   4

/* SI5338 I2C interface */
#   define SI5338_I2C          (0xE0)
#   define OC_I2C_ENABLE       (1<<7)
#   define OC_I2C_STA          (1<<7)
#   define OC_I2C_STO          (1<<6)
#   define OC_I2C_WR           (1<<4)
#   define OC_I2C_RD           (1<<5)
#   define OC_I2C_TIP          (1<<1)
#   define OC_I2C_RXACK        (1<<7)
#   define OC_I2C_NACK         (1<<3)

/* IQ balance correction */
#   define DEFAULT_GAIN_CORRECTION     0x1000
#   define GAIN_OFFSET                 0
#   define DEFAULT_PHASE_CORRECTION    0x0000
#   define PHASE_OFFSET                16
#   define DEFAULT_CORRECTION  ( (DEFAULT_PHASE_CORRECTION << PHASE_OFFSET) | \
                              (DEFAULT_GAIN_CORRECTION  << GAIN_OFFSET)  )

/* LMS6002D SPI interface */
#define LMS6002D_WRITE_BIT  (1 << 7)

/* VCTCXO tamer register offsets */
#   define VT_CTRL_ADDR      (0x00)
#   define VT_STAT_ADDR      (0x01)
#   define VT_ERR_1S_ADDR    (0x04)
#   define VT_ERR_10S_ADDR   (0x0C)
#   define VT_ERR_100S_ADDR  (0x14)

/* VCTCXO tamer control/status bits */
#   define VT_CTRL_RESET     (0x01)
#   define VT_CTRL_IRQ_EN    (1<<4)
#   define VT_CTRL_IRQ_CLR   (1<<5)
#   define VT_CTRL_TUNE_MODE (0xC0)

#   define VT_STAT_ERR_1S    (0x01)
#   define VT_STAT_ERR_10S   (1<<1)
#   define VT_STAT_ERR_100S  (1<<2)

#else
#   define INLINE
    void SIMULATION_FLUSH_UART();
#endif

/* Define a global variable containing the current VCTCXO DAC setting.
 * This is a 'cached' value of what is written to the DAC and is used
 * for the calibration algorithm to avoid unnecessary read requests
 * going out to the DAC.
 */
extern uint16_t vctcxo_trim_dac_value;

/* Define a cached version of the VCTCXO tamer control register */
extern uint8_t vctcxo_tamer_ctrl_reg;

/**
 * Initialize NIOS II device interfaces.
 *
 * This should be called prior to any other device access function.
 */
void bladerf_nios_init(struct pkt_buf *pkt, struct vctcxo_tamer_pkt_buf *vctcxo_tamer_pkt);

/**
 * Read from an LMS6002D register
 *
 * @param   addr    Address to read from
 *
 * @return  Register data
 */
uint8_t lms6_read(uint8_t addr);

/**
 * Write to an LMS6002D register
 *
 * @param   addr    Register address to write to
 * @param   data    Data to write
 */
void lms6_write(uint8_t addr, uint8_t data);

/**
 * Read from Si5338 clock generator register
 *
 * @param   addr    Address to read from
 *
 * @return  Register data
 */
uint8_t si5338_read(uint8_t addr);

/**
 * Write to Si5338 clock generator register
 *
 * @param   addr    Register address
 * @param   data    Data to write
 */
void si5338_write(uint8_t addr, uint8_t data);

/**
 * Write a command to the VCTCXO trim DAC
 *
 * @param   cmd     DAC command
 * @param   data    command data to write
 */
void vctcxo_trim_dac_write(uint8_t cmd, uint16_t val);

/**
 * Read from the VCTCXO trim DAC
 *
 * @param   cmd     DAC command
 * @param   data    Read data
 */
void vctcxo_trim_dac_read(uint8_t cmd, uint16_t *val);

/**
 * Write a value to the ADF4351 synthesizer (on the XB-200)
 *
 * @param   val     Value to write
 */
void adf4351_write(uint32_t val);

/**
 * Read bladeRF device control register
 *
 * TODO: Annotate bit<->config relationship
 *
 * @return Device control bit map
 */
INLINE uint32_t control_reg_read(void);

/**
 * Write bladeRF device configuration register
 *
 * TODO: Annotate bit<->config relationship
 *
 * @param   value   Device configuration bit map
 */
INLINE void control_reg_write(uint32_t value);

/**
 * Get IQ balance gain value
 *
 * @param m         Which module to query
 * @return RX IQ gain correction value
 */
uint16_t iqbal_get_gain(bladerf_module m);

/**
 * Set IQ balance gain correction value
 *
 * @param m         Which module to configure
 * @param value     Gain correction value
 */
void iqbal_set_gain(bladerf_module m, uint16_t value);

/**
 * Get IQ balance phase value
 *
 * @param m         Which module to query
 * @return IQ phase correction value
 */
uint16_t iqbal_get_phase(bladerf_module m);

/**
 * Set IQ balance phase correction value
 *
 * @param m         Which module to configure
 * @param value     Phase correction value
 */
void iqbal_set_phase(bladerf_module m, uint16_t value);

/**
 * Read from GPIO expansion port
 *
 * TODO: Annotate the bit <-> pin relationship
 *
 * @return GPIO port values. Outputs pins read back as '0'.
 */
INLINE uint32_t expansion_port_read(void);

/**
 * Write to GPIO expansion port
 *
 * TODO: Annotate the bit <-> pin relationship
 *
 * @param   value   Port value to write. Writing to pins configured as input
 *                  does not affect their state.
 */
INLINE void expansion_port_write(uint32_t value);

/**
 * Get the direction settings of the expansion port GPIOs
 *
 * TODO: Annotate the bit <-> pin relationship
 * TODO: Annotate bit <-> direction
 *
 * @return Direction bit map
 */
INLINE uint32_t expansion_port_get_direction();

/**
 * Configure GPIO expansion port pin directions
 *
 * TODO: Annotate the bit <-> pin relationship
 * TODO: Annotate bit <-> direction
 *
 * @param   dir     Bit map of pin directions
 */
INLINE void expansion_port_set_direction(uint32_t dir);

/**
 * Read timestamp from time tamer
 */
uint64_t time_tamer_read(bladerf_module m);

/**
 * Reset one of the time tamer's timestamp counters to 0
 */
INLINE void time_tamer_reset(bladerf_module m);

/**
 * Clear the interrupt for the specified timer tamer module
 */
INLINE void timer_tamer_clear_interrupt(bladerf_module m);

/**
 * Schedule a timer interrupt on the specified module
 *
 * @param   m       Module to schedule interrupt for
 * @param   time    Timestamp to schedule interrupt to occur at.
 *                  If this is in the past, the interrupt will file immediately.
 */
void tamer_schedule(bladerf_module m, uint64_t time);

/**
 * Read the command UART request buffer
 */
INLINE void command_uart_read_request(uint8_t *command);

/**
 * Write the command UART response buffer
 */
INLINE void command_uart_write_response(uint8_t *command);

/**
 * Enable interrupts from the VCTCXO Tamer module
 *
 * @param   enable  true or false
 */
void vctcxo_tamer_enable_isr(bool enable);

/**
 * Clear interrupts from the VCTCXO Tamer module
 */
void vctcxo_tamer_clear_isr();

/**
 * Reset the counters in the VCTCXO Tamer module. Setting is sticky,
 * so counters must be explicitly taken out of reset.
 *
 * @param   reset   true or false
 */
void vctcxo_tamer_reset_counters( bool reset );

/**
 * Sets the VCTCXO Tamer mode
 *
 * @param mode      One of the BLADERF_VCTCXO_TAMER_* values.
 *
 */
void vctcxo_tamer_set_tune_mode(bladerf_vctcxo_tamer_mode mode);

/**
 * Gets the current VCTCXO Tamer mode
 *
 * @return Current mode or BLADERF_VCTCXO_TAMER_MODE_INVALID on failure.
 */
bladerf_vctcxo_tamer_mode vctcxo_tamer_get_tune_mode();

/**
 * Read VCTCXO tamer count error registers
 *
 * @param   addr    Address of byte 0 of error count register
 * @return  VCTCXO  count error
 */
int32_t vctcxo_tamer_read_count(uint8_t addr);

/**
 * Read single VCTCXO tamer register (e.g. control/status)
 *
 * @param   addr    Address of register to read
 * @return  data    Register contents
 */
uint8_t vctcxo_tamer_read(uint8_t addr);

/**
 * Write single VCTCXO tamer register
 *
 * @param   addr    Address of register to write
 * @param   data    Value to write at the specified address
 */
void vctcxo_tamer_write(uint8_t addr, uint8_t data);

/**
 * @return FPGA version
 */
static inline  uint32_t fpga_version(void)
{
    return FPGA_VERSION;
};


/* A number of rountines define here are implemented as just a register
 * access, where incurring function call overhead is wasteful. Therefore,
 * these have been implemented as static inline functions.
 *
 * To allow us to switch between the NIOS II implementation and some simulation
 * code on a host PC, these are tucked away in the below header.
 */
#ifndef BLADERF_NIOS_PC_SIMULATION
#   include "devices_inline.h"
#endif


/* The LMS6002 header contains a few inline functions which contain
 * macros that utilize lms6_read/write, so we need thes declared before
 * including this header */
#include "lms.h"

#endif
