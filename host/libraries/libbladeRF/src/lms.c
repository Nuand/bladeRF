/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2013 Nuand LLC
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
 * If you're diving into this file, have the following documentation handy.
 *
 * As most registers don't have a clearly defined names, or are not grouped by
 * a specific set of functionality, there's little value in providing named
 * macro definitions, hence the hard-coded addresses and bitmasks.
 *
 * LMS6002D Project page:
 *  http://www.limemicro.com/products/LMS6002D.php?sector=default
 *
 * LMS6002D Datasheet:
 *  http://www.limemicro.com/download/LMS6002Dr2-DataSheet-1.2r0.pdf
 *
 * LMS6002D Programming and Calibration Guide:
 *  http://www.limemicro.com/download/LMS6002Dr2-Programming_and_Calibration_Guide-1.1r1.pdf
 *
 * LMS6002D FAQ:
 *  http://www.limemicro.com/download/FAQ_v1.0r10.pdf
 *
 */
#include <libbladeRF.h>
#include "lms.h"
#include "bladerf_priv.h"
#include "log.h"
#include "rel_assert.h"

#define kHz(x) (x * 1000)
#define MHz(x) (x * 1000000)
#define GHz(x) (x * 1000000000)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

/* LPF conversion table */
static const unsigned int uint_bandwidths[] = {
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

#define FREQ_RANGE(low_, high_, value_) \
{ \
    FIELD_INIT(.low, low_), \
    FIELD_INIT(.high,  high_), \
    FIELD_INIT(.value, value_), \
}

/* Frequency Range table. Corresponds to the LMS FREQSEL table */
static const struct freq_range {
    uint32_t    low;
    uint32_t    high;
    uint8_t     value;
} bands[] = {
    FREQ_RANGE(232500000,   285625000,   0x27),
    FREQ_RANGE(285625000,   336875000,   0x2f),
    FREQ_RANGE(336875000,   405000000,   0x37),
    FREQ_RANGE(405000000,   465000000,   0x3f),
    FREQ_RANGE(465000000,   571250000,   0x26),
    FREQ_RANGE(571250000,   673750000,   0x2e),
    FREQ_RANGE(673750000,   810000000,   0x36),
    FREQ_RANGE(810000000,   930000000,   0x3e),
    FREQ_RANGE(930000000,   1142500000,  0x25),
    FREQ_RANGE(1142500000,  1347500000,  0x2d),
    FREQ_RANGE(1347500000,  1620000000,  0x35),
    FREQ_RANGE(1620000000,  1860000000,  0x3d),
    FREQ_RANGE(1860000000u, 2285000000u, 0x24),
    FREQ_RANGE(2285000000u, 2695000000u, 0x2c),
    FREQ_RANGE(2695000000u, 3240000000u, 0x34),
    FREQ_RANGE(3240000000u, 3720000000u, 0x3c),
};

 /*
 * The LMS FAQ (Rev 1.0r10, Section 5.20) states that the RXVGA1 codes may be
 * converted to dB via:
 *      value_db = 20 * log10(127 / (127 - code))
 *
 * However, an offset of 5 appears to be required, yielding:
 *      value_db =  5 + 20 * log10(127 / (127 - code))
 *
 */
static const uint8_t rxvga1_lut_code2val[] = {
    5,  5,  5,  5,  5,  5,  5,  5,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
    6,  6,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  8,  8,  8,  8,  8,  8,
    8,  8,  8,  8,  8,  9,  9,  9,  9,  9,  9,  9,  9,  9,  10, 10, 10, 10, 10,
    10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 13, 13,
    13, 13, 13, 13, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 16, 16, 16, 16, 17,
    17, 17, 18, 18, 18, 18, 19, 19, 19, 20, 20, 21, 21, 22, 22, 22, 23, 24, 24,
    25, 25, 26, 27, 28, 29, 30
};

/* The closest values from the above forumla have been selected.
 * indicides 0 - 4 are clamped to 5dB */
static const uint8_t rxvga1_lut_val2code[] = {
    2,  2,  2,  2,   2,   2,   14,  26,  37,  47,  56,  63,  70,  76,  82,  87,
    91, 95, 99, 102, 104, 107, 109, 111, 113, 114, 116, 117, 118, 119, 120,
};

static const uint8_t lms_reg_dumpset[] = {
    /* Top level configuration */
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0E, 0x0F,

    /* TX PLL Configuration */
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B,
    0x1C, 0x1D, 0x1E, 0x1F,

    /* RX PLL Configuration */
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B,
    0x2C, 0x2D, 0x2E, 0x2F,

    /* TX LPF Modules Configuration */
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,

    /* TX RF Modules Configuration */
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B,
    0x4C, 0x4D, 0x4E, 0x4F,

    /* RX LPF, ADC, and DAC Modules Configuration */
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B,
    0x5C, 0x5D, 0x5E, 0x5F,

    /* RX VGA2 Configuration */
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,

    /* RX FE Modules Configuration */
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C
};

/* Register 0x08:  RF loopback config and additional BB config
 *
 * LBRFEN[3:0] @ [3:0]
 *  0000 - RF loopback disabled
 *  0001 - TXMIX output connected to LNA1 path
 *  0010 - TXMIX output connected to LNA2 path
 *  0011 - TXMIX output connected to LNA3 path
 *  else - Reserved
 *
 * LBEN_OPIN @ [4]
 *  0   - Disabled
 *  1   - TX BB loopback signal is connected to RX output pins
 *
 * LBEN_VGA2IN @ [5]
 *  0   - Disabled
 *  1   - TX BB loopback signal is connected to RXVGA2 input
 *
 * LBEN_LPFIN @ [6]
 *  0   - Disabled
 *  1   - TX BB loopback signal is connected to RXLPF input
 *
 */
#define LBEN_OPIN   (1 << 4)
#define LBEN_VGA2IN (1 << 5)
#define LBEN_LPFIN  (1 << 6)
#define LBEN_MASK   (LBEN_OPIN | LBEN_VGA2IN | LBEN_LPFIN)

#define LBRFEN_LNA1 1
#define LBRFEN_LNA2 2
#define LBRFEN_LNA3 3
#define LBRFEN_MASK 0xf     /* [3:2] are marked reserved */


/* Register 0x46: Baseband loopback config
 *
 * LOOPBBEN[1:0] @ [3:2]
 *  00 - All Baseband loops opened (default)
 *  01 - TX loopback path connected from TXLPF output
 *  10 - TX loopback path connected from TXVGA1 output
 *  11 - TX loopback path connected from Env/peak detect output
 */
#define LOOPBBEN_TXLPF  (1 << 2)
#define LOOPBBEN_TXVGA  (2 << 2)
#define LOOPBBEN_ENVPK  (3 << 2)
#define LOOBBBEN_MASK   (3 << 2)

static inline int is_loopback_enabled(struct bladerf *dev)
{
    bladerf_loopback loopback;
    int status;

    status = lms_get_loopback_mode(dev, &loopback);
    if (status != 0) {
        return status;
    }

    return loopback != BLADERF_LB_NONE;
}

static int write_pll_config(struct bladerf *dev, bladerf_module module,
                                     uint32_t frequency, uint8_t freqsel)
{
    int status;
    uint8_t regval;
    uint8_t selout;
    uint8_t addr;

    if (module == BLADERF_MODULE_TX) {
        addr = 0x15;
    } else {
        addr = 0x25;
    }

    status = bladerf_lms_read(dev, addr, &regval);
    if (status != 0) {
        return status;
    }

    status = is_loopback_enabled(dev);
    if (status < 0) {
        return status;
    }

    if (status == 0) {
        /* Loopback not enabled - update the PLL output buffer. */
        selout = (frequency < BLADERF_BAND_HIGH ? 1 : 2);
        regval = (freqsel << 2) | selout;
    } else {
        /* Loopback is enabled - don't touch PLL output buffer. */
        regval = (regval & ~0xfc) | (freqsel << 2);
    }

    return bladerf_lms_write(dev, addr, regval);
}


int lms_lpf_enable(struct bladerf *dev, bladerf_module mod, bool enable)
{
    int status;
    uint8_t data;
    const uint8_t reg = (mod == BLADERF_MODULE_RX) ? 0x54 : 0x34;

    status = bladerf_lms_read(dev, reg, &data);
    if (status != 0) {
        return status;
    }

    if (enable) {
        data |= (1 << 1);
    } else {
        data &= ~(1 << 1);
    }

    status = bladerf_lms_write(dev, reg, data);
    if (status != 0) {
        return status;
    }

    /* Check to see if we are bypassed */
    status = bladerf_lms_read(dev, reg + 1, &data);
    if (status != 0) {
        return status;
    } else if (data & (1 << 6)) {
        /* Bypass is enabled; switch back to normal operation */
        data &= ~(1 << 6);
        status = bladerf_lms_write(dev, reg + 1, data);
    }

    return status;
}

int lms_lpf_get_mode(struct bladerf *dev, bladerf_module mod,
                     bladerf_lpf_mode *mode)
{
    int status;
    uint8_t data;
    const uint8_t reg = (mod == BLADERF_MODULE_RX) ? 0x54 : 0x34;

    status = bladerf_lms_read(dev, reg, &data);
    if (status != 0) {
        return status;
    }

    if ((data & (1 << 1) ) == 0) {
        *mode = BLADERF_LPF_DISABLED;
    } else {
        status = bladerf_lms_read(dev, reg + 1, &data);
        if (status != 0) {
            return status;
        }

        if (data & (1 << 6)) {
            *mode = BLADERF_LPF_BYPASSED;
        } else {
            *mode = BLADERF_LPF_NORMAL;
        }
    }

    return 0;
}

int lms_lpf_set_mode(struct bladerf *dev, bladerf_module mod,
                     bladerf_lpf_mode mode)
{
    int status;
    const uint8_t reg = (mod == BLADERF_MODULE_RX) ? 0x54 : 0x34;
    uint8_t data_l, data_h;

    status = bladerf_lms_read(dev, reg, &data_l);
    if (status != 0) {
        return status;
    }

    status = bladerf_lms_read(dev, reg + 1, &data_h);
    if (status != 0) {
        return status;
    }

    if (mode == BLADERF_LPF_DISABLED) {
        data_l &= ~(1 << 1);    /* Power down LPF */
    } else if (mode == BLADERF_LPF_BYPASSED) {
        data_l |= (1 << 1);     /* Enable LPF */
        data_h |= (1 << 6);     /* Enable LPF bypass */
    } else {
        data_l |= (1 << 1);     /* Enable LPF */
        data_h &= ~(1 << 6);    /* Disable LPF bypass */
    }

    status = bladerf_lms_write(dev, reg, data_l);
    if (status != 0) {
        return status;
    }

    status = bladerf_lms_write(dev, reg + 1, data_h);
    return status;
}

int lms_set_bandwidth(struct bladerf *dev, bladerf_module mod, lms_bw bw)
{
    int status;
    uint8_t data;
    const uint8_t reg = (mod == BLADERF_MODULE_RX) ? 0x54 : 0x34;

    status = bladerf_lms_read(dev, reg, &data);
    if (status != 0) {
        return status;
    }

    data &= ~0x3c;      /* Clear out previous bandwidth setting */
    data |= (bw << 2);  /* Apply new bandwidth setting */

    return bladerf_lms_write(dev, reg, data);

}


int lms_get_bandwidth(struct bladerf *dev, bladerf_module mod, lms_bw *bw)
{
    int status;
    uint8_t data;
    const uint8_t reg = (mod == BLADERF_MODULE_RX) ? 0x54 : 0x34;

    status = bladerf_lms_read(dev, reg, &data);
    if (status != 0) {
        return status;
    }

    /* Fetch bandwidth table index from reg[5:2] */
    data >>= 2;
    data &= 0xf;

    assert(data < ARRAY_SIZE(uint_bandwidths));
    *bw = (lms_bw)data;
    return 0;
}

lms_bw lms_uint2bw(unsigned int req)
{
    lms_bw ret;

    if (     req <= kHz(1500)) ret = BW_1p5MHz;
    else if (req <= kHz(1750)) ret = BW_1p75MHz;
    else if (req <= kHz(2500)) ret = BW_2p5MHz;
    else if (req <= kHz(2750)) ret = BW_2p75MHz;
    else if (req <= MHz(3)  )  ret = BW_3MHz;
    else if (req <= kHz(3840)) ret = BW_3p84MHz;
    else if (req <= MHz(5)  )  ret = BW_5MHz;
    else if (req <= kHz(5500)) ret = BW_5p5MHz;
    else if (req <= MHz(6)  )  ret = BW_6MHz;
    else if (req <= MHz(7)  )  ret = BW_7MHz;
    else if (req <= kHz(8750)) ret = BW_8p75MHz;
    else if (req <= MHz(10) )  ret = BW_10MHz;
    else if (req <= MHz(12) )  ret = BW_12MHz;
    else if (req <= MHz(14) )  ret = BW_14MHz;
    else if (req <= MHz(20) )  ret = BW_20MHz;
    else                       ret = BW_28MHz;

    return ret;
}

/* Return the table entry */
unsigned int lms_bw2uint(lms_bw bw)
{
    unsigned int idx = bw & 0xf;
    assert(idx < ARRAY_SIZE(uint_bandwidths));
    return uint_bandwidths[idx];
}

/* Enable dithering on the module PLL */
int lms_dither_enable(struct bladerf *dev, bladerf_module mod,
                      uint8_t nbits, bool enable)
{
    int status;

    /* Select the base address based on which PLL we are configuring */
    const uint8_t reg = (mod == BLADERF_MODULE_RX) ? 0x24 : 0x14;
    uint8_t data;

    /* Valid range is 1 - 8 bits (inclusive) */
    if (nbits < 1 || nbits > 8) {
        return BLADERF_ERR_INVAL;
    }

    /* Read what we currently have in there */
    status = bladerf_lms_read(dev, reg, &data);
    if (status != 0) {
        return status;
    }

    if (enable) {
        /* Enable dithering */
        data |= (1 << 7);

        /* Clear out the previous setting of the number of bits to dither */
        data &= ~(7 << 4);

        /* Update with the desired number of bits to dither */
        data |= (((nbits - 1) & 7) << 4);

    } else {
        /* Clear dithering enable bit */
        data &= ~(1 << 7);
    }

    /* Write it out */
    status = bladerf_lms_write(dev, reg, data);
    return status;
}

/* Soft reset of the LMS */
int lms_soft_reset(struct bladerf *dev)
{

    int status = bladerf_lms_write(dev, 0x05, 0x12);

    if (status == 0) {
        status = bladerf_lms_write(dev, 0x05, 0x32);
    }

    return status;
}

/* Set the gain on the LNA */
int lms_lna_set_gain(struct bladerf *dev, bladerf_lna_gain gain)
{
    int status;
    uint8_t data;

    if (gain == BLADERF_LNA_GAIN_BYPASS || gain == BLADERF_LNA_GAIN_MID ||
        gain == BLADERF_LNA_GAIN_MAX) {

        status = bladerf_lms_read(dev, 0x75, &data);
        if (status == 0) {
            data &= ~(3 << 6);          /* Clear out previous gain setting */
            data |= ((gain & 3) << 6);  /* Update gain value */
            status = bladerf_lms_write(dev, 0x75, data);
        }

    } else {
        status = BLADERF_ERR_INVAL;
    }

    return status;
}

int lms_lna_get_gain(struct bladerf *dev, bladerf_lna_gain *gain)
{
    int status;
    uint8_t data;

    status = bladerf_lms_read(dev, 0x75, &data);
    if (status == 0) {
        data >>= 6;
        data &= 3;
        *gain = (bladerf_lna_gain)data;

        if (*gain == BLADERF_LNA_GAIN_UNKNOWN) {
            status = BLADERF_ERR_INVAL;
        }
    }

    return status;
}

/* Select which LNA to enable */
int lms_select_lna(struct bladerf *dev, lms_lna lna)
{
    int status;
    uint8_t data;

    status = bladerf_lms_read(dev, 0x75, &data);
    if (status != 0) {
        return status;
    }

    data &= ~(3 << 4);
    data |= ((lna & 3) << 4);

    return bladerf_lms_write(dev, 0x75, data);
}

/* Enable bit is in reserved register documented in this thread:
 *  https://groups.google.com/forum/#!topic/limemicro-opensource/8iTannzlfzg
 */
int lms_rxvga1_enable(struct bladerf *dev, bool enable)
{
    int status;
    uint8_t data;

    status = bladerf_lms_read(dev, 0x7d, &data);
    if (status != 0) {
        return status;
    }

    if (enable) {
        data &= ~(1 << 3);
    } else {
        data |= (1 << 3);
    }

    return bladerf_lms_write(dev, 0x7d, data);
}

/* Set the RFB_TIA_RXFE mixer gain */
int lms_rxvga1_set_gain(struct bladerf *dev, int gain)
{
    if (gain > BLADERF_RXVGA1_GAIN_MAX) {
        gain = BLADERF_RXVGA1_GAIN_MAX;
        log_info("Clamping RXVGA1 gain to %ddB\n", gain);
    } else if (gain < BLADERF_RXVGA1_GAIN_MIN) {
        gain = BLADERF_RXVGA1_GAIN_MIN;
        log_info("Clamping RXVGA1 gain to %ddB\n", gain);
    }

    return bladerf_lms_write(dev, 0x76, rxvga1_lut_val2code[gain]);
}

/* Get the RFB_TIA_RXFE mixer gain */
int lms_rxvga1_get_gain(struct bladerf *dev, int *gain)
{
    uint8_t data;
    int status = bladerf_lms_read(dev, 0x76, &data);

    if (status == 0) {
        data &= 0x7f;
        if (data > 120) {
            data = 120;
        }

        *gain = rxvga1_lut_code2val[data];
    }

    return status;
}

/* Enable RXVGA2 */
int lms_rxvga2_enable(struct bladerf *dev, bool enable)
{
    int status;
    uint8_t data;

    status = bladerf_lms_read(dev, 0x64, &data);
    if (status != 0) {
        return status;
    }

    if (enable) {
        data |= (1 << 1);
    } else {
        data &= ~(1 << 1);
    }

    return bladerf_lms_write(dev, 0x64, data);
}


/* Set the gain on RXVGA2 */
int lms_rxvga2_set_gain(struct bladerf *dev, int gain)
{
    if (gain > BLADERF_RXVGA2_GAIN_MAX) {
        gain = BLADERF_RXVGA2_GAIN_MAX;
        log_info("Clamping RXVGA2 gain to %ddB\n", gain);
    } else if (gain < BLADERF_RXVGA2_GAIN_MIN) {
        gain = BLADERF_RXVGA2_GAIN_MIN;
        log_info("Clamping RXVGA2 gain to %ddB\n", gain);
    }

    /* 3 dB per register code */
    return bladerf_lms_write(dev, 0x65, gain / 3);
}

int lms_rxvga2_get_gain(struct bladerf *dev, int *gain)
{

    uint8_t data;
    const int status = bladerf_lms_read(dev, 0x65, &data);

    if (status == 0) {
        /* 3 dB per code */
        data *= 3;
        *gain = data;
    }

    return status;
}

int lms_select_pa(struct bladerf *dev, lms_pa pa)
{
    int status;
    uint8_t data;

    status = bladerf_lms_read(dev, 0x44, &data);

    /* Disable PA1, PA2, and AUX PA - we'll enable as requested below. */
    data &= ~0x1C;

    /* AUX PA powered down */
    data |= (1 << 1);

    switch (pa) {
        case PA_AUX:
            data &= ~(1 << 1);  /* Power up the AUX PA */
            break;

        case PA_1:
            data |= (2 << 2);   /* PA_EN[2:0] = 010 - Enable PA1 */
            break;

        case PA_2:
            data |= (4 << 2);   /* PA_EN[2:0] = 100 - Enable PA2 */
            break;

        case PA_NONE:
            break;

        default:
            assert(!"Invalid PA selection");
            status = BLADERF_ERR_INVAL;
    }

    if (status == 0) {
        status = bladerf_lms_write(dev, 0x44, data);
    }

    return status;

};

int lms_peakdetect_enable(struct bladerf *dev, bool enable)
{
    int status;
    uint8_t data;

    status = bladerf_lms_read(dev, 0x44, &data);

    if (status == 0) {
        if (enable) {
            data &= ~(1 << 0);
        } else {
            data |= (1 << 0);
        }
        status = bladerf_lms_write(dev, 0x44, data);
    }

    return status;
}

int lms_enable_rffe(struct bladerf *dev, bladerf_module module, bool enable)
{
    int status;
    uint8_t data;
    uint8_t addr = (module == BLADERF_MODULE_TX ? 0x40 : 0x70);

    status = bladerf_lms_read(dev, addr, &data);
    if (status == 0) {

        if (module == BLADERF_MODULE_TX) {
            if (enable) {
                data |= (1 << 1);
            } else {
                data &= ~(1 << 1);
            }
        } else {
            if (enable) {
                data |= (1 << 0);
            } else {
                data &= ~(1 << 0);
            }
        }

        status = bladerf_lms_write(dev, addr, data);
    }

    return status;
}

int lms_txvga2_set_gain(struct bladerf *dev, int gain_int)
{
    int status;
    uint8_t data;
    int8_t gain;

    if (gain_int > BLADERF_TXVGA2_GAIN_MAX) {
        gain = BLADERF_TXVGA2_GAIN_MAX;
        log_info("Clamping TXVGA2 gain to %ddB\n", gain);
    } else if (gain_int < BLADERF_TXVGA2_GAIN_MIN) {
        gain = 0;
        log_info("Clamping TXVGA2 gain to %ddB\n", gain);
    } else {
        gain = gain_int;
    }

    status = bladerf_lms_read(dev, 0x45, &data);
    if (status == 0) {
        data &= ~(0x1f << 3);
        data |= ((gain & 0x1f) << 3);
        status = bladerf_lms_write(dev, 0x45, data);
    }

    return status;
}

int lms_txvga2_get_gain(struct bladerf *dev, int *gain)
{
    int status;
    uint8_t data;

    status = bladerf_lms_read(dev, 0x45, &data);

    if (status == 0) {
        *gain = (data >> 3) & 0x1f;

        /* Register values of 25-31 all correspond to 25 dB */
        if (*gain > 25) {
            *gain = 25;
        }
    }

    return status;
}

int lms_txvga1_set_gain(struct bladerf *dev, int gain_int)
{
    int8_t gain;

    if (gain_int < BLADERF_TXVGA1_GAIN_MIN) {
        gain = BLADERF_TXVGA1_GAIN_MIN;
        log_info("Clamping TXVGA1 gain to %ddB\n", gain);
    } else if (gain_int > BLADERF_TXVGA1_GAIN_MAX) {
        gain = BLADERF_TXVGA1_GAIN_MAX;
        log_info("Clamping TXVGA1 gain to %ddB\n", gain);
    } else {
        gain = gain_int;
    }

    /* Apply offset to convert gain to register table index */
    gain = (gain + 35);

    /* Since 0x41 is only VGA1GAIN, we don't need to RMW */
    return bladerf_lms_write(dev, 0x41, gain);
}

int lms_txvga1_get_gain(struct bladerf *dev, int *gain)
{
    int status;
    uint8_t data;

    status = bladerf_lms_read(dev, 0x41, &data);
    if (status == 0) {
        /* Clamp to max value */
        data = data & 0x1f;

        /* Convert table index to value */
        *gain = data - 35;
    }

    return status;
}

static inline int enable_lna_power(struct bladerf *dev, bool enable)
{
    int status;
    uint8_t regval;

    /* Magic test register to power down LNAs */
    status = bladerf_lms_read(dev, 0x7d, &regval);
    if (status != 0) {
        return status;
    }

    if (enable) {
        regval &= ~(1 << 0);
    } else {
        regval |= (1 << 0);
    }

    status = bladerf_lms_write(dev, 0x7d, regval);
    if (status != 0) {
        return status;
    }

    /* Decode test registers */
    status = bladerf_lms_read(dev, 0x70, &regval);
    if (status != 0) {
        return status;
    }

    if (enable) {
        regval &= ~(1 << 1);
    } else {
        regval |= (1 << 1);
    }

    return bladerf_lms_write(dev, 0x70, regval);
}

/* Power up/down RF loopback switch */
static inline int enable_rf_loopback_switch(struct bladerf *dev, bool enable)
{
    int status;
    uint8_t regval;

    status = bladerf_lms_read(dev, 0x0b, &regval);
    if (status != 0) {
        return status;
    }

    if (enable) {
        regval |= (1 << 0);
    } else {
        regval &= ~(1 << 0);
    }

    return bladerf_lms_write(dev, 0x0b, regval);
}


/* Configure TX-side of loopback */
static int loopback_tx(struct bladerf *dev, bladerf_loopback mode)
{
    int status = 0;

    switch(mode) {
        case BLADERF_LB_BB_TXLPF_RXVGA2:
        case BLADERF_LB_BB_TXLPF_RXLPF:
        case BLADERF_LB_BB_TXVGA1_RXVGA2:
        case BLADERF_LB_BB_TXVGA1_RXLPF:
            break;

        case BLADERF_LB_RF_LNA1:
        case BLADERF_LB_RF_LNA2:
        case BLADERF_LB_RF_LNA3:
            status = lms_select_pa(dev, PA_AUX);
            break;

        case BLADERF_LB_NONE:
        {
            struct lms_freq f;

            /* Re-enable the appropriate PA, based upon our current frequency */
            status = lms_get_frequency(dev, BLADERF_MODULE_TX, &f);
            if (status != 0) {
                return status;
            }

            status = lms_select_band(dev, BLADERF_MODULE_TX,
                                     lms_frequency_to_hz(&f));
            break;
        }

        default:
            assert(!"Invalid loopback mode encountered");
            status = BLADERF_ERR_INVAL;
    }

    return status;
}

/* Configure RX-side of loopback */
static int loopback_rx(struct bladerf *dev, bladerf_loopback mode)
{
    int status;
    bladerf_lpf_mode lpf_mode;
    uint8_t lna;
    uint8_t regval;

    status = lms_lpf_get_mode(dev, BLADERF_MODULE_RX, &lpf_mode);
    if (status != 0) {
        return status;
    }

    switch (mode) {
        case BLADERF_LB_BB_TXLPF_RXVGA2:
        case BLADERF_LB_BB_TXVGA1_RXVGA2:

            /* Ensure RXVGA2 is enabled */
            status = lms_rxvga2_enable(dev, true);
            if (status != 0) {
                return status;
            }

            /* RXLPF must be disabled */
            status = lms_lpf_set_mode(dev, BLADERF_MODULE_RX,
                                      BLADERF_LPF_DISABLED);
            if (status != 0) {
                return status;
            }
            break;

        case BLADERF_LB_BB_TXLPF_RXLPF:
        case BLADERF_LB_BB_TXVGA1_RXLPF:

            /* RXVGA1 must be disabled */
            status = lms_rxvga1_enable(dev, false);
            if (status != 0) {
                return status;
            }

            /* Enable the RXLPF if needed */
            if (lpf_mode == BLADERF_LPF_DISABLED) {
                status = lms_lpf_set_mode(dev, BLADERF_MODULE_RX,
                        BLADERF_LPF_NORMAL);
                if (status != 0) {
                    return status;
                }
            }

            /* Ensure RXVGA2 is enabled */
            status = lms_rxvga2_enable(dev, true);
            if (status != 0) {
                return status;
            }

            break;

        case BLADERF_LB_RF_LNA1:
        case BLADERF_LB_RF_LNA2:
        case BLADERF_LB_RF_LNA3:
            lna = mode - BLADERF_LB_RF_LNA1 + 1;
            assert(lna >= 1 && lna <= 3);

            /* Power down LNAs */
            status = enable_lna_power(dev, false);
            if (status != 0) {
                return status;
            }

            /* Ensure RXVGA1 is enabled */
            status = lms_rxvga1_enable(dev, true);
            if (status != 0) {
                return status;
            }

            /* Enable the RXLPF if needed */
            if (lpf_mode == BLADERF_LPF_DISABLED) {
                status = lms_lpf_set_mode(dev, BLADERF_MODULE_RX,
                        BLADERF_LPF_NORMAL);
                if (status != 0) {
                    return status;
                }
            }

            /* Ensure RXVGA2 is enabled */
            status = lms_rxvga2_enable(dev, true);
            if (status != 0) {
                return status;
            }

            /* Select output buffer in RX PLL and select the desired LNA */
            status = bladerf_lms_read(dev, 0x25, &regval);
            if (status != 0) {
                return status;
            }

            regval &= ~0x03;
            regval |= lna;

            status = bladerf_lms_write(dev, 0x25, regval);
            if (status != 0) {
                return status;
            }

            status = lms_select_lna(dev, lna);
            if (status != 0) {
                return status;
            }

            /* Enable RF loopback switch */
            status = enable_rf_loopback_switch(dev, true);
            if (status != 0) {
                return status;
            }

            break;

        case BLADERF_LB_NONE:
        {
            struct lms_freq f;

            /* Ensure all RX blocks are enabled */
            status = lms_rxvga1_enable(dev, true);
            if (status != 0) {
                return status;
            }

            if (lpf_mode == BLADERF_LPF_DISABLED) {
                status = lms_lpf_set_mode(dev, BLADERF_MODULE_RX,
                        BLADERF_LPF_NORMAL);
                if (status != 0) {
                    return status;
                }
            }

            status = lms_rxvga2_enable(dev, true);
            if (status != 0) {
                return status;
            }

            /* Disable RF loopback switch */
            status = enable_rf_loopback_switch(dev, false);
            if (status != 0) {
                return status;
            }

            /* Power up LNAs */
            status = enable_lna_power(dev, true);
            if (status != 0) {
                return status;
            }

            /* Select the relevant LNA based upon our current frequency */
            status = lms_get_frequency(dev, BLADERF_MODULE_RX, &f);
            if (status != 0) {
                return status;
            }

            status = lms_select_band(dev, BLADERF_MODULE_RX,
                                     lms_frequency_to_hz(&f));
            break;
        }

        default:
            assert(!"Invalid loopback mode encountered");
            status = BLADERF_ERR_INVAL;
    }

    return status;
}

/* Configure "switches" in loopback path */
static int loopback_path(struct bladerf *dev, bladerf_loopback mode)
{
    int status;
    uint8_t loopbben, lben_lbrf;

    status = bladerf_lms_read(dev, 0x46, &loopbben);
    if (status != 0) {
        return status;
    }

    status = bladerf_lms_read(dev, 0x08, &lben_lbrf);
    if (status != 0) {
        return status;
    }

    /* Default to baseband loopback being disabled  */
    loopbben &= ~LOOBBBEN_MASK;

    /* Default to RF and BB loopback options being disabled */
    lben_lbrf &= ~(LBRFEN_MASK | LBEN_MASK);

    switch(mode) {
        case BLADERF_LB_BB_TXLPF_RXVGA2:
            loopbben |= LOOPBBEN_TXLPF;
            lben_lbrf |= LBEN_VGA2IN;
            break;

        case BLADERF_LB_BB_TXLPF_RXLPF:
            loopbben |= LOOPBBEN_TXLPF;
            lben_lbrf |= LBEN_LPFIN;
            break;

        case BLADERF_LB_BB_TXVGA1_RXVGA2:
            loopbben |= LOOPBBEN_TXVGA;
            lben_lbrf |= LBEN_VGA2IN;
            break;

        case BLADERF_LB_BB_TXVGA1_RXLPF:
            loopbben |= LOOPBBEN_TXVGA;
            lben_lbrf |= LBEN_LPFIN;
            break;

        case BLADERF_LB_RF_LNA1:
            lben_lbrf |= LBRFEN_LNA1;
            break;

        case BLADERF_LB_RF_LNA2:
            lben_lbrf |= LBRFEN_LNA2;
            break;

        case BLADERF_LB_RF_LNA3:
            lben_lbrf |= LBRFEN_LNA3;
            break;

        case BLADERF_LB_NONE:
            break;

        default:
            return BLADERF_ERR_INVAL;
    }

    status = bladerf_lms_write(dev, 0x46, loopbben);
    if (status == 0) {
        status = bladerf_lms_write(dev, 0x08, lben_lbrf);
    }

    return status;
}


int lms_set_loopback_mode(struct bladerf *dev, bladerf_loopback mode)
{
    int status;

    /* Verify a valid mode is provided before shutting anything down */
    switch (mode) {
        case BLADERF_LB_BB_TXLPF_RXVGA2:
        case BLADERF_LB_BB_TXLPF_RXLPF:
        case BLADERF_LB_BB_TXVGA1_RXVGA2:
        case BLADERF_LB_BB_TXVGA1_RXLPF:
        case BLADERF_LB_RF_LNA1:
        case BLADERF_LB_RF_LNA2:
        case BLADERF_LB_RF_LNA3:
        case BLADERF_LB_NONE:
            break;

        default:
            return BLADERF_ERR_INVAL;
    }

    /* Disable all PA/LNAs while entering loopback mode or making changes */
    status = lms_select_pa(dev, PA_NONE);
    if (status != 0) {
        return status;
    }

    status = lms_select_lna(dev, LNA_NONE);
    if (status != 0) {
        return status;
    }

    /* Disconnect loopback paths while we re-configure blocks */
    status = loopback_path(dev, BLADERF_LB_NONE);
    if (status != 0) {
        return status;
    }

    /* Configure the RX side of the loopback path */
    status = loopback_rx(dev, mode);
    if (status != 0) {
        return status;
    }

    /* Configure the TX side of the path */
    status = loopback_tx(dev, mode);
    if (status != 0) {
        return status;
    }

    /* Configure "switches" along the loopback path */
    status = loopback_path(dev, mode);
    if (status != 0) {
        return status;
    }

    return 0;
}

int lms_get_loopback_mode(struct bladerf *dev, bladerf_loopback *loopback)
{
    int status;
    uint8_t lben_lbrfen, loopbben;


    status = bladerf_lms_read(dev, 0x08, &lben_lbrfen);
    if (status != 0) {
        return status;
    }

    status = bladerf_lms_read(dev, 0x46, &loopbben);
    if (status != 0) {
        return status;
    }

    switch (lben_lbrfen & 0x7) {
        case LBRFEN_LNA1:
            *loopback = BLADERF_LB_RF_LNA1;
            return 0;

        case LBRFEN_LNA2:
            *loopback = BLADERF_LB_RF_LNA2;
            return 0;

        case LBRFEN_LNA3:
            *loopback = BLADERF_LB_RF_LNA3;
            return 0;

        default:
            break;
    }

    switch (lben_lbrfen & LBEN_MASK) {
        case LBEN_VGA2IN:
            if (loopbben & LOOPBBEN_TXLPF) {
                *loopback = BLADERF_LB_BB_TXLPF_RXVGA2;
                return 0;
            } else if (loopbben & LOOPBBEN_TXVGA) {
                *loopback = BLADERF_LB_BB_TXVGA1_RXVGA2;
                return 0;
            }
            break;

        case LBEN_LPFIN:
            if (loopbben & LOOPBBEN_TXLPF) {
                *loopback = BLADERF_LB_BB_TXLPF_RXLPF;
                return 0;
            } else if (loopbben & LOOPBBEN_TXVGA) {
                *loopback = BLADERF_LB_BB_TXVGA1_RXLPF;
                return 0;
            }
            break;

        default:
            break;
    }

    *loopback = BLADERF_LB_NONE;
    return 0;
}

/* Top level power down of the LMS */
int lms_power_down(struct bladerf *dev)
{
    int status;
    uint8_t data;

    status = bladerf_lms_read(dev, 0x05, &data);
    if (status == 0) {
        data &= ~(1 << 4);
        status = bladerf_lms_write(dev, 0x05, data);
    }

    return status;
}

/* Enable the PLL of a module */
int lms_pll_enable(struct bladerf *dev, bladerf_module mod, bool enable)
{
    int status;
    const uint8_t reg = (mod == BLADERF_MODULE_RX) ? 0x24 : 0x14;
    uint8_t data;

    status = bladerf_lms_read(dev, reg, &data);
    if (status == 0) {
        if (enable) {
            data |= (1 << 3);
        } else {
            data &= ~(1 << 3);
        }
        status = bladerf_lms_write(dev, reg, data);
    }

    return status;
}

/* Enable the RX subsystem */
int lms_rx_enable(struct bladerf *dev, bool enable)
{
    int status;
    uint8_t data;

    status = bladerf_lms_read(dev, 0x05, &data);
    if (status == 0) {
        if (enable) {
            data |= (1 << 2);
        } else {
            data &= ~(1 << 2);
        }
        status = bladerf_lms_write(dev, 0x05, data);
    }

    return status;
}

/* Enable the TX subsystem */
int lms_tx_enable(struct bladerf *dev, bool enable)
{
    int status;
    uint8_t data;

    status = bladerf_lms_read(dev, 0x05, &data);

    if (status == 0) {
        if (enable) {
            data |= (1 << 3);
        } else {
            data &= ~(1 << 3);
        }
        status = bladerf_lms_write(dev, 0x05, data);
    }

    return status;
}

/* Converts frequency structure to Hz */
uint32_t lms_frequency_to_hz(struct lms_freq *f)
{
    uint64_t pll_coeff;
    uint32_t div;

    pll_coeff = (((uint64_t)f->nint) << 23) + f->nfrac;
    div = (f->x << 23);

    return (uint32_t)(((f->reference * pll_coeff) + (div >> 1)) / div);
}

/* Print a frequency structure */
void lms_print_frequency(struct lms_freq *f)
{
    log_verbose("---- Frequency ----\n");
    log_verbose("  x        : %d\n", f->x);
    log_verbose("  nint     : %d\n", f->nint);
    log_verbose("  nfrac    : %u\n", f->nfrac);
    log_verbose("  freqsel  : 0x%02x\n", f->freqsel);
    log_verbose("  reference: %u\n", f->reference);
    log_verbose("  freq     : %u\n", lms_frequency_to_hz(f));
}

/* Get the frequency structure */
int lms_get_frequency(struct bladerf *dev, bladerf_module mod,
                      struct lms_freq *f)
{
    const uint8_t base = (mod == BLADERF_MODULE_RX) ? 0x20 : 0x10;
    int status;
    uint8_t data;

    status = bladerf_lms_read(dev, base + 0, &data);
    if (status != 0) {
        return status;
    }

    f->nint = ((uint16_t)data) << 1;

    status = bladerf_lms_read(dev, base + 1, &data);
    if (status != 0) {
        return status;
    }

    f->nint |= (data & 0x80) >> 7;
    f->nfrac = ((uint32_t)data & 0x7f) << 16;

    status = bladerf_lms_read(dev, base + 2, &data);
    if (status != 0) {
        return status;
    }

    f->nfrac |= ((uint32_t)data)<<8;

    status = bladerf_lms_read(dev, base + 3, &data);
    if (status != 0) {
        return status;
    }

    f->nfrac |= data;

    status = bladerf_lms_read(dev, base + 5, &data);
    if (status != 0) {
        return status;
    }

    f->freqsel = (data>>2);
    f->x = 1 << ((f->freqsel & 7) - 3);
    f->reference = 38400000;

    return status;
}

#define VCO_HIGH 0x02
#define VCO_NORM 0x00
#define VCO_LOW 0x01
static inline int tune_vcocap(struct bladerf *dev, uint8_t base, uint8_t data)
{
    int start_i = -1, stop_i = -1;
    int i;
    uint8_t vcocap = 32;
    uint8_t step = vcocap >> 1;
    uint8_t vtune;
    int status;

    status = bladerf_lms_read(dev, base + 9, &data);
    if (status != 0) {
        return status;
    }

    data &= ~(0x3f);
    for (i = 0; i < 6; i++) {
        status = bladerf_lms_write(dev, base + 9, vcocap | data);
        if (status != 0) {
            return status;
        }

        status = bladerf_lms_read(dev, base + 10, &vtune);
        if (status != 0) {
            return status;
        }

        vtune >>= 6;

        if (vtune == VCO_NORM) {
            log_verbose( "Found normal at VCOCAP: %d\n", vcocap );
            break;
        } else if (vtune == VCO_HIGH) {
            log_verbose( "Too high: %d -> %d\n", vcocap, vcocap + step );
            vcocap += step ;
        } else if (vtune == VCO_LOW) {
            log_verbose( "Too low: %d -> %d\n", vcocap, vcocap - step );
            vcocap -= step ;
        } else {
            log_error( "Invalid VTUNE value encountered\n" );
            return BLADERF_ERR_UNEXPECTED;
        }

        step >>= 1;
    }

    if (vtune != VCO_NORM) {
        log_debug( "VTUNE is not locked at the end of initial loop\n" );
        return BLADERF_ERR_UNEXPECTED;
    }

    start_i = stop_i = vcocap;
    while (start_i > 0 && vtune == VCO_NORM) {
        start_i -= 1;

        status = bladerf_lms_write(dev, base + 9, start_i | data);
        if (status != 0) {
            return status;
        }

        status = bladerf_lms_read(dev, base + 10, &vtune);
        if (status != 0) {
            return status;
        }

        vtune >>= 6;
    }

    start_i += 1;
    log_verbose( "Found lower limit VCOCAP: %d\n", start_i );

    status = bladerf_lms_write(dev, base + 9, vcocap | data );
    if (status != 0) {
        return status;
    }

    status = bladerf_lms_read(dev, base + 10, &vtune);
    if (status != 0) {
        return status;
    }

    vtune >>= 6;

    while (stop_i < 64 && vtune == VCO_NORM) {
        stop_i += 1;

        status = bladerf_lms_write(dev, base + 9, stop_i | data);
        if (status != 0) {
            return status;
        }

        status = bladerf_lms_read(dev, base + 10, &vtune);
        if (status != 0) {
            return status;
        }

        vtune >>= 6;
    }

    stop_i -= 1;
    log_verbose( "Found upper limit VCOCAP: %d\n", stop_i );

    vcocap = (start_i + stop_i) >> 1 ;

    log_verbose( "Goldilocks VCOCAP: %d\n", vcocap );

    status = bladerf_lms_write(dev, base + 9, vcocap | data );
    if (status != 0) {
        return status;
    }

    status = bladerf_lms_read(dev, base + 10, &vtune);
    if (status != 0) {
        return status;
    }

    vtune >>= 6;
    log_verbose( "VTUNE: %d\n", vtune );
    if (vtune != VCO_NORM) {
        status = BLADERF_ERR_UNEXPECTED;
        log_warning("VCOCAP could not converge and VTUNE is not locked - %d\n",
                    vtune);
    }

    return status;
}

/* Set the frequency of a module */
int lms_set_frequency(struct bladerf *dev, bladerf_module mod, uint32_t freq)
{
    /* Select the base address based on which PLL we are configuring */
    const uint8_t base = (mod == BLADERF_MODULE_RX) ? 0x20 : 0x10;
    const uint64_t ref_clock = 38400000;
    uint32_t lfreq = freq;
    uint8_t freqsel = bands[0].value;
    uint16_t nint;
    uint32_t nfrac;
    struct lms_freq f;
    uint8_t data;
    uint64_t vco_x;
    uint64_t temp;
    int status, dsm_status;
    uint8_t i = 0;

    /* Clamp out of range values */
    if (lfreq < BLADERF_FREQUENCY_MIN) {
        lfreq = BLADERF_FREQUENCY_MIN;
        log_info("Clamping frequency to: %u\n", lfreq);
    } else if (lfreq > BLADERF_FREQUENCY_MAX) {
        lfreq = BLADERF_FREQUENCY_MAX;
        log_info("Clamping frequency to: %u\n", lfreq);
    }

    /* Figure out freqsel */

    while(i < 16) {
        if ((lfreq > bands[i].low) && (lfreq <= bands[i].high)) {
            freqsel = bands[i].value;
            break;
        }
        i++;
    }

    /* Calculate integer portion of the frequency value */
    vco_x = ((uint64_t)1) << ((freqsel & 7) - 3);
    temp = (vco_x * freq) / ref_clock;
    assert(temp <= UINT16_MAX);
    nint = (uint16_t)temp;

    /* Calculate the fractional portion, rounding to the nearest value */
    temp = (1 << 23) * (vco_x * freq - nint * ref_clock);
    temp = (temp + ref_clock / 2) / ref_clock;
    assert(temp <= UINT32_MAX);
    nfrac = (uint32_t)temp;

    assert(vco_x <= UINT8_MAX);
    f.x = (uint8_t)vco_x;
    f.nint = nint;
    f.nfrac = nfrac;
    f.freqsel = freqsel;
    assert(ref_clock <= UINT32_MAX);
    f.reference = (uint32_t)ref_clock;
    lms_print_frequency(&f);

    /* Turn on the DSMs */
    status = bladerf_lms_read(dev, 0x09, &data);
    if (status == 0) {
        data |= 0x05;
        status = bladerf_lms_write(dev, 0x09, data);
    }

    if (status != 0) {
        log_debug("Failed to turn on DSMs\n");
        return status;
    }

    status = write_pll_config(dev, mod, freq, freqsel);
    if (status != 0) {
        goto lms_set_frequency_error;
    }

    data = nint >> 1;
    status = bladerf_lms_write(dev, base + 0, data);
    if (status != 0) {
        goto lms_set_frequency_error;
    }

    data = ((nint & 1) << 7) | ((nfrac >> 16) & 0x7f);
    status = bladerf_lms_write(dev, base + 1, data);
    if (status != 0) {
        goto lms_set_frequency_error;
    }

    data = ((nfrac >> 8) & 0xff);
    status = bladerf_lms_write(dev, base + 2, data);
    if (status != 0) {
        goto lms_set_frequency_error;
    }

    data = (nfrac & 0xff);
    status = bladerf_lms_write(dev, base + 3, data);
    if (status != 0) {
        goto lms_set_frequency_error;
    }

    /* Set the PLL Ichp, Iup and Idn currents */
    status = bladerf_lms_read(dev, base + 6, &data);
    if (status != 0) {
        goto lms_set_frequency_error;
    }

    data &= ~(0x1f);
    data |= 0x0c;

    status = bladerf_lms_write(dev, base + 6, data);
    if (status != 0) {
        goto lms_set_frequency_error;
    }

    status = bladerf_lms_read(dev, base + 7, &data);
    if (status != 0) {
        goto lms_set_frequency_error;
    }

    data &= ~(0x1f);
    data |= 3;

    status = bladerf_lms_write(dev, base + 7, data);
    if (status != 0) {
        goto lms_set_frequency_error;
    }

    status = bladerf_lms_read(dev, base + 8, &data);
    if (status != 0) {
        goto lms_set_frequency_error;
    }

    data &= ~(0x1f);
    status = bladerf_lms_write(dev, base + 8, data);
    if (status != 0) {
        goto lms_set_frequency_error;
    }

    /* Loop through the VCOCAP to figure out optimal values */
    status = tune_vcocap(dev, base, data);

lms_set_frequency_error:
    /* Turn off the DSMs */
    dsm_status = bladerf_lms_read(dev, 0x09, &data);
    if (dsm_status == 0) {
        data &= ~(0x05);
        dsm_status = bladerf_lms_write(dev, 0x09, data);
    }

    return (status == 0) ? dsm_status : status;
}

int lms_dump_registers(struct bladerf *dev)
{
    int status = 0;
    uint8_t data,i;
    const uint16_t num_reg = sizeof(lms_reg_dumpset);

    for (i = 0; i < num_reg; i++) {
        status = bladerf_lms_read(dev, lms_reg_dumpset[i], &data);
        if (status != 0) {
            log_debug("Failed to read LMS @ 0x%02x\n", lms_reg_dumpset[i]);
            return status;
        } else {
            log_debug("LMS[0x%02x] = 0x%02x\n", lms_reg_dumpset[i], data);
        }
    }

    return status;
}

/* Reference LMS6002D calibration guide, section 4.1 flow chart */
static int lms_dc_cal_loop(struct bladerf *dev, uint8_t base,
                           uint8_t cal_address, uint8_t *dc_regval)
{
    int status;
    uint8_t i, val, control;
    bool done = false;
    const unsigned int max_cal_count = 25;

    log_debug("Calibrating module %2.2x:%2.2x\n", base, cal_address);

    /* Set the calibration address for the block, and start it up */
    status = bladerf_lms_read(dev, base + 0x03, &val);
    if (status != 0) {
        return status;
    }

    val &= ~(0x07);
    val |= cal_address&0x07;

    status = bladerf_lms_write(dev, base + 0x03, val);
    if (status != 0) {
        return status;
    }

    /* Start the calibration by toggling DC_START_CLBR */
    val |= (1 << 5);
    status = bladerf_lms_write(dev, base + 0x03, val);
    if (status != 0) {
        return status;
    }

    val &= ~(1 << 5);
    status = bladerf_lms_write(dev, base + 0x03, val);
    if (status != 0) {
        return status;
    }

    control = val;

    /* Main loop checking the calibration */
    for (i = 0 ; i < max_cal_count; i++) {
        /* Read active low DC_CLBR_DONE */
        status = bladerf_lms_read(dev, base + 0x01, &val);
        if (status != 0) {
            return status;
        }

        if (((val >> 1) & 1) == 0) {
            /* We think we're done, but we need to check DC_LOCK */
            if (((val >> 2) & 7) != 0 && ((val >> 2) & 7) != 7) {
                log_debug("Converged in %d iterations for %2x:%2x\n", i + 1,
                          base, cal_address );
                done = true;
                break;
            } else {
                log_debug( "DC_CLBR_DONE but no DC_LOCK - rekicking\n" );

                control |= (1 << 5);
                status = bladerf_lms_write(dev, base + 0x03, control);
                if (status != 0) {
                    return status;
                }

                control &= ~(1 << 5);
                status =bladerf_lms_write(dev, base + 0x03, control);
                if (status != 0) {
                    return status;
                }
            }
        }
    }

    if (done == false) {
        log_warning("Never converged - DC_CLBR_DONE: %d DC_LOCK: %d\n",
                    (val >> 1) & 1, (val >> 2) & 7);
        status = BLADERF_ERR_UNEXPECTED;
    } else {
        /* See what the DC register value is and return it to the caller */
        status = bladerf_lms_read(dev, base, dc_regval);
        if (status == 0) {
            *dc_regval &= 0x3f;
            log_debug( "DC_REGVAL: %d\n", *dc_regval );
        }
    }

    return status;
}

int lms_calibrate_dc(struct bladerf *dev, bladerf_cal_module module)
{
    int status;

    /* Working variables */
    uint8_t cal_clock, base, addrs, i, val, dc_regval;

    /* Saved values that are to be restored */
    uint8_t clockenables, reg0x71, reg0x7c;
    bladerf_lna_gain lna_gain;
    int rxvga1, rxvga2;

    /* Save off the top level clock enables */
    status = bladerf_lms_read(dev, 0x09, &clockenables);
    if (status != 0) {
        return status;
    }

    val = clockenables;
    cal_clock = 0 ;
    switch (module) {
        case BLADERF_DC_CAL_LPF_TUNING:
            cal_clock = (1 << 5);  /* CLK_EN[5] - LPF CAL Clock */
            base = 0x00;
            addrs = 1;
            break;

        case BLADERF_DC_CAL_TX_LPF:
            cal_clock = (1 << 1);  /* CLK_EN[1] - TX LPF DCCAL Clock */
            base = 0x30;
            addrs = 2;
            break;

        case BLADERF_DC_CAL_RX_LPF:
            cal_clock = (1 << 3);  /* CLK_EN[3] - RX LPF DCCAL Clock */
            base = 0x50;
            addrs = 2;
            break;

        case BLADERF_DC_CAL_RXVGA2:
            cal_clock = (1 << 4);  /* CLK_EN[4] - RX VGA2 DCCAL Clock */
            base = 0x60;
            addrs = 5;
            break;

        default:
            return BLADERF_ERR_INVAL;
    }

    /* Enable the appropriate clock based on the module */
    status = bladerf_lms_write(dev, 0x09, clockenables | cal_clock);
    if (status != 0) {
        return status;
    }

    /* Special case for RX LPF or RX VGA2 */
    if (module == BLADERF_DC_CAL_RX_LPF || module == BLADERF_DC_CAL_RXVGA2) {

        /* Connect LNA to the external pads and interally terminate */
        status = bladerf_lms_read(dev, 0x71, &reg0x71);
        if (status != 0) {
            return status;
        }

        val = reg0x71;
        val &= ~(1 << 7);

        status = bladerf_lms_write(dev, 0x71, val);
        if (status != 0) {
            return status;
        }

        status = bladerf_lms_read(dev, 0x7c, &reg0x7c);
        if (status != 0) {
            return status;
        }

        val = reg0x7c;
        val |= (1 << 2);

        status = bladerf_lms_write(dev, 0x7c, val);
        if (status != 0) {
            return status;
        }

        /* Set maximum gain for everything, but save off current values */
        status = bladerf_get_lna_gain(dev, &lna_gain);
        if (status != 0) {
            return status;
        }

        status = bladerf_set_lna_gain(dev, BLADERF_LNA_GAIN_MAX);
        if (status != 0) {
            return status;
        }

        status = bladerf_get_rxvga1(dev, &rxvga1);
        if (status != 0) {
            return status;
        }

        status = bladerf_set_rxvga1(dev, BLADERF_RXVGA1_GAIN_MAX);
        if (status != 0) {
            return status;
        }

        status = bladerf_get_rxvga2(dev, &rxvga2);
        if (status != 0) {
            return status;
        }

       status = bladerf_set_rxvga2(dev, BLADERF_RXVGA2_GAIN_MAX);
       if (status != 0) {
           return status;
       }
    }

    /* Figure out number of addresses to calibrate based on module */
    for (i = 0; i < addrs ; i++) {
        status = lms_dc_cal_loop(dev, base, i, &dc_regval) ;
        if (status != 0) {
            return status;
        }
    }

    /* Special case for LPF tuning module where results are
     * written to TX/RX LPF DCCAL */
    if (module == BLADERF_DC_CAL_LPF_TUNING) {

        /* Set the DC level to RX and TX DCCAL modules */
        status = bladerf_lms_read(dev, 0x35, &val);
        if (status == 0) {
            val &= ~(0x3f);
            val |= dc_regval;
            status = bladerf_lms_write(dev, 0x35, val);
        }

        if (status != 0) {
            return status;
        }

        status = bladerf_lms_read(dev, 0x55, &val);
        if (status == 0) {
            val &= ~(0x3f);
            val |= dc_regval;
            status = bladerf_lms_write(dev, 0x55, val);
        }

        if (status != 0) {
            return status;
        }

    /* Special case for RX LPF or RX VGA2 */
    } else if (module == BLADERF_DC_CAL_RX_LPF ||
               module == BLADERF_DC_CAL_RXVGA2) {

        /* Restore previously saved LNA Gain, VGA1 gain and VGA2 gain */
        status = bladerf_set_rxvga2(dev, rxvga2);
        if (status != 0) {
            return status;
        }

        status = bladerf_set_rxvga1(dev, rxvga1);
        if (status != 0) {
            return status;
        }

        status = bladerf_set_lna_gain(dev, lna_gain);
        if (status != 0) {
            return status;
        }

        status = bladerf_lms_write(dev, 0x71, reg0x71);
        if (status != 0) {
            return status;
        }

        status = bladerf_lms_write(dev, 0x7c, reg0x7c);
        if (status != 0) {
            return status;
        }
    }

    /* Restore original clock enables */
    status = bladerf_lms_write(dev, 0x09, clockenables);
    return status;
}

int lms_select_band(struct bladerf *dev, bladerf_module module,
                    unsigned int freq)
{
    int status;
    lms_lna lna;
    lms_pa pa;

    /* If loopback mode disabled, avoid changing the PA or LNA selection,
     * as these need to remain are powered down or disabled */
    status = is_loopback_enabled(dev);
    if (status < 0) {
        return status;
    } else if (status > 0) {
        return 0;
    }

    lna = (freq >= BLADERF_BAND_HIGH) ? LNA_2 : LNA_1;
    pa  = (freq >= BLADERF_BAND_HIGH) ? PA_2 : PA_1;

    if (module == BLADERF_MODULE_TX) {
        status = lms_select_pa(dev, pa);
    } else {
        status = lms_select_lna(dev, lna);
    }

    return status;
}
