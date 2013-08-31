#include <string.h>

#include "libbladeRF.h"
#include "bladerf_priv.h"
#include "debug.h"

#define SI5338_EN_A		0x01
#define SI5338_EN_B		0x02

#define SI5338_F_VCO   (38400000UL * 66UL)
#define SI5338_BC_MAX 	0x40000000   // see pag. 9 si5338-RM rev 1.2, value NOT included

// maybe there should be a better way to report error string ?
#define error_printf printf

/**
 * This is used set or recreate the si5338 frequency
 * Each si clock can be set independently
 */
struct si5338_clock {
    int clock_index;           // 0,1,2,3
    uint32_t sample_rate;      // if used as setter this is what I wish
    uint32_t numerator;        // for the rational functions, it is NOT b but has the same meaning
    uint32_t denominator;      // for the rational functions, it is NOT c but has the same meaning
    uint8_t  enable_port_bits; // You may want Just port A out or both port A & B
    uint32_t *f_outP;          // write result here

    // what follows are all calculated fields

    uint32_t sample_rate_r;    // the sample rate adjusted with the r multiplier
    uint32_t numerator_r;      // the numerator adjusted with the r multiplier

    uint32_t P1,P2,P3;         // as from section 5.2 of Reference manual
    uint32_t a,b,c,r;

    int clock_base;            // this can be gotten from block_index
    uint8_t regs[10];
    uint8_t raw_r;
};


/**
 * gets the basic regs from Si
 * @return 0 if all is fine or an error code
 */
static int si5338_get_sample_rate_regs ( struct bladerf *dev, struct si5338_clock *retP )
{
    int i,retcode;

    for (i = 0; i < 9; i++)  {
        if ( (retcode=bladerf_si5338_read(dev, retP->clock_base + i, retP->regs+i)) < 0 )    {
        	error_printf("Could not read from si5338 (%d): %s\n",retcode, bladerf_strerror(retcode));
            return retcode;
            }
        }

    if ( (retcode=bladerf_si5338_read(dev, 31 + retP->clock_index, &retP->raw_r)) < 0 ) {
    	error_printf("Could not read from si5338 (%d): %s\n",retcode, bladerf_strerror(retcode));
        return retcode;
    }

    return 0;
}

static unsigned int bytes_to_uint32 ( uint8_t msb, uint8_t ms1, uint8_t ms2, uint8_t lsb, uint8_t last_shift )
{
    unsigned int risul = msb;
    risul = (risul << 9) + ms1;
    risul = (risul << 8) + ms2;
    risul = (risul << last_shift) + lsb;
    return risul;
}

/**
 * verify that a value is within ranges
 * @return 0 if good an error code otherwise
 */
static int si5228_verify_a ( struct si5338_clock *clockP )
{
	switch (clockP->a)
		{
		case 4:
		case 6:
		case 8:
			return 0;

		default:
			break;
		}

	if ( clockP->a >=9 && clockP->a <= 567 ) return 0;

	error_printf("si5338 invalid a=%u for sample_rate_r=%u \n", clockP->a,clockP->sample_rate_r);

	return BLADERF_ERR_INVAL;
}

static void si5338_get_sample_rate_calc ( struct si5338_clock *retP )
{
    uint64_t c = retP->c = retP->P3;
    uint32_t p2 = retP->P2;
    uint32_t b=0;
    uint32_t p1 ;
    uint64_t A, a, f_twice, divisor ;
    uint32_t B ;
    int i;

    // c should never be zero, but if it is at least we do not crash
    if ( c == 0 ) return;

    for (i=1; i<128; i++) {
        B = c*i+p2;

        if ( (B % 128) == 0 ) {
            b = B / 128;
            retP->b = b;
            break;
        }
    }

    p1 = retP->P1;

    A = (p1 + 512);
    A = A * c;
    A = A - b * 128;
    A = (A * 10) / ( c * 128 );  // switch to fixed decimal point

    // get the integer value
    a = A / 10;

    // do manual rounding....
    if ( A % 10 > 5 ) a++;

    retP->a = a;

// step by step to avoid too much compiler optimization
    f_twice = SI5338_F_VCO * c;
    divisor = a * c + b;
    f_twice = f_twice / divisor;
    f_twice = f_twice / retP->r;

    retP->f_outP[0] = f_twice / 2;  // yes, compiler may optimize this to >> 1
}

