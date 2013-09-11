#include <libbladeRF.h>
#include "lms.h"
#include "bladerf_priv.h"
#include "log.h"

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
};

// Frequency Range table
struct freq_range {
    uint32_t    low;
    uint32_t    high;
    uint8_t     value;
};

const struct freq_range bands[] = {
    { FIELD_INIT(.low, 232500000),   FIELD_INIT(.high, 285625000),   FIELD_INIT(.value, 0x27) },
    { FIELD_INIT(.low, 285625000),   FIELD_INIT(.high, 336875000),   FIELD_INIT(.value, 0x2f) },
    { FIELD_INIT(.low, 336875000),   FIELD_INIT(.high, 405000000),   FIELD_INIT(.value, 0x37) },
    { FIELD_INIT(.low, 405000000),   FIELD_INIT(.high, 465000000),   FIELD_INIT(.value, 0x3f) },
    { FIELD_INIT(.low, 465000000),   FIELD_INIT(.high, 571250000),   FIELD_INIT(.value, 0x26) },
    { FIELD_INIT(.low, 571250000),   FIELD_INIT(.high, 673750000),   FIELD_INIT(.value, 0x2e) },
    { FIELD_INIT(.low, 673750000),   FIELD_INIT(.high, 810000000),   FIELD_INIT(.value, 0x36) },
    { FIELD_INIT(.low, 810000000),   FIELD_INIT(.high, 930000000),   FIELD_INIT(.value, 0x3e) },
    { FIELD_INIT(.low, 930000000),   FIELD_INIT(.high, 1142500000),  FIELD_INIT(.value, 0x25) },
    { FIELD_INIT(.low, 1142500000),  FIELD_INIT(.high, 1347500000),  FIELD_INIT(.value, 0x2d) },
    { FIELD_INIT(.low, 1347500000),  FIELD_INIT(.high, 1620000000),  FIELD_INIT(.value, 0x35) },
    { FIELD_INIT(.low, 1620000000),  FIELD_INIT(.high, 1860000000),  FIELD_INIT(.value, 0x3d) },
    { FIELD_INIT(.low, 1860000000u), FIELD_INIT(.high, 2285000000u), FIELD_INIT(.value, 0x24) },
    { FIELD_INIT(.low, 2285000000u), FIELD_INIT(.high, 2695000000u), FIELD_INIT(.value, 0x2c) },
    { FIELD_INIT(.low, 2695000000u), FIELD_INIT(.high, 3240000000u), FIELD_INIT(.value, 0x34) },
    { FIELD_INIT(.low, 3240000000u), FIELD_INIT(.high, 3720000000u), FIELD_INIT(.value, 0x3c) }
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
};

// When enabling an LPF, we must select both the module and the filter bandwidth
void lms_lpf_enable(struct bladerf *dev, bladerf_module mod, lms_bw_t bw)
{
    uint8_t reg = (mod == BLADERF_MODULE_RX) ? 0x54 : 0x34;
    uint8_t data;
    // Check to see which bandwidth we have selected
    bladerf_lms_read(dev, reg, &data);
    data |= (1<<1);
    if ((lms_bw_t)(data&0x3c>>2) != bw)
    {
        data &= ~0x3c;
        data |= (bw<<2);
    }
    bladerf_lms_write(dev, reg, data);
    // Check to see if we are bypassed
    bladerf_lms_read(dev, reg+1, &data);
    if (data&(1<<6))
    {
        data &= ~(1<<6);
        bladerf_lms_write(dev, reg+1, data);
    }
    return;
}

void lms_lpf_get_mode(struct bladerf *dev, bladerf_module mod, bladerf_lpf_mode *mode)
{
    uint8_t reg = (mod == BLADERF_MODULE_RX) ? 0x54 : 0x34;
    uint8_t data;

    bladerf_lms_read(dev, reg, &data);
    if (!(data&(1<<6))) {
        *mode = BLADERF_LPF_DISABLED;
    } else {
        bladerf_lms_read(dev, reg+1, &data);
        if (data&(1<<6)) {
            *mode = BLADERF_LPF_BYPASSED;
        } else {
            *mode = BLADERF_LPF_NORMAL;
        }
    }
}

void lms_lpf_set_mode(struct bladerf *dev, bladerf_module mod, bladerf_lpf_mode mode)
{
    uint8_t reg = (mod == BLADERF_MODULE_RX) ? 0x54 : 0x34;
    uint8_t data_l, data_h;
    bladerf_lms_read(dev, reg,   &data_l);
    bladerf_lms_read(dev, reg+1, &data_h);
    if (mode == BLADERF_LPF_DISABLED) {
        data_l &= ~(1<<6);
    } else if (mode == BLADERF_LPF_BYPASSED) {
        data_l |= (1<<6);
        data_h |= (1<<6);
    } else {
        data_l |= (1<<6);
        data_h &= ~(1<<6);
    }
    bladerf_lms_write(dev, reg  , data_l);
    bladerf_lms_write(dev, reg+1, data_h);
}

// Get the bandwidth for the selected module
lms_bw_t lms_get_bandwidth(struct bladerf *dev, bladerf_module mod)
{
    uint8_t data;
    uint8_t reg = (mod == BLADERF_MODULE_RX) ? 0x54 : 0x34;
    bladerf_lms_read(dev, reg, &data);
    data &= 0x3c;
    data >>= 2;
    return (lms_bw_t)data;
}

