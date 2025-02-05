/*
 * This file is part of the bladeRF project
 *
 * Copyright (C) 202 Nuand LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define _USE_MATH_DEFINES
#include <math.h>
#endif
#include "conversions.h"
#include "host_config.h"
#include "minmax.h"
#include "parse.h"
#include "rel_assert.h"
#include "rxtx_impl.h"
#include "doc/cmd_help.h"

int gen_write_fmt_sample(FILE *fp, enum rxtx_fmt fmt, int16_t s_i, int16_t s_q) {
    int status = 0;
    if (fmt == RXTX_FMT_CSV) {
        status = fprintf(fp, "%d, %d\n", s_i, s_q);
    } else if (fmt == RXTX_FMT_BIN_SC16Q11) {
        status = fwrite(&s_i, sizeof(int16_t), 1, fp);
        if (status > 0) {
            status = fwrite(&s_q, sizeof(int16_t), 1, fp);
        }
    } else if (fmt == RXTX_FMT_BIN_SC8Q7) {
        status = fwrite(&s_i, sizeof(int8_t), 1, fp);
        if (status > 0) {
            status = fwrite(&s_q, sizeof(int8_t), 1, fp);
        }
    }
    return status <= 0;
}

int cmd_generate(struct cli_state *s, int argc, char **argv)
{
    FILE *fp;
    char *delim;
    char *val;
    int status;
    int n_samples = 0;
    int mag = 2047;
    int i;

    if (s->bit_mode_8bit) {
        mag /= 16;
    }
    int remaining_argc;

    enum rxtx_fmt fmt = RXTX_FMT_CSV;
    unsigned int n;
    bool ok;

    assert(argc > 0);

    if (argc < 3) {
        printf("%s\n", CLI_CMD_HELPTEXT_generate);
        return CLI_RET_NARGS;
    }

    status = expand_and_open(argv[1], "w+", &fp);
    if (status != 0) {
        return status;
    }

    for (i = 2; i < argc; i++) {
        delim = strchr(argv[i], '=');
        if (!delim) {
            break;
        }
        *delim = 0;
        val = delim + 1;
        if (!strcasecmp("n", argv[i])) {
            /* Configure number of samples to genearte */

            n = str2uint_suffix(val, 0, UINT_MAX, rxtx_kmg_suffixes,
                                (int)rxtx_kmg_suffixes_len, &ok);

            if (ok) {
                n_samples = n;
            } else {
                cli_err(s, argv[0], RXTX_ERRMSG_VALUE(argv[i], val));
                status = CLI_RET_INVPARAM;
                goto out;
            }
        } else if (!strcasecmp("mag", argv[i])) {
            /* Configure magnitude of samples */

            n = str2uint_suffix(val, 0, UINT_MAX, rxtx_kmg_suffixes,
                                (int)rxtx_kmg_suffixes_len, &ok);

            if (ok) {
                mag = n;
            } else {
                cli_err(s, argv[0], RXTX_ERRMSG_VALUE(argv[i], val));
                status = CLI_RET_INVPARAM;
                goto out;
            }
        } else if (!strcasecmp("format", argv[i])) {
            fmt = rxtx_str2fmt(val, s);
            if (fmt == RXTX_FMT_INVALID) {
                cli_err(s, argv[0], RXTX_ERRMSG_VALUE(argv[i], val));
                status = CLI_RET_INVPARAM;
                goto out;
            }
        } else {
            cli_err(s, argv[0], "Unexpected parameter %s", argv[i]);
            status = CLI_RET_INVPARAM;
            goto out;
        }
    }

    remaining_argc = argc - i;

    if (!remaining_argc) {
            cli_err(s, argv[0], "Unspecified waveform");
            status = CLI_RET_NARGS;
            goto out;
    }

    if (!strcasecmp(argv[i], "cw")) {
        if (remaining_argc != 2) {
            cli_err(s, argv[0], "Incorrect number of arguments for %s", argv[i]);
            status = CLI_RET_NARGS;
            goto out;
        }

        n = str2uint_suffix(argv[i+1], 0, UINT_MAX, rxtx_kmg_suffixes,
                            (int)rxtx_kmg_suffixes_len, &ok);
        if (!ok) {
            cli_err(s, argv[0], RXTX_ERRMSG_VALUE(argv[i], argv[i+1]));
            status = CLI_RET_INVPARAM;
            goto out;
        }
        for (i = 0; i < (int)n; i++) {
            if (n_samples && i > n_samples) {
                break;
            }

            status = gen_write_fmt_sample(fp, fmt,
                            mag * cos(i * (double)(2*M_PI/(float)n)),
                            mag * sin(i * (double)(2*M_PI/(float)n))
                        );
            if (status) {
                status = CLI_RET_FILEOP;
                goto out;
            }
        }
        n_samples = i;
    } else if (!strcasecmp(argv[i], "prn")) {
        if (remaining_argc != 1) {
            cli_err(s, argv[0], "Incorrect number of arguments for %s", argv[i]);
            status = CLI_RET_NARGS;
            goto out;

        }
#define DENOMINATOR (4096)
#define RAND_CHAN   ((rand()/DENOMINATOR) * (2*mag) / (RAND_MAX/DENOMINATOR) - mag)
        for (i = 0; i < n_samples; i++) {
            if (gen_write_fmt_sample(fp, fmt, RAND_CHAN, RAND_CHAN)) {
                status = CLI_RET_FILEOP;
                goto out;
            }
        }
    } else {
        cli_err(s, argv[0], "Unrecognized waveform %s", argv[i]);
        status = CLI_RET_INVPARAM;
        goto out;
    }

    printf("  Wrote %d samples to %s.\n", n_samples, argv[1]);

    /* Perform file conversion (if needed) and open input file */
    MUTEX_LOCK(&s->tx->file_mgmt.file_lock);
    MUTEX_LOCK(&s->tx->file_mgmt.file_meta_lock);
    MUTEX_LOCK(&s->tx->param_lock);
    s->tx->file_mgmt.format = fmt;
    ((struct tx_params *)s->tx->params)->repeat = 0;
    if (s->tx->file_mgmt.path) {
        free(s->tx->file_mgmt.path);
    }
    s->tx->file_mgmt.path = strdup(argv[1]);
    status = expand_and_open(s->tx->file_mgmt.path, fmt == RXTX_FMT_CSV ? "r" : "rb",
                                &s->tx->file_mgmt.file);
    if (status) {
        printf("  Configure tx by running:\n"
           "    tx config file=%s format=%s repeat=0\n\n",
           argv[1], fmt == RXTX_FMT_CSV ? "csv" : "bin");

    }

    MUTEX_UNLOCK(&s->tx->param_lock);
    MUTEX_UNLOCK(&s->tx->file_mgmt.file_meta_lock);
    MUTEX_UNLOCK(&s->tx->file_mgmt.file_lock);

    printf("\n");

out:
    fclose(fp);
    return status;
}