static void print_clock (struct si5338_clock *clockP)
{
    int i;
    char buff[500];  // make sure it is big enough
    char *insP = buff;

    dbg_printf("want_freq....: %dHz\n", clockP->sample_rate);
    dbg_printf("numerator:     %d\n", clockP->numerator);
    dbg_printf("denominator:   %d\n", clockP->denominator);
    dbg_printf("si5338 r ....: %d\n", clockP->r);
    dbg_printf("sample_rate_r: %dHz\n", clockP->sample_rate_r);
    dbg_printf("numerator_r:   %d\n", clockP->numerator_r);

    dbg_printf("a:  %u\n", clockP->a);
    dbg_printf("b:  %u\n", clockP->b);
    dbg_printf("c:  %u\n", clockP->c);
    dbg_printf("p1: %x\n", clockP->P1);
    dbg_printf("p2: %x\n", clockP->P2);
    dbg_printf("p3: %x\n", clockP->P3);
    dbg_printf("clock_base: %d\n", clockP->clock_base);

    for (i = 0; i < 9; i++) {
    	int written = sprintf(insP,"[%d]=0x%.2x ", i, clockP->regs[i]);
    	if ( written < 0 ) break;
    	insP += written;
    }

    sprintf(insP,"[r]=0x%.2x", clockP->raw_r);

    dbg_printf("%s\n",buff);

    if ( clockP->f_outP ) dbg_printf("out_freq : %dHz\n", clockP->f_outP[0]);

}


static int si5338_get_sample_rate ( struct bladerf *dev, struct si5338_clock *clockP )
{
    int retcode;
    uint8_t *valP, shi;

    clockP->clock_base = 53 + clockP->clock_index * 11;

    // gets the raw sample rate regs
    if ( (retcode=si5338_get_sample_rate_regs(dev, clockP )) ) return retcode;

    // I now need to reverse the packing, see pag. 10 of reference manual
    valP = clockP->regs;
    clockP->P1 = bytes_to_uint32 ( 0, valP[2] & 0x03, valP[1], valP[0], 8);
    clockP->P2 = bytes_to_uint32 ( valP[5], valP[4], valP[3], valP[2] >> 2, 6 );
    clockP->P3 = bytes_to_uint32 ( valP[9] & 0x3F, valP[8], valP[7], valP[6], 8 );

    shi = (clockP->raw_r >> 2) & 0x03;
    clockP->r = 1 << shi;

    si5338_get_sample_rate_calc(clockP);

    print_clock(clockP);

    return 0;
}

/**
 * get the sample rate back from the Si5338
 * @param module the subsection I wish to know about
 * @param rateP pointer to result
 * @return 0 if all fine or an error code
 */
int bladerf_get_sample_rate (struct bladerf *dev, bladerf_module module, unsigned int *rateP )
{
    // safety check
    if ( rateP == NULL ) return BLADERF_ERR_UNEXPECTED;

    struct si5338_clock clock;

    memset(&clock,0,sizeof(clock));

    clock.f_outP = rateP;

    switch ( module ) {
        case BLADERF_MODULE_RX:
            clock.clock_index = 1;
            return si5338_get_sample_rate(dev,&clock);

        case BLADERF_MODULE_TX:
            clock.clock_index = 2;
            return si5338_get_sample_rate(dev,&clock);

        default:
            break;
    }

    return BLADERF_ERR_INVAL;
}

/**
 * Euclide algorithm to find gcd between two numbers, see wikipedia
 * @return the greater common divisor of the two numbers
 */
static uint32_t euclide_gcd(uint32_t n1, uint32_t n2)
{
        uint32_t a, b, mcd = 1;

        if (n1 > n2){
                a = n1;
                b = n2;
        } else {
                a = n2;
                b = n1;
        }

        if ((a % b) == 0)
                mcd = b;
        else
                mcd = euclide_gcd (b, a % b);

        return mcd;
}

/**
 * The hard part is to take care of the remaining bits of the rational part, I know that they are less than one Hz
 * I also know that they will surely increase the output frequency, maybe by a little, this means that
 * this extra bit of remainder actually adds to b and eventually subtract one from a
 * I need to find the contribution of 1 unit of samplerate_r since that is my "Hz" part
 * @return the new remainder after having added the rational part
 */
static uint32_t sample_rate_calc_rational_remainder ( struct si5338_clock *clockP, uint32_t remainder )
{
	if ( clockP->numerator_r == 0 ) return remainder;

	uint32_t sr1 = clockP->sample_rate_r+1;

    uint32_t a1 = SI5338_F_VCO / sr1;

    uint32_t rm1 = SI5338_F_VCO - (a1 * sr1);

    // the difference between rm1 and remainder is my contribution for one unit
    // it may happen that 1 unit trip over to the next a in any case one_usnit is always positive
    uint32_t one_unit = rm1 > remainder ? rm1 - remainder : remainder - rm1;

	// I now have to find how much of this "one unit" to take
    uint32_t contribution = clockP->numerator_r * one_unit / clockP->denominator;

    dbg_printf("one_unit=%d numerator=%d denominator=%d contribution=%d\n",one_unit,clockP->numerator_r,clockP->denominator,(uint32_t)contribution);

    // fractional part increase when output frequency increase
    contribution = remainder + contribution;

    if ( contribution >= clockP->sample_rate_r ) {
    	dbg_printf("contribution=%u >= than sample_rate_r=%u \n",contribution,clockP->sample_rate_r);
    	contribution -= clockP->sample_rate_r;
    	// it is a decrement since a is really in the divisor part
    	clockP->a--;
		}

    return contribution;
}


