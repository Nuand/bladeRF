/*
 * "Small Hello World" example.
 *
 * This example prints 'Hello from Nios II' to the STDOUT stream. It runs on
 * the Nios II 'standard', 'full_featured', 'fast', and 'low_cost' example
 * designs. It requires a STDOUT  device in your system's hardware.
 *
 * The purpose of this example is to demonstrate the smallest possible Hello
 * World application, using the Nios II HAL library.  The memory footprint
 * of this hosted application is ~332 bytes by default using the standard
 * reference design.  For a more fully featured Hello World application
 * example, see the example titled "Hello World".
 *
 * The memory footprint of this example has been reduced by making the
 * following changes to the normal "Hello World" example.
 * Check in the Nios II Software Developers Manual for a more complete
 * description.
 *
 * In the SW Application project (small_hello_world):
 *
 *  - In the C/C++ Build page
 *
 *    - Set the Optimization Level to -Os
 *
 * In System Library project (small_hello_world_syslib):
 *  - In the C/C++ Build page
 *
 *    - Set the Optimization Level to -Os
 *
 *    - Define the preprocessor option ALT_NO_INSTRUCTION_EMULATION
 *      This removes software exception handling, which means that you cannot
 *      run code compiled for Nios II cpu with a hardware multiplier on a core
 *      without a the multiply unit. Check the Nios II Software Developers
 *      Manual for more details.
 *
 *  - In the System Library page:
 *    - Set Periodic system timer and Timestamp timer to none
 *      This prevents the automatic inclusion of the timer driver.
 *
 *    - Set Max file descriptors to 4
 *      This reduces the size of the file handle pool.
 *
 *    - Check Main function does not exit
 *    - Uncheck Clean exit (flush buffers)
 *      This removes the unneeded call to exit when main returns, since it
 *      won't.
 *
 *    - Check Don't use C++
 *      This builds without the C++ support code.
 *
 *    - Check Small C library
 *      This uses a reduced functionality C library, which lacks
 *      support for buffering, file IO, floating point and getch(), etc.
 *      Check the Nios II Software Developers Manual for a complete list.
 *
 *    - Check Reduced device drivers
 *      This uses reduced functionality drivers if they're available. For the
 *      standard design this means you get polled UART and JTAG UART drivers,
 *      no support for the LCD driver and you lose the ability to program
 *      CFI compliant flash devices.
 *
 *    - Check Access device drivers directly
 *      This bypasses the device file system to access device drivers directly.
 *      This eliminates the space required for the device file system services.
 *      It also provides a HAL version of libc services that access the drivers
 *      directly, further reducing space. Only a limited number of libc
 *      functions are available in this configuration.
 *
 *    - Use ALT versions of stdio routines:
 *
 *           Function                  Description
 *        ===============  =====================================
 *        alt_printf       Only supports %s, %x, and %c ( < 1 Kbyte)
 *        alt_putstr       Smaller overhead than puts with direct drivers
 *                         Note this function doesn't add a newline.
 *        alt_putchar      Smaller overhead than putchar with direct drivers
 *        alt_getchar      Smaller overhead than getchar with direct drivers
 *
 */

#include "sys/alt_stdio.h"
#include "system.h"
#include "altera_avalon_spi.h"
#include "altera_avalon_uart_regs.h"
#include "altera_avalon_jtag_uart_regs.h"
#include "priv/alt_busy_sleep.h"
#include "priv/alt_file.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "sys/alt_dev.h"

#define LMS_READ 	0
#define LMS_WRITE 	(1<<7)
#define LMS_VERBOSE 1
#define kHz(x) (x*1000)
#define MHz(x) (x*1000000)
#define GHz(x) (x*1000000000)


// Register offsets from the base
#define I2C 				OC_I2C_MASTER_0_BASE
#define OC_I2C_PRESCALER 	0
#define OC_I2C_CTRL 		2
#define OC_I2C_DATA 		3
#define OC_I2C_CMD_STATUS 	4

#define SI5338_I2C 			(0xE0)
#define OC_I2C_ENABLE 		(1<<7)
#define OC_I2C_STA 			(1<<7)
#define OC_I2C_STO 			(1<<6)
#define OC_I2C_WR  			(1<<4)
#define OC_I2C_RD 			(1<<5)
#define OC_I2C_TIP 			(1<<1)
#define OC_I2C_RXACK 		(1<<7)
#define OC_I2C_NACK 		(1<<3)

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
//	alt_printf( "DAC Writing: %x\n", val ) ;
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
//		alt_printf( "Invalid read address: %x\n", address ) ;
	} else {
		alt_avalon_spi_command( SPI_0_BASE, 0, 1, &address, 0, 0, ALT_AVALON_SPI_COMMAND_MERGE ) ;
		rv = alt_avalon_spi_command( SPI_0_BASE, 0, 0, 0, 1, val, 0 ) ;
		if( rv != 1 )
		{
//			alt_putstr( "SPI data read did not work :(\n") ;
		}
	}
	if( LMS_VERBOSE )
	{
//		alt_printf( "r-addr: %x data: %x\n", address, *val ) ;
	}
	return ;
}

// SPI Write
void lms_spi_write( uint8_t address, uint8_t val )
{
	if( LMS_VERBOSE )
	{
//		alt_printf( "w-addr: %x data: %x\n", address, val ) ;
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


const char msg[] = "bladeRF FSK example!\n" ;

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

  // Set the prescaler for 384kHz with a 38.4MHz clock
  IOWR_16DIRECT(I2C, OC_I2C_PRESCALER, 0x20 ) ;
  IOWR_8DIRECT(I2C, OC_I2C_CTRL, OC_I2C_ENABLE ) ;

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
