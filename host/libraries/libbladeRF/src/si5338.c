#include <string.h>
#include <inttypes.h>

#include "libbladeRF.h"
#include "bladerf_priv.h"
#include "log.h"

#define SI5338_EN_A     0x01
#define SI5338_EN_B     0x02

#define SI5338_F_VCO    (38400000UL * 66UL)

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
    log_error( "Could not read from si5338 (%d): %s\n", error, s );
    return;
}

void si5338_write_error(int error, const char *s)
{
    log_error( "Could not write to si5338 (%d): %s\n", error, s );
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

    if (r->num >= r->den) {
        /* Get whole number */
        uint64_t whole = r->num / r->den;
        r->integer += whole;
        r->num = r->num - whole*r->den;
    }

    /* Reduce fraction */
    val = si5338_gcd(r->num, r->den);
    r->num /= val;
    r->den /= val;

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

    log_debug( "Unpacked P1: 0x%8.8x (%u) P2: 0x%8.8x (%u) P3: 0x%8.8x (%u)\n", ms->p1, ms->p1, ms->p2, ms->p2, ms->p3, ms->p3 );

    /* c = p3 */
    ms->c = ms->p3;

    /* a =  (p1+512)/128
     *
     * NOTE: The +64 is for rounding purposes.
     */
    ms->a = (ms->p1+512)/128;

    /* b = (((p1+512)-128*a)*c + (b % c) + 64)/128 */
    temp = (ms->p1+512)-128*ms->a;
    temp = (temp * ms->c) + ms->p2;
    temp = (temp + 64) / 128;
    ms->b = temp;

    log_debug( "Unpacked a + b/c: %d + %d/%d\n", ms->a, ms->b, ms->c );
    log_debug( "Unpacked r: %d\n", ms->r );
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
    temp = ms->a * ms->c + ms->b;
    temp = temp * 128 ;
    temp = temp / ms->c - 512;
    ms->p1 = temp;
    //ms->p1 = ms->a * ms->c + ms->b;
    //ms->p1 = ms->p1 * 128;
    //ms->p1 = ms->p1 / ms->c - 512;

    /* p2 = (b * 128) % c */
    ms->p2 = ms->b * 128;
    ms->p2 = ms->p2 % ms->c;

    /* p3 = c */
    ms->p3 = ms->c;

    log_info( "MSx P1: 0x%8.8x (%u) P2: 0x%8.8x (%u) P3: 0x%8.8x (%u)\n", ms->p1, ms->p1, ms->p2, ms->p2, ms->p3, ms->p3 );

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

static int si5338_write_multisynth(struct bladerf *dev, struct si5338_multisynth *ms)
{
    int i, status;
    uint8_t r_power, r_count, val;

    log_debug( "Writing MS%d\n", ms->index );

    /* Write out the enables */
    status = bladerf_si5338_read(dev, 36 + ms->index, &val);
    if (status < 0) {
        si5338_read_error(status, bladerf_strerror(status));
        return status;
    }
    val &= ~(7);
    val |= ms->enable;
    log_debug( "Wrote enable register: 0x%2.2x\n", val );
    status = bladerf_si5338_write(dev, 36 + ms->index, val);
    if (status < 0) {
        si5338_write_error(status, bladerf_strerror(status));
        return status;
    }

    /* Write out the registers */
    for (i = 0 ; i < 10 ; i++) {
        status = bladerf_si5338_write(dev, ms->base + i, *(ms->regs+i));
        if (status < 0) {
            si5338_write_error(status, bladerf_strerror(status));
            return status;
        }
        log_debug( "Wrote regs[%d]: 0x%2.2x\n", i, *(ms->regs+i) );
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

    log_debug( "Wrote r register: 0x%2.2x\n", val );

    status = bladerf_si5338_write(dev, 31 + ms->index, val);
    if (status < 0) {
        si5338_write_error(status, bladerf_strerror(status));
    }

    return status ;
}

static int si5338_read_multisynth(struct bladerf *dev, struct si5338_multisynth *ms)
{
    int i, status;
    uint8_t val;

    log_debug( "Reading MS%d\n", ms->index );

    /* Read the enable bits */
    status = bladerf_si5338_read(dev, 36 + ms->index, &val);
    if (status < 0) {
        si5338_read_error(status, bladerf_strerror(status));
        return status ;
    }
    ms->enable = val&7;
    log_debug( "Read enable register: 0x%2.2x\n", val );

    /* Read all of the multisynth registers */
    for (i = 0; i < 10; i++) {
        status = bladerf_si5338_read(dev, ms->base + i, ms->regs+i);
        if (status < 0) {
            si5338_read_error(status, bladerf_strerror(status));
            return status;
        }
        log_debug( "Read regs[%d]: 0x%2.2x\n", i, *(ms->regs+i) );
    }

    /* Populate the RxDIV value from the register */
    status = bladerf_si5338_read(dev, 31 + ms->index, &val);
    if (status < 0) {
        si5338_read_error(status, bladerf_strerror(status));
        return status;
    }
    /* RxDIV is stored as a power of 2, so restore it on readback */
    log_debug( "Read r register: 0x%2.2x\n", val );
    val = (val>>2)&7;
    ms->r = (1<<val);

    /* Unpack the regs into appropriate values */
    si5338_unpack_regs(ms) ;

    return 0;
}

static void si5338_calculate_samplerate(struct si5338_multisynth *ms, struct bladerf_rational_rate *rate)
{
    struct bladerf_rational_rate abc;
    abc.integer = ms->a;
    abc.num = ms->b;
    abc.den = ms->c;

    rate->integer = 0;
    rate->num = SI5338_F_VCO * abc.den;
    rate->den = (uint64_t)ms->r*2*(abc.integer * abc.den + abc.num);
    si5338_rational_reduce(rate);

    log_info( "Calculated samplerate: %"PRIu64" + %"PRIu64"/%"PRIu64"\n", rate->integer, rate->num, rate->den );

    return;
}

static int si5338_calculate_multisynth(struct si5338_multisynth *ms, struct bladerf_rational_rate *rate)
{

    struct bladerf_rational_rate req;
    struct bladerf_rational_rate abc;
    uint8_t r_value, r_power;

    /* Don't muss with the users data */
    req = *rate;

    /* Double requested frequency since LMS requires 2:1 clock:sample rate */
    si5338_rational_double(&req);

    /* Find a suitable R value */
    r_value = 1;
    r_power = 0;
    while (req.integer < 5000000 && r_value < 32) {
        si5338_rational_double(&req);
        r_value <<= 1;
        r_power++;
    }

    if (r_value == 32 && req.integer < 5000000) {
        log_error( "Sample rate requires r > 32\n" );
        return BLADERF_ERR_INVAL;
    } else {
        log_info( "Found r value of: %d\n", r_value );
    }

    /* Find suitable MS (a, b, c) values */
    abc.integer = 0;
    abc.num = SI5338_F_VCO * req.den;
    abc.den = req.integer * req.den + req.num;
    si5338_rational_reduce(&abc);

    log_info( "MSx a + b/c: %"PRIu64" + %"PRIu64"/%"PRIu64"\n", abc.integer, abc.num, abc.den );

    /* Check values to make sure they are OK */
    if (abc.integer < 8) {
        switch (abc.integer) {
            case 0:
            case 1:
            case 2:
            case 3:
            case 5:
            case 7:
                log_error( "Integer portion too small: %"PRIu64"\n", abc.integer );
                return BLADERF_ERR_INVAL;
        }
    } else if (abc.integer > 567) {
        log_error( "Integer portion too large: %"PRIu64"\n", abc.integer );
        return BLADERF_ERR_INVAL;
    }

    /* Loss of precision if num or den are greater than 2^30-1 */
    while (abc.num > (1<<30) || abc.den > (1<<30) ) {
        log_warning( "Loss of precision in reducing fraction from %"PRIu64"/%"PRIu64" to %"PRIu64"/%"PRIu64"\n", abc.num, abc.den, abc.num>>1, abc.den>>1);
        abc.num >>= 1;
        abc.den >>= 1;
    }

    log_info( "MSx a + b/c: %"PRIu64" + %"PRIu64"/%"PRIu64"\n", abc.integer, abc.num, abc.den );

    /* Set it in the multisynth */
    ms->a = abc.integer;
    ms->b = abc.num;
    ms->c = abc.den;
    ms->r = r_value;

    /* Pack the registers */
    si5338_pack_regs(ms);

    return 0;
}

int si5338_set_rational_sample_rate(struct bladerf *dev, bladerf_module module, struct bladerf_rational_rate *rate, struct bladerf_rational_rate *actual)
{
    struct si5338_multisynth ms;
    struct bladerf_rational_rate req;
    int status;

    /* Save off the value */
    req = *rate;

    /* Setup the multisynth enables and index */
    ms.enable = SI5338_EN_A;
    if (module == BLADERF_MODULE_TX) {
        ms.enable |= SI5338_EN_B;
    }

    /* Set the multisynth module we're dealing with */
    ms.index = (module == BLADERF_MODULE_RX) ? 1 : 2;

    /* Update the base address register */
    si5338_update_base(&ms);

    /* Calculate multisynth values */
    status = si5338_calculate_multisynth(&ms, &req);
    if(status != 0) {
        return status;
    }

    /* Get the actual rate */
    si5338_calculate_samplerate(&ms, actual);

    /* Program it to the part */
    status = si5338_write_multisynth(dev, &ms);

    /* Done */
    return status ;
}

int si5338_set_sample_rate(struct bladerf *dev, bladerf_module module, uint32_t rate, uint32_t *actual)
{
    struct bladerf_rational_rate req, act;
    int status;

    memset(&act, 0, sizeof(act));
    log_info( "Setting integer sample rate: %d\n", rate );
    req.integer = rate;
    req.num = 0;
    req.den = 1;

    status = si5338_set_rational_sample_rate(dev, module, &req, &act);

    if (status == 0 && act.num != 0) {
        log_warning( "Non-integer sample rate set from integer sample rate, truncating output\n" );
    }

    *actual = act.integer;
    log_info( "Set actual integer sample rate: %d\n", act.integer );

    return status ;
}

int si5338_get_rational_sample_rate(struct bladerf *dev, bladerf_module module, struct bladerf_rational_rate *rate)
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
        si5338_read_error( status, bladerf_strerror(status) );
        return status;
    }

    si5338_calculate_samplerate(&ms, rate);

    return 0;
}

int si5338_get_sample_rate(struct bladerf *dev, bladerf_module module, unsigned int *rate)
{
    struct bladerf_rational_rate actual;
    int status;

    status = si5338_get_rational_sample_rate(dev, module, &actual);

    if (status) {
        si5338_read_error( status, bladerf_strerror(status) );
        return status;
    }

    if (actual.num != 0) {
        log_warning( "Fractional sample rate truncated during integer sample rate retrieval\n" );
    }

    *rate = actual.integer;

    return 0;
}

