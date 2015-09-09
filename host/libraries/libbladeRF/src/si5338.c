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
#include <string.h>
#include <inttypes.h>
#include "rel_assert.h"

#include "libbladeRF.h"
#include "si5338.h"
#include "host_config.h"
#include "bladerf_priv.h"
#include "log.h"

#define SI5338_EN_A     0x01
#define SI5338_EN_B     0x02

#define SI5338_F_VCO    (38400000UL * 66UL)

/**
 * Configure a multisynth for either the RX/TX sample clocks (index=1 or 2)
 * or for the SMB output (index=3).
 */
int si5338_set_rational_multisynth(struct bladerf *dev,
                                   uint8_t index, uint8_t channel,
                                   struct bladerf_rational_rate *rate,
                                   struct bladerf_rational_rate *actual);

/**
 * This is used set or recreate the si5338 frequency
 * Each si5338 multisynth module can be set independently
 */
struct si5338_multisynth {
    /* Multisynth to program (0-3) */
    uint8_t index;

    /* Base address of the multisynth */
    uint16_t base;

    /* Requested and actual sample rates */
    struct bladerf_rational_rate requested;
    struct bladerf_rational_rate actual;

    /* Enables for A and/or B outputs */
    uint8_t enable;

    /* f_out = fvco / (a + b/c) / r */
    uint32_t a, b, c, r;

    /* (a, b, c) in multisynth (p1, p2, p3) form */
    uint32_t p1, p2, p3;

    /* (p1, p2, p3) in register form */
    uint8_t regs[10];
};

void si5338_read_error(int error, const char *s)
{
    log_debug("Could not read from si5338 (%d): %s\n", error, s);
    return;
}

void si5338_write_error(int error, const char *s)
{
    log_debug("Could not write to si5338 (%d): %s\n", error, s);
    return;
}

static uint64_t si5338_gcd(uint64_t a, uint64_t b)
{
    uint64_t t;
    while (b != 0) {
        t = b;
        b = a % t;
        a = t;
    }
    return a;
}

static void si5338_rational_reduce(struct bladerf_rational_rate *r)
{
    int64_t val;

    if ((r->den > 0) && (r->num >= r->den)) {
        /* Get whole number */
        uint64_t whole = r->num / r->den;
        r->integer += whole;
        r->num = r->num - whole*r->den;
    }

    /* Reduce fraction */
    val = si5338_gcd(r->num, r->den);
    if (val > 0) {
        r->num /= val;
        r->den /= val;
    }

    return ;
}

static void si5338_rational_double(struct bladerf_rational_rate *r)
{
    r->integer *= 2;
    r->num *= 2;
    si5338_rational_reduce(r);
    return;
}

/**
 * Update the base address of the selected multisynth
 */
static void si5338_update_base(struct si5338_multisynth *ms)
{
    ms->base = 53 + ms->index*11 ;
    return;
}

/**
 * Unpack the recently read registers into (p1, p2, p3) and (a, b, c)
 *
 * Precondition:
 *  regs[10] and r have been read
 *
 * Post-condition:
 *  (p1, p2, p3), (a, b, c) and actual are populated
 */
static void si5338_unpack_regs(struct si5338_multisynth *ms)
{
    uint64_t temp;

    /* Zeroize */
    ms->p1 = ms->p2 = ms->p3 = 0;

    /* Populate */
    ms->p1 =                                ((ms->regs[2]&3)<<16) | (ms->regs[1]<<8) | (ms->regs[0]);
    ms->p2 = (ms->regs[5]<<22)          |       (ms->regs[4]<<14) | (ms->regs[3]<<6) | ((ms->regs[2]>>2)&0x3f);
    ms->p3 = ((ms->regs[9]&0x3f)<<24)   |       (ms->regs[8]<<16) | (ms->regs[7]<<8) | (ms->regs[6]);

    log_verbose("Unpacked P1: 0x%8.8x (%u) P2: 0x%8.8x (%u) P3: 0x%8.8x (%u)\n",
                ms->p1, ms->p1, ms->p2, ms->p2, ms->p3, ms->p3);

    /* c = p3 */
    ms->c = ms->p3;

    /* a =  (p1+512)/128
     *
     * NOTE: The +64 is for rounding purposes.
     */
    ms->a = (ms->p1+512)/128;

    /* b = (((p1+512)-128*a)*c + (b % c) + 64)/128 */
    temp = (ms->p1+512)-128*(uint64_t)ms->a;
    temp = (temp * ms->c) + ms->p2;
    temp = (temp + 64) / 128;
    assert(temp <= UINT32_MAX);
    ms->b = (uint32_t)temp;

    log_verbose("Unpacked a + b/c: %d + %d/%d\n", ms->a, ms->b, ms->c);
    log_verbose("Unpacked r: %d\n", ms->r);
}

