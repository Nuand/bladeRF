/*
 * Copyright (c) 2013 Nuand LLC
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
#include "sys/alt_stdio.h"
#include "system.h"
#include "altera_avalon_spi.h"
#include "altera_avalon_uart_regs.h"
#include "altera_avalon_jtag_uart_regs.h"
#include "altera_avalon_pio_regs.h"
#include "priv/alt_busy_sleep.h"
#include "priv/alt_file.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "sys/alt_dev.h"

#define LMS_READ            0
#define LMS_WRITE           (1<<7)
#define LMS_VERBOSE         1
#define kHz(x)              (x*1000)
#define MHz(x)              (x*1000000)
#define GHz(x)              (x*1000000000)


//when version id is moved to a qsys port these will be removed
#define FPGA_VERSION_ID         0x7777
#define FPGA_VERSION_MAJOR      0
#define FPGA_VERSION_MINOR      0
#define FPGA_VERSION_PATCH      3
#define FPGA_VERSION            (FPGA_VERSION_MAJOR | (FPGA_VERSION_MINOR << 8) | (FPGA_VERSION_PATCH << 16))

#define TIME_TAMER              TIME_TAMER_0_BASE
// Register offsets from the base
#define I2C                 BLADERF_OC_I2C_MASTER_0_BASE
#define OC_I2C_PRESCALER    0
#define OC_I2C_CTRL         2
#define OC_I2C_DATA         3
#define OC_I2C_CMD_STATUS   4

#define SI5338_I2C          (0xE0)
#define OC_I2C_ENABLE       (1<<7)
#define OC_I2C_STA          (1<<7)
#define OC_I2C_STO          (1<<6)
#define OC_I2C_WR           (1<<4)
#define OC_I2C_RD           (1<<5)
#define OC_I2C_TIP          (1<<1)
#define OC_I2C_RXACK        (1<<7)
#define OC_I2C_NACK         (1<<3)

#define DEFAULT_GAIN_CORRECTION 0x1000
#define GAIN_OFFSET 0
#define DEFAULT_PHASE_CORRECTION 0x0000
#define PHASE_OFFSET 16
#define DEFAULT_CORRECTION ( (DEFAULT_PHASE_CORRECTION << PHASE_OFFSET)|  (DEFAULT_GAIN_CORRECTION << GAIN_OFFSET))


void si5338_complete_transfer( uint8_t check_rxack ) {
    if( (IORD_8DIRECT(I2C, OC_I2C_CMD_STATUS)&OC_I2C_TIP) == 0 ) {
        while( (IORD_8DIRECT(I2C, OC_I2C_CMD_STATUS)&OC_I2C_TIP) == 0 ) { } ;
    }
    while( IORD_8DIRECT(I2C, OC_I2C_CMD_STATUS)&OC_I2C_TIP ) { } ;
    while( check_rxack && IORD_8DIRECT(I2C, OC_I2C_CMD_STATUS)&OC_I2C_RXACK ) { } ;
}

void si5338_read( uint8_t addr, uint8_t *data ) {

    // Set the address to the Si5338
    IOWR_8DIRECT(I2C, OC_I2C_DATA, SI5338_I2C ) ;
    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_STA | OC_I2C_WR ) ;
    si5338_complete_transfer( 1 ) ;

    IOWR_8DIRECT(I2C, OC_I2C_DATA, addr ) ;
    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_WR | OC_I2C_STO ) ;
    si5338_complete_transfer( 1 ) ;

    // Next transfer is a read operation, so '1' in the read/write bit
    IOWR_8DIRECT(I2C, OC_I2C_DATA, SI5338_I2C | 1 ) ;
    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_STA | OC_I2C_WR ) ;
    si5338_complete_transfer( 1 ) ;

    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_RD | OC_I2C_NACK | OC_I2C_STO ) ;
    si5338_complete_transfer( 0 ) ;

    *data = IORD_8DIRECT(I2C, OC_I2C_DATA) ;
    return ;
}

void si5338_write( uint8_t addr, uint8_t data ) {

    // Set the address to the Si5338
    IOWR_8DIRECT(I2C, OC_I2C_DATA, SI5338_I2C) ;
    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_STA | OC_I2C_WR ) ;
    si5338_complete_transfer( 1 ) ;

    IOWR_8DIRECT(I2C, OC_I2C_DATA, addr) ;
    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_CMD_STATUS | OC_I2C_WR ) ;
    si5338_complete_transfer( 1 ) ;

    IOWR_8DIRECT(I2C, OC_I2C_DATA, data ) ;
    IOWR_8DIRECT(I2C, OC_I2C_CMD_STATUS, OC_I2C_WR | OC_I2C_STO ) ;
    si5338_complete_transfer( 0 ) ;

    return ;
}

// Trim DAC write
void dac_write( uint16_t val ) {
//    alt_printf( "DAC Writing: %x\n", val ) ;
    uint8_t data[3] ;
    data[0] = 0x28, data[1] = 0, data[2] = 0 ;
    alt_avalon_spi_command( SPI_1_BASE, 0, 3, data, 0, 0, 0 ) ;
    data[0] = 0x08, data[1] = (val>>8)&0xff, data[2] = val&0xff  ;
    alt_avalon_spi_command( SPI_1_BASE, 0, 3, data, 0, 0, 0) ;
    return ;
}

// SPI Read
void lms_spi_read( uint8_t address, uint8_t *val )
{
    uint8_t rv ;
    if( address > 0x7f )
    {
//        alt_printf( "Invalid read address: %x\n", address ) ;
    } else {
        alt_avalon_spi_command( SPI_0_BASE, 0, 1, &address, 0, 0, ALT_AVALON_SPI_COMMAND_MERGE ) ;
        rv = alt_avalon_spi_command( SPI_0_BASE, 0, 0, 0, 1, val, 0 ) ;
        if( rv != 1 )
        {
//            alt_putstr( "SPI data read did not work :(\n") ;
        }
    }
    if( LMS_VERBOSE )
    {
//        alt_printf( "r-addr: %x data: %x\n", address, *val ) ;
    }
    return ;
}

// SPI Write
void lms_spi_write( uint8_t address, uint8_t val )
{
    if( LMS_VERBOSE )
    {
//        alt_printf( "w-addr: %x data: %x\n", address, val ) ;
    }
    /*if( address > 0x7f )
    {
        alt_printf( "Invalid write address: %x\n", address ) ;
    } else*/ {
        uint8_t data[2] = { address |= LMS_WRITE, val } ;
        alt_avalon_spi_command( SPI_0_BASE, 0, 2, data, 0, 0, 0 ) ;
    }
    return ;
}

