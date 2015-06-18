/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (c) 2015 Nuand LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "hop_set.h"
#include "conversions.h"

#define INC 16

static const struct numeric_suffix freq_suffix[] = {
    { FIELD_INIT(.suffix, "k"), FIELD_INIT(.multiplier, 1000) },
    { FIELD_INIT(.suffix, "K"), FIELD_INIT(.multiplier, 1000) },
    { FIELD_INIT(.suffix, "m"), FIELD_INIT(.multiplier, 1000 * 1000) },
    { FIELD_INIT(.suffix, "M"), FIELD_INIT(.multiplier, 1000 * 1000) },
    { FIELD_INIT(.suffix, "g"), FIELD_INIT(.multiplier, 1000 * 1000 * 1000) },
    { FIELD_INIT(.suffix, "G"), FIELD_INIT(.multiplier, 1000 * 1000 * 1000) },
};

static const size_t freq_suffix_count =
                            sizeof(freq_suffix) / sizeof(freq_suffix[0]);

static inline char * strip_chars(char *str)
{
    size_t i;
    size_t len = strlen(str);

    for (i = 0; i < len; i++) {
        switch (str[i]) {
            case '\n':
            case '\t':
            case ' ':
            case ',':
            case ':':
            case ';':
                str[i] = '\0';
                return str;
        }
    }

    return str;
}

struct hop_set * hop_set_load(const char *filename)
{
    int status = -1;
    struct hop_set *h = NULL;
    FILE *in = NULL;
    size_t actual_count = INC;
    char buf[81];
    bool ok;

    in = fopen(filename, "r");
    if (in == NULL) {
        fprintf(stderr, "Failed to open %s: %s\n",
                filename, strerror(errno));
        return NULL;
    }

    h = malloc(sizeof(h[0]));
    if (h == NULL) {
        goto out;
    }

    h->params = calloc(actual_count, sizeof(h->params[0]));
    if (h->params == NULL) {
        goto out;
    }

    h->count = 0;
    h->idx = 0;

    while (fgets(buf, sizeof(buf), in) != NULL) {
        if (h->count >= actual_count) {
            struct hop_params *tmp =
                realloc(h->params, (h->count + INC) * sizeof(h->params[0]));

            if (tmp == NULL) {
                perror("realloc");
                goto out;
            }

            actual_count += INC;
            h->params = tmp;
        }

        strip_chars(buf);
        if (strlen(buf) > 0) {
            h->params[h->count].f = str2uint_suffix(buf,
                                                  BLADERF_FREQUENCY_MIN,
                                                  BLADERF_FREQUENCY_MAX,
                                                  freq_suffix, freq_suffix_count,
                                                  &ok);
            if (!ok) {
                fprintf(stderr, "Invalid frequency value: %s\n", buf);
                goto out;
            }

            h->count++;
        }
    }

    status = 0;

out:
    fclose(in);

    if (status != 0) {
        hop_set_free(h);
        h = NULL;
    }

    return h;
}

int hop_set_load_quick_tunes(struct bladerf *dev,
                             bladerf_module m,
                             struct hop_set *h)
{
    int status;
    size_t i;

    for (i = 0; i < h->count; i++) {
        status = bladerf_set_frequency(dev, m, h->params[i].f);
        if (status != 0) {
            fprintf(stderr, "Failed to set frequency: %s\n",
                    bladerf_strerror(status));
            return status;
        }

        status = bladerf_get_quick_tune(dev, m, &h->params[i].qt);
        if (status != 0) {
            fprintf(stderr, "Failed to get quick tune parameters: %s\n",
                    bladerf_strerror(status));
            return status;
        }
    }

    return 0;
}

unsigned int hop_set_next(struct hop_set *h, struct hop_params *p_usr)
{
    struct hop_params *p = &h->params[h->idx];

    if (p_usr != NULL) {
        memcpy(p_usr, p, sizeof(p[0]));
    }

    h->idx = (h->idx + 1) % h->count;

    return p->f;
}

void hop_set_free(struct hop_set *h)
{
    if (h != NULL) {
        free(h->params);
        free(h);
    }
}

/* Exercises this hop_set interface */
#ifdef HOP_SET_TEST
int main(int argc, char *argv[])
{
    int status = 0;
    unsigned int i;
    struct bladerf *dev = NULL;
    struct hop_set *h = NULL;
    const char *dev_str = NULL;
    bool have_quick_tune = false;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file> [device]\n", argv[0]);
        return 1;
    }

    if (argc >= 3) {
        dev_str = argv[2];
    }

    h = hop_set_load(argv[1]);
    if (h == NULL) {
        fprintf(stderr, "Could not load hop set from %s.\n", argv[1]);
        return 1;
    }

    status = bladerf_open(&dev, dev_str);
    if (status != 0) {
        if (status == BLADERF_ERR_NODEV) {
            printf("No bladeRF found. Not fetching quick tune params.\n");
        } else {
            fprintf(stderr, "Failed to open device: %s\n",
                    bladerf_strerror(status));
            goto out;
        }
    } else {
        status = hop_set_load_quick_tunes(dev, BLADERF_MODULE_RX, h);
        if (status != 0) {
            goto out;
        } else {
            have_quick_tune = true;
        }
    }

    printf("Loaded %zd entries.\n", h->count);

    /* Wrap around once to verify hop_set_next() doesn't roll out of bounds */
    for (i = 0; i < 2 *  h->count; i++) {

        if (have_quick_tune) {
            struct hop_params p;
            hop_set_next(h, &p);

            printf("f=%-10u nint=%-6u nfrac=%-8u flags=0x%02x vcocap=%u\n",
                    p.f, p.qt.nint, p.qt.nfrac, p.qt.flags, p.qt.vcocap);
        } else {
            printf("f=%u\n", hop_set_next(h, NULL));
        }
    }

out:
    bladerf_close(dev);
    hop_set_free(h);
    return status;
}
#endif
