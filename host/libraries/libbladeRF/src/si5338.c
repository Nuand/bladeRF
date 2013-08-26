#include <string.h>

#include "libbladeRF.h"
#include "bladerf_priv.h"
#include "debug.h"


struct tspec {
    int id;
    int enA, enB;
    unsigned out_freq;
    unsigned real_freq;
    unsigned en;
    unsigned a, b, c;
    unsigned r, rpow;
    unsigned p1, p2, p3;

    int base;
    unsigned char regs[10];
    // 0: p1[7:0]
    // 1: p1[15:8]
    // 2: p2[5:0] & p1[17:16]
    // 3: p2[13:6]
    // 4: p2[21:14]
    // 5: p2[29:22]
    // 6: p3[7:0]
    // 7: p3[15:8]
    // 8: p3[23:16]
    // 9: p3[29:24]
};

#define NUM_MS 4

#define SI5338_F_VCO (38400000UL * 66UL)

/**
 * This is used to read a set of registers and calculate back to frequency
 */
struct si5338_readT 	{
	int block_index;
    int base;             // this can be gotten from block_index

    unsigned int P1,P2,P3;   // as from section 5.2 of Reference manual
    unsigned int a_1dec, b,c,r;

    unsigned int *FoutxP;  // write result here

    unsigned char regs[10];
    unsigned char rpow_raw;
};

#define NUM_MS 4





static void print_ms(struct tspec *ts) {
    int i;
    dbg_printf("out_freq: %dHz\n", ts->out_freq);
    dbg_printf("real_freq: %dHz\n", ts->real_freq);
    dbg_printf("en: %d\n", ts->en);
    dbg_printf("a: %d\n", ts->a);
    dbg_printf("b: %d\n", ts->b);
    dbg_printf("c: %d\n", ts->c);
    dbg_printf("r: %d\n", ts->r);
    dbg_printf("p1: %x\n", ts->p1);
    dbg_printf("p2: %x\n", ts->p2);
    dbg_printf("p3: %x\n", ts->p3);

    for (i = 0; i < 9; i++) dbg_printf("[%d]=0x%.2x\n", ts->base + i, ts->regs[i]);
}




static void configure_ms(struct bladerf *dev, struct tspec *ts) {
    int i;
    bladerf_si5338_write(dev, 36 + ts->id, (ts->enA ? 1 : 0) | (ts->enB ? 2 : 0) );
    for (i = 0; i < 9; i++) {
        bladerf_si5338_write(dev, ts->base + i, ts->regs[i]);
    }
    bladerf_si5338_write(dev, 31 + ts->id, 0xC0 | (ts->rpow << 2));
}

static int nodecimals(double num) {
    int a;
    a = num;
    return ((double)(num - (double)a) == 0.0f);
}