// Entry point
int main()
{
  struct uart_pkt {
      unsigned char magic;
#define UART_PKT_MAGIC          'N'

      //  | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
      //  |  dir  |  dev  |     cnt       |
      unsigned char mode;
#define UART_PKT_MODE_CNT_MASK   0xF
#define UART_PKT_MODE_CNT_SHIFT  0

#define UART_PKT_MODE_DEV_MASK   0x30
#define UART_PKT_MODE_DEV_SHIFT  4
#define UART_PKT_DEV_GPIO        (0<<UART_PKT_MODE_DEV_SHIFT)
#define UART_PKT_DEV_LMS         (1<<UART_PKT_MODE_DEV_SHIFT)
#define UART_PKT_DEV_VCTCXO      (2<<UART_PKT_MODE_DEV_SHIFT)
#define UART_PKT_DEV_SI5338      (3<<UART_PKT_MODE_DEV_SHIFT)

#define UART_PKT_MODE_DIR_MASK   0xC0
#define UART_PKT_MODE_DIR_SHIFT  6
#define UART_PKT_MODE_DIR_READ   (2<<UART_PKT_MODE_DIR_SHIFT)
#define UART_PKT_MODE_DIR_WRITE  (1<<UART_PKT_MODE_DIR_SHIFT)
  };

  struct uart_cmd {
      unsigned char addr;
      unsigned char data;
  };

  // Set the prescaler for 400kHz with an 80MHz clock (prescaer = clock / (5*desired) - 1)
  IOWR_16DIRECT(I2C, OC_I2C_PRESCALER, 39 ) ;
  IOWR_8DIRECT(I2C, OC_I2C_CTRL, OC_I2C_ENABLE ) ;

  // Set the UART divisor to 14 to get 4000000bps UART (baud rate = clock/(divisor + 1))
  IOWR_ALTERA_AVALON_UART_DIVISOR(UART_0_BASE, 19) ;

  // Set the IQ Correction parameters to 0
  IOWR_ALTERA_AVALON_PIO_DATA(IQ_CORR_RX_PHASE_GAIN_BASE, DEFAULT_CORRECTION);
  IOWR_ALTERA_AVALON_PIO_DATA(IQ_CORR_TX_PHASE_GAIN_BASE, DEFAULT_CORRECTION);

  /* Event loop never exits. */
  {
      char state;
      enum {
          LOOKING_FOR_MAGIC,
          READING_MODE,
          READING_CMDS,
          EXECUTE_CMDS
      };

      unsigned short i, cnt;
      unsigned char mode;
      unsigned char buf[14];
      struct uart_cmd *cmd_ptr;
      uint16_t dacval;

      state = LOOKING_FOR_MAGIC;
      while(1)
      {
          // Check if anything is in the FSK UART
          if( IORD_ALTERA_AVALON_UART_STATUS(UART_0_BASE) & ALTERA_AVALON_UART_STATUS_RRDY_MSK )
          {
              uint8_t val ;

              val = IORD_ALTERA_AVALON_UART_RXDATA(UART_0_BASE) ;

              switch (state) {
              case LOOKING_FOR_MAGIC:
                  if (val == UART_PKT_MAGIC)
                      state = READING_MODE;
                  break;
              case READING_MODE:
                  mode = val;
                  if ((mode & UART_PKT_MODE_CNT_MASK) > 7) {
                      mode &= ~UART_PKT_MODE_CNT_MASK;
                      mode |= 7;
                  }
                  i = 0;
                  cnt = (mode & UART_PKT_MODE_CNT_MASK) * sizeof(struct uart_cmd);
                  state = READING_CMDS;
                  break;
              case READING_CMDS:
                  // cnt here means the number of bytes to read
                  buf[i++] = val;
                  if (!--cnt)
                      state = EXECUTE_CMDS;
                  break;
              default:
                  break;
              }

              void write_uart(unsigned char val) {
                  while (!(IORD_ALTERA_AVALON_UART_STATUS(UART_0_BASE) & ALTERA_AVALON_UART_STATUS_TRDY_MSK));
                  IOWR_ALTERA_AVALON_UART_TXDATA(UART_0_BASE,  val);
              }

              if (state == EXECUTE_CMDS) {
                  write_uart(UART_PKT_MAGIC);
                  write_uart(mode);
                  // cnt here means the number of commands
                  cnt = (mode & UART_PKT_MODE_CNT_MASK);
                  cmd_ptr = (struct uart_cmd *)buf;

                  if ((mode & UART_PKT_MODE_DEV_MASK) == UART_PKT_DEV_LMS) {
                      for (i = 0; i < cnt; i++) {
                          if ((mode & UART_PKT_MODE_DIR_MASK) == UART_PKT_MODE_DIR_READ) {
                              lms_spi_read(cmd_ptr->addr, &cmd_ptr->data);
                          } else if ((mode & UART_PKT_MODE_DIR_MASK) == UART_PKT_MODE_DIR_WRITE) {
                              lms_spi_write(cmd_ptr->addr, cmd_ptr->data);
                              cmd_ptr->data = 0;
                          } else {
                              cmd_ptr->addr = 0;
                              cmd_ptr->data = 0;
                          }
                          cmd_ptr++;
                      }
                  }
                  if ((mode & UART_PKT_MODE_DEV_MASK) == UART_PKT_DEV_SI5338) {
                      for (i = 0; i < cnt; i++) {
                          if ((mode & UART_PKT_MODE_DIR_MASK) == UART_PKT_MODE_DIR_READ) {
                              uint8_t tmpvar;
                              si5338_read(cmd_ptr->addr, &tmpvar);
                              cmd_ptr->data = tmpvar;
                          } else if ((mode & UART_PKT_MODE_DIR_MASK) == UART_PKT_MODE_DIR_WRITE) {
                              si5338_write(cmd_ptr->addr, cmd_ptr->data);
                              cmd_ptr->data = 0;
                          } else {
                              cmd_ptr->addr = 0;
                              cmd_ptr->data = 0;
                          }
                          cmd_ptr++;
                      }
                  }
                  if ((mode & UART_PKT_MODE_DEV_MASK) == UART_PKT_DEV_GPIO) {
                    uint32_t device;
                    for (i = 0; i < cnt; i++) {
                        switch(cmd_ptr->addr)
                        {
                            case 0:case 1:case 2: case 3:
                                device = PIO_0_BASE;break;
                            case 4: case 5: case 6: case 7:
                                device = IQ_CORR_RX_PHASE_GAIN_BASE;
                                cmd_ptr->addr -= 4;
                                break;
                            case 8: case 9: case 10: case 11:
                                device = IQ_CORR_TX_PHASE_GAIN_BASE;
                                cmd_ptr->addr -= 8;
                                break;
                            case 12: case 13: case 14: case 15:
                                device = FPGA_VERSION_ID;
                                cmd_ptr->addr -= 12;
                                break;

                            case 16: case 17: case 18: case 19:
                            case 20: case 21: case 22: case 23:
                            case 24: case 25: case 26: case 27:
                            case 28: case 29: case 30: case 31:
                                cmd_ptr->addr -= 16;
                                device = TIME_TAMER;
                                break;
                            default:
                                //error
                                device = PIO_0_BASE;
                        }

                        if ((mode & UART_PKT_MODE_DIR_MASK) == UART_PKT_MODE_DIR_READ) {
                            if (device == FPGA_VERSION_ID)
                            {
                                cmd_ptr->data = (FPGA_VERSION >> (cmd_ptr->addr * 8));
                            }
                            else if (device == TIME_TAMER)
                            {
                                cmd_ptr->data = IORD_8DIRECT(TIME_TAMER, cmd_ptr->addr);
                            }
                            else
                            {
                                cmd_ptr->data = (IORD_ALTERA_AVALON_PIO_DATA(device)) >> (cmd_ptr->addr * 8);
                            }
                        } else if ((mode & UART_PKT_MODE_DIR_MASK) == UART_PKT_MODE_DIR_WRITE) {
                            if (device == TIME_TAMER) {
                                IOWR_8DIRECT(TIME_TAMER, cmd_ptr->addr, 1) ;
                            } else {
                                uint32_t tmpvar;
                                tmpvar = IORD_ALTERA_AVALON_PIO_DATA(device);
                                tmpvar &= ~ (0xff << (8 * cmd_ptr->addr));
                                tmpvar |= cmd_ptr->data << (8 * cmd_ptr->addr);
                                IOWR_ALTERA_AVALON_PIO_DATA(device, tmpvar);
                                cmd_ptr->data = 0;
                            }
                        } else {
                            cmd_ptr->addr = 0;
                            cmd_ptr->data = 0;
                        }

                        cmd_ptr++;
                    }
                  }
                  if ((mode & UART_PKT_MODE_DEV_MASK) == UART_PKT_DEV_VCTCXO) {
                      if ((mode & UART_PKT_MODE_DIR_MASK) == UART_PKT_MODE_DIR_READ) {
                          cmd_ptr->data = dacval >> (cmd_ptr->addr * 8);
                      } else if ((mode & UART_PKT_MODE_DIR_MASK) == UART_PKT_MODE_DIR_WRITE) {
                          dacval &= ~ (0xff << (8 * cmd_ptr->addr));
                          dacval |= cmd_ptr->data << (8 * cmd_ptr->addr);
                          if (cmd_ptr->addr == 1) {
                              dac_write(dacval);
                          }
                          cmd_ptr->data = 0;
                      } else {
                          cmd_ptr->addr = 0;
                          cmd_ptr->data = 0;
                      }
                  }

                  cmd_ptr = (struct uart_cmd *)buf;
                  for (i = 0; i < cnt; i++) {
                      write_uart(cmd_ptr->addr);
                      write_uart(cmd_ptr->data);
                      cmd_ptr++;
                  }
                  for (i = 2 + cnt * sizeof(struct uart_cmd); (i % 16); i++) {
                      write_uart(0xff);
                  }
                  state = LOOKING_FOR_MAGIC;
              }
          }

      }
  }

  return 0;
}
