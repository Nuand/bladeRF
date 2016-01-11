/*
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2013-2015 Nuand LLC
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
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <libbladeRF.h>

#include "dc_calibration.h"
#include "rel_assert.h"
#include "calibrate.h"
#include "common.h"
#include "cmd.h"

#define MAX_PHASE   4096
#define MIN_PHASE   (-MAX_PHASE)

#define MAX_GAIN    4096
#define MIN_GAIN    (-MAX_GAIN)

static int show_lms_cals(struct cli_state *s)
{
    int status = 0;
    struct bladerf_lms_dc_cals lms_cals =
        { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

    status = bladerf_lms_get_dc_cals(s->dev, &lms_cals);
    if (status != 0) {
        s->last_lib_error = status;
        status = CLI_RET_LIBBLADERF;
    } else {
        putchar('\n');
        printf("  LPF tuning module: %d\n\n", lms_cals.lpf_tuning);
        printf("  TX LPF I filter: %d\n", lms_cals.tx_lpf_i);
        printf("  TX LPF Q filter: %d\n\n", lms_cals.tx_lpf_q);
        printf("  RX LPF I filter: %d\n", lms_cals.rx_lpf_i);
        printf("  RX LPF Q filter: %d\n\n", lms_cals.rx_lpf_q);
        printf("  RX VGA2 DC reference module: %d\n", lms_cals.dc_ref);
        printf("  RX VGA2 stage 1, I channel: %d\n", lms_cals.rxvga2a_i);
        printf("  RX VGA2 stage 1, Q channel: %d\n", lms_cals.rxvga2a_q);
        printf("  RX VGA2 stage 2, I channel: %d\n", lms_cals.rxvga2b_i);
        printf("  RX VGA2 stage 2, Q channel: %d\n", lms_cals.rxvga2b_q);
        putchar('\n');
    }

    return status;
}

static int cal_lms(struct cli_state *s, int argc, char **argv)
{
    int status = 0;
    bool ok;
    bool have_lms_cals = false;
    struct bladerf_lms_dc_cals lms_cals =
        { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

    if (argc == 2) {
       status = dc_calibration_lms6(s->dev, "all");
       if (status == 0) {
           status = show_lms_cals(s);
       }

       return status;
    }

    if (argc == 3) {
        if (!strcasecmp(argv[2], "show")) {
            status = show_lms_cals(s);
        } else {
            status = dc_calibration_lms6(s->dev, argv[2]);
            if (status == BLADERF_ERR_INVAL) {
                cli_err(s, argv[0], "Invalid LMS module specified: %s\n", argv[2]);
                status = CLI_RET_INVPARAM;
            } else if (status != 0) {
                s->last_lib_error = status;
                status = CLI_RET_LIBBLADERF;
            }
        }

        return status;
    }

    if (argc == 4 && !strcasecmp(argv[2], "tuning")) {
        lms_cals.lpf_tuning = (int16_t) str2int(argv[3], 0, UINT8_MAX, &ok);
        if (!ok) {
            cli_err(s, argv[0], "Invalid LPF tuning value: %s\n", argv[3]);
            status = CLI_RET_INVPARAM;
        }

        have_lms_cals = true;

    } else if (argc == 5 && !strcasecmp(argv[2], "txlpf")) {
        lms_cals.tx_lpf_i = (int16_t) str2int(argv[3], 0, UINT8_MAX, &ok);
        if (!ok) {
            cli_err(s, argv[0], "Invalid TX LPF I value: %s\n", argv[3]);
            status = CLI_RET_INVPARAM;
        }

        lms_cals.tx_lpf_q = (int16_t) str2int(argv[4], 0, UINT8_MAX, &ok);
        if (!ok) {
            cli_err(s, argv[0], "Invalid TX LPF Q value: %s\n", argv[4]);
            status = CLI_RET_INVPARAM;
        }

        have_lms_cals = true;
    } else if (argc == 5 && !strcasecmp(argv[2], "rxlpf")) {
        lms_cals.rx_lpf_i = (int16_t) str2int(argv[3], 0, UINT8_MAX, &ok);
        if (!ok) {
            cli_err(s, argv[0], "Invalid TX LPF I value: %s\n", argv[3]);
            status = CLI_RET_INVPARAM;
        }

        lms_cals.rx_lpf_q = (int16_t) str2int(argv[4], 0, UINT8_MAX, &ok);
        if (!ok) {
            cli_err(s, argv[0], "Invalid RX LPF Q value: %s\n", argv[4]);
            status = CLI_RET_INVPARAM;
        }

        have_lms_cals = true;
    } else if (argc == 8 && !strcasecmp(argv[2], "rxvga2")) {
        lms_cals.dc_ref = (int16_t) str2int(argv[3], 0, UINT8_MAX, &ok);
        if (!ok) {
            cli_err(s, argv[0], "Invalid RXVGA2 DC_REF value: %s\n", argv[3]);
            status = CLI_RET_INVPARAM;
        }

        lms_cals.rxvga2a_i = (int16_t) str2int(argv[4], 0, UINT8_MAX, &ok);
        if (!ok) {
            cli_err(s, argv[0], "Invalid RXVGA2 stage 1 I value: %s\n", argv[4]);
            status = CLI_RET_INVPARAM;
        }

        lms_cals.rxvga2a_q = (int16_t) str2int(argv[5], 0, UINT8_MAX, &ok);
        if (!ok) {
            cli_err(s, argv[0], "Invalid RXVGA2 stage 1 Q value: %s\n", argv[5]);
            status = CLI_RET_INVPARAM;
        }

        lms_cals.rxvga2b_i = (int16_t) str2int(argv[6], 0, UINT8_MAX, &ok);
        if (!ok) {
            cli_err(s, argv[0], "Invalid RXVGA2 stage 2 I value: %s\n", argv[6]);
            status = CLI_RET_INVPARAM;
        }

        lms_cals.rxvga2b_q = (int16_t) str2int(argv[7], 0, UINT8_MAX, &ok);
        if (!ok) {
            cli_err(s, argv[0], "Invalid RXVGA2 stage 2 Q value: %s\n", argv[7]);
            status = CLI_RET_INVPARAM;
        }

        have_lms_cals = true;
    } else {
        cli_err(s, argv[0],
                "Invalid LMS module or incorrect number of parameterss\n");
        status = CLI_RET_INVPARAM;
    }

    if (status == 0 && have_lms_cals) {
        status = bladerf_lms_set_dc_cals(s->dev, &lms_cals);
        if (status != 0) {
            s->last_lib_error = status;
            status = CLI_RET_LIBBLADERF;
        }
    }

    return status;
}

static void print_cal_params(const char *m,
                             const struct dc_calibration_params *p,
                             bool prepend_newline, bool append_newline)
{
    if (prepend_newline) {
        putchar('\n');
    }

    printf("%s DC I: Value = %5d, Error = % 2.3f\n", m, p->corr_i, p->error_i);
    printf("%s DC Q: Value = %5d, Error = % 2.3f\n", m, p->corr_q, p->error_q);

    if (append_newline) {
        putchar('\n');
    }
}

static int cal_dc_correction_params(struct cli_state *s, int argc, char **argv)
{
    int status = 0;

    if (argc < 3) {
        return CLI_RET_NARGS;
    }

    if (argc == 3) {
        struct dc_calibration_params cal_params;
        bool rx = false, tx = false;

        if (!strcasecmp(argv[2], "rx") || !strcasecmp(argv[2], "rxtx")) {
            rx = true;
        }

        if (!strcasecmp(argv[2], "tx") || !strcasecmp(argv[2], "rxtx")) {
            tx = true;
        }

        if (rx) {
            memset(&cal_params, 0, sizeof(cal_params));
            status = bladerf_get_frequency(s->dev, BLADERF_MODULE_RX,
                                           &cal_params.frequency);
            if (status != 0) {
                s->last_lib_error = status;
                return CLI_RET_LIBBLADERF;
            }


            status = dc_calibration_rx(s->dev, &cal_params, 1, false);
            if (status != 0) {
                s->last_lib_error = status;
                return CLI_RET_LIBBLADERF;
            }

            print_cal_params("RX", &cal_params, true, !tx);
        }

        if (status == 0 && tx) {
            memset(&cal_params, 0, sizeof(cal_params));
            status = bladerf_get_frequency(s->dev, BLADERF_MODULE_TX,
                                           &cal_params.frequency);
            if (status != 0) {
                s->last_lib_error = status;
                return CLI_RET_LIBBLADERF;
            }

            status = dc_calibration_tx(s->dev, &cal_params, 1, false);
            if (status != 0) {
                s->last_lib_error = status;
                return CLI_RET_LIBBLADERF;
            }

            print_cal_params("TX", &cal_params, true, true);
        }

        if (!rx && !tx) {
            cli_err(s, argv[0], "Invalid module: %s\n", argv[1]);
            return CLI_RET_INVPARAM;
        }


    } else if (argc == 5) {
        bladerf_module module;
        int16_t i, q;
        bool ok;

        if (!strcasecmp(argv[2], "rx")) {
            module = BLADERF_MODULE_RX;
        } else if (!strcasecmp(argv[2], "tx")) {
            module = BLADERF_MODULE_TX;
        } else {
            cli_err(s, argv[0], "Invalid module: %s\n", argv[2]);
            return CLI_RET_INVPARAM;
        }

        i = (int16_t) str2int(argv[3], -2048, 2048, &ok);
        if (!ok) {
            cli_err(s, argv[0], "Invalid I value: %s\n", argv[3]);
            return CLI_RET_INVPARAM;
        }

        q = (int16_t) str2int(argv[4], -2048, 2048, &ok);
        if (!ok) {
            cli_err(s, argv[0], "Invalid Q value: %s\n", argv[4]);
            return CLI_RET_INVPARAM;
        }

        status = bladerf_set_correction(s->dev, module,
                                        BLADERF_CORR_LMS_DCOFF_I, i);

        if (status != 0) {
            s->last_lib_error = status;
            return CLI_RET_LIBBLADERF;
        }

        status = bladerf_set_correction(s->dev, module,
                                        BLADERF_CORR_LMS_DCOFF_Q, q);

        if (status != 0) {
            s->last_lib_error = status;
            return CLI_RET_LIBBLADERF;
        }

    } else {
        return CLI_RET_NARGS;
    }

    return 0;
}

static int cal_iq_correction_params(struct cli_state *s, int argc , char **argv)
{
    int status;
    bladerf_module module;
    bool ok;
    int16_t value;

    if (argc != 5) {
        return CLI_RET_NARGS;
    }

    if (!strcasecmp(argv[2], "rx")) {
        module = BLADERF_MODULE_RX;
    } else if (!strcasecmp(argv[2], "tx")) {
        module = BLADERF_MODULE_TX;
    } else {
        cli_err(s, argv[0], "Invalid module: %s\n", argv[2]);
        return CLI_RET_INVPARAM;
    }

    if (!strcasecmp(argv[3], "phase")) {
       value = str2int(argv[4], MIN_PHASE, MAX_PHASE, &ok);
       if (ok) {
           status = bladerf_set_correction(s->dev, module,
                                           BLADERF_CORR_FPGA_PHASE, value);
       } else {
           cli_err(s, argv[0], "Phase value must be in [%d, %d]\n",
                   MIN_PHASE, MAX_PHASE);
           return CLI_RET_INVPARAM;
       }
    } else if (!strcasecmp(argv[3], "gain")) {
        value = str2int(argv[4], MIN_GAIN, MAX_GAIN, &ok);
        if (ok) {
           status = bladerf_set_correction(s->dev, module,
                                           BLADERF_CORR_FPGA_GAIN, value);
        } else {
           cli_err(s, argv[0], "Gain value must be in [%d, %d]\n",
                   MIN_GAIN, MAX_GAIN);
           return CLI_RET_INVPARAM;
        }
    } else {
        cli_err(s, argv[0], "Invalid IQ correction parameter: %s\n", argv[3]);
        return CLI_RET_INVPARAM;
    }

    if (status < 0) {
        s->last_lib_error = status;
        return CLI_RET_LIBBLADERF;
    } else {
        return 0;
    }
}

static struct dc_calibration_params * prepare_dc_cal_params(unsigned int f_min,
                                                            unsigned int f_max,
                                                            unsigned int f_inc,
                                                            size_t *num_freqs)
{
    struct dc_calibration_params *p = NULL;
    const unsigned int len = ((f_max - f_min + 1) + (f_inc - 1)) / f_inc;
    unsigned int f, n;

    p = calloc(len, sizeof(p[0]));
    if (p == NULL) {
        return NULL;
    }

    for (f = f_min, n = 0; f <= f_max && n < len; n++, f += f_inc) {
        p[n].frequency = f;
    }

    assert(n == len);
    *num_freqs = len;

    return p;
}

static int save_table_results(const char *filename,
                              struct bladerf *dev, bladerf_module module,
                              const struct dc_calibration_params *params,
                              size_t num_params)
{
    int status = 0;
    struct bladerf_lms_dc_cals lms_dc_cals;
    struct bladerf_image *image = NULL;
    size_t i;
    size_t off = 0;
    uint32_t n_frequencies_le = 0;

    static const uint16_t magic = HOST_TO_LE16_CONST(0x1ab1);
    static const uint32_t reserved = HOST_TO_LE32_CONST(0x00000000);
    static const uint32_t tbl_version = HOST_TO_LE32_CONST(0x00000001);
    static const size_t lms_data_size = 10; /* 10 uint8_t register values */


    const size_t entry_size = sizeof(uint32_t) +   /* Frequency */
                              2 * sizeof(int16_t); /* DC I and Q valus */

    const size_t table_size = num_params * entry_size;

    const size_t data_size = sizeof(magic) + sizeof(reserved) +
                             sizeof(tbl_version) + sizeof(n_frequencies_le) +
                             lms_data_size + table_size;

    assert(num_params < UINT32_MAX);
    assert(data_size <= UINT_MAX);

    n_frequencies_le = HOST_TO_LE32((uint32_t) num_params);

    status = bladerf_lms_get_dc_cals(dev, &lms_dc_cals);
    if (status != 0) {
        return status;
    }

    if (module == BLADERF_MODULE_RX) {
        image = bladerf_alloc_image(BLADERF_IMAGE_TYPE_RX_DC_CAL,
                                    0xffffffff, (unsigned int) data_size);
    } else {
        image = bladerf_alloc_image(BLADERF_IMAGE_TYPE_TX_DC_CAL,
                                    0xffffffff, (unsigned int) data_size);
    }

    if (image == NULL) {
        return BLADERF_ERR_MEM;
    }

    /* Fill in header */
    memcpy(&image->data[off], &magic, sizeof(magic));
    off += sizeof(magic);

    memcpy(&image->data[off], &reserved, sizeof(reserved));
    off += sizeof(reserved);

    memcpy(&image->data[off], &tbl_version, sizeof(tbl_version));
    off += sizeof(tbl_version);

    memcpy(&image->data[off], &n_frequencies_le, sizeof(n_frequencies_le));
    off += sizeof(n_frequencies_le);

    image->data[off++] = (uint8_t)lms_dc_cals.lpf_tuning;
    image->data[off++] = (uint8_t)lms_dc_cals.tx_lpf_i;
    image->data[off++] = (uint8_t)lms_dc_cals.tx_lpf_q;
    image->data[off++] = (uint8_t)lms_dc_cals.rx_lpf_i;
    image->data[off++] = (uint8_t)lms_dc_cals.rx_lpf_q;
    image->data[off++] = (uint8_t)lms_dc_cals.dc_ref;
    image->data[off++] = (uint8_t)lms_dc_cals.rxvga2a_i;
    image->data[off++] = (uint8_t)lms_dc_cals.rxvga2a_q;
    image->data[off++] = (uint8_t)lms_dc_cals.rxvga2b_i;
    image->data[off++] = (uint8_t)lms_dc_cals.rxvga2b_q;

    for (i = 0; i < num_params; i++) {
        uint32_t freq;
        int16_t corr_i, corr_q;

        freq   = HOST_TO_LE32((uint32_t) params[i].frequency);
        corr_i = HOST_TO_LE16(params[i].corr_i);
        corr_q = HOST_TO_LE16(params[i].corr_q);

        memcpy(&image->data[off], &freq, sizeof(freq));
        off += sizeof(freq);

        memcpy(&image->data[off], &corr_i, sizeof(corr_i));
        off += sizeof(corr_q);

        memcpy(&image->data[off], &corr_q, sizeof(corr_q));
        off += sizeof(corr_q);
    }

    status = bladerf_image_write(image, filename);

    bladerf_free_image(image);
    return status;
}