/*
 * Pack (a, b, c, r) into (p1, p2, p3) and regs[]
 */
static void si5338_pack_regs(struct si5338_multisynth *ms)
{
    /* Precondition:
     *  (a, b, c) and r have been populated
     *
     * Post-condition:
     *  (p1, p2, p3) and regs[10] are populated
     */

    /* p1 = (a * c + b) * 128 / c - 512 */
    uint64_t temp;
    temp = (uint64_t)ms->a * ms->c + ms->b;
    temp = temp * 128 ;
    temp = temp / ms->c - 512;
    assert(temp <= UINT32_MAX);
    ms->p1 = (uint32_t)temp;
    //ms->p1 = ms->a * ms->c + ms->b;
    //ms->p1 = ms->p1 * 128;
    //ms->p1 = ms->p1 / ms->c - 512;

    /* p2 = (b * 128) % c */
    temp = (uint64_t)ms->b * 128;
    temp = temp % ms->c;
    assert(temp <= UINT32_MAX);
    ms->p2 = (uint32_t)temp;

    /* p3 = c */
    ms->p3 = ms->c;

    log_verbose("MSx P1: 0x%8.8x (%u) P2: 0x%8.8x (%u) P3: 0x%8.8x (%u)\n",
                ms->p1, ms->p1, ms->p2, ms->p2, ms->p3, ms->p3);

    /* Regs */
    ms->regs[0] = ms->p1 & 0xff;
    ms->regs[1] = (ms->p1 >> 8) & 0xff;
    ms->regs[2] = ((ms->p2 & 0x3f) << 2) | ((ms->p1 >> 16) & 0x3);
    ms->regs[3] = (ms->p2 >> 6) & 0xff;
    ms->regs[4] = (ms->p2 >> 14) & 0xff;
    ms->regs[5] = (ms->p2 >> 22) & 0xff;
    ms->regs[6] = ms->p3 & 0xff;
    ms->regs[7] = (ms->p3 >> 8) & 0xff;
    ms->regs[8] = (ms->p3 >> 16) & 0xff;
    ms->regs[9] = (ms->p3 >> 24) & 0xff;

    return ;
}

static int si5338_write_multisynth(struct bladerf *dev,
                                   struct si5338_multisynth *ms)
{
    int i, status;
    uint8_t r_power, r_count, val;

    log_verbose("Writing MS%d\n", ms->index);

    /* Write out the enables */
    status = SI5338_READ(dev, 36 + ms->index, &val);
    if (status < 0) {
        si5338_read_error(status, bladerf_strerror(status));
        return status;
    }
    val |= ms->enable;
    log_verbose("Wrote enable register: 0x%2.2x\n", val);
    status = SI5338_WRITE(dev, 36 + ms->index, val);
    if (status < 0) {
        si5338_write_error(status, bladerf_strerror(status));
        return status;
    }

    /* Write out the registers */
    for (i = 0 ; i < 10 ; i++) {
        status = SI5338_WRITE(dev, ms->base + i, *(ms->regs+i));
        if (status < 0) {
            si5338_write_error(status, bladerf_strerror(status));
            return status;
        }
        log_verbose("Wrote regs[%d]: 0x%2.2x\n", i, *(ms->regs+i));
    }