/**
 * Calculated a,b,c to satisfy the si equations
 * The difficult part is to be sure that params are within range to avoid possible overflow on arithmetic
 * blade has quite big VCO, so extra steps are needed to be sure that there is no overflow
 * see pag 9 of reference manual
 */
static int set_sample_rate_calc_abc ( struct si5338_clock *clockP )
{
	int errcode;

	// find out the integer part of the result
    clockP->a = SI5338_F_VCO / clockP->sample_rate_r;

    // fist verify of a
    if ( (errcode=si5228_verify_a(clockP)) ) return errcode;

    // I still have this subpart left that are not integer divisible by sample_rate_r
    uint32_t remainder = SI5338_F_VCO - (clockP->a * clockP->sample_rate_r);

    // calculate the new remainder using the rational part
    remainder = sample_rate_calc_rational_remainder(clockP,remainder);

    // second verify of a since the rational may have changed it
    if ( (errcode=si5228_verify_a(clockP)) ) return errcode;

    if ( remainder == 0 ) {
    	// Note: When MSn or MSx is an integer, you must set b = 0 and c = 1
    	clockP->b=0;
    	clockP->c=1;
    	return 0;
		}

    // the gcd is the biggest number that divides both as integers
    uint32_t gcd = euclide_gcd(clockP->sample_rate_r, remainder);

    // I now have to check if any of the b,c satisfy si conditions
    uint32_t b = remainder / gcd;

    if ( b >= SI5338_BC_MAX ) {
		error_printf("cannot find suitable b ratio %dHz on MS%d \n", clockP->sample_rate, clockP->clock_index);
		return BLADERF_ERR_INVAL;
    }

    uint32_t c = clockP->sample_rate_r / gcd;

    if ( c >= SI5338_BC_MAX ) {
		error_printf("cannot find suitable c ratio %dHz on MS%d \n", clockP->sample_rate, clockP->clock_index);
		return BLADERF_ERR_INVAL;
    }

    clockP->b = b;
    clockP->c = c;

    return 0;
}

/**
 * Calculates a r that satisfy si5338
 * This works even for rational sample rate since if anything the rational part adds to the main frequency
 * @return 0 if all is fine or an error code
 */
static int set_sample_rate_calc_r ( struct si5338_clock *portP )
{
	int j;

	for (j=0; j <= 5; j++ ) {
        // see pag. 27 of si5338 RM rev 1.2 1/13
		// find an R that makes this MS's fout > 5MHz

    	portP->r = 1 << j;   // this is the r I will be using
    	portP->raw_r = j;    // this is what goes into the chip

    	uint32_t candidate = portP->sample_rate * portP->r;

		if ( candidate >= 5000000)	{
			portP->sample_rate_r = candidate;
			break;
		    }
		}

	if ( ! portP->sample_rate_r ) {
		error_printf("Frequency %dHz is too low\n", portP->sample_rate);
		return BLADERF_ERR_INVAL;
	}

	// the multiplier applies even to the rational part
	portP->numerator_r = portP->numerator * portP->r;

	if ( portP->numerator_r > 0 )
		{
		// normalize again if required since numerator may be bigger
		portP->sample_rate_r = portP->sample_rate_r + (portP->numerator_r / portP->denominator);
		portP->numerator_r   = portP->numerator_r % portP->denominator;
		dbg_printf("numerator=%d denominator=%d\n",portP->numerator_r,portP->denominator);
		}

	return 0;
}

/**
 * calculate P1, P2, P3 based off page 9 in the Si5338 reference manual
 * make sure we have enough space to handle all cases
 */
static void set_sample_rate_calc_regs (struct si5338_clock *portP)
{
    // P1 = (a * c + b) * 128 / c - 512;
    uint64_t P1 = portP->a * portP->c + portP->b;
             P1 = P1 * 128;
             P1 = P1 / portP->c - 512;

    // P2 = (b * 128) % c;
    uint64_t P2 = (uint64_t)portP->b * 128;
             P2 = P2 % portP->c;

    uint32_t p1 = portP->P1 = P1;
    uint32_t p2 = portP->P2 = P2;
    uint32_t p3 = portP->P3 = portP->c;

    uint8_t *regs = portP->regs;
    regs[0] = p1 & 0xff;
    regs[1] = (p1 >> 8) & 0xff;
    regs[2] = ((p2 & 0x3f) << 2) | ((p1 >> 16) & 0x3);
    regs[3] = (p2 >> 6) & 0xff;
    regs[4] = (p2 >> 14) & 0xff;
    regs[5] = (p2 >> 22) & 0xff;
    regs[6] = p3 & 0xff;
    regs[7] = (p3 >> 8) & 0xff;
    regs[8] = (p3 >> 16) & 0xff;
    regs[9] = (p3 >> 24) & 0xff;
}

