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

// Frequency selection structure
typedef struct {
	uint16_t nint ;
	uint32_t nfrac ;
	uint8_t freqsel ;
	uint32_t reference ;
} lms_freq_t ;

// Bandwidth selection
typedef enum {
	BW_28MHz,
	BW_20MHz,
	BW_12MHz,
	BW_14MHz,
	BW_10MHz,
	BW_8p75MHz,
	BW_7MHz,
	BW_6MHz,
	BW_5p5MHz,
	BW_5MHz,
	BW_3p84MHz,
	BW_3MHz,
	BW_2p75MHz,
	BW_2p5MHz,
	BW_1p75MHz,
	BW_1p5MHz,
} lms_bw_t ;

// Module selection for those which have both RX and TX constituents
typedef enum
{
	RX,
	TX
} lms_module_t ;

// Loopback options
typedef enum {
	LB_BB_LPF,
	LB_BB_VGA2,
	LB_BB_OP,
	LB_RF_LNA1,
	LB_RF_LNA2,
	LB_RF_LNA3,
	LB_NONE
} lms_loopback_mode_t ;

// LNA options
typedef enum {
	LNA_NONE,
	LNA_1,
	LNA_2,
	LNA_3
} lms_lna_t ;

// LNA gain options
typedef enum {
	LNA_UNKNOWN,
	LNA_BYPASS,
	LNA_MID,
	LNA_MAX
} lms_lna_gain_t ;

typedef enum {
	TXLB_BB,
	TXLB_RF
} lms_txlb_t ;

typedef enum {
	PA_AUX,
	PA_1,
	PA_2,
	PA_ALL
} lms_pa_t ;

// Frequency Range table
typedef struct {
	uint32_t low ;
	uint32_t high ;
	uint8_t value ;
} freq_range_t ;

const freq_range_t bands[] = {
	{ .low =  232500000, .high =  285625000, .value = 0x27 },
	{ .low =  285625000, .high =  336875000, .value = 0x2f },
	{ .low =  336875000, .high =  405000000, .value = 0x37 },
	{ .low =  405000000, .high =  465000000, .value = 0x3f },
	{ .low =  465000000, .high =  571250000, .value = 0x26 },
	{ .low =  571250000, .high =  673750000, .value = 0x2e },
	{ .low =  673750000, .high =  810000000, .value = 0x36 },
	{ .low =  810000000, .high =  930000000, .value = 0x3e },
	{ .low =  930000000, .high = 1142500000, .value = 0x25 },
	{ .low = 1142500000, .high = 1347500000, .value = 0x2d },
	{ .low = 1347500000, .high = 1620000000, .value = 0x35 },
	{ .low = 1620000000, .high = 1860000000, .value = 0x3d },
	{ .low = 1860000000u, .high = 2285000000u, .value = 0x24 },
	{ .low = 2285000000u, .high = 2695000000u, .value = 0x2c },
	{ .low = 2695000000u, .high = 3240000000u, .value = 0x34 },
	{ .low = 3240000000u, .high = 3720000000u, .value = 0x3c }
};

// Trim DAC write
void dac_write( uint16_t val ) {
	alt_printf( "DAC Writing: %x\n", val ) ;
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
		alt_printf( "Invalid read address: %x\n", address ) ;
	} else {
		alt_avalon_spi_command( SPI_0_BASE, 0, 1, &address, 0, 0, ALT_AVALON_SPI_COMMAND_MERGE ) ;
		rv = alt_avalon_spi_command( SPI_0_BASE, 0, 0, 0, 1, val, 0 ) ;
		if( rv != 1 )
		{
			alt_putstr( "SPI data read did not work :(\n") ;
		}
	}
	if( LMS_VERBOSE )
	{
		alt_printf( "r-addr: %x data: %x\n", address, *val ) ;
	}
	return ;
}