lms_bw_t lms_uint2bw(unsigned int req)
{
    lms_bw_t ret;
    if (     req <= kHz(1500)) ret = BW_1p5MHz;
    else if (req <= kHz(1750)) ret = BW_1p75MHz;
    else if (req <= kHz(2500)) ret = BW_2p5MHz;
    else if (req <= kHz(2750)) ret = BW_2p75MHz;
    else if (req <= MHz(3)  ) ret = BW_3MHz;
    else if (req <= kHz(3840)) ret = BW_3p84MHz;
    else if (req <= MHz(5)  ) ret = BW_5MHz;
    else if (req <= kHz(5500)) ret = BW_5p5MHz;
    else if (req <= MHz(6)  ) ret = BW_6MHz;
    else if (req <= MHz(7)  ) ret = BW_7MHz;
    else if (req <= kHz(8750)) ret = BW_8p75MHz;
    else if (req <= MHz(10) ) ret = BW_10MHz;
    else if (req <= MHz(12) ) ret = BW_12MHz;
    else if (req <= MHz(14) ) ret = BW_14MHz;
    else if (req <= MHz(20) ) ret = BW_20MHz;
    else                        ret = BW_28MHz;
    return ret;
}

unsigned int lms_bw2uint(lms_bw_t bw)
{
    /* Return the table entry */
    return uint_bandwidths[bw&0xf];
}

// Enable dithering on the module PLL
void lms_dither_enable(struct bladerf *dev, bladerf_module mod, uint8_t nbits)
{
    // Select the base address based on which PLL we are configuring
    uint8_t reg = (mod == BLADERF_MODULE_RX) ? 0x24 : 0x14;
    uint8_t data;

    // Read what we currently have in there
    bladerf_lms_read(dev, reg, &data);

    // Enable dithering
    data |= (1<<7);

    // Clear out the number of bits from before
    data &= ~(7<<4);

    // Put in the number of bits to dither
    data |= ((nbits-1)&7);

    // Write it out
    bladerf_lms_write(dev, reg, data);
    return;
}

// Disable dithering on the module PLL
void lms_dither_disable(struct bladerf *dev, bladerf_module mod)
{
    uint8_t reg = (mod == BLADERF_MODULE_RX) ? 0x24 : 0x14;
    uint8_t data;
    bladerf_lms_read(dev, reg, &data);
    data &= ~(1<<7);
    bladerf_lms_write(dev, reg, data);
    return;
}

// Soft reset of the LMS
void lms_soft_reset(struct bladerf *dev)
{
    bladerf_lms_write(dev, 0x05, 0x12);
    bladerf_lms_write(dev, 0x05, 0x32);
    return;
}

// Set the gain on the LNA
void lms_lna_set_gain(struct bladerf *dev, bladerf_lna_gain gain)
{
    uint8_t data;
    bladerf_lms_read(dev, 0x75, &data);
    data &= ~(3<<6);
    data |= ((gain&3)<<6);
    bladerf_lms_write(dev, 0x75, data);
    return;
}

void lms_lna_get_gain(struct bladerf *dev, bladerf_lna_gain *gain)
{
    uint8_t data;
    bladerf_lms_read(dev, 0x75, &data);
    data >>= 6;
    data &= 3;
    *gain = (bladerf_lna_gain)data;
    return;
}

// Select which LNA to enable
void lms_lna_select(struct bladerf *dev, lms_lna_t lna)
{
    uint8_t data;
    bladerf_lms_read(dev, 0x75, &data);
    data &= ~(3<<4);
    data |= ((lna&3)<<4);
    bladerf_lms_write(dev, 0x75, data);
    return;
}

// Disable RXVGA1
void lms_rxvga1_disable(struct bladerf *dev)
{
    // Set bias current to 0
    bladerf_lms_write(dev, 0x7b, 0x03);
    return;
}

// Enable RXVGA1
void lms_rxvga1_enable(struct bladerf *dev)
{
    // Set bias current to nominal
    bladerf_lms_write(dev, 0x7b, 0x33);
    return;
}

// Set the RFB_TIA_RXFE mixer gain
void lms_rxvga1_set_gain(struct bladerf *dev, uint8_t gain)
{
    uint8_t data;
    if (gain > 120) {
        log_info("%s: %d being clamped to 120\n", __FUNCTION__, gain);
        gain = 120;
    }
    bladerf_lms_read(dev, 0x76, &data);
    data &= ~(0x7f);
    data |= gain;
    bladerf_lms_write(dev, 0x76, gain&0x7f);
    return;
}

// Get the RFB_TIA_RXFE mixer gain
void lms_rxvga1_get_gain(struct bladerf *dev, uint8_t *gain)
{
    uint8_t data;
    bladerf_lms_read(dev, 0x76, &data);
    *gain = data&0x7f;
    return;
}

// Disable RXVGA2
void lms_rxvga2_disable(struct bladerf *dev)
{
    uint8_t data;
    bladerf_lms_read(dev, 0x64, &data);
    data &= ~(1<<1);
    bladerf_lms_write(dev, 0x64, data);
    return;
}

