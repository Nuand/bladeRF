#include <string.h>
#include "libbladeRF.h"
#include "bladerf_priv.h"

// this file needs to be linked with definitions for si5338_i2c_write(), and
// possibly si5338_printf()
#define si5338_printf(...)

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

static void print_ms(struct tspec *ts) {
#ifdef SI5338_DBG
    int i;
    si5338_printf("out_freq: %dHz\n", ts->out_freq);
    si5338_printf("real_freq: %dHz\n", ts->real_freq);
    si5338_printf("en: %d\n", ts->en);
    si5338_printf("a: %d\n", ts->a);
    si5338_printf("b: %d\n", ts->b);
    si5338_printf("c: %d\n", ts->c);
    si5338_printf("r: %d\n", ts->r);
    si5338_printf("p1: %x\n", ts->p1);
    si5338_printf("p2: %x\n", ts->p2);
    si5338_printf("p3: %x\n", ts->p3);
    for (i = 0; i < 9; i++) {
        si5338_printf("regs[%d] = 0x%.2x\n", ts->base + i, ts->regs[i]);
    }
#endif
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
                si5338_printf("Requested frequency of %dHz on MS%d is too low\n", ms[i].out_freq, i);
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
                si5338_printf("Could not find a+b/c for %dHz on MS%d\n", ms[i].out_freq, i);
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
