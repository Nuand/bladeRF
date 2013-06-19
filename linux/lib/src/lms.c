#include "libbladeRF.h"
#include "liblms.h"

#ifndef PRIu32
#define PRIu32 "lu"
#endif

#define lms_printf(...)

// LPF conversion table
const unsigned int uint_bandwidths[] = {
    MHz(28),
    MHz(20),
    MHz(14),
    MHz(12),
    MHz(10),
    kHz(8750),
    MHz(7),
    MHz(6),
    kHz(5500),
    MHz(5),
    kHz(3840),
    MHz(3),
    kHz(2750),
    kHz(2500),
    kHz(1750),
    kHz(1500)
} ;

// Frequency Range table
struct freq_range {
    uint32_t    low ;
    uint32_t    high ;
    uint8_t     value ;
} ;

const struct freq_range bands[] = {
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


const uint8_t lms_reg_dumpset[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0E, 0x0F,

    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,

    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,

    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,

    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,

    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,

    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C
} ;

// When enabling an LPF, we must select both the module and the filter bandwidth
void lms_lpf_enable( struct bladerf *dev, lms_module_t mod, lms_bw_t bw )
{
    uint8_t reg = (mod == RX) ? 0x54 : 0x34 ;
    uint8_t data ;
    // Check to see which bandwidth we have selected
    lms_spi_read( dev, reg, &data ) ;
    if( (lms_bw_t)(data&0x3c>>2) != bw )
    {
        data &= ~0x3c ;
        data |= (bw<<2) ;
        data |= (1<<1) ;
        lms_spi_write( dev, reg, data ) ;
    }
    // Check to see if we are bypassed
    lms_spi_read( dev, reg+1, &data ) ;
    if( data&(1<<6) )
    {
        data &= ~(1<<6) ;
        lms_spi_write( dev, reg+1, data ) ;
    }
    return ;
}

void lms_lpf_bypass( struct bladerf *dev, lms_module_t mod )
{
    uint8_t reg = (mod == RX) ? 0x55 : 0x35 ;
    uint8_t data ;
    lms_spi_read( dev, reg, &data ) ;
    data |= (1<<6) ;
    lms_spi_write( dev, reg, data ) ;
    return ;
}

// Disable the LPF for a specific module
void lms_lpf_disable( struct bladerf *dev, lms_module_t mod )
{
    uint8_t reg = (mod == RX) ? 0x54 : 0x34 ;
    lms_spi_write( dev, reg, 0x00 ) ;
    return ;
}

// Get the bandwidth for the selected module
lms_bw_t lms_get_bandwidth( struct bladerf *dev, lms_module_t mod )
{
    uint8_t data ;
    uint8_t reg = (mod == RX) ? 0x54 : 0x34 ;
    lms_spi_read( dev, reg, &data ) ;
    data &= 0x3c ;
    data >>= 2 ;
    return (lms_bw_t)data ;
}

lms_bw_t lms_uint2bw( unsigned int req )
{
    lms_bw_t ret ;
    if(      req <= kHz(1500) ) ret = BW_1p5MHz ;
    else if( req <= kHz(1750) ) ret = BW_1p75MHz ;
    else if( req <= kHz(2500) ) ret = BW_2p5MHz ;
    else if( req <= kHz(2750) ) ret = BW_2p75MHz ;
    else if( req <= MHz(3)    ) ret = BW_3MHz ;
    else if( req <= kHz(3840) ) ret = BW_3p84MHz ;
    else if( req <= MHz(5)    ) ret = BW_5MHz ;
    else if( req <= kHz(5500) ) ret = BW_5p5MHz ;
    else if( req <= MHz(6)    ) ret = BW_6MHz ;
    else if( req <= MHz(7)    ) ret = BW_7MHz ;
    else if( req <= kHz(8750) ) ret = BW_8p75MHz ;
    else if( req <= MHz(10)   ) ret = BW_10MHz ;
    else if( req <= MHz(12)   ) ret = BW_12MHz ;
    else if( req <= MHz(14)   ) ret = BW_14MHz ;
    else if( req <= MHz(20)   ) ret = BW_20MHz ;
    else                        ret = BW_28MHz ;
    return ret ;
}

unsigned int lms_bw2uint( lms_bw_t bw )
{
    /* Return the table entry */
    return uint_bandwidths[bw&0xf] ;
}

// Enable dithering on the module PLL
void lms_dither_enable( struct bladerf *dev, lms_module_t mod, uint8_t nbits )
{
    // Select the base address based on which PLL we are configuring
    uint8_t reg = (mod == RX) ? 0x24 : 0x14 ;
    uint8_t data ;

    // Read what we currently have in there
    lms_spi_read( dev, reg, &data ) ;

    // Enable dithering
    data |= (1<<7) ;

    // Clear out the number of bits from before
    data &= ~(7<<4) ;

    // Put in the number of bits to dither
    data |= ((nbits-1)&7) ;

    // Write it out
    lms_spi_write( dev, reg, data ) ;
    return ;
}

// Disable dithering on the module PLL
void lms_dither_disable( struct bladerf *dev, lms_module_t mod )
{
    uint8_t reg = (mod == RX) ? 0x24 : 0x14 ;
    uint8_t data ;
    lms_spi_read( dev, reg, &data ) ;
    data &= ~(1<<7) ;
    lms_spi_write( dev, reg, data ) ;
    return ;
}

// Soft reset of the LMS
void lms_soft_reset( struct bladerf *dev )
{
    lms_spi_write( dev, 0x05, 0x12 ) ;
    lms_spi_write( dev, 0x05, 0x32 ) ;
    return ;
}

// Set the gain on the LNA
void lms_lna_set_gain( struct bladerf *dev, lms_lna_gain_t gain )
{
    uint8_t data ;
    lms_spi_read( dev, 0x75, &data ) ;
    data &= ~(3<<6) ;
    data |= ((gain&3)<<6) ;
    lms_spi_write( dev, 0x75, data ) ;
    return ;
}

void lms_lna_get_gain( struct bladerf *dev, lms_lna_gain_t *gain )
{
    uint8_t data ;
    lms_spi_read( dev, 0x75, &data ) ;
    data >>= 6 ;
    data &= 3 ;
    *gain = (lms_lna_gain_t)data ;
    return ;
}

// Select which LNA to enable
void lms_lna_select( struct bladerf *dev, lms_lna_t lna )
{
    uint8_t data ;
    lms_spi_read( dev, 0x75, &data ) ;
    data &= ~(3<<4) ;
    data |= ((lna&3)<<4) ;
    lms_spi_write( dev, 0x75, data ) ;
    return ;
}

// Disable RXVGA1
void lms_rxvga1_disable(struct bladerf *dev )
{
    // Set bias current to 0
    lms_spi_write( dev, 0x7b, 0x03 ) ;
    return ;
}

// Enable RXVGA1
void lms_rxvga1_enable(struct bladerf *dev )
{
    // Set bias current to nominal
    lms_spi_write( dev, 0x7b, 0x33 ) ;
    return ;
}

// Set the RFB_TIA_RXFE mixer gain
void lms_rxvga1_set_gain(struct bladerf *dev, uint8_t gain)
{
    uint8_t data ;
    if( gain > 120 ) {
        fprintf( stderr, "%s: %d being clamped to 120\n", __FUNCTION__, gain ) ;
        gain = 120 ;
    }
    lms_spi_read( dev, 0x76, &data ) ;
    data &= ~(0x3f) ;
    data |= gain ;
    lms_spi_write( dev, 0x76, gain&0x3f ) ;
    return ;
}

// Get the RFB_TIA_RXFE mixer gain
void lms_rxvga1_get_gain(struct bladerf *dev, uint8_t *gain)
{
    uint8_t data ;
    lms_spi_read( dev, 0x76, &data ) ;
    *gain = data&0x3f ;
    return ;
}

// Disable RXVGA2
void lms_rxvga2_disable(struct bladerf *dev )
{
    uint8_t data ;
    lms_spi_read( dev, 0x64, &data ) ;
    data &= ~(1<<1) ;
    lms_spi_write( dev, 0x64, data ) ;
    return ;
}

// Set the gain on RXVGA2
void lms_rxvga2_set_gain( struct bladerf *dev, uint8_t gain )
{
    uint8_t data ;
    // NOTE: Gain is calculated as gain*3dB and shouldn't really
    // go above 30dB
    if( (gain&0x1f) > 10 )
    {
        lms_printf( "Setting gain above 30dB? You crazy!!\n" ) ;
    }
    lms_spi_read( dev, 0x65, &data ) ;
    data &= ~(0x1f) ;
    data |= gain ;
    lms_spi_write( dev, 0x65, data ) ;
    return ;
}

void lms_rxvga2_get_gain( struct bladerf *dev, uint8_t *gain )
{
    uint8_t data ;
    lms_spi_read( dev, 0x65, &data ) ;
    *gain = data&0x1f ;
    return ;
}

// Enable RXVGA2
void lms_rxvga2_enable( struct bladerf *dev, uint8_t gain )
{
    uint8_t data ;
    lms_spi_read( dev, 0x64, &data ) ;
    data |= (1<<1) ;
    lms_spi_write( dev, 0x64, data ) ;
    lms_rxvga2_set_gain( dev, gain ) ;
    return ;
}

// Enable PA (PA_ALL is NOT valid for enabling)
void lms_pa_enable( struct bladerf *dev, lms_pa_t pa )
{
    uint8_t data ;
    lms_spi_read( dev, 0x44, &data ) ;
    if( pa == PA_AUX )
    {
        data &= ~(1<<1) ;
    } else if( pa == PA_1 )
    {
        data &= ~(3<<3) ;
        data |= (1<<3) ;
    } else if( pa == PA_2 )
    {
        data &= ~(3<<3) ;
        data |= (2<<3) ;
    }
    lms_spi_write( dev, 0x44, data ) ;
    return ;
}

// Disable PA
void lms_pa_disable( struct bladerf *dev, lms_pa_t pa )
{
    uint8_t data ;
    lms_spi_read( dev, 0x44, &data ) ;
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
    lms_spi_write( dev, 0x44, data ) ;
    return ;
}

void lms_peakdetect_enable( struct bladerf *dev )
{
    uint8_t data ;
    lms_spi_read( dev, 0x44, &data ) ;
    data &= ~(1<<0) ;
    lms_spi_write( dev, 0x44, data ) ;
    return ;
}

void lms_peakdetect_disable( struct bladerf *dev )
{
    uint8_t data ;
    lms_spi_read( dev, 0x44, &data ) ;
    data |= (1<<0) ;
    lms_spi_write( dev, 0x44, data ) ;
    return ;
}

// Enable TX loopback
void lms_tx_loopback_enable( struct bladerf *dev, lms_txlb_t mode )
{
    uint8_t data ;
    switch(mode)
    {
        case TXLB_BB:
            lms_spi_read( dev, 0x46, &data ) ;
            data |= (3<<2) ;
            lms_spi_write( dev, 0x46, data ) ;
            break ;
        case TXLB_RF:
            // Disable all the PA's first
            lms_pa_disable( dev, PA_ALL ) ;
            // Connect up the switch
            lms_spi_read( dev, 0x0b, &data ) ;
            data |= (1<<0) ;
            lms_spi_write( dev, 0x0b, data ) ;
            // Enable the AUX PA only
            lms_pa_enable( dev, PA_AUX ) ;
            lms_peakdetect_enable( dev );
            // Make sure we're muxed over to the AUX mux
            lms_spi_read( dev, 0x45, &data ) ;
            data &= ~(7<<0) ;
            lms_spi_write( dev, 0x45, data ) ;
            break ;
    }
    return ;
}

void lms_txvga2_set_gain( struct bladerf *dev, uint8_t gain )
{
    uint8_t data ;
    if( gain > 25 ) {
        gain = 25 ;
    }
    lms_spi_read( dev, 0x45, &data ) ;
    data &= ~(0x1f<<3) ;
    data |= ((gain&0x1f)<<3) ;
    lms_spi_write( dev, 0x45, data ) ;
    return ;
}

void lms_txvga2_get_gain( struct bladerf *dev, uint8_t *gain )
{
    uint8_t data ;
    lms_spi_read( dev, 0x45, &data ) ;
    *gain = (data>>3)&0x1f ;
    return ;
}

void lms_txvga1_set_gain( struct bladerf *dev, int8_t gain )
{
    gain = (gain+35) ;
    /* Since 0x41 is only VGA1GAIN, we don't need to RMW */
    lms_spi_write( dev, 0x41, gain&0x1f ) ;
    return ;
}

void lms_txvga1_get_gain( struct bladerf *dev, int8_t *gain )
{
    uint8_t data ;
    lms_spi_read( dev, 0x41, &data ) ;
    data = data&0x1f ;
    *gain -= 35 ;
    *gain += data ;
    return ;
}

// Disable TX loopback
void lms_tx_loopback_disable( struct bladerf *dev, lms_txlb_t mode )
{
    uint8_t data ;
    switch(mode)
    {
        case TXLB_BB:
            lms_spi_read( dev, 0x46, &data ) ;
            data &= ~(3<<2) ;
            lms_spi_write( dev, 0x46, data ) ;
            break ;
        case TXLB_RF:
            // Disable the AUX PA
            lms_pa_disable( dev, PA_AUX ) ;
            // Disconnect the switch
            lms_spi_read( dev, 0x0b, &data ) ;
            data &= ~(1<<0) ;
            lms_spi_write( dev, 0x0b, data ) ;
            // Power up the LNA's
            lms_spi_write( dev, 0x70, 0 ) ;
            break ;
    }
    return ;
}

// Loopback enable
void lms_loopback_enable( struct bladerf *dev, lms_loopback_mode_t mode )
{
    uint8_t data ;
    switch(mode)
    {
        case LB_BB_LPF:
            // Disable RXVGA1 first
            lms_rxvga1_disable( dev ) ;

            // Enable BB TX and RX loopback
            lms_tx_loopback_enable( dev, TXLB_BB ) ;
            lms_spi_write( dev, 0x08, 1<<6 ) ;
            break ;

        case LB_BB_VGA2:
            // Disable RXLPF first
            lms_lpf_disable( dev, RX ) ;

            // Enable TX and RX loopback
            lms_tx_loopback_enable( dev, TXLB_BB ) ;
            lms_spi_write( dev, 0x08, 1<<5 ) ;
            break ;

        case LB_BB_OP:
            // Disable RXLPF, RXVGA2, and RXVGA1
            lms_rxvga1_disable( dev ) ;
            lms_rxvga2_disable( dev ) ;
            lms_lpf_disable( dev, RX ) ;

            // Enable TX and RX loopback
            lms_tx_loopback_enable( dev, TXLB_BB ) ;
            lms_spi_write( dev, 0x08, 1<<4 ) ;
            break ;

        case LB_RF_LNA1:
        case LB_RF_LNA2:
        case LB_RF_LNA3:
            // Disable all LNAs
            lms_lna_select( dev, LNA_NONE ) ;

            // Enable AUX PA, PD[0], and loopback
            lms_tx_loopback_enable( dev, TXLB_RF ) ;
            lms_spi_read( dev, 0x7d, &data ) ;
            data |= 1 ;
            lms_spi_write( dev, 0x7d, data ) ;

            // Choose the LNA (1 = LNA1, 2 = LNA2, 3 = LNA3)
            lms_spi_write( dev, 0x08, (mode - LB_RF_LNA_START) ) ;

            // Set magical decode test registers bit
            lms_spi_write( dev, 0x70, (1<<1) ) ;
            break ;

        case LB_RF_LNA_START:
        case LB_NONE:
            // Weird
            break ;
    }
    return ;
}

// Figure out what loopback mode we're in (if any at all!)
lms_loopback_mode_t lms_get_loopback_mode( struct bladerf *dev )
{
    uint8_t data ;
    lms_loopback_mode_t mode = LB_NONE ;
    lms_spi_read( dev, 0x08, &data ) ;
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
void lms_loopback_disable( struct bladerf *dev, lms_lna_t lna, lms_bw_t bw )
{
    // Read which type of loopback mode we were in
    lms_loopback_mode_t mode = lms_get_loopback_mode( dev ) ;

    // Disable all RX loopback modes
    lms_spi_write( dev, 0x08, 0 ) ;

    switch(mode)
    {
        case LB_BB_LPF:
            // Disable TX baseband loopback
            lms_tx_loopback_disable( dev, TXLB_BB ) ;
            // Enable RXVGA1
            lms_rxvga1_enable( dev ) ;
            break ;
        case LB_BB_VGA2:
            // Disable TX baseband loopback
            lms_tx_loopback_disable( dev, TXLB_BB ) ;
            // Enable RXLPF
            lms_lpf_enable( dev, RX, bw ) ;
            break ;
        case LB_BB_OP:
            // Disable TX baseband loopback
            lms_tx_loopback_disable( dev, TXLB_BB ) ;
            // Enable RXLPF, RXVGA1 and RXVGA2
            lms_lpf_enable( dev, RX, bw ) ;
            lms_rxvga2_enable( dev, 30/3 ) ;
            lms_rxvga1_enable( dev ) ;
            break ;
        case LB_RF_LNA1:
        case LB_RF_LNA2:
        case LB_RF_LNA3:
            // Disable TX RF loopback
            lms_tx_loopback_disable( dev, TXLB_RF ) ;
            // Enable selected LNA
            lms_lna_select( dev, lna ) ;
            break ;
        case LB_RF_LNA_START:
        case LB_NONE:
            // Weird
            break ;
    }
    return ;
}

// Top level power down of the LMS
void lms_power_down( struct bladerf *dev )
{
    uint8_t data ;
    lms_spi_read( dev, 0x05, &data ) ;
    data &= ~(1<<4) ;
    lms_spi_write( dev, 0x05, data ) ;
    return ;
}

// Enable the PLL of a module
void lms_pll_enable( struct bladerf *dev, lms_module_t mod )
{
    uint8_t reg = (mod == RX) ? 0x24 : 0x14 ;
    uint8_t data ;
    lms_spi_read( dev, reg, &data ) ;
    data |= (1<<3) ;
    lms_spi_write( dev, reg, data ) ;
    return ;
}

// Disable the PLL of a module
void lms_pll_disable( struct bladerf *dev, lms_module_t mod )
{
    uint8_t reg = (mod == RX) ? 0x24 : 0x14 ;
    uint8_t data ;
    lms_spi_read( dev, reg, &data ) ;
    data &= ~(1<<3) ;
    lms_spi_write( dev, reg, data ) ;
    return ;
}

// Enable the RX subsystem
void lms_rx_enable( struct bladerf *dev )
{
    uint8_t data ;
    lms_spi_read( dev, 0x05, &data ) ;
    data |= (1<<2) ;
    lms_spi_write( dev, 0x05, data ) ;
    return ;
}

// Disable the RX subsystem
void lms_rx_disable( struct bladerf *dev )
{
    uint8_t data ;
    lms_spi_read( dev, 0x05, &data ) ;
    data &= ~(1<<2) ;
    lms_spi_write( dev, 0x05, data ) ;
    return ;
}

// Enable the TX subsystem
void lms_tx_enable( struct bladerf *dev )
{
    uint8_t data ;
    lms_spi_read( dev, 0x05, &data ) ;
    data |= (1<<3) ;
    lms_spi_write( dev, 0x05, data ) ;
    return ;
}

// Disable the TX subsystem
void lms_tx_disable( struct bladerf *dev )
{
    uint8_t data ;
    lms_spi_read( dev, 0x05, &data ) ;
    data &= ~(1<<3) ;
    lms_spi_write( dev, 0x05, data ) ;
    return ;
}

// Print a frequency structure
void lms_print_frequency( struct lms_freq *f )
{
    lms_printf( "  x        : %d\n", f->x ) ;
    lms_printf( "  nint     : %d\n", f->nint ) ;
    lms_printf( "  nfrac    : %"PRIu32"\n", f->nfrac ) ;
    lms_printf( "  freqsel  : %x\n", f->freqsel ) ;
    lms_printf( "  reference: %"PRIu32"\n", f->reference ) ;
    lms_printf( "  freq     : %"PRIu32"\n", (uint32_t) ( ((uint64_t)((f->nint<<23) + f->nfrac)) * (f->reference/f->x) >>23) )  ;
}

// Get the frequency structure
void lms_get_frequency( struct bladerf *dev, lms_module_t mod, struct lms_freq *f ) {
    uint8_t base = (mod == RX) ? 0x20 : 0x10 ;
    uint8_t data ;
    lms_spi_read( dev, base+0, &data ) ;
    f->nint = ((uint16_t)data) << 1 ;
    lms_spi_read( dev, base+1, &data ) ;
    f->nint |= (data&0x80)>>7 ;
    f->nfrac = ((uint32_t)data&0x7f)<<16 ;
    lms_spi_read( dev, base+2, &data ) ;
    f->nfrac |= ((uint32_t)data)<<8 ;
    lms_spi_read( dev, base+3, &data) ;
    f->nfrac |= data ;
    lms_spi_read( dev, base+5, &data ) ;
    f->freqsel = (data>>2) ;
    f->x = 1 << ((f->freqsel&7)-3);
    f->reference = 38400000 ;
    return ;
}

// Set the frequency of a module
void lms_set_frequency( struct bladerf *dev, lms_module_t mod, uint32_t freq )
{
    // Select the base address based on which PLL we are configuring
    uint8_t base = (mod == RX) ? 0x20 : 0x10 ;
    uint32_t lfreq = freq ;
    uint8_t freqsel = bands[0].value ;
    uint16_t nint ;
    uint32_t nfrac ;
    struct lms_freq f ;
    uint8_t data ;
    uint32_t x;
    uint32_t reference = 38400000 ;
    uint64_t vcofreq ;
    uint32_t left ;


    // Turn on the DSMs
    lms_spi_read( dev, 0x09, &data ) ;
    data |= 0x05 ;
    lms_spi_write( dev, 0x09, data ) ;

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

    x = 1 << ((freqsel&7)-3);
    //nint = floor( 2^(freqsel(2:0)-3) * f_lo / f_ref)
    //nfrac = floor(2^23 * (((x*f_lo)/f_ref) -nint))
    {
        vcofreq = (uint64_t)freq*x ;

        nint = vcofreq/reference ;
        left = vcofreq - nint*reference ;
        nfrac = 0 ;
        {
            // Long division ...
            int i ;
            for( i = 0 ; i < 24 ; i++ ) {
                if( left >= reference ) {
                    left = left - reference ;
                    nfrac = (nfrac << 1) + 1 ;
                } else {
                    nfrac <<= 1 ;
                }
                left <<= 1 ;
            }
        }

        //        temp = (uint64_t)((uint64_t)x*(uint64_t)freq) ;
        //        nint = ((uint64_t)x*(uint64_t)freq)/(uint64_t)reference ;
        //        {
        //        	uint32_t left =
        //        }
        //        nfrac = (temp - (nint*reference))<<23 ;

    }
    //nfrac = (lfreq>>2) - (lfreq>>5) - (lfreq>>12) ;
    //nfrac <<= ((freqsel&7)-3) ;
    f.x = x ;
    f.nint = nint ;
    f.nfrac = nfrac ;
    f.freqsel = freqsel ;
    f.reference = reference ;
    lms_print_frequency( &f ) ;

    // Program freqsel, selout (rx only), nint and nfrac
    if( mod == RX )
    {
        lms_spi_write( dev, base+5, freqsel<<2 | (freq < 1500000000 ? 1 : 2 ) ) ;
    } else {
        //		lms_spi_write( dev, base+5, freqsel<<2 ) ;
        lms_spi_write( dev, base+5, freqsel<<2 | (freq < 1500000000 ? 1 : 2 ) ) ;
    }
    data = nint>>1 ;// lms_printf( "%x\n", data ) ;
    lms_spi_write( dev, base+0, data ) ;
    data = ((nint&1)<<7) | ((nfrac>>16)&0x7f) ;//  lms_printf( "%x\n", data ) ;
    lms_spi_write( dev, base+1, data ) ;
    data = ((nfrac>>8)&0xff) ;//  lms_printf( "%x\n", data ) ;
    lms_spi_write( dev, base+2, data ) ;
    data = (nfrac&0xff) ;//  lms_printf( "%x\n", data ) ;
    lms_spi_write( dev, base+3, data ) ;

    // Set the PLL Ichp, Iup and Idn currents
    lms_spi_read( dev, base+6, &data ) ;
    data &= ~(0x1f) ;
    data |= 0x0c ;
    lms_spi_write( dev, base+6, data ) ;
    lms_spi_read( dev, base+7, &data ) ;
    data &= ~(0x1f) ;
    data |= 3 ;
    data = 0xe3;
    lms_spi_write( dev, base+7, data ) ;
    lms_spi_read( dev, base+8, &data ) ;
    data &= ~(0x1f) ;
    lms_spi_write( dev, base+8, data ) ;

    // Loop through the VCOCAP to figure out optimal values
    lms_spi_read( dev, base+9, &data ) ;
    data &= ~(0x3f) ;
    {
        uint8_t i, vtune, low = 64, high = 0;
        for( i = 0 ; i < 64 ; i++ )
        {
            data &= ~(0x3f) ;
            data |= i ;
            lms_spi_write( dev, base+9, data ) ;
            lms_spi_read( dev, base+10, &vtune ) ;
            if( (vtune&0xc0) == 0xc0 )
            {
                lms_printf( "MESSED UP!!!!!\n" ) ;
            }
            if( vtune&0x80 )
            {
                //lms_printf( "Setting HIGH\n" ) ;
                high = i ;
            }
            if( (vtune&0x40) && low == 64 )
            {
                low = i ;
                break ;
            }
        }
        lms_printf( "LOW: %x HIGH: %x VCOCAP: %x\n", low, high, (low+high)>>1 ) ;
        data &= ~(0x3f) ;
        data |= ((low+high)>>1) ;
        lms_spi_write( dev, base+9, data ) ;
        lms_spi_write( dev, base+9, data ) ;
        lms_spi_read( dev, base+10, &vtune ) ;
        lms_printf( "VTUNE: %x\n", vtune&0xc0 ) ;
    }

    // Turn off the DSMs
    lms_spi_read( dev, 0x09, &data ) ;
    data &= ~(0x05) ;
    lms_spi_write( dev, 0x09, data ) ;

    return ;
}

void lms_dump_registers(struct bladerf *dev)
{
    uint8_t data,i;
    uint16_t num_reg = sizeof(lms_reg_dumpset);
    for (i = 0; i < num_reg; i++)
    {
        lms_spi_read( dev, lms_reg_dumpset[i], &data ) ;
        lms_printf( "addr: %x data: %x\n", lms_reg_dumpset[i], data ) ;
    }
}

void lms_calibrate_dc(struct bladerf *dev)
{
    // RX path
    lms_spi_write( dev, 0x09, 0x8c ) ; // CLK_EN[3]
    lms_spi_write( dev, 0x43, 0x08 ) ; // I filter
    lms_spi_write( dev, 0x43, 0x28 ) ; // Start Calibration
    lms_spi_write( dev, 0x43, 0x08 ) ; // Stop calibration

    lms_spi_write( dev, 0x43, 0x09 ) ; // Q Filter
    lms_spi_write( dev, 0x43, 0x29 ) ;
    lms_spi_write( dev, 0x43, 0x09 ) ;

    lms_spi_write( dev, 0x09, 0x84 ) ;

    lms_spi_write( dev, 0x09, 0x94 ) ; // CLK_EN[4]
    lms_spi_write( dev, 0x66, 0x00 ) ; // Enable comparators

    lms_spi_write( dev, 0x63, 0x08 ) ; // DC reference module
    lms_spi_write( dev, 0x63, 0x28 ) ;
    lms_spi_write( dev, 0x63, 0x08 ) ;

    lms_spi_write( dev, 0x63, 0x09 ) ;
    lms_spi_write( dev, 0x63, 0x29 ) ;
    lms_spi_write( dev, 0x63, 0x09 ) ;

    lms_spi_write( dev, 0x63, 0x0a ) ;
    lms_spi_write( dev, 0x63, 0x2a ) ;
    lms_spi_write( dev, 0x63, 0x0a ) ;

    lms_spi_write( dev, 0x63, 0x0b ) ;
    lms_spi_write( dev, 0x63, 0x2b ) ;
    lms_spi_write( dev, 0x63, 0x0b ) ;

    lms_spi_write( dev, 0x63, 0x0c ) ;
    lms_spi_write( dev, 0x63, 0x2c ) ;
    lms_spi_write( dev, 0x63, 0x0c ) ;

    lms_spi_write( dev, 0x66, 0x0a ) ;
    lms_spi_write( dev, 0x09, 0x84 ) ;

    // TX path
    lms_spi_write( dev, 0x57, 0x04 ) ;
    lms_spi_write( dev, 0x09, 0x42 ) ;

    lms_spi_write( dev, 0x33, 0x08 ) ;
    lms_spi_write( dev, 0x33, 0x28 ) ;
    lms_spi_write( dev, 0x33, 0x08 ) ;

    lms_spi_write( dev, 0x33, 0x09 ) ;
    lms_spi_write( dev, 0x33, 0x29 ) ;
    lms_spi_write( dev, 0x33, 0x09 ) ;

    lms_spi_write( dev, 0x57, 0x84 ) ;
    lms_spi_write( dev, 0x09, 0x81 ) ;

    lms_spi_write( dev, 0x42, 0x77 ) ;
    lms_spi_write( dev, 0x43, 0x7f ) ;

    return ;
}

void lms_lpf_init(struct bladerf *dev)
{
    lms_spi_write( dev, 0x06, 0x0d ) ;
    lms_spi_write( dev, 0x17, 0x43 ) ;
    lms_spi_write( dev, 0x27, 0x43 ) ;
    lms_spi_write( dev, 0x41, 0x1f ) ;
    lms_spi_write( dev, 0x44, 1<<3 ) ;
    lms_spi_write( dev, 0x45, 0x1f<<3 ) ;
    lms_spi_write( dev, 0x48, 0xc  ) ;
    lms_spi_write( dev, 0x49, 0xc ) ;
    lms_spi_write( dev, 0x57, 0x84 ) ;
    return ;
}


int lms_config_init(struct bladerf *dev, struct lms_xcvr_config *config)
{

    lms_soft_reset( dev ) ;
    lms_lpf_init( dev ) ;
    lms_tx_enable( dev ) ;
    lms_rx_enable( dev) ;

    lms_spi_write( dev, 0x48, 20 ) ;
    lms_spi_write( dev, 0x49, 20 ) ;

    lms_set_frequency( dev, RX,  config->rx_freq_hz ) ;
    lms_set_frequency( dev, TX,  config->tx_freq_hz ) ;

    lms_lna_select( dev, config->lna  ) ;
    lms_pa_enable( dev, config->pa ) ;

    if( config->loopback_mode == LB_NONE ) {
        lms_loopback_disable( dev, config->lna, config->tx_bw ) ;
    } else {
        lms_loopback_enable(dev, config->loopback_mode);
    }

    return 0;
}