static int set_sample_rate_write_regs(struct bladerf *dev, struct si5338_clock *portP) {
    int i,errcode;

    if ( (errcode=bladerf_si5338_write(dev, 36 + portP->clock_index, portP->enable_port_bits )) ) return errcode;

    for (i = 0; i < 9; i++) {
    	if ( (errcode=bladerf_si5338_write(dev, portP->clock_base + i, portP->regs[i] )) ) return errcode;
    }

    return bladerf_si5338_write(dev, 31 + portP->clock_index, 0xC0 | (portP->raw_r << 2));
}

/**
 * This just convert the sample rate into a,b,c that is suitable
 * @return invalid params if frequency cannot be set
 */
static int set_sample_rate_A (struct bladerf *dev, struct si5338_clock *portP)
{
	int errcode;

    portP->clock_base = 53 + portP->clock_index * 11;

    // denominator can never be zero
	portP->denominator = 1;

	if ( (errcode=set_sample_rate_calc_r(portP)) ) return errcode;

	if ( (errcode=set_sample_rate_calc_abc(portP)) ) return errcode;

	set_sample_rate_calc_regs(portP);

	print_clock(portP);

	if ( (errcode=set_sample_rate_write_regs(dev, portP)) ) return errcode;

	if ( portP->f_outP ) return si5338_get_sample_rate(dev, portP);

	return 0;
}



/**
 * sets the sample rate as requested
 */
int bladerf_set_sample_rate(struct bladerf *dev, bladerf_module module, uint32_t rate, uint32_t *actualP)
{
    struct si5338_clock risul;

    memset(&risul,0,sizeof(risul));

    risul.f_outP = actualP;  // may be NULL
    risul.sample_rate = rate * 2;   // LMS needs one clock for I and one clock for Q

    switch ( module ) {
        case BLADERF_MODULE_RX:
            risul.clock_index = 1;
            risul.enable_port_bits = SI5338_EN_A;
            return set_sample_rate_A(dev,&risul);

        case BLADERF_MODULE_TX:
            risul.clock_index = 2;
            risul.enable_port_bits = SI5338_EN_A | SI5338_EN_B;
            return set_sample_rate_A(dev,&risul);

        default:
            break;
    }

    return BLADERF_ERR_INVAL;
}



/**
 * This just convert the sample rate into a,b,c that is suitable
 * @return invalid params if frequency cannot be set
 */
static int set_rational_rate_A (struct bladerf *dev, struct si5338_clock *clockP)
{
	int errcode;

    clockP->clock_base = 53 + clockP->clock_index * 11;

	// denominator can never be zero
	if ( clockP->denominator == 0 ) clockP->denominator = 1;

	if ( clockP->numerator > 0 )
		{
	    // normalize the rational part so it is always less than one Hz
		// someone may wish to define frequency just as 13M/48
		clockP->sample_rate = clockP->sample_rate + (clockP->numerator / clockP->denominator);
		// I still have to take care of the remainder, if there is one
		clockP->numerator = clockP->numerator % clockP->denominator;
		}

	if ( (errcode=set_sample_rate_calc_r(clockP)) ) return errcode;

	if ( (errcode=set_sample_rate_calc_abc(clockP)) ) return errcode;

	set_sample_rate_calc_regs(clockP);

	print_clock(clockP);

	if ( (errcode=set_sample_rate_write_regs(dev, clockP)) ) return errcode;


	return 0;
}


/**
 * @return 0 if operation successful or an appropriate error code
 */
int si5338_set_rational_sample_rate(struct bladerf *dev, bladerf_module module, uint32_t frequency, uint32_t numerator, uint32_t denom)
{
    struct si5338_clock risul;

    memset(&risul,0,sizeof(risul));

    // this is mainly the frequency we wish, the decimal part just adds to this
    // as the other one LMS needs one clock for I and one clock for Q
    risul.sample_rate = frequency * 2;
    risul.numerator   = numerator * 2;
    risul.denominator = denom;

    switch ( module ) {
        case BLADERF_MODULE_RX:
            risul.clock_index = 1;
            risul.enable_port_bits = SI5338_EN_A;
            return set_rational_rate_A(dev,&risul);

        case BLADERF_MODULE_TX:
            risul.clock_index = 2;
            risul.enable_port_bits = SI5338_EN_A | SI5338_EN_B;
            return set_rational_rate_A(dev,&risul);

        default:
            break;
    }

    return BLADERF_ERR_INVAL;
}