static int __si5338_do_multisynth(struct bladerf *dev, struct tspec *ms, unsigned vco_freq) {
    struct tspec vco;
    int i, j, found;
    double rem;
    unsigned long long a, b, c;
    unsigned long long p1, p2, p3;
    double bfl;

    vco.out_freq = vco_freq;

    for (i = 0; i < NUM_MS; i++) {
        ms[i].id = i;
        ms[i].base = 53 + i * 11;
        if (!ms[i].enA && !ms[i].enB)
            ms[i].enA = ms[i].enB = 1;
        if (!ms[i].out_freq)
            continue;
        ms[i].r = 1;
        ms[i].en = 1;
        if (ms[i].out_freq < 5000000) {
            // find an R that makes this MS's fout > 5MHz
            found = 0;
            for (j = 0; j <= 5; j++) {
                if (ms[i].out_freq * (1 << j) >= 5000000) {
                    ms[i].rpow = j;
                    ms[i].r = 1 << j;
                    ms[i].real_freq = ms[i].out_freq;
                    ms[i].out_freq *= ms[i].r;
                    found = 1;
                    break;
                }
            }

            if (!found) {
                dbg_printf("Requested frequency of %dHz on MS%d is too low\n", ms[i].out_freq, i);
                return -1;
            }
        }
    }

    // simplest algorithm
    for (i = 0; i < NUM_MS; i++) {
        if (!ms[i].en)
            continue;
        a = vco.out_freq / ms[i].out_freq;

        // find a ratio that works
        rem = (vco.out_freq - a * ms[i].out_freq) / (double)ms[i].out_freq;
        if (rem != 0) {
            found = 0;
            for (c = 1; c < ((2ULL<<30)-1); c++) {
                bfl = rem * c;
                if (bfl < 1)
                    continue;
                if (nodecimals(bfl)) {
                    b = bfl;
                    found = 1;
                    break;
                }

            }
            if (!found) {
                dbg_printf("Could not find a+b/c for %dHz on MS%d\n", ms[i].out_freq, i);
                return -1;
            }
        } else {
            c = 1;
            b = 0;
        }

        if (i == 1)
            ms[i].enB = 0;

        ms[i].a = a;
        ms[i].b = b;
        ms[i].c = c;

        // calculate P1, P2, P3 based off page 9 in the Si5338 reference manual
        ms[i].p1 = p1 = (a * c + b) * 128 / c - 512;
        ms[i].p2 = p2 = (b * 128) % c;
        ms[i].p3 = p3 = c;

        ms[i].regs[0] = p1 & 0xff;
        ms[i].regs[1] = (p1 >> 8) & 0xff;
        ms[i].regs[2] = ((p2 & 0x3f) << 2) | ((p1 >> 16) & 0x3);
        ms[i].regs[3] = (p2 >> 6) & 0xff;
        ms[i].regs[4] = (p2 >> 14) & 0xff;
        ms[i].regs[5] = (p2 >> 22) & 0xff;
        ms[i].regs[6] = p3 & 0xff;
        ms[i].regs[7] = (p3 >> 8) & 0xff;
        ms[i].regs[8] = (p3 >> 16) & 0xff;
        ms[i].regs[9] = (p3 >> 24) & 0xff;

        print_ms(&ms[i]);
        configure_ms(dev, &ms[i]);
    }

    return 0;
}

// hardcode these for now
int in_freq = 38400000;
int vco_ms_n = 66; // this is the VCO's divider

int si5338_set_tx_freq(struct bladerf *dev, unsigned freq) {
    struct tspec ms[NUM_MS];

    memset(&ms, 0, sizeof(ms));

    ms[2].out_freq = freq;
    ms[2].enA = ms[2].enB = 1;

    return __si5338_do_multisynth(dev, ms, in_freq * vco_ms_n);
}

int si5338_set_rx_freq(struct bladerf *dev, unsigned freq) {
    struct tspec ms[NUM_MS];

    memset(&ms, 0, sizeof(ms));

    ms[1].out_freq = freq;
    ms[1].enA = 1;

    return __si5338_do_multisynth(dev, ms, in_freq * vco_ms_n);
}

int si5338_set_mimo_mode(struct bladerf *dev, int mode) {
    return 0;
}

int si5338_set_exp_clk(struct bladerf *dev, int enabled, unsigned freq) {
    // TODO implement disabling
    struct tspec ms[NUM_MS];

    memset(&ms, 0, sizeof(ms));

    ms[3].out_freq = freq;
    ms[3].enB = 1;

    return __si5338_do_multisynth(dev, ms, in_freq * vco_ms_n);
}

/**
 * gets the basic regs from Si
 * @return 0 if all is fine or an error code
 */
static int sis5338_get_sample_rate_regs ( struct bladerf *dev, struct si5338_readT *retP )  {
    int i,retcode;

    for (i = 0; i < 9; i++)  {
        if ( (retcode=bladerf_si5338_read(dev, retP->base + i, retP->regs+i)) < 0 )	{
        	dbg_printf("sis5338_get_sample_rate_regs: ioctl failed\n");
			return retcode;
			}
        }

    if ( (retcode=bladerf_si5338_read(dev, 31 + retP->block_index, &retP->rpow_raw)) < 0 )
    		return retcode;

    return 0;
}

static unsigned int bytes_to_uint32 ( uint8_t msb, uint8_t ms1, uint8_t ms2, uint8_t lsb, uint8_t last_shift ) {
	unsigned int risul = msb;
	risul = (risul << 9) + ms1;
	risul = (risul << 8) + ms2;
	risul = (risul << last_shift) + lsb;
	return risul;
}


