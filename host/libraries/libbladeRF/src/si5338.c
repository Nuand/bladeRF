#include <string.h>

#include "libbladeRF.h"
#include "bladerf_priv.h"
#include "log.h"


#define SI5338_EN_A		0x01
#define SI5338_EN_B		0x02

#define SI5338_F_VCO (38400000UL * 66UL)


/**
 * This is used set or recreate the si5338 frequency
 * Each si port can be set independently
 */
struct si5338_port {
    int block_index;
    int base;                  // this can be gotten from block_index

    uint32_t want_sample_rate; // if used as setter this is what I wish
    uint8_t enable_port_bits;  // You may want Just port A out or both port A & B

    uint32_t sample_rate_r;    // the sample rate adjusted with the r multiplier

    uint32_t P1,P2,P3;         // as from section 5.2 of Reference manual
    uint32_t a,b,c,r;

    uint32_t *f_outP;          // write result here

    uint8_t regs[10];
    uint8_t raw_r;
};


/**
 * gets the basic regs from Si
 * @return 0 if all is fine or an error code
 */
static int si5338_get_sample_rate_regs ( struct bladerf *dev, struct si5338_port *retP )
{
    int i,retcode;

    for (i = 0; i < 9; i++)  {
        if ( (retcode=bladerf_si5338_read(dev, retP->base + i, retP->regs+i)) < 0 )    {
            log_error("Could not read from si5338 (%d): %s\n",retcode, bladerf_strerror(retcode));
            return retcode;
            }
        }

    if ( (retcode=bladerf_si5338_read(dev, 31 + retP->block_index, &retP->raw_r)) < 0 ) {
            log_error("Could not read from si5338 (%d): %s\n",retcode, bladerf_strerror(retcode));
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


static void si5338_get_sample_rate_calc ( struct si5338_port *retP )
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

static void print_si5338_port(struct si5338_port *portP)
{
    int i;
    log_debug("out_freq: %dHz\n", portP->f_outP[0]);
    log_debug("a:  %d\n", portP->a);
    log_debug("b:  %d\n", portP->b);
    log_debug("c:  %d\n", portP->c);
    log_debug("r:  %d\n", portP->r);
    log_debug("p1: %x\n", portP->P1);
    log_debug("p2: %x\n", portP->P2);
    log_debug("p3: %x\n", portP->P3);

    for (i = 0; i < 9; i++) log_debug("[%d]=0x%.2x\n", portP->base + i, portP->regs[i]);

    log_debug("[r]=0x%.2x\n", portP->raw_r);
}


static int si5338_get_module_sample_rate ( struct bladerf *dev, struct si5338_port *retP )
{
    int retcode;
    uint8_t *valP, shi;
    // gets the raw sample rate regs
    if ( (retcode=si5338_get_sample_rate_regs(dev, retP )) ) return retcode;

    // I now need to reverse the packing, see pag. 10 of reference manual
    valP = retP->regs;
    retP->P1 = bytes_to_uint32 ( 0, valP[2] & 0x03, valP[1], valP[0], 8);
    retP->P2 = bytes_to_uint32 ( valP[5], valP[4], valP[3], valP[2] >> 2, 6 );
    retP->P3 = bytes_to_uint32 ( valP[9] & 0x3F, valP[8], valP[7], valP[6], 8 );

    shi = (retP->raw_r >> 2) & 0x03;
    retP->r = 1 << shi;

    si5338_get_sample_rate_calc(retP);

    print_si5338_port(retP);

    return 0;
}

/**
 * get the sample rate back from the Si5338
 * @param module the subsection I wish to know about
 * @param rateP pointer to result
 * @return 0 if all fine or an error code
 */
int si5338_get_sample_rate(struct bladerf *dev, bladerf_module module, unsigned int *rateP)
{
    struct si5338_port risul;

    // safety check
    if ( rateP == NULL ) return BLADERF_ERR_UNEXPECTED;

    memset(&risul,0,sizeof(risul));

    risul.f_outP = rateP;

    switch ( module ) {
        case BLADERF_MODULE_RX:
            risul.block_index = 1;
            risul.base = 53 + 1 * 11;
            return si5338_get_module_sample_rate(dev,&risul);

        case BLADERF_MODULE_TX:
            risul.block_index = 2;
            risul.base = 53 + 2 * 11;
            return si5338_get_module_sample_rate(dev,&risul);

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
 * Calculated a,b,c to satisfy the si equations
 * see pag 9 of reference manual
 */
static int set_sample_rate_calc_abc ( struct si5338_port *portP )
{
    uint32_t remainder, gcd, b, c;

	// find out the integer part of the result
    portP->a = SI5338_F_VCO / portP->sample_rate_r;

    if ( portP->a < 4 || portP->a > 567 ) {
		log_warning("a=%d too big \n", portP->a);
		return BLADERF_ERR_INVAL;
    }

    // I still have this hertz left that are not integer divisible by VCO
    remainder = SI5338_F_VCO - (portP->a * portP->sample_rate_r);

    if ( remainder == 0 )
		{
    	portP->b=0;
    	portP->c=1;
    	return 0;
		}

    // the gcd is the biggest number that divides both as integers
    gcd = euclide_gcd(portP->sample_rate_r, remainder);

//    dbg_printf("VCO=%u a=%u sr=%u remainder=%u gcd=%d \n",SI5338_F_VCO,portP->sample_rate_r,portP->a,remainder,gcd);

    // I now have to check if any of the b,c satisfy si conditions
    b = remainder / gcd;

    if ( b > 0x40000000 ) {
		log_warning("cannot find suitable b ratio %dHz on MS%d \n", portP->want_sample_rate, portP->block_index);
		return BLADERF_ERR_INVAL;
    }

    c = portP->sample_rate_r / gcd;

    if ( c > 0x40000000 ) {
		log_warning("cannot find suitable c ratio %dHz on MS%d \n", portP->want_sample_rate, portP->block_index);
		return BLADERF_ERR_INVAL;
    }

    portP->b = b;
    portP->c = c;

    return 0;
}

/**
 * calculates a r that satisfy si5338
 * @return 0 if all is fine or an error code
 */
static int set_sample_rate_calc_r ( struct si5338_port *portP )
{
    uint32_t candidate;
	int j;

	for (j=0; j <= 5; j++ ) {
        // see pag. 27 of si5338 RM rev 1.2 1/13
		// find an R that makes this MS's fout > 5MHz

    	portP->r = 1 << j;   // this is the r I will be using
    	portP->raw_r = j;    // this is what goes into the chip

    	candidate = portP->want_sample_rate * portP->r;

		if ( candidate >= 5000000)	{
			portP->sample_rate_r = candidate;
			break;
		    }
		}

	if ( ! portP->sample_rate_r ) {
		log_warning("Requested frequency of %dHz on MS%d is too low\n", portP->want_sample_rate, portP->block_index);
		return BLADERF_ERR_INVAL;
	}

	return 0;
}

/**
 * calculate P1, P2, P3 based off page 9 in the Si5338 reference manual
 * make sure we have enough space to handle all cases
 */
static void set_sample_rate_calc_regs (struct si5338_port *portP)
{
    uint64_t P1, P2, p1, p2, p3;
    uint8_t *regs;

    // P1 = (a * c + b) * 128 / c - 512;
    P1 = portP->a * portP->c + portP->b;
    P1 = P1 * 128;
    P1 = P1 / portP->c - 512;

    // P2 = (b * 128) % c;
    P2 = portP->b * 128;
    P2 = P2 % portP->c;

    p1 = portP->P1 = P1;
    p2 = portP->P2 = P2;
    p3 = portP->P3 = portP->c;

    regs = portP->regs;
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

static int set_sample_rate_write_regs(struct bladerf *dev, struct si5338_port *portP) {
    int i,errcode;

    if ( (errcode=bladerf_si5338_write(dev, 36 + portP->block_index, portP->enable_port_bits )) ) return errcode;

    for (i = 0; i < 9; i++) {
    	if ( (errcode=bladerf_si5338_write(dev, portP->base + i, portP->regs[i] )) ) return errcode;
    }

    return bladerf_si5338_write(dev, 31 + portP->block_index, 0xC0 | (portP->raw_r << 2));
}

/**
 * This just convert the sample rate into a,b,c that is suitable
 * @return invalid params if frequency cannot be set
 */
static int set_sample_rate_A (struct bladerf *dev, struct si5338_port *portP)
{
	int errcode;

    portP->base = 53 + portP->block_index * 11;

	if ( (errcode=set_sample_rate_calc_r(portP)) ) return errcode;

	if ( (errcode=set_sample_rate_calc_abc(portP)) ) return errcode;

	set_sample_rate_calc_regs(portP);

	print_si5338_port(portP);

	if ( (errcode=set_sample_rate_write_regs(dev, portP)) ) return errcode;

	if ( portP->f_outP ) return si5338_get_module_sample_rate(dev, portP);

	return 0;
}



/**
 * sets the sample rate as requested
 */
int si5338_set_sample_rate(struct bladerf *dev, bladerf_module module, uint32_t rate, uint32_t *actualP)
{
    struct si5338_port risul;

    memset(&risul,0,sizeof(risul));

    risul.f_outP = actualP;  // may be NULL
    risul.want_sample_rate = rate * 2;   // LMS needs one clock for I and one clock for Q

    switch ( module ) {
        case BLADERF_MODULE_RX:
            risul.block_index = 1;
            risul.enable_port_bits = SI5338_EN_A;
            return set_sample_rate_A(dev,&risul);

        case BLADERF_MODULE_TX:
            risul.block_index = 2;
            risul.enable_port_bits = SI5338_EN_A | SI5338_EN_B;
            return set_sample_rate_A(dev,&risul);

        default:
            break;
    }

    return BLADERF_ERR_INVAL;
}


