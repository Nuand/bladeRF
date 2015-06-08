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
#include "fpga_version.h"

/* Detect if we are in NIOS Build tools */
#ifdef __hal__
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
#   include "altera_avalon_uart_regs.h"
#   include "altera_avalon_jtag_uart_regs.h"
#   include "altera_avalon_pio_regs.h"
#   include "sys/alt_irq.h"
#   define INLINE static inline
#   define SIMULATION_FLUSH_UART()

#   define FX3_UART            UART_0_BASE
#   define I2C                 BLADERF_OC_I2C_MASTER_0_BASE

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


#else
#   define INLINE
    void SIMULATION_FLUSH_UART();
#endif

enum bladerf_module {
    BLADERF_MODULE_RX,
    BLADERF_MODULE_TX,
};

/**
 * Initialize NIOS II device interfaces.
 *
 * This should be called prior to any other device access function.
 */
void bladerf_nios_init(void);

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
 * Adjust the VCTCXO trim DAC
 *
 * @param   val     DAC value to write
 */
void vctcxo_trim_dac_write(uint16_t val);

/**
 * Write a value to the ADF4351 synthesizer (on the XB-200)
 *
 * @param   val     Value to write
 */
void adf4351_write(uint32_t val);

/**
 * Determine if the FX3 UART has a byte availble for a read
 *
 * @return true if a byte is available, false otherwise
 */
INLINE bool fx3_uart_has_data(void);

/**
 * Read a byte from the FX3 UART
 *
 * @return byte value
 */
INLINE uint8_t fx3_uart_read(void);

/**
 * Write a byte to the FX3 UART
 *
 * @param   data    Data byte to write
 */
INLINE void fx3_uart_write(uint8_t data);

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
uint16_t iqbal_get_gain(enum bladerf_module m);

/**
 * Set IQ balance gain correction value
 *
 * @param m         Which module to configure
 * @param value     Gain correction value
 */
void iqbal_set_gain(enum bladerf_module m, uint16_t value);

/**
 * Get IQ balance phase value
 *
 * @param m         Which module to query
 * @return IQ phase correction value
 */
uint16_t iqbal_get_phase(enum bladerf_module m);

/**
 * Set IQ balance phase correction value
 *
 * @param m         Which module to configure
 * @param value     Phase correction value
 */
void iqbal_set_phase(enum bladerf_module m, uint16_t value);

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
uint64_t time_tamer_read(enum bladerf_module m);

/**
 * Reset one of the time tamer's timestamp counters to 0
 */
INLINE void time_tamer_reset(enum bladerf_module m);

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

#endif