/* See libbladeRF's dc_cal_table.c for the packed table data format */
static int cal_table(struct cli_state *s, int argc, char **argv)
{
    int status;
    bool ok;
    bladerf_module module;
    char *filename = NULL;
    size_t filename_len = 1024;
    FILE *write_check = NULL;

    struct dc_calibration_params *params = NULL;
    size_t num_params = 0;

    /* The XB-200 does not affect the minimum, as we're tuning the LMS here. */
    unsigned int f_min = BLADERF_FREQUENCY_MIN;
    unsigned int f_inc = 10000000;
    unsigned int f_max = BLADERF_FREQUENCY_MAX;

    if (argc == 4 || argc == 6 || argc == 7) {
        /* Only DC tables are currently supported.
         * IQ tables may be added in the future */
        if (strcasecmp(argv[2], "dc")) {
            cli_err(s, argv[0], "Invalid table type: %s\n", argv[2]);
            return CLI_RET_INVPARAM;
        }

        if (!strcasecmp(argv[3], "rx")) {
            module = BLADERF_MODULE_RX;
        } else if (!strcasecmp(argv[3], "tx")) {
            module = BLADERF_MODULE_TX;
        } else {
            cli_err(s, argv[0], "Invalid module: %s\n", argv[2]);
            return CLI_RET_INVPARAM;
        }
    } else {
        return CLI_RET_NARGS;
    }

    if (argc >= 6) {
        f_min = str2uint_suffix(argv[4],
                                BLADERF_FREQUENCY_MIN, BLADERF_FREQUENCY_MAX,
                                freq_suffixes, NUM_FREQ_SUFFIXES, &ok);

        if (!ok) {
            cli_err(s, argv[0], "Invalid min frequency (%s)\n", argv[4]);
            return CLI_RET_INVPARAM;
        }

        f_max = str2uint_suffix(argv[5],
                                BLADERF_FREQUENCY_MIN, BLADERF_FREQUENCY_MAX,
                                freq_suffixes, NUM_FREQ_SUFFIXES, &ok);

        if (!ok) {
            cli_err(s, argv[0], "Invalid max frequency (%s)\n", argv[5]);
            return CLI_RET_INVPARAM;
        }

        if (argc >= 7) {
            f_inc = str2uint_suffix(argv[6], 1, BLADERF_FREQUENCY_MAX,
                                    freq_suffixes, NUM_FREQ_SUFFIXES, &ok);

            if (!ok) {
                cli_err(s, argv[0],
                        "Invalid frequency increment (%s)\n", argv[6]);
                return CLI_RET_INVPARAM;
            }
        }

    }

    if (f_min >= f_max) {
        cli_err(s, argv[0], "Low frequency cannot be >= high frequency\n");
        return CLI_RET_INVPARAM;
    }

    if (((f_max - f_min) / f_inc) == 0) {
        cli_err(s, argv[0], "The specified frequency increment would yield "
                            "an empty table.\n");

        return CLI_RET_INVPARAM;
    }

    filename = calloc(1, filename_len + 1);
    if (filename == NULL) {
        return CLI_RET_MEM;
    }

    status = bladerf_get_serial(s->dev, filename);
    if (status != 0) {
        goto out;
    }

    filename_len -= strlen(filename);
    if (module == BLADERF_MODULE_RX) {
        strncat(filename, "_dc_rx.tbl", filename_len);
    } else {
        strncat(filename, "_dc_tx.tbl", filename_len);
    }

    /* This operation may take a bit of time, so let's make sure we
     * actually have write access before kicking things off.  Note that
     * access is inherently checked later when the file is actually written,
     * should permissions change in the meantime. */
    write_check = fopen(filename, "wb");
    if (!write_check) {
        if (errno == EACCES) {
            status = BLADERF_ERR_PERMISSION;
        } else {
            status = BLADERF_ERR_IO;
        }
        goto out;
    }

    fclose(write_check);
    write_check = NULL;

    /* Not much we care to do if this fails. Throw away the return value
     * to make this explicit to our static analysis tools */
    (void) remove(filename);

    params = prepare_dc_cal_params(f_min, f_max, f_inc, &num_params);
    if (params == NULL) {
        status = BLADERF_ERR_MEM;;
        goto out;
    }

    status = dc_calibration(s->dev, module, params, num_params, true);
    if (status != 0) {
        goto out;
    }

    status = save_table_results(filename, s->dev, module, params, num_params);
    if (status == 0) {
        printf("\n  Done.\n\n");
    }

out:
    if (status != 0) {
        s->last_lib_error = status;
        status = CLI_RET_LIBBLADERF;
    }

    free(filename);
    free(params);

    return status;
}

int cmd_calibrate(struct cli_state *state, int argc, char **argv)
{
    int status;

    if (argc >= 2) {
        if (!strcasecmp(argv[1], "lms")) {
            status = cal_lms(state, argc, argv);
        } else if (!strcasecmp(argv[1], "table")) {
            status = cal_table(state, argc, argv);
        } else if (!strcasecmp(argv[1], "dc")) {
            status = cal_dc_correction_params(state, argc, argv);
        } else if (!strcasecmp(argv[1], "iq")) {
            status = cal_iq_correction_params(state, argc, argv);
        } else {
            cli_err(state, argv[0], "Invalid operation: %s\n", argv[1]);
            status = CLI_RET_INVPARAM;
        }
    } else {
        return CLI_RET_NARGS;
    }

    return status;
}