    /* Calculate r_power from c_count */
    r_power = 0;
    r_count = ms->r >> 1 ;
    while (r_count > 0) {
        r_count >>= 1;
        r_power++;
    }

    /* Set the r value to the log2(r_count) to match Figure 18 */
    val = 0xc0;
    val |= (r_power<<2);

    log_verbose("Wrote r register: 0x%2.2x\n", val);

    status = SI5338_WRITE(dev, 31 + ms->index, val);
    if (status < 0) {
        si5338_write_error(status, bladerf_strerror(status));
    }

    return status ;
}

static int si5338_read_multisynth(struct bladerf *dev,
                                  struct si5338_multisynth *ms)
{
    int i, status;
    uint8_t val;

    log_verbose("Reading MS%d\n", ms->index);

    /* Read the enable bits */
    status = SI5338_READ(dev, 36 + ms->index, &val);
    if (status < 0) {
        si5338_read_error(status, bladerf_strerror(status));
        return status ;
    }
    ms->enable = val&7;
    log_verbose("Read enable register: 0x%2.2x\n", val);

    /* Read all of the multisynth registers */
    for (i = 0; i < 10; i++) {
        status = SI5338_READ(dev, ms->base + i, ms->regs+i);
        if (status < 0) {
            si5338_read_error(status, bladerf_strerror(status));
            return status;
        }
        log_verbose("Read regs[%d]: 0x%2.2x\n", i, *(ms->regs+i));
    }

    /* Populate the RxDIV value from the register */
    status = SI5338_READ(dev, 31 + ms->index, &val);
    if (status < 0) {
        si5338_read_error(status, bladerf_strerror(status));
        return status;
    }
    /* RxDIV is stored as a power of 2, so restore it on readback */
    log_verbose("Read r register: 0x%2.2x\n", val);
    val = (val>>2)&7;
    ms->r = (1<<val);

    /* Unpack the regs into appropriate values */
    si5338_unpack_regs(ms) ;

    return 0;
}

static void si5338_calculate_ms_freq(struct si5338_multisynth *ms,
                                     struct bladerf_rational_rate *rate)
{
    struct bladerf_rational_rate abc;
    abc.integer = ms->a;
    abc.num = ms->b;
    abc.den = ms->c;

    rate->integer = 0;
    rate->num = SI5338_F_VCO * abc.den;
    rate->den = (uint64_t)ms->r*(abc.integer * abc.den + abc.num);

    /* Compensate for doubling of frequency for LMS sampling clocks */
    if(ms->index == 1 || ms->index == 2) {
        rate->den *= 2;
    }

    si5338_rational_reduce(rate);

    log_verbose("Calculated multisynth frequency: %"
                PRIu64" + %"PRIu64"/%"PRIu64"\n",
                rate->integer, rate->num, rate->den);

    return;
}

static int si5338_calculate_multisynth(struct si5338_multisynth *ms,
                                       struct bladerf_rational_rate *rate)
{

    struct bladerf_rational_rate req;
    struct bladerf_rational_rate abc;
    uint8_t r_value, r_power;

    /* Don't muss with the users data */
    req = *rate;

    /* Double requested frequency for sample clocks since LMS requires
     * 2:1 clock:sample rate
     */
    if(ms->index == 1 || ms->index == 2) {
        si5338_rational_double(&req);
    }

    /* Find a suitable R value */
    r_value = 1;
    r_power = 0;
    while (req.integer < 5000000 && r_value < 32) {
        si5338_rational_double(&req);
        r_value <<= 1;
        r_power++;
    }

    if (r_value == 32 && req.integer < 5000000) {
        log_debug("Sample rate requires r > 32\n");
        return BLADERF_ERR_INVAL;
    } else {
        log_verbose("Found r value of: %d\n", r_value);
    }

    /* Find suitable MS (a, b, c) values */
    abc.integer = 0;
    abc.num = SI5338_F_VCO * req.den;
    abc.den = req.integer * req.den + req.num;
    si5338_rational_reduce(&abc);

    log_verbose("MSx a + b/c: %"PRIu64" + %"PRIu64"/%"PRIu64"\n",
                abc.integer, abc.num, abc.den);