// Set the gain on RXVGA2
void lms_rxvga2_set_gain(struct bladerf *dev, uint8_t gain)
{
    uint8_t data;
    // NOTE: Gain is calculated as gain*3dB and shouldn't really
    // go above 30dB
    if ((gain&0x1f) > 10)
    {
        log_warning("Setting gain above 30dB? You crazy!!\n");
    }
    bladerf_lms_read(dev, 0x65, &data);
    data &= ~(0x1f);
    data |= gain;
    bladerf_lms_write(dev, 0x65, data);
    return;
}

void lms_rxvga2_get_gain(struct bladerf *dev, uint8_t *gain)
{
    uint8_t data;
    bladerf_lms_read(dev, 0x65, &data);
    *gain = data&0x1f;
    return;
}

// Enable RXVGA2
void lms_rxvga2_enable(struct bladerf *dev, uint8_t gain)
{
    uint8_t data;
    bladerf_lms_read(dev, 0x64, &data);
    data |= (1<<1);
    bladerf_lms_write(dev, 0x64, data);
    lms_rxvga2_set_gain(dev, gain);
    return;
}

// Enable PA (PA_ALL is NOT valid for enabling)
void lms_pa_enable(struct bladerf *dev, lms_pa_t pa)
{
    uint8_t data;
    bladerf_lms_read(dev, 0x44, &data);
    if (pa == PA_AUX)
    {
        data &= ~(1<<1);
    } else if (pa == PA_1)
    {
        data &= ~(3<<3);
        data |= (1<<3);
    } else if (pa == PA_2)
    {
        data &= ~(3<<3);
        data |= (2<<3);
    }
    bladerf_lms_write(dev, 0x44, data);
    return;
}

// Disable PA
void lms_pa_disable(struct bladerf *dev, lms_pa_t pa)
{
    uint8_t data;
    bladerf_lms_read(dev, 0x44, &data);
    if (pa == PA_ALL)
    {
        data |= (1<<1);
        data &= ~(4<<2);
        data &= ~(2<<2);
    } else if (pa == PA_AUX)
    {
        data |= (1<<1);
    } else if (pa == PA_1)
    {
        data &= ~(4<<2);
    } else { // pa == PA_2
        data &= ~(2<<2);
    }
    bladerf_lms_write(dev, 0x44, data);
    return;
}

void lms_peakdetect_enable(struct bladerf *dev)
{
    uint8_t data;
    bladerf_lms_read(dev, 0x44, &data);
    data &= ~(1<<0);
    bladerf_lms_write(dev, 0x44, data);
    return;
}

void lms_peakdetect_disable(struct bladerf *dev)
{
    uint8_t data;
    bladerf_lms_read(dev, 0x44, &data);
    data |= (1<<0);
    bladerf_lms_write(dev, 0x44, data);
    return;
}

void lms_enable_rffe(struct bladerf *dev, bladerf_module module, bool enable)
{
    uint8_t data;
    uint8_t base = module == BLADERF_MODULE_TX ? 0x40 : 0x70 ;

    bladerf_lms_read(dev, base, &data);
    if (enable) {
        data |= (1<<1);
    } else {
        data &= ~(1<<1);
    }
    bladerf_lms_write(dev, base, data);
    return;
}

// Enable TX loopback
void lms_tx_loopback_enable(struct bladerf *dev, lms_txlb_t mode)
{
    uint8_t data;
    switch(mode)
    {
        case TXLB_BB:
            bladerf_lms_read(dev, 0x46, &data);
            data |= (3<<2);
            bladerf_lms_write(dev, 0x46, data);
            break;
        case TXLB_RF:
            // Disable all the PA's first
            lms_pa_disable(dev, PA_ALL);
            // Connect up the switch
            bladerf_lms_read(dev, 0x0b, &data);
            data |= (1<<0);
            bladerf_lms_write(dev, 0x0b, data);
            // Enable the AUX PA only
            lms_pa_enable(dev, PA_AUX);
            lms_peakdetect_enable(dev);
            // Make sure we're muxed over to the AUX mux
            bladerf_lms_read(dev, 0x45, &data);
            data &= ~(7<<0);
            bladerf_lms_write(dev, 0x45, data);
            break;
    }
    return;
}

void lms_txvga2_set_gain(struct bladerf *dev, uint8_t gain)
{
    uint8_t data;
    if (gain > 25) {
        gain = 25;
    }
    bladerf_lms_read(dev, 0x45, &data);
    data &= ~(0x1f<<3);
    data |= ((gain&0x1f)<<3);
    bladerf_lms_write(dev, 0x45, data);
    return;
}

void lms_txvga2_get_gain(struct bladerf *dev, uint8_t *gain)
{
    uint8_t data;
    bladerf_lms_read(dev, 0x45, &data);
    *gain = (data>>3)&0x1f;
    return;
}

void lms_txvga1_set_gain(struct bladerf *dev, int8_t gain)
{
    gain = (gain+35);
    /* Since 0x41 is only VGA1GAIN, we don't need to RMW */
    bladerf_lms_write(dev, 0x41, gain&0x1f);
    return;
}