static void sis5338_get_sample_rate_calc ( struct si5338_readT *retP ) {
	uint64_t c = retP->c = retP->P3;

	unsigned int p2 = retP->P2;

	unsigned int b=0;

	int i;

	// c should never be zero, but if it is at least we do not crash
	if ( c == 0 ) return;

	for (i=1; i<128; i++)
			{
			unsigned int B = c*i+p2;

			if ( (B % 128) == 0 )
					{
					b = B / 128;

					retP->b = b;

					break;
					}
			}

	unsigned int p1 = retP->P1;

	uint64_t A = (p1 + 512);
	A = A * c;
	A = A - b * 128;
	A = (A * 10) / ( c * 128 );  // switch to fixed decimal point

	// bpadalino want to embed this into NIOS II....
	retP->a_1dec = A;

	// go back to the integer value
	uint64_t a = A / 10;

	// do manual rounding....
	if ( A % 10 > 5 ) a++;

// step by step to avoid too much compiler optimization
	uint64_t f_twice = SI5338_F_VCO * c;
	uint64_t divisor = a * c + b;
    f_twice = f_twice / divisor;
    f_twice = f_twice / retP->r;

	retP->FoutxP[0] = f_twice / 2;  // yes, compiler may optimize this to >> 1
}

static void print_Si5338_readT(struct si5338_readT *ts)  {
    int i;
    dbg_printf("out_freq: %dHz\n", ts->FoutxP[0]);
    dbg_printf("a_1dec: %d\n", ts->a_1dec);
    dbg_printf("b:      %d\n", ts->b);
    dbg_printf("c:      %d\n", ts->c);
    dbg_printf("r:      %d\n", ts->r);
    dbg_printf("p1: %x\n", ts->P1);
    dbg_printf("p2: %x\n", ts->P2);
    dbg_printf("p3: %x\n", ts->P3);

    for (i = 0; i < 9; i++) dbg_printf("[%d]=0x%.2x\n", ts->base + i, ts->regs[i]);

    dbg_printf("[r]=0x%.2x\n", ts->rpow_raw);
}


static int sis5338_get_sample_rate_A ( struct bladerf *dev, struct si5338_readT *retP )  {
	int retcode;

	// gets the raw sample rate regs
	if ( (retcode=sis5338_get_sample_rate_regs(dev, retP )) ) return retcode;

	// I now need to reverse the packing, see pag. 10 of reference manual
	unsigned char *valP = retP->regs;
	retP->P1 = bytes_to_uint32 ( 0, valP[2] & 0x03, valP[1], valP[0], 8);
	retP->P2 = bytes_to_uint32 ( valP[5], valP[4], valP[3], valP[2] >> 2, 6 );
	retP->P3 = bytes_to_uint32 ( valP[9] & 0x3F, valP[8], valP[7], valP[6], 8 );

	uint8_t shi = (retP->rpow_raw >> 2) & 0x03;
	retP->r = 1 << shi;

	sis5338_get_sample_rate_calc(retP);

	print_Si5338_readT(retP);

	return 0;
}

/**
 * get the sample rate back from the Si5338
 * @param module the subsection I wish to know about
 * @param rateP pointer to result
 * @return 0 if all fine or an error code
 */
int bladerf_get_sample_rate (struct bladerf *dev, bladerf_module module, unsigned int *rateP ) {
	// safety check
	if ( rateP == NULL ) return -1;

	struct si5338_readT risul;

	memset(&risul,0,sizeof(risul));

	risul.FoutxP = rateP;

	switch ( module )
			{
			case BLADERF_MODULE_RX:
					risul.block_index = 1;
					risul.base = 53 + 1 * 11;
					return sis5338_get_sample_rate_A(dev,&risul);

			case BLADERF_MODULE_TX:
					risul.block_index = 2;
					risul.base = 53 + 2 * 11;
					return sis5338_get_sample_rate_A(dev,&risul);

			default:
					break;
			}

	return -1;
}