    /* Check values to make sure they are OK */
    if (abc.integer < 8) {
        switch (abc.integer) {
            case 0:
            case 1:
            case 2:
            case 3:
            case 5:
            case 7:
                log_debug("Integer portion too small: %"PRIu64"\n", abc.integer);
                return BLADERF_ERR_INVAL;
        }
    } else if (abc.integer > 567) {
        log_debug("Integer portion too large: %"PRIu64"\n", abc.integer);
        return BLADERF_ERR_INVAL;
    }

    /* Loss of precision if num or den are greater than 2^30-1 */
    while (abc.num > (1<<30) || abc.den > (1<<30) ) {
        log_debug("Loss of precision in reducing fraction from "
                  "%"PRIu64"/%"PRIu64" to %"PRIu64"/%"PRIu64"\n",
                  abc.num, abc.den, abc.num>>1, abc.den>>1);
        abc.num >>= 1;
        abc.den >>= 1;
    }

    log_verbose("MSx a + b/c: %"PRIu64" + %"PRIu64"/%"PRIu64"\n",
                abc.integer, abc.num, abc.den);

    /* Set it in the multisynth */
    assert(abc.integer <= UINT32_MAX);
    assert(abc.num <= UINT32_MAX);
    assert(abc.den <= UINT32_MAX);
    ms->a = (uint32_t)abc.integer;
    ms->b = (uint32_t)abc.num;
    ms->c = (uint32_t)abc.den;
    ms->r = r_value;

    /* Pack the registers */
    si5338_pack_regs(ms);

    return 0;
}

int si5338_set_rational_sample_rate(struct bladerf *dev, bladerf_module module,
                                    struct bladerf_rational_rate *rate,
                                    struct bladerf_rational_rate *actual)
{
    uint8_t index = (module == BLADERF_MODULE_RX) ? 1 : 2;
    uint8_t channel = SI5338_EN_A;

    /* Enforce minimum sample rate */
    si5338_rational_reduce(rate);
    if (rate->integer < BLADERF_SAMPLERATE_MIN) {
        log_debug("%s: provided sample rate violates minimum\n", __FUNCTION__);
        return BLADERF_ERR_INVAL;
    }

    if (module == BLADERF_MODULE_TX) {
        channel |= SI5338_EN_B;
    }

    return si5338_set_rational_multisynth(dev, index, channel, rate, actual);
}

int si5338_set_rational_smb_freq(struct bladerf *dev,
                                 struct bladerf_rational_rate *rate,
                                 struct bladerf_rational_rate *actual)
{
    /* Enforce minimum and maximum frequencies */
    si5338_rational_reduce(rate);
    if (rate->integer < BLADERF_SMB_FREQUENCY_MIN) {
        log_debug("%s: provided SMB freq violates minimum\n", __FUNCTION__);
        return BLADERF_ERR_INVAL;
    } else if (rate->integer > BLADERF_SMB_FREQUENCY_MAX) {
        log_debug("%s: provided SMB freq violates maximum\n", __FUNCTION__);
        return BLADERF_ERR_INVAL;
    }

    return si5338_set_rational_multisynth(dev, 3, SI5338_EN_A, rate, actual);
}

int si5338_set_rational_multisynth(struct bladerf *dev,
                                   uint8_t index, uint8_t channel,
                                   struct bladerf_rational_rate *rate,
                                   struct bladerf_rational_rate *actual_ret)
{
    struct si5338_multisynth ms;
    struct bladerf_rational_rate req;
    struct bladerf_rational_rate actual;
    int status;

    si5338_rational_reduce(rate);

    /* Save off the value */
    req = *rate;

    /* Setup the multisynth enables and index */
    ms.index = index;
    ms.enable = channel;

    /* Update the base address register */
    si5338_update_base(&ms);

    /* Calculate multisynth values */
    status = si5338_calculate_multisynth(&ms, &req);
    if(status != 0) {
        return status;
    }

    /* Get the actual rate */
    si5338_calculate_ms_freq(&ms, &actual);
    if (actual_ret) {
        memcpy(actual_ret, &actual, sizeof(*actual_ret));
    }