void lms_txvga1_get_gain(struct bladerf *dev, int8_t *gain)
{
    uint8_t data;
    bladerf_lms_read(dev, 0x41, &data);
    data = data&0x1f;
    *gain -= 35;
    *gain += data;
    return;
}

// Disable TX loopback
void lms_tx_loopback_disable(struct bladerf *dev, lms_txlb_t mode)
{
    uint8_t data;
    switch(mode)
    {
        case TXLB_BB:
            bladerf_lms_read(dev, 0x46, &data);
            data &= ~(3<<2);
            bladerf_lms_write(dev, 0x46, data);
            break;
        case TXLB_RF:
            // Disable the AUX PA
            lms_pa_disable(dev, PA_AUX);
            // Disconnect the switch
            bladerf_lms_read(dev, 0x0b, &data);
            data &= ~(1<<0);
            bladerf_lms_write(dev, 0x0b, data);
            // Power up the LNA's
            bladerf_lms_write(dev, 0x70, 0);
            break;
    }
    return;
}

// Loopback enable
void lms_loopback_enable(struct bladerf *dev, bladerf_loopback mode)
{
    uint8_t data;
    switch(mode)
    {
        case BLADERF_LB_BB_LPF:
            // Disable RXVGA1 first
            lms_rxvga1_disable(dev);

            // Enable BB TX and RX loopback
            lms_tx_loopback_enable(dev, TXLB_BB);
            bladerf_lms_write(dev, 0x08, 1<<6);
            break;

        case BLADERF_LB_BB_VGA2:
            // Disable RXLPF first
            lms_lpf_set_mode(dev, BLADERF_MODULE_RX, BLADERF_LPF_DISABLED);

            // Enable TX and RX loopback
            lms_tx_loopback_enable(dev, TXLB_BB);
            bladerf_lms_write(dev, 0x08, 1<<5);
            break;

        case BLADERF_LB_BB_OP:
            // Disable RXLPF, RXVGA2, and RXVGA1
            lms_rxvga1_disable(dev);
            lms_rxvga2_disable(dev);
            lms_lpf_set_mode(dev, BLADERF_MODULE_RX, BLADERF_LPF_DISABLED);

            // Enable TX and RX loopback
            lms_tx_loopback_enable(dev, TXLB_BB);
            bladerf_lms_write(dev, 0x08, 1<<4);
            break;

        case BLADERF_LB_RF_LNA1:
        case BLADERF_LB_RF_LNA2:
        case BLADERF_LB_RF_LNA3:
            // Disable all LNAs
            lms_lna_select(dev, LNA_NONE);

            // Enable AUX PA, PD[0], and loopback
            lms_tx_loopback_enable(dev, TXLB_RF);
            bladerf_lms_read(dev, 0x7d, &data);
            data |= 1;
            bladerf_lms_write(dev, 0x7d, data);

            // Choose the LNA (1 = LNA1, 2 = LNA2, 3 = LNA3)
            bladerf_lms_write(dev, 0x08, (mode - BLADERF_LB_RF_LNA_START));

            // Set magical decode test registers bit
            bladerf_lms_write(dev, 0x70, (1<<1));
            break;

        case BLADERF_LB_RF_LNA_START:
        case BLADERF_LB_NONE:
            // Weird
            break;
    }
    return;
}

// Figure out what loopback mode we're in (if any at all!)
bladerf_loopback lms_get_loopback_mode(struct bladerf *dev)
{
    uint8_t data;
    bladerf_loopback mode = BLADERF_LB_NONE;
    bladerf_lms_read(dev, 0x08, &data);
    if (data == 0)
    {
        mode = BLADERF_LB_NONE;
    } else if (data&(1<<6))
    {
        mode = BLADERF_LB_BB_LPF;
    } else if (data&(1<<5))
    {
        mode = BLADERF_LB_BB_VGA2;
    } else if (data&(1<<4))
    {
        mode = BLADERF_LB_BB_OP;
    } else if ((data&0xf) == 1)
    {
        mode = BLADERF_LB_RF_LNA1;
    } else if ((data&0xf) == 2)
    {
        mode = BLADERF_LB_RF_LNA2;
    } else if ((data&0xf) == 3)
    {
        mode = BLADERF_LB_RF_LNA3;
    }
    return mode;
}

// Disable loopback mode - must choose which LNA to hook up and what bandwidth you want
void lms_loopback_disable(struct bladerf *dev, lms_lna_t lna, lms_bw_t bw)
{
    // Read which type of loopback mode we were in
    bladerf_loopback mode = lms_get_loopback_mode(dev);

    // Disable all RX loopback modes
    bladerf_lms_write(dev, 0x08, 0);

    switch(mode)
    {
        case BLADERF_LB_BB_LPF:
            // Disable TX baseband loopback
            lms_tx_loopback_disable(dev, TXLB_BB);
            // Enable RXVGA1
            lms_rxvga1_enable(dev);
            break;
        case BLADERF_LB_BB_VGA2:
            // Disable TX baseband loopback
            lms_tx_loopback_disable(dev, TXLB_BB);
            // Enable RXLPF
            lms_lpf_enable(dev, BLADERF_MODULE_RX, bw);
            break;
        case BLADERF_LB_BB_OP:
            // Disable TX baseband loopback
            lms_tx_loopback_disable(dev, TXLB_BB);
            // Enable RXLPF, RXVGA1 and RXVGA2
            lms_lpf_enable(dev, BLADERF_MODULE_RX, bw);
            lms_rxvga2_enable(dev, 30/3);
            lms_rxvga1_enable(dev);
            break;
        case BLADERF_LB_RF_LNA1:
        case BLADERF_LB_RF_LNA2:
        case BLADERF_LB_RF_LNA3:
            // Disable TX RF loopback
            lms_tx_loopback_disable(dev, TXLB_RF);
            // Enable selected LNA
            lms_lna_select(dev, lna);
            break;
        case BLADERF_LB_RF_LNA_START:
        case BLADERF_LB_NONE:
            // Weird
            break;
    }
    return;
}