// SPI Write
void lms_spi_write( uint8_t address, uint8_t val )
{
	if( LMS_VERBOSE )
	{
		alt_printf( "w-addr: %x data: %x\n", address, val ) ;
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

// When enabling an LPF, we must select both the module and the filter bandwidth
void lms_lpf_enable( lms_module_t mod, lms_bw_t bw )
{
	uint8_t reg = (mod == RX) ? 0x54 : 0x34 ;
	uint8_t data ;
	// Check to see which bandwidth we have selected
	lms_spi_read( reg, &data ) ;
	if( (lms_bw_t)(data&0x3c>>2) != bw )
	{
		data &= ~0x3c ;
		data |= (bw<<2) ;
		data |= (1<<1) ;
		lms_spi_write( reg, data ) ;
	}
	// Check to see if we are bypassed
	lms_spi_read( reg+1, &data ) ;
	if( data&(1<<6) )
	{
		data &= ~(1<<6) ;
		lms_spi_write( reg+1, data ) ;
	}
	return ;
}

void lms_lpf_bypass( lms_module_t mod )
{
	uint8_t reg = (mod == RX) ? 0x55 : 0x35 ;
	uint8_t data ;
	lms_spi_read( reg, &data ) ;
	data |= (1<<6) ;
	lms_spi_write( reg, data ) ;
	return ;
}

// Disable the LPF for a specific module
void lms_lpf_disable( lms_module_t mod )
{
	uint8_t reg = (mod == RX) ? 0x54 : 0x34 ;
	lms_spi_write( reg, 0x00 ) ;
	return ;
}

// Get the bandwidth for the selected module
lms_bw_t lms_get_bandwidth( lms_module_t mod )
{
	uint8_t data ;
	uint8_t reg = (mod == RX) ? 0x54 : 0x34 ;
	lms_spi_read( reg, &data ) ;
	data &= 0x3c ;
	data >>= 2 ;
	return (lms_bw_t)data ;
}

// Enable dithering on the module PLL
void lms_dither_enable( lms_module_t mod, uint8_t nbits )
{
	// Select the base address based on which PLL we are configuring
	uint8_t reg = (mod == RX) ? 0x24 : 0x14 ;
	uint8_t data ;

	// Read what we currently have in there
	lms_spi_read( reg, &data ) ;

	// Enable dithering
	data |= (1<<7) ;

	// Clear out the number of bits from before
	data &= ~(7<<4) ;

	// Put in the number of bits to dither
	data |= ((nbits-1)&7) ;

	// Write it out
	lms_spi_write( reg, data ) ;
	return ;
}

// Disable dithering on the module PLL
void lms_dither_disable( lms_module_t mod )
{
	uint8_t reg = (mod == RX) ? 0x24 : 0x14 ;
	uint8_t data ;
	lms_spi_read( reg, &data ) ;
	data &= ~(1<<7) ;
	lms_spi_write( reg, data ) ;
	return ;
}

// Soft reset of the LMS
void lms_soft_reset( )
{
	lms_spi_write( 0x05, 0x12 ) ;
	lms_spi_write( 0x05, 0x32 ) ;
	return ;
}

// Set the gain on the LNA
void lms_lna_set_gain( lms_lna_gain_t gain )
{
	uint8_t data ;
	lms_spi_read( 0x75, &data ) ;
	data &= ~(3<<6) ;
	data |= ((gain&3)<<6) ;
	lms_spi_write( 0x75, data ) ;
	return ;
}

// Select which LNA to enable
void lms_lna_select( lms_lna_t lna )
{
	uint8_t data ;
	lms_spi_read( 0x75, &data ) ;
	data &= ~(3<<4) ;
	data |= ((lna&3)<<4) ;
	lms_spi_write( 0x75, data ) ;
	return ;
}

// Disable RXVGA1
void lms_rxvga1_disable()
{
	// Set bias current to 0
	lms_spi_write( 0x7b, 0x03 ) ;
	return ;
}

// Enable RXVGA1
void lms_rxvga1_enable()
{
	// Set bias current to nominal
	lms_spi_write( 0x7b, 0x33 ) ;
	return ;
}

// Disable RXVGA2
void lms_rxvga2_disable()
{
	uint8_t data ;
	lms_spi_read( 0x64, &data ) ;
	data &= ~(1<<1) ;
	lms_spi_write( 0x64, data ) ;
	return ;
}

// Set the gain on RXVGA2
void lms_rxvga2_set_gain( uint8_t gain )
{
	// NOTE: Gain is calculated as gain*3dB and shouldn't really
	// go above 30dB
	if( (gain&0x1f) > 10 )
	{
		alt_putstr( "Setting gain above 30dB? You crazy!!\n" ) ;
	}
	lms_spi_write( 0x65, (0x1f)&gain ) ;
	return ;
}

// Enable RXVGA2
void lms_rxvga2_enable( uint8_t gain )
{
	uint8_t data ;
	lms_spi_read( 0x64, &data ) ;
	data |= (1<<1) ;
	lms_spi_write( 0x64, data ) ;
	lms_rxvga2_set_gain( gain ) ;
	return ;
}

// Enable PA (PA_ALL is NOT valid for enabling)
void lms_pa_enable( lms_pa_t pa )
{
	uint8_t data ;
	lms_spi_read( 0x44, &data ) ;
	if( pa == PA_AUX )
	{
		data &= ~(1<<1) ;
	} else if( pa == PA_1 )
	{
		data &= ~(3<<2) ;
		data |= (2<<1) ;
	} else if( pa == PA_2 )
	{
		data &= ~(3<<2) ;
		data |= (4<<1) ;
	}
	lms_spi_write( 0x44, data ) ;
	return ;
}

// Disable PA
void lms_pa_disable( lms_pa_t pa )
{
	uint8_t data ;
	lms_spi_read( 0x44, &data ) ;
	if( pa == PA_ALL )
	{
		data |= (1<<1) ;
		data &= ~(4<<2) ;
		data &= ~(2<<2) ;
	} else if( pa == PA_AUX )
	{
		data |= (1<<1) ;
	} else if( pa == PA_1 )
	{
		data &= ~(4<<2) ;
	} else { // pa == PA_2
		data &= ~(2<<2) ;
	}
	lms_spi_write( 0x44, data ) ;
	return ;
}

void lms_peakdetect_enable( )
{
	uint8_t data ;
	lms_spi_read( 0x44, &data ) ;
	data &= ~(1<<0) ;
	lms_spi_write( 0x44, data ) ;
	return ;
}

void lms_peakdetect_disable( )
{
	uint8_t data ;
	lms_spi_read( 0x44, &data ) ;
	data |= (1<<0) ;
	lms_spi_write( 0x44, data ) ;
	return ;
}

// Enable TX loopback
void lms_tx_loopback_enable( lms_txlb_t mode )
{
	uint8_t data ;
	switch(mode)
	{
		case TXLB_BB:
			lms_spi_read( 0x46, &data ) ;
			data |= (3<<2) ;
			lms_spi_write( 0x46, data ) ;
			break ;
		case TXLB_RF:
			// Disable all the PA's first
			lms_pa_disable( PA_ALL ) ;
			// Connect up the switch
			lms_spi_read( 0x0b, &data ) ;
			data |= (1<<0) ;
			lms_spi_write( 0x0b, data ) ;
			// Enable the AUX PA only
			lms_pa_enable( PA_AUX ) ;
			lms_peakdetect_enable( );
			// Make sure we're muxed over to the AUX mux
			lms_spi_read( 0x45, &data ) ;
			data &= ~(7<<0) ;
			lms_spi_write( 0x45, data ) ;
			break ;
	}
	return ;
}

// Disable TX loopback
void lms_tx_loopback_disable( lms_txlb_t mode )
{
	uint8_t data ;
	switch(mode)
	{
		case TXLB_BB:
			lms_spi_read( 0x46, &data ) ;
			data &= ~(3<<2) ;
			lms_spi_write( 0x46, data ) ;
			break ;
		case TXLB_RF:
			// Disable the AUX PA
			lms_pa_disable( PA_AUX ) ;
			// Disconnect the switch
			lms_spi_read( 0x0b, &data ) ;
			data &= ~(1<<0) ;
			lms_spi_write( 0x0b, data ) ;
			break ;
	}
	return ;
}

// Loopback enable
void lms_loopback_enable( lms_loopback_mode_t mode )
{
	uint8_t data ;
	switch(mode)
	{
		case LB_BB_LPF:
			// Disable RXVGA1 first
			lms_rxvga1_disable() ;

			// Enable BB TX and RX loopback
			lms_tx_loopback_enable( TXLB_BB ) ;
			lms_spi_write( 0x08, 1<<6 ) ;
			break ;

		case LB_BB_VGA2:
			// Disable RXLPF first
			lms_lpf_disable( RX ) ;

			// Enable TX and RX loopback
			lms_tx_loopback_enable( TXLB_BB ) ;
			lms_spi_write( 0x08, 1<<5 ) ;
			break ;

		case LB_BB_OP:
			// Disable RXLPF, RXVGA2, and RXVGA1
			lms_rxvga1_disable() ;
			lms_rxvga2_disable() ;
			lms_lpf_disable( RX ) ;

			// Enable TX and RX loopback
			lms_tx_loopback_enable( TXLB_BB ) ;
			lms_spi_write( 0x08, 1<<4 ) ;
			break ;

		case LB_RF_LNA1:
			// Disable all LNAs
			lms_lna_select( LNA_NONE ) ;

			// Enable AUX PA, PD[0], and loopback
			lms_tx_loopback_enable( TXLB_RF ) ;
//			lms_lna_select( LNA_1 ) ;
			lms_spi_read( 0x7d, &data ) ;
			data |= 1 ;
			lms_spi_write( 0x7d, data ) ;
			lms_spi_write( 0x08, 1 ) ;
			break ;

		case LB_RF_LNA2:
			// Disable all LNAs
			lms_lna_select( LNA_NONE ) ;

			// Enable AUX PA, PD[0], and loopback
			lms_tx_loopback_enable( TXLB_RF ) ;
//			lms_lna_select( LNA_2 ) ;
			lms_spi_read( 0x7d, &data ) ;
			data |= 1 ;
			lms_spi_write( 0x7d, data ) ;
			lms_spi_write( 0x08, 2 ) ;
			break ;

		case LB_RF_LNA3:
			// Disable all LNAs
			lms_lna_select( LNA_NONE ) ;

			// Enable AUX PA, PD[0], and loopback
			lms_tx_loopback_enable( TXLB_RF ) ;
//			lms_lna_select( LNA_3 ) ;
			lms_spi_read( 0x7d, &data ) ;
			data |= 1 ;
			lms_spi_write( 0x7d, data ) ;
			lms_spi_write( 0x08, 3 ) ;
			break ;

		case LB_NONE:
			// Weird
			break ;
	}
	return ;
}

// Figure out what loopback mode we're in (if any at all!)
lms_loopback_mode_t lms_get_loopback_mode( )
{
	uint8_t data ;
	lms_loopback_mode_t mode = LB_NONE ;
	lms_spi_read( 0x08, &data ) ;
	if( data == 0 )
	{
		mode = LB_NONE ;
	} else if( data&(1<<6) )
	{
		mode = LB_BB_LPF ;
	} else if( data&(1<<5) )
	{
		mode = LB_BB_VGA2 ;
	} else if( data&(1<<4) )
	{
		mode = LB_BB_OP ;
	} else if( (data&0xf) == 1 )
	{
		mode = LB_RF_LNA1 ;
	} else if( (data&0xf) == 2 )
	{
		mode = LB_RF_LNA2 ;
	} else if( (data&0xf) == 3 )
	{
		mode = LB_RF_LNA3 ;
	}
	return mode ;
}

// Disable loopback mode - must choose which LNA to hook up and what bandwidth you want
void lms_loopback_disable( lms_lna_t lna, lms_bw_t bw )
{
	// Read which type of loopback mode we were in
	lms_loopback_mode_t mode = lms_get_loopback_mode() ;

	// Disable all RX loopback modes
	lms_spi_write( 0x08, 0 ) ;

	switch(mode)
	{
		case LB_BB_LPF:
			// Disable TX baseband loopback
			lms_tx_loopback_disable( TXLB_BB ) ;
			// Enable RXVGA1
			lms_rxvga1_enable() ;
			break ;
		case LB_BB_VGA2:
			// Disable TX baseband loopback
			lms_tx_loopback_disable( TXLB_BB ) ;
			// Enable RXLPF
			lms_lpf_enable( RX, bw ) ;
			break ;
		case LB_BB_OP:
			// Disable TX baseband loopback
			lms_tx_loopback_disable( TXLB_BB ) ;
			// Enable RXLPF, RXVGA1 and RXVGA2
			lms_lpf_enable( RX, bw ) ;
			lms_rxvga2_enable( 30/3 ) ;
			lms_rxvga1_enable() ;
			break ;
		case LB_RF_LNA1:
		case LB_RF_LNA2:
		case LB_RF_LNA3:
			// Disable TX RF loopback
			lms_tx_loopback_disable( TXLB_RF ) ;
			// Enable selected LNA
			lms_lna_select( lna ) ;
			break ;
		case LB_NONE:
			// Weird
			break ;
	}
	return ;
}

// Top level power down of the LMS
void lms_power_down( )
{
	uint8_t data ;
	lms_spi_read( 0x05, &data ) ;
	data &= ~(1<<4) ;
	lms_spi_write( 0x05, data ) ;
	return ;
}

// Enable the PLL of a module
void lms_pll_enable( lms_module_t mod )
{
	uint8_t reg = (mod == RX) ? 0x24 : 0x14 ;
	uint8_t data ;
	lms_spi_read( reg, &data ) ;
	data |= (1<<3) ;
	lms_spi_write( reg, data ) ;
	return ;
}

// Disable the PLL of a module
void lms_pll_disable( lms_module_t mod )
{
	uint8_t reg = (mod == RX) ? 0x24 : 0x14 ;
	uint8_t data ;
	lms_spi_read( reg, &data ) ;
	data &= ~(1<<3) ;
	lms_spi_write( reg, data ) ;
	return ;
}

// Enable the RX subsystem
void lms_rx_enable( )
{
	uint8_t data ;
	lms_spi_read( 0x05, &data ) ;
	data |= (1<<2) ;
	lms_spi_write( 0x05, data ) ;
	return ;
}

// Disable the RX subsystem
void lms_rx_disable( )
{
	uint8_t data ;
	lms_spi_read( 0x05, &data ) ;
	data &= ~(1<<2) ;
	lms_spi_write( 0x05, data ) ;
	return ;
}

// Enable the TX subsystem
void lms_tx_enable( )
{
	uint8_t data ;
	lms_spi_read( 0x05, &data ) ;
	data |= (1<<3) ;
	lms_spi_write( 0x05, data ) ;
	return ;
}

// Disable the TX subsystem
void lms_tx_disable( )
{
	uint8_t data ;
	lms_spi_read( 0x05, &data ) ;
	data &= ~(1<<3) ;
	lms_spi_write( 0x05, data ) ;
	return ;
}

// Print a frequency structure
void lms_print_frequency( lms_freq_t *f )
{
	alt_printf( "  nint     : %x\n", f->nint ) ;
	alt_printf( "  nfrac    : %x\n", f->nfrac ) ;
	alt_printf( "  freqsel  : %x\n", f->freqsel ) ;
}

// Get the frequency structure
void lms_get_frequency( lms_module_t mod, lms_freq_t *f ) {
	uint8_t base = (mod == RX) ? 0x20 : 0x10 ;
	uint8_t data ;
	lms_spi_read( base+0, &data ) ;
	f->nint = ((uint16_t)data) << 1 ;
	lms_spi_read( base+1, &data ) ;
	f->nint |= (data&0x80)>>7 ;
	f->nfrac = ((uint32_t)data&0x7f)<<16 ;
	lms_spi_read( base+2, &data ) ;
	f->nfrac |= ((uint32_t)data)<<8 ;
	lms_spi_read( base+3, &data) ;
	f->nfrac |= data ;
	lms_spi_read( base+5, &data ) ;
	f->freqsel = (data>>2) ;
	f->reference = 38400000 ;
	return ;
}

// Set the frequency of a module
void lms_set_frequency( lms_module_t mod, uint32_t freq )
{
	// Select the base address based on which PLL we are configuring
	uint8_t base = (mod == RX) ? 0x20 : 0x10 ;
	uint32_t lfreq = freq ;
	uint8_t freqsel = bands[0].value ;
	uint16_t nint ;
	uint32_t nfrac ;
	lms_freq_t f ;
	uint8_t data ;
	uint32_t reference = 38400000 ;

	// Figure out freqsel
	if( lfreq < bands[0].low )
	{
		// Too low
	} else if( lfreq > bands[15].high )
	{
		// Too high!
	} else
	{
		uint8_t i = 0 ;
		while( i < 16 )
		{
			if( (lfreq > bands[i].low) && (lfreq <= bands[i].high) )
			{
				freqsel = bands[i].value ;
				break ;
			}
			i++ ;
		}
	}

	// Calculate nint
	nint = 0 ;
	reference >>= ((freqsel&7)-3) ;
	while( lfreq > reference )
	{
		lfreq -= reference ;
		nint++ ;
	}

	// Turn on the DSMs
	lms_spi_read( 0x09, &data ) ;
	data |= 0x05 ;
	lms_spi_write( 0x09, data ) ;

	// Fractional portion left in freq
	nfrac = (lfreq>>2) - (lfreq>>5) - (lfreq>>12) ;
	nfrac <<= ((freqsel&7)-3) ;

	f.nint = nint ;
	f.nfrac = nfrac ;
	f.freqsel = freqsel ;
	lms_print_frequency( &f ) ;

	// Program freqsel, selout (rx only), nint and nfrac
	if( mod == RX )
	{
		lms_spi_write( base+5, freqsel<<2 | (freq < 1500000000 ? 1 : 2 ) ) ;
	} else {
//		lms_spi_write( base+5, freqsel<<2 ) ;
		lms_spi_write( base+5, freqsel<<2 | (freq < 1500000000 ? 1 : 2 ) ) ;
	}
	data = nint>>1 ;// alt_printf( "%x\n", data ) ;
	lms_spi_write( base+0, data ) ;
	data = ((nint&1)<<7) | ((nfrac>>16)&0x7f) ;//  alt_printf( "%x\n", data ) ;
	lms_spi_write( base+1, data ) ;
	data = ((nfrac>>8)&0xff) ;//  alt_printf( "%x\n", data ) ;
	lms_spi_write( base+2, data ) ;
	data = (nfrac&0xff) ;//  alt_printf( "%x\n", data ) ;
	lms_spi_write( base+3, data ) ;

	// Set the PLL Ichp, Iup and Idn currents
	lms_spi_read( base+6, &data ) ;
	data &= ~(0x1f) ;
	data |= 0x0c ;
	lms_spi_write( base+6, data ) ;
	lms_spi_read( base+7, &data ) ;
	data &= ~(0x1f) ;
	data |= 3 ;
	lms_spi_write( base+7, data ) ;
	lms_spi_read( base+8, &data ) ;
	data &= ~(0x1f) ;
	lms_spi_write( base+8, data ) ;

	// Loop through the VCOCAP to figure out optimal values
	lms_spi_read( base+9, &data ) ;
	data &= ~(0x3f) ;
	{
		uint8_t i, vtune, low = 64, high = 0;
		for( i = 0 ; i < 64 ; i++ )
		{
			data &= ~(0x3f) ;
			data |= i ;
			lms_spi_write( base+9, data ) ;
			lms_spi_read( base+10, &vtune ) ;
			if( (vtune&0xc0) == 0xc0 )
			{
				alt_putstr( "MESSED UP!!!!!\n" ) ;
			}
			if( vtune&0x80 )
			{
				alt_putstr( "Setting HIGH\n" ) ;
				high = i ;
			}
			if( (vtune&0x40) && low == 64 )
			{
				low = i ;
				break ;
			}
		}
		alt_printf( "LOW: %x HIGH: %x VCOCAP: %x\n", low, high, (low+high)>>1 ) ;
		data &= ~(0x3f) ;
		data |= ((low+high)>>1) ;
		lms_spi_write( base+9, data ) ;
		lms_spi_read( base+10, &vtune ) ;
		alt_printf( "VTUNE: %x\n", vtune&0xc0 ) ;
	}

	// Turn off the DSMs
//	lms_spi_read( 0x09, &data ) ;
//	data &= ~(0x05) ;
//	lms_spi_write( 0x09, data ) ;

	return ;
}

void lms_dump_registers(void)
{
	uint8_t data ;
	lms_spi_read( 0x02, &data ) ;
	lms_spi_read( 0x03, &data ) ;
	lms_spi_read( 0x05, &data ) ;
	lms_spi_read( 0x06, &data ) ;
	lms_spi_read( 0x07, &data ) ;
	lms_spi_read( 0x08, &data ) ;
	lms_spi_read( 0x09, &data ) ;
	lms_spi_read( 0x0A, &data ) ;
	lms_spi_read( 0x0B, &data ) ;
	lms_spi_read( 0x10, &data ) ;
	lms_spi_read( 0x11, &data ) ;
	lms_spi_read( 0x12, &data ) ;
	lms_spi_read( 0x13, &data ) ;
	lms_spi_read( 0x14, &data ) ;
	lms_spi_read( 0x15, &data ) ;
	lms_spi_read( 0x16, &data ) ;
	lms_spi_read( 0x17, &data ) ;
	lms_spi_read( 0x18, &data ) ;
	lms_spi_read( 0x19, &data ) ;
	lms_spi_read( 0x1A, &data ) ;
	lms_spi_read( 0x1B, &data ) ;
	lms_spi_read( 0x1C, &data ) ;
	lms_spi_read( 0x20, &data ) ;
	lms_spi_read( 0x21, &data ) ;
	lms_spi_read( 0x22, &data ) ;
	lms_spi_read( 0x23, &data ) ;
	lms_spi_read( 0x24, &data ) ;
	lms_spi_read( 0x25, &data ) ;
	lms_spi_read( 0x26, &data ) ;
	lms_spi_read( 0x27, &data ) ;
	lms_spi_read( 0x28, &data ) ;
	lms_spi_read( 0x29, &data ) ;
	lms_spi_read( 0x2A, &data ) ;
	lms_spi_read( 0x2B, &data ) ;
	lms_spi_read( 0x2C, &data ) ;
	lms_spi_read( 0x32, &data ) ;
	lms_spi_read( 0x33, &data ) ;
	lms_spi_read( 0x34, &data ) ;
	lms_spi_read( 0x35, &data ) ;
	lms_spi_read( 0x36, &data ) ;
	lms_spi_read( 0x40, &data ) ;
	lms_spi_read( 0x41, &data ) ;
	lms_spi_read( 0x42, &data ) ;
	lms_spi_read( 0x43, &data ) ;
	lms_spi_read( 0x44, &data ) ;
	lms_spi_read( 0x45, &data ) ;
	lms_spi_read( 0x46, &data ) ;
	lms_spi_read( 0x47, &data ) ;
	lms_spi_read( 0x48, &data ) ;
	lms_spi_read( 0x49, &data ) ;
	lms_spi_read( 0x4A, &data ) ;
	lms_spi_read( 0x4B, &data ) ;
	lms_spi_read( 0x4C, &data ) ;
	lms_spi_read( 0x4D, &data ) ;
	lms_spi_read( 0x52, &data ) ;
	lms_spi_read( 0x53, &data ) ;
	lms_spi_read( 0x54, &data ) ;
	lms_spi_read( 0x55, &data ) ;
	lms_spi_read( 0x56, &data ) ;
	lms_spi_read( 0x57, &data ) ;
	lms_spi_read( 0x58, &data ) ;
	lms_spi_read( 0x59, &data ) ;
	lms_spi_read( 0x5A, &data ) ;
	lms_spi_read( 0x5B, &data ) ;
	lms_spi_read( 0x5C, &data ) ;
	lms_spi_read( 0x5D, &data ) ;
	lms_spi_read( 0x5E, &data ) ;
	lms_spi_read( 0x5F, &data ) ;
	lms_spi_read( 0x62, &data ) ;
	lms_spi_read( 0x63, &data ) ;
	lms_spi_read( 0x64, &data ) ;
	lms_spi_read( 0x65, &data ) ;
	lms_spi_read( 0x66, &data ) ;
	lms_spi_read( 0x67, &data ) ;
	lms_spi_read( 0x68, &data ) ;
	lms_spi_read( 0x70, &data ) ;
	lms_spi_read( 0x71, &data ) ;
	lms_spi_read( 0x72, &data ) ;
	lms_spi_read( 0x73, &data ) ;
	lms_spi_read( 0x74, &data ) ;
	lms_spi_read( 0x75, &data ) ;
	lms_spi_read( 0x76, &data ) ;
	lms_spi_read( 0x77, &data ) ;
	lms_spi_read( 0x78, &data ) ;
	lms_spi_read( 0x79, &data ) ;
	lms_spi_read( 0x7A, &data ) ;
	lms_spi_read( 0x7B, &data ) ;
	lms_spi_read( 0x7C, &data ) ;
	lms_spi_read( 0x7D, &data ) ;
}

void lms_calibrate_dc(void)
{
	// RX path
	lms_spi_write( 0x09, 0x8c ) ; // CLK_EN[3]
	lms_spi_write( 0x43, 0x08 ) ; // I filter
	lms_spi_write( 0x43, 0x28 ) ; // Start Calibration
	lms_spi_write( 0x43, 0x08 ) ; // Stop calibration

	lms_spi_write( 0x43, 0x09 ) ; // Q Filter
	lms_spi_write( 0x43, 0x29 ) ;
	lms_spi_write( 0x43, 0x09 ) ;

	lms_spi_write( 0x09, 0x84 ) ;

	lms_spi_write( 0x09, 0x94 ) ; // CLK_EN[4]
	lms_spi_write( 0x66, 0x00 ) ; // Enable comparators

	lms_spi_write( 0x63, 0x08 ) ; // DC reference module
	lms_spi_write( 0x63, 0x28 ) ;
	lms_spi_write( 0x63, 0x08 ) ;

	lms_spi_write( 0x63, 0x09 ) ;
	lms_spi_write( 0x63, 0x29 ) ;
	lms_spi_write( 0x63, 0x09 ) ;

	lms_spi_write( 0x63, 0x0a ) ;
	lms_spi_write( 0x63, 0x2a ) ;
	lms_spi_write( 0x63, 0x0a ) ;

	lms_spi_write( 0x63, 0x0b ) ;
	lms_spi_write( 0x63, 0x2b ) ;
	lms_spi_write( 0x63, 0x0b ) ;

	lms_spi_write( 0x63, 0x0c ) ;
	lms_spi_write( 0x63, 0x2c ) ;
	lms_spi_write( 0x63, 0x0c ) ;

	lms_spi_write( 0x66, 0x0a ) ;
	lms_spi_write( 0x09, 0x84 ) ;

	// TX path
	lms_spi_write( 0x57, 0x04 ) ;
	lms_spi_write( 0x09, 0x42 ) ;

	lms_spi_write( 0x33, 0x08 ) ;
	lms_spi_write( 0x33, 0x28 ) ;
	lms_spi_write( 0x33, 0x08 ) ;

	lms_spi_write( 0x33, 0x09 ) ;
	lms_spi_write( 0x33, 0x29 ) ;
	lms_spi_write( 0x33, 0x09 ) ;

	lms_spi_write( 0x57, 0x84 ) ;
	lms_spi_write( 0x09, 0x81 ) ;

	lms_spi_write( 0x42, 0x77 ) ;
	lms_spi_write( 0x43, 0x7f ) ;

	return ;
}

void lms_lpf_init(void)
{
	lms_spi_write( 0x06, 0x0d ) ;
	lms_spi_write( 0x17, 0x43 ) ;
	lms_spi_write( 0x27, 0x43 ) ;
	lms_spi_write( 0x41, 0x1f ) ;
	lms_spi_write( 0x44, 1<<3 ) ;
	lms_spi_write( 0x45, 0x1f<<3 ) ;
	lms_spi_write( 0x48, 0x1f ) ;
	lms_spi_write( 0x49, 0x1f ) ;
	return ;
}

const char msg[] = "bladeRF FSK example!\n" ;

// Entry point
int main()
{
  alt_u32 rv ;
  alt_u8 i, flag;
  alt_u8 data ;
  lms_bw_t bw ;
  alt_putstr("bladeRF LMS6002D SPI Register Readback!\n");
  alt_putstr("---------------------------------------\n");

//  lms_freq_t f ;

//  lms_lpf_init();

//  lms_calibrate_dc( ) ;
//
//  lms_tx_disable() ;
//  lms_rx_disable() ;
//
//  lms_soft_reset() ;
//  lms_tx_enable() ;
//  lms_rx_enable() ;
//  lms_pa_disable( PA_ALL ) ;
//
//  lms_lpf_init( ) ;
//
//  lms_set_frequency( RX,  394000000 ) ;
//  lms_set_frequency( TX,  394000000 ) ;
//  lms_set_frequency( TX,  1536000000 ) ;
//  lms_set_frequency( TX,  1536000000 ) ;
//  lms_set_frequency( TX,  1536000000 ) ;
//
//  lms_rxvga2_set_gain( 30 ) ;

//  lms_lna_select( LNA_1 ) ;
//  lms_pa_enable( PA_1 ) ;
////
//  lms_loopback_enable( LB_RF_LNA1 ) ;
//  lms_pa_enable( PA_AUX ) ;
//  lms_power_down() ;
//  lms_get_frequency( RX, &f ) ;
//  lms_print_frequency( &f ) ;
//  lms_get_frequency( TX, &f ) ;
//  lms_print_frequency( &f ) ;
//
//  lms_dump_registers() ;
//  	lms_spi_write( 0x09, 1<<7 ) ;
//  lms_lpf_bypass( RX ) ;
//  lms_lpf_bypass( TX ) ;
//
//  lms_lpf_enable( RX, BW_28MHz ) ;
//  lms_lpf_enable( TX, BW_28MHz ) ;

////  // Config from thing
//  lms_spi_write( 0x82, 0x1F ) ;
//  lms_spi_write( 0x83, 0x08 ) ;
//  lms_spi_write( 0x85, 0x3E ) ;
//  lms_spi_write( 0x86, 0x0D ) ;
//  lms_spi_write( 0x87, 0x00 ) ;
//  lms_spi_write( 0x88, 0x01 ) ;
//  lms_spi_write( 0x89, 0xC5 ) ;
//  lms_spi_write( 0x8A, 0x00 ) ;
//  lms_spi_write( 0x8B, 0x09 ) ; // 0x09 for loopback
//  lms_spi_write( 0x90, 0x34 ) ;
//  lms_spi_write( 0x91, 0x15 ) ;
//  lms_spi_write( 0x92, 0x55 ) ;
//  lms_spi_write( 0x93, 0x55 ) ;
//  lms_spi_write( 0x94, 0x88 ) ;
//  lms_spi_write( 0x95, 0x99 ) ;
//  lms_spi_write( 0x96, 0x8C ) ;
//  lms_spi_write( 0x97, 0xE3 ) ;
//  lms_spi_write( 0x98, 0x40 ) ;
//  lms_spi_write( 0x99, 0x99 ) ;
//  lms_spi_write( 0x9A, 0x03 ) ;
//  lms_spi_write( 0x9B, 0x76 ) ;
//  lms_spi_write( 0x9C, 0x38 ) ;
//  lms_spi_write( 0xA0, 0x34 ) ;
//  lms_spi_write( 0xA1, 0x15 ) ;
//  lms_spi_write( 0xA2, 0x55 ) ;
//  lms_spi_write( 0xA3, 0x55 ) ;
//  lms_spi_write( 0xA4, 0x88 ) ;
//  lms_spi_write( 0xA5, 0x99 ) ;
//  lms_spi_write( 0xA6, 0x8C ) ;
//  lms_spi_write( 0xA7, 0xE3 ) ;
//  lms_spi_write( 0xA8, 0x40 ) ;
//  lms_spi_write( 0xA9, 0x9F ) ;
//  lms_spi_write( 0xAA, 0x03 ) ;
//  lms_spi_write( 0xAB, 0x76 ) ;
//  lms_spi_write( 0xAC, 0x38 ) ;
//  lms_spi_write( 0xB2, 0x1F ) ;
//  lms_spi_write( 0xB3, 0x08 ) ;
//  lms_spi_write( 0xB4, 0x02 ) ;
//  lms_spi_write( 0xB5, 0x0C ) ;
//  lms_spi_write( 0xB6, 0x30 ) ;
//  lms_spi_write( 0xC0, 0x02 ) ;
//  lms_spi_write( 0xC1, 0x15 ) ;
//  lms_spi_write( 0xC2, 0x80 ) ;
//  lms_spi_write( 0xC3, 0x80 ) ;
//  lms_spi_write( 0xC4, 0x03 ) ; // 0x03 for lb
//  lms_spi_write( 0xC5, 0x00 ) ; // 0x00 for lb
//  lms_spi_write( 0xC6, 0x00 ) ;
//  lms_spi_write( 0xC7, 0x40 ) ;
//  lms_spi_write( 0xC8, 0x0C ) ;
//  lms_spi_write( 0xC9, 0x0C ) ;
//  lms_spi_write( 0xCA, 0x18 ) ;
//  lms_spi_write( 0xCB, 0x50 ) ;
//  lms_spi_write( 0xCC, 0x07 ) ;
//  lms_spi_write( 0xCD, 0x00 ) ;
//  lms_spi_write( 0xD2, 0x1F ) ;
//  lms_spi_write( 0xD3, 0x08 ) ;
//  lms_spi_write( 0xD4, 0x12 ) ;
//  lms_spi_write( 0xD5, 0x0C ) ;
//  lms_spi_write( 0xD6, 0x30 ) ;
//  lms_spi_write( 0xD7, 0x94 ) ;
//  lms_spi_write( 0xD8, 0x00 ) ;
//  lms_spi_write( 0xD9, 0x09 ) ;
//  lms_spi_write( 0xDA, 0x20 ) ;
//  lms_spi_write( 0xDB, 0x00 ) ;
//  lms_spi_write( 0xDC, 0x00 ) ;
//  lms_spi_write( 0xDD, 0x00 ) ;
//  lms_spi_write( 0xDE, 0x00 ) ;
//  lms_spi_write( 0xDF, 0x1F ) ;
//  lms_spi_write( 0xE2, 0x1F ) ;
//  lms_spi_write( 0xE3, 0x08 ) ;
//  lms_spi_write( 0xE4, 0x32 ) ;
//  lms_spi_write( 0xE5, 0x03 ) ; // 0x01 !!
//  lms_spi_write( 0xE6, 0x00 ) ;
//  lms_spi_write( 0xE7, 0x00 ) ;
//  lms_spi_write( 0xE8, 0x01 ) ;
//  lms_spi_write( 0xF0, 0x03 ) ;
//  lms_spi_write( 0xF1, 0x80 ) ;
//  lms_spi_write( 0xF2, 0x80 ) ;
//  lms_spi_write( 0xF3, 0x00 ) ;
//  lms_spi_write( 0xF4, 0x00 ) ;
//  lms_spi_write( 0xF5, 0xD0 ) ; // 0xd0
//  lms_spi_write( 0xF6, 0x78 ) ;
//  lms_spi_write( 0xF7, 0x00 ) ;
//  lms_spi_write( 0xF8, 0x1C ) ;
//  lms_spi_write( 0xF9, 0x37 ) ;
//  lms_spi_write( 0xFA, 0x77 ) ;
//  lms_spi_write( 0xFB, 0x77 ) ;
//  lms_spi_write( 0xFC, 0x18 ) ;
//  lms_spi_write( 0xFD, 0x01 ) ;

  // 500MHz out of PA1
  lms_spi_write( 0X82, 0x1F ) ;
  lms_spi_write( 0X83, 0x08 ) ;
  lms_spi_write( 0X85, 0x3E ) ;
  lms_spi_write( 0X86, 0x0D ) ;
  lms_spi_write( 0X87, 0x00 ) ;
  lms_spi_write( 0X88, 0x00 ) ;
  lms_spi_write( 0X89, 0x45 ) ;
  lms_spi_write( 0X8A, 0x00 ) ;
  lms_spi_write( 0X8B, 0x08 ) ;
  lms_spi_write( 0X90, 0x34 ) ;
  lms_spi_write( 0X91, 0x15 ) ;
  lms_spi_write( 0X92, 0x55 ) ;
  lms_spi_write( 0X93, 0x55 ) ;
  lms_spi_write( 0X94, 0x88 ) ;
  lms_spi_write( 0X95, 0x99 ) ;
  lms_spi_write( 0X96, 0x8C ) ;
  lms_spi_write( 0X97, 0xE3 ) ;
  lms_spi_write( 0X98, 0x40 ) ;
  lms_spi_write( 0X99, 0x99 ) ;
  lms_spi_write( 0X9A, 0x03 ) ;
  lms_spi_write( 0X9B, 0x76 ) ;
  lms_spi_write( 0X9C, 0x38 ) ;
  lms_spi_write( 0XA0, 0x34 ) ;
  lms_spi_write( 0XA1, 0x15 ) ;
  lms_spi_write( 0XA2, 0x55 ) ;
  lms_spi_write( 0XA3, 0x55 ) ;
  lms_spi_write( 0XA4, 0x88 ) ;
  lms_spi_write( 0XA5, 0x99 ) ;
  lms_spi_write( 0XA6, 0x8C ) ;
  lms_spi_write( 0XA7, 0xE3 ) ;
  lms_spi_write( 0XA8, 0x40 ) ;
  lms_spi_write( 0XA9, 0x9F ) ;
  lms_spi_write( 0XAA, 0x03 ) ;
  lms_spi_write( 0XAB, 0x76 ) ;
  lms_spi_write( 0XAC, 0x38 ) ;
  lms_spi_write( 0XB2, 0x1F ) ;
  lms_spi_write( 0XB3, 0x08 ) ;
  lms_spi_write( 0XB4, 0x02 ) ;
  lms_spi_write( 0XB5, 0x0C ) ; // 0x0C for normal LPF
  lms_spi_write( 0XB6, 0x30 ) ;
  lms_spi_write( 0XC0, 0x02 ) ;
  lms_spi_write( 0XC1, 0x1F ) ;
  lms_spi_write( 0XC2, 0x80 ) ;
  lms_spi_write( 0XC3, 0x80 ) ;
  lms_spi_write( 0XC4, 0x0B ) ;
  lms_spi_write( 0XC5, 0xC8 ) ;
  lms_spi_write( 0XC6, 0x00 ) ;
  lms_spi_write( 0XC7, 0x40 ) ;
  lms_spi_write( 0XC8, 0x0C ) ;
  lms_spi_write( 0XC9, 0x0C ) ;
  lms_spi_write( 0XCA, 0x18 ) ;
  lms_spi_write( 0XCB, 0x50 ) ;
  lms_spi_write( 0XCC, 0x00 ) ;
  lms_spi_write( 0XCD, 0x00 ) ;
  lms_spi_write( 0XD2, 0x1F ) ;
  lms_spi_write( 0XD3, 0x08 ) ;
  lms_spi_write( 0XD4, 0x02 ) ;
  lms_spi_write( 0XD5, 0x0C ) ;
  lms_spi_write( 0XD6, 0x30 ) ;
  lms_spi_write( 0XD7, 0x94 ) ;
  lms_spi_write( 0XD8, 0x00 ) ;
  lms_spi_write( 0XD9, 0x09 ) ;
  lms_spi_write( 0XDA, 0x20 ) ;
  lms_spi_write( 0XDB, 0x00 ) ;
  lms_spi_write( 0XDC, 0x00 ) ;
  lms_spi_write( 0XDD, 0x00 ) ;
  lms_spi_write( 0XDE, 0x00 ) ;
  lms_spi_write( 0XDF, 0x1F ) ;
  lms_spi_write( 0XE2, 0x1F ) ;
  lms_spi_write( 0XE3, 0x08 ) ;
  lms_spi_write( 0XE4, 0x32 ) ;
  lms_spi_write( 0XE5, 0x01 ) ;
  lms_spi_write( 0XE6, 0x00 ) ;
  lms_spi_write( 0XE7, 0x00 ) ;
  lms_spi_write( 0XE8, 0x01 ) ;
  lms_spi_write( 0XF0, 0x01 ) ;
  lms_spi_write( 0XF1, 0x80 ) ;
  lms_spi_write( 0XF2, 0x80 ) ;
  lms_spi_write( 0XF3, 0x00 ) ;
  lms_spi_write( 0XF4, 0x00 ) ;
  lms_spi_write( 0XF5, 0x50 ) ;
  lms_spi_write( 0XF6, 0x02 ) ; // 0x78
  lms_spi_write( 0XF7, 0x00 ) ;
  lms_spi_write( 0XF8, 0x1C ) ;
  lms_spi_write( 0XF9, 0x37 ) ;
  lms_spi_write( 0XFA, 0x77 ) ;
  lms_spi_write( 0XFB, 0x77 ) ;
  lms_spi_write( 0XFC, 0x18 ) ;
  lms_spi_write( 0XFD, 0x00 ) ;


//  lms_set_frequency( RX,  284000000 ) ;
//  lms_set_frequency( TX,  284000000 ) ;
//
//  lms_dump_registers() ;



//    lms_set_frequency( RX,  500000000 ) ;
//    lms_set_frequency( TX,  500000000 ) ;

  {
	  uint16_t x = 0 ;
	  while(1) {
		  dac_write( x ) ;
		  alt_busy_sleep( 1000000 ) ;
		  x += 1024 ;
	  }
  }
  dac_write( 0xffff ) ;
//  while (1) {
//	  dac_write( 0 ) ;
 // }


  /* Event loop never exits. */
  {
	  uint32_t downcount = 1 ;
	  uint8_t letter = 'a' ;
	  while(1)
	  {
		  // Check if anything is in the JTAG UART
		  uint32_t reg = IORD_ALTERA_AVALON_JTAG_UART_DATA(JTAG_UART_0_BASE) ;
		  if( reg & ALTERA_AVALON_JTAG_UART_DATA_RVALID_MSK )
		  {
			  // Get value from JTAG UART
			  uint8_t letter = (uint8_t)(reg & ALTERA_AVALON_JTAG_UART_DATA_DATA_MSK) ;

			  // Write it out to the FSK UART
			  while( (IORD_ALTERA_AVALON_UART_STATUS(UART_0_BASE) & ALTERA_AVALON_UART_STATUS_TRDY_MSK) == 0 ) { ; }
			  IOWR_ALTERA_AVALON_UART_TXDATA(UART_0_BASE, letter) ;
		  }

		  // Check if anything is in the FSK UART
		  if( IORD_ALTERA_AVALON_UART_STATUS(UART_0_BASE) & ALTERA_AVALON_UART_STATUS_RRDY_MSK )
		  {
			  uint8_t val ;
			  val = IORD_ALTERA_AVALON_UART_RXDATA(UART_0_BASE) ;

			  // Write it out the JTAG UART
			  alt_printf( "%c", val ) ;
		  }

//		  {
//			  while( (IORD_ALTERA_AVALON_UART_STATUS(UART_0_BASE) & ALTERA_AVALON_UART_STATUS_TRDY_MSK) == 0 ) {
//				  uint8_t a ;
//				  a = 1 ;
//			  };
//			  IOWR_ALTERA_AVALON_UART_TXDATA(UART_0_BASE, letter) ;
//			  alt_printf( "Writing: '%c'\n", letter ) ;
//			  letter = letter + 1 ;
//			  if( letter > 'z' ) {
//				  letter = 'a' ;
//			  }
//		  }
	  }
  }

  alt_putstr( "-- End of main loop --\n" ) ;

  // It's a trap!!
  //dac_write( 0xffff ) ;
  //while (1) {
//	  dac_write( 0xffff ) ;
//  }

  return 0;
}