    /* Program it to the part */
    status = si5338_write_multisynth(dev, &ms);

    /* Done */
    return status ;
}

int si5338_set_sample_rate(struct bladerf *dev, bladerf_module module,
                           uint32_t rate, uint32_t *actual)
{
    struct bladerf_rational_rate req, act;
    int status;

    memset(&act, 0, sizeof(act));
    log_verbose("Setting integer sample rate: %d\n", rate);
    req.integer = rate;
    req.num = 0;
    req.den = 1;

    status = si5338_set_rational_sample_rate(dev, module, &req, &act);

    if (status == 0 && act.num != 0) {
        log_info("Non-integer sample rate set from integer sample rate, "
                 "truncating output.\n");
    }

    assert(act.integer <= UINT32_MAX);

    if (actual) {
        *actual = (uint32_t)act.integer;
    }
    log_verbose("Set actual integer sample rate: %d\n", act.integer);

    return status ;
}

int si5338_set_smb_freq(struct bladerf *dev, uint32_t rate, uint32_t *actual)
{
    struct bladerf_rational_rate req, act;
    int status;

    memset(&act, 0, sizeof(act));
    log_verbose("Setting integer SMB frequency: %d\n", rate);
    req.integer = rate;
    req.num = 0;
    req.den = 1;

    status = si5338_set_rational_smb_freq(dev, &req, &act);

    if (status == 0 && act.num != 0) {
        log_info("Non-integer SMB frequency set from integer frequency, "
                 "truncating output.\n");
    }

    assert(act.integer <= UINT32_MAX);

    if (actual) {
        *actual = (uint32_t)act.integer;
    }
    log_verbose("Set actual integer SMB frequency: %d\n", act.integer);

    return status;
}

int si5338_get_rational_sample_rate(struct bladerf *dev, bladerf_module module,
                                    struct bladerf_rational_rate *rate)
{

    struct si5338_multisynth ms;
    int status;

    /* Select the multisynth we want to read */
    ms.index = (module == BLADERF_MODULE_RX) ? 1 : 2;

    /* Update the base address */
    si5338_update_base(&ms);

    /* Readback */
    status = si5338_read_multisynth(dev, &ms);

    if (status) {
        si5338_read_error(status, bladerf_strerror(status));
        return status;
    }

    si5338_calculate_ms_freq(&ms, rate);

    return 0;
}

int si5338_get_rational_smb_freq(struct bladerf *dev,
                                 struct bladerf_rational_rate *rate)
{
    struct si5338_multisynth ms;
    int status;

    /* Select MS3 for the SMB output */
    ms.index = 3;
    si5338_update_base(&ms);

    status = si5338_read_multisynth(dev, &ms);

    if (status) {
        si5338_read_error(status, bladerf_strerror(status));
        return status;
    }

    si5338_calculate_ms_freq(&ms, rate);

    return 0;
}

int si5338_get_sample_rate(struct bladerf *dev, bladerf_module module,
                           unsigned int *rate)
{
    struct bladerf_rational_rate actual;
    int status;

    status = si5338_get_rational_sample_rate(dev, module, &actual);

    if (status) {
        si5338_read_error(status, bladerf_strerror(status));
        return status;
    }

    if (actual.num != 0) {
        log_debug("Fractional sample rate truncated during integer sample rate"
                  "retrieval\n");
    }

    assert(actual.integer <= UINT_MAX);
    *rate = (unsigned int)actual.integer;

    return 0;
}

int si5338_get_smb_freq(struct bladerf *dev, unsigned int *rate)
{
    struct bladerf_rational_rate actual;
    int status;

    status = si5338_get_rational_smb_freq(dev, &actual);

    if (status) {
        si5338_read_error(status, bladerf_strerror(status));
        return status;
    }

    if (actual.num != 0) {
        log_debug("Fractional SMB frequency truncated during integer SMB"
                  " frequency retrieval\n");
    }

    assert(actual.integer <= UINT_MAX);
    *rate = (unsigned int)actual.integer;

    return 0;
}