// Top level power down of the LMS
void lms_power_down(struct bladerf *dev)
{
    uint8_t data;
    bladerf_lms_read(dev, 0x05, &data);
    data &= ~(1<<4);
    bladerf_lms_write(dev, 0x05, data);
    return;
}

// Enable the PLL of a module
void lms_pll_enable(struct bladerf *dev, bladerf_module mod)
{
    uint8_t reg = (mod == BLADERF_MODULE_RX) ? 0x24 : 0x14;
    uint8_t data;
    bladerf_lms_read(dev, reg, &data);
    data |= (1<<3);
    bladerf_lms_write(dev, reg, data);
    return;
}

// Disable the PLL of a module
void lms_pll_disable(struct bladerf *dev, bladerf_module mod)
{
    uint8_t reg = (mod == BLADERF_MODULE_RX) ? 0x24 : 0x14;
    uint8_t data;
    bladerf_lms_read(dev, reg, &data);
    data &= ~(1<<3);
    bladerf_lms_write(dev, reg, data);
    return;
}

// Enable the RX subsystem
void lms_rx_enable(struct bladerf *dev)
{
    uint8_t data;
    bladerf_lms_read(dev, 0x05, &data);
    data |= (1<<2);
    bladerf_lms_write(dev, 0x05, data);
    return;
}

// Disable the RX subsystem
void lms_rx_disable(struct bladerf *dev)
{
    uint8_t data;
    bladerf_lms_read(dev, 0x05, &data);
    data &= ~(1<<2);
    bladerf_lms_write(dev, 0x05, data);
    return;
}

// Enable the TX subsystem
void lms_tx_enable(struct bladerf *dev)
{
    uint8_t data;
    bladerf_lms_read(dev, 0x05, &data);
    data |= (1<<3);
    bladerf_lms_write(dev, 0x05, data);
    return;
}

// Disable the TX subsystem
void lms_tx_disable(struct bladerf *dev)
{
    uint8_t data;
    bladerf_lms_read(dev, 0x05, &data);
    data &= ~(1<<3);
    bladerf_lms_write(dev, 0x05, data);
    return;
}

// Converts frequency structure to Hz
uint32_t lms_frequency_to_hz(struct lms_freq *f)
{
    uint64_t pll_coeff;
    uint32_t div;

    pll_coeff = (((uint64_t)f->nint) << 23) + f->nfrac;
    div = (f->x << 23);

    return (uint32_t)(((f->reference * pll_coeff) + (div >> 1)) / div);
}

// Print a frequency structure
void lms_print_frequency(struct lms_freq *f)
{
    log_debug("  x        : %d\n", f->x);
    log_debug("  nint     : %d\n", f->nint);
    log_debug("  nfrac    : %u\n", f->nfrac);
    log_debug("  freqsel  : %x\n", f->freqsel);
    log_debug("  reference: %u\n", f->reference);
    log_debug("  freq     : %u\n", lms_frequency_to_hz(f));
}

// Get the frequency structure
void lms_get_frequency(struct bladerf *dev, bladerf_module mod, struct lms_freq *f) {
    uint8_t base = (mod == BLADERF_MODULE_RX) ? 0x20 : 0x10;
    uint8_t data;
    bladerf_lms_read(dev, base+0, &data);
    f->nint = ((uint16_t)data) << 1;
    bladerf_lms_read(dev, base+1, &data);
    f->nint |= (data&0x80)>>7;
    f->nfrac = ((uint32_t)data&0x7f)<<16;
    bladerf_lms_read(dev, base+2, &data);
    f->nfrac |= ((uint32_t)data)<<8;
    bladerf_lms_read(dev, base+3, &data);
    f->nfrac |= data;
    bladerf_lms_read(dev, base+5, &data);
    f->freqsel = (data>>2);
    f->x = 1 << ((f->freqsel&7)-3);
    f->reference = 38400000;
    return;
}

// Set the frequency of a module
void lms_set_frequency(struct bladerf *dev, bladerf_module mod, uint32_t freq)
{
    // Select the base address based on which PLL we are configuring
    uint8_t base = (mod == BLADERF_MODULE_RX) ? 0x20 : 0x10;
    uint32_t lfreq = freq;
    uint8_t freqsel = bands[0].value;
    uint16_t nint;
    uint32_t nfrac;
    struct lms_freq f;
    uint8_t data;
    uint64_t ref_clock = 38400000;
    uint64_t vco_x;

    // Turn on the DSMs
    bladerf_lms_read(dev, 0x09, &data);
    data |= 0x05;
    bladerf_lms_write(dev, 0x09, data);

    // Figure out freqsel
    if (lfreq < bands[0].low)
    {
        // Too low
    } else if (lfreq > bands[15].high)
    {
        // Too high!
    } else
    {
        uint8_t i = 0;
        while(i < 16)
        {
            if ((lfreq > bands[i].low) && (lfreq <= bands[i].high))
            {
                freqsel = bands[i].value;
                break;
            }
            i++;
        }
    }

    vco_x = 1 << ((freqsel & 7) - 3);
    nint = (vco_x * freq) / ref_clock;
    nfrac = ((1 << 23) * (vco_x * freq - nint * ref_clock)) / ref_clock;

    f.x = vco_x;
    f.nint = nint;
    f.nfrac = nfrac;
    f.freqsel = freqsel;
    f.reference = ref_clock;
    lms_print_frequency(&f);

    // Program freqsel, selout (rx only), nint and nfrac
    if (mod == BLADERF_MODULE_RX) {
        bladerf_lms_write(dev, base+5, freqsel<<2 | (freq < 1500000000 ? 1 : 2));
    } else {
        bladerf_lms_write(dev, base+5, freqsel<<2);
    }
    data = nint>>1;// dbg_printf("%x\n", data) ;
    bladerf_lms_write(dev, base+0, data);
    data = ((nint&1)<<7) | ((nfrac>>16)&0x7f);//  dbg_printf("%x\n", data) ;
    bladerf_lms_write(dev, base+1, data);
    data = ((nfrac>>8)&0xff);//  dbg_printf("%x\n", data) ;
    bladerf_lms_write(dev, base+2, data);
    data = (nfrac&0xff);//  dbg_printf("%x\n", data) ;
    bladerf_lms_write(dev, base+3, data);

    // Set the PLL Ichp, Iup and Idn currents
    bladerf_lms_read(dev, base+6, &data);
    data &= ~(0x1f);
    data |= 0x0c;
    bladerf_lms_write(dev, base+6, data);
    bladerf_lms_read(dev, base+7, &data);
    data &= ~(0x1f);
    data |= 3;
    data = 0xe3;
    bladerf_lms_write(dev, base+7, data);
    bladerf_lms_read(dev, base+8, &data);
    data &= ~(0x1f);
    bladerf_lms_write(dev, base+8, data);

    // Loop through the VCOCAP to figure out optimal values
    bladerf_lms_read(dev, base+9, &data);
    data &= ~(0x3f);
    {
#define VCO_HIGH 0x02
#define VCO_NORM 0x00
#define VCO_LOW 0x01

        int start_i = -1, stop_i = -1, avg_i;
        int state = VCO_HIGH;
        int i;
        uint8_t v;

        int vcocap;

        for (i=0; i<64; i++) {
            uint8_t v;

            bladerf_lms_write(dev, base + 9, i | 0x80);
            bladerf_lms_read(dev, base + 10, &v);

            vcocap = v >> 6;

            if (vcocap == VCO_HIGH) {
                continue;
            } else if (vcocap == VCO_LOW) {
                if (state == VCO_NORM) {
                    stop_i = i - 1;
                    state = VCO_LOW;
                }
            } else if (vcocap == VCO_NORM) {
                if (state == VCO_HIGH) {
                    start_i = i;
                    state = VCO_NORM;
                }
            } else {
                log_warning("Invalid VCOCAP\n");
            }
        }

        if (state == VCO_NORM)
            stop_i = 63;

        if ((start_i == -1) || (stop_i == -1))
            log_warning("Can't find VCOCAP value while tuning\n");

        avg_i = (start_i + stop_i) >> 1;

        bladerf_lms_write(dev, base + 9, avg_i | data);

        bladerf_lms_read(dev, base + 10, &v);
        log_debug("VTUNE: %x\n", v >> 6);
    }

    // Turn off the DSMs
    bladerf_lms_read(dev, 0x09, &data);
    data &= ~(0x05);
    bladerf_lms_write(dev, 0x09, data);

    return;
}

void lms_dump_registers(struct bladerf *dev)
{
    uint8_t data,i;
    uint16_t num_reg = sizeof(lms_reg_dumpset);
    for (i = 0; i < num_reg; i++)
    {
        bladerf_lms_read(dev, lms_reg_dumpset[i], &data);
        log_info("addr: %x data: %x\n", lms_reg_dumpset[i], data);
    }
}

/*
void lms_calibrate_dc(struct bladerf *dev)
{
    // RX path
    bladerf_lms_write(dev, 0x09, 0x8c); // CLK_EN[3]
    bladerf_lms_write(dev, 0x43, 0x08); // I filter
    bladerf_lms_write(dev, 0x43, 0x28); // Start Calibration
    bladerf_lms_write(dev, 0x43, 0x08); // Stop calibration

    bladerf_lms_write(dev, 0x43, 0x09); // Q Filter
    bladerf_lms_write(dev, 0x43, 0x29);
    bladerf_lms_write(dev, 0x43, 0x09);

    bladerf_lms_write(dev, 0x09, 0x84);

    bladerf_lms_write(dev, 0x09, 0x94); // CLK_EN[4]
    bladerf_lms_write(dev, 0x66, 0x00); // Enable comparators

    bladerf_lms_write(dev, 0x63, 0x08); // DC reference module
    bladerf_lms_write(dev, 0x63, 0x28);
    bladerf_lms_write(dev, 0x63, 0x08);

    bladerf_lms_write(dev, 0x63, 0x09);
    bladerf_lms_write(dev, 0x63, 0x29);
    bladerf_lms_write(dev, 0x63, 0x09);

    bladerf_lms_write(dev, 0x63, 0x0a);
    bladerf_lms_write(dev, 0x63, 0x2a);
    bladerf_lms_write(dev, 0x63, 0x0a);

    bladerf_lms_write(dev, 0x63, 0x0b);
    bladerf_lms_write(dev, 0x63, 0x2b);
    bladerf_lms_write(dev, 0x63, 0x0b);

    bladerf_lms_write(dev, 0x63, 0x0c);
    bladerf_lms_write(dev, 0x63, 0x2c);
    bladerf_lms_write(dev, 0x63, 0x0c);

    bladerf_lms_write(dev, 0x66, 0x0a);
    bladerf_lms_write(dev, 0x09, 0x84);

    // TX path
    bladerf_lms_write(dev, 0x57, 0x04);
    bladerf_lms_write(dev, 0x09, 0x42);

    bladerf_lms_write(dev, 0x33, 0x08);
    bladerf_lms_write(dev, 0x33, 0x28);
    bladerf_lms_write(dev, 0x33, 0x08);

    bladerf_lms_write(dev, 0x33, 0x09);
    bladerf_lms_write(dev, 0x33, 0x29);
    bladerf_lms_write(dev, 0x33, 0x09);

    bladerf_lms_write(dev, 0x57, 0x84);
    bladerf_lms_write(dev, 0x09, 0x81);

    bladerf_lms_write(dev, 0x42, 0x77);
    bladerf_lms_write(dev, 0x43, 0x7f);

    return;
}
*/

void lms_lpf_init(struct bladerf *dev)
{
    bladerf_lms_write(dev, 0x06, 0x0d);
    bladerf_lms_write(dev, 0x17, 0x43);
    bladerf_lms_write(dev, 0x27, 0x43);
    bladerf_lms_write(dev, 0x41, 0x1f);
    bladerf_lms_write(dev, 0x44, 1<<3);
    bladerf_lms_write(dev, 0x45, 0x1f<<3);
    bladerf_lms_write(dev, 0x48, 0xc);
    bladerf_lms_write(dev, 0x49, 0xc);
    bladerf_lms_write(dev, 0x57, 0x84);
    return;
}


int lms_config_init(struct bladerf *dev, struct lms_xcvr_config *config)
{

    lms_soft_reset(dev);
    lms_lpf_init(dev);
    lms_tx_enable(dev);
    lms_rx_enable(dev);

    bladerf_lms_write(dev, 0x48, 20);
    bladerf_lms_write(dev, 0x49, 20);

    lms_set_frequency(dev, BLADERF_MODULE_RX, config->rx_freq_hz);
    lms_set_frequency(dev, BLADERF_MODULE_TX, config->tx_freq_hz);

    lms_lna_select(dev, config->lna);
    lms_pa_enable(dev, config->pa);

    if (config->loopback_mode == BLADERF_LB_NONE) {
        lms_loopback_disable(dev, config->lna, config->tx_bw);
    } else {
        lms_loopback_enable(dev, config->loopback_mode);
    }

    return 0;
}

#define LMS_MAX_CAL_COUNT   25

static int lms_dc_cal_loop(struct bladerf *dev, uint8_t base, uint8_t cal_address, uint8_t *dc_regval)
{
    /* Reference LMS6002D calibration guide, section 4.1 flow chart */
    uint8_t i, val, control;
    bool done = false;

    log_debug( "Calibrating module %2.2x:%2.2x\n", base, cal_address );

    /* Set the calibration address for the block, and start it up */
    bladerf_lms_read(dev, base+0x03, &val);
    val &= ~(0x07);
    val |= cal_address&0x07;
    bladerf_lms_write(dev, base+0x03, val);

    /* Zeroize the DC countval - this is here because sometimes the loop doesn't
     * always assert DC_CLBR_DONE, so this is an attempt to force that to happen */
    /*bladerf_lms_write(dev, base+0x02, 0);
    val |= (1<<4);
    bladerf_lms_write(dev, base+0x03, val);
    val &= ~(1<<4);
    bladerf_lms_write(dev, base+0x03, val);*/

//    /* Start the calibration by toggling DC_START_CLBR */
    val |= (1<<5);
    bladerf_lms_write(dev, base+0x03, val);
    val &= ~(1<<5);
    bladerf_lms_write(dev, base+0x03, val);

    control = val;

    /* Main loop checking the calibration */
    for (i = 0 ; i < LMS_MAX_CAL_COUNT ; i++) {
        /* Start the calibration by toggling DC_START_CLBR */
//        control |= (1<<5);
//        bladerf_lms_write(dev, base+0x03, control);
//        control &= ~(1<<5);
//        bladerf_lms_write(dev, base+0x03, control);

        /* Read active low DC_CLBR_DONE */
        bladerf_lms_read(dev, base+0x01, &val);

        if( ((val>>1)&1) == 0 ) {
            /* We think we're done, but we need to check DC_LOCK */
            if (((val>>2)&7) != 0 && ((val>>2)&7) != 7) {
                log_debug( "Converged in %d iterations for %2x:%2x\n", i+1, base, cal_address );
                done = true;
                break ;
            } else {
                log_debug( "DC_CLBR_DONE but no DC_LOCK - rekicking\n" );
                control |= (1<<5);
                bladerf_lms_write(dev, base+0x03, control);
                control &= ~(1<<5);
                bladerf_lms_write(dev, base+0x03, control);
            }
        }
    }

    if (done == false) {
        log_warning( "Never converged - DC_CLBR_DONE: %d DC_LOCK: %d\n", (val>>1)&1, (val>>2)&7 );
    }

    /* See what the DC register value is and return it to the caller */
    bladerf_lms_read(dev, base, dc_regval);
    *dc_regval &= 0x3f;
    log_debug( "DC_REGVAL: %d\n", *dc_regval );

    /* TODO: If it didn't converge, return back some error */
    return 0;
}

int lms_calibrate_dc(struct bladerf *dev, bladerf_cal_module module)
{
    /* Working variables */
    uint8_t cal_clock, base, addrs, i, val, dc_regval;

    /* Saved values that are to be restored */
    uint8_t clockenables, reg0x71, reg0x7c;
    bladerf_lna_gain lna_gain;
    int rxvga1, rxvga2;

    /* Save off the top level clock enables */
    bladerf_lms_read(dev, 0x09, &clockenables);

    val = clockenables;
    cal_clock = 0 ;
    switch (module) {
        case BLADERF_DC_CAL_LPF_TUNING:
            cal_clock = (1<<5);  /* CLK_EN[5] - LPF CAL Clock */
            base = 0x00;
            addrs = 1;
            break;

        case BLADERF_DC_CAL_TX_LPF:
            cal_clock = (1<<1);  /* CLK_EN[1] - TX LPF DCCAL Clock */
            base = 0x30;
            addrs = 2;
            break;

        case BLADERF_DC_CAL_RX_LPF:
            cal_clock = (1<<3);  /* CLK_EN[3] - RX LPF DCCAL Clock */
            base = 0x50;
            addrs = 2;
            break;

        case BLADERF_DC_CAL_RXVGA2:
            cal_clock = (1<<4);  /* CLK_EN[4] - RX VGA2 DCCAL Clock */
            base = 0x60;
            addrs = 5;
            break;

        default:
            return BLADERF_ERR_INVAL;
    }

    /* Enable the appropriate clock based on the module */
    bladerf_lms_write(dev, 0x09, clockenables | cal_clock);

    /* Special case for RX LPF or RX VGA2 */
    if (module == BLADERF_DC_CAL_RX_LPF || module == BLADERF_DC_CAL_RXVGA2) {
        /* Connect LNA to the external pads and interally terminate */
        bladerf_lms_read(dev, 0x71, &reg0x71);
        val = reg0x71;
        val &= ~(1<<7);
        bladerf_lms_write(dev, 0x71, val);

        bladerf_lms_read(dev, 0x7c, &reg0x7c);
        val = reg0x7c;
        val |= (1<<2);
        bladerf_lms_write(dev, 0x7c, val);

        /* Set maximum gain for everything, but save off current values */
        bladerf_get_lna_gain(dev, &lna_gain);
        bladerf_set_lna_gain(dev, BLADERF_LNA_GAIN_MAX);

        bladerf_get_rxvga1(dev, &rxvga1);
        bladerf_set_rxvga1(dev, 30);

        bladerf_get_rxvga2(dev, &rxvga2);
        bladerf_set_rxvga2(dev, 30);
    }

    /* Figure out number of addresses to calibrate based on module */
    for (i = 0; i < addrs ; i++) {
        lms_dc_cal_loop(dev, base, i, &dc_regval) ;
    }

    /* Special case for LPF tuning module where results are written to TX/RX LPF DCCAL */
    if (module == BLADERF_DC_CAL_LPF_TUNING) {
        /* Set the DC level to RX and TX DCCAL modules */
        bladerf_lms_read(dev, 0x35, &val);
        val &= ~(0x3f);
        val |= dc_regval;
        bladerf_lms_write(dev, 0x35, val);

        bladerf_lms_read(dev, 0x55, &val);
        val &= ~(0x3f);
        val |= dc_regval;
        bladerf_lms_write(dev, 0x55, val);
    }
    /* Special case for RX LPF or RX VGA2 */
    else if (module == BLADERF_DC_CAL_RX_LPF || module == BLADERF_DC_CAL_RXVGA2) {
        /* Restore previously saved LNA Gain, VGA1 gain and VGA2 gain */
        bladerf_set_rxvga2(dev, rxvga2);
        bladerf_set_rxvga1(dev, rxvga1);
        bladerf_set_lna_gain(dev, lna_gain);
        bladerf_lms_write(dev, 0x71, reg0x71);
        bladerf_lms_write(dev, 0x7c, reg0x7c);
    }

    /* Restore original clock enables */
    bladerf_lms_write(dev, 0x09, clockenables);

    /* TODO: Return something appropriate if something goes awry */
    return 0;
}

