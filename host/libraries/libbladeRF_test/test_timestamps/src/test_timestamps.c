/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014 Nuand LLC
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
#include <inttypes.h>
#include <libbladeRF.h>
#include <string.h>
#include "test_timestamps.h"

int perform_sync_init(struct bladerf *dev, bladerf_module module,
                      unsigned int buf_size, struct app_params *p)
{
    int status;

    if (buf_size == 0) {
        buf_size = p->buf_size;
    }

    status = bladerf_sync_config(dev,
                                 module,
                                 BLADERF_FORMAT_SC16_Q11_META,
                                 p->num_buffers,
                                 buf_size,
                                 p->num_xfers,
                                 p->timeout_ms);

    if (status != 0) {
        fprintf(stderr, "Failed to configure %s sync i/f: %s\n",
                module == BLADERF_MODULE_RX ? "RX" : "TX",
                bladerf_strerror(status));
    } else {
        status = bladerf_enable_module(dev, module, true);
        if (status != 0) {
        fprintf(stderr, "Failed to enable %s module: %s\n",
                module == BLADERF_MODULE_RX ? "RX" : "TX",
                bladerf_strerror(status));
        }
    }

    return status;
}

int enable_counter_mode(struct bladerf *dev, bool enable)
{
    int status;
    uint32_t regval;

    status = bladerf_config_gpio_read(dev, &regval);
    if (status != 0) {
        fprintf(stderr, "Failed to read FPGA GPIO config: %s\n",
                bladerf_strerror(status));
    } else {
        if (enable) {
            regval |= BLADERF_GPIO_COUNTER_ENABLE;
        } else {
            regval &= ~BLADERF_GPIO_COUNTER_ENABLE;
        }

        status = bladerf_config_gpio_write(dev, regval);
        if (status != 0) {
        fprintf(stderr, "Failed to write FPGA GPIO config: %s\n",
                bladerf_strerror(status));
        }
    }

    return status;
}

bool counter_data_is_valid(int16_t *samples, size_t n_samples, uint32_t ctr)
{
    size_t i;

    for (i = 0; i < 2 * n_samples; i += 2, ctr++) {
        const uint32_t val = extract_counter_val((uint8_t*) &samples[i]);
        if (val != ctr) {
            fprintf(stderr, "Invalid counter value @ sample %"PRIu64". "
                    "Expected 0x%"PRIx32", got 0x%"PRIx32"\n",
                    (uint64_t) i, ctr, val);

            return false;
        }
    }

    return true;
}

int test_fn_txrx_hwloop(struct bladerf *dev, struct app_params *p)
{
    int status = 0;
    char cwd[256];
    char dev_tx[128] = "\0";
    char dev_rx[128] = "\0";
    char test_cmd[256] = "/output/libbladeRF_test_txrx_hwloop\0";
    int cmd = 0;
    size_t i = 0;
    bool skip_print = false;
    bool user_input_complete = false;

    if(getcwd(cwd, sizeof(cwd)) == NULL) {
        fprintf(stderr, "[Error] Can't get current directory\n");
        return -1;
    };

    while (cmd != 'q' && status == 0 && user_input_complete == false) {
        if (!skip_print) {
            printf("\nTest %u\n", (unsigned int) i + 1);
            printf("---------------------------------\n");
            printf("Test setup\n");
            printf(" t - TX bladeRF device string.\n");
            printf(" r - RX bladeRF device string.\n");
            printf(" b - Number of samples in a burst.\n");
            printf(" p - Length between timestamps in samples.\n");
            printf(" f - %% of burst to fill with [2000,2000].\n");
            printf("      others set to [0,0].\n");
            printf(" l - Enables RX device for TX capture.\n");
            printf(" c - Outputs CSV RX capture from TX device.\n");
            printf("       for multi-device fill comparison.\n");
            printf(" i - Number of pulses.\n");
            printf(" q - (Q)uit.\n\n");
            if (strcmp(dev_tx,"\0") == 0 && strcmp(dev_rx,"\0") != 0) {
                printf("TX device string required\n");
            } else if (strcmp(dev_rx,"\0") == 0 && strcmp(dev_tx, "\0") != 0) {
                printf("RX device string required\n");
            }
            printf("> ");
        }

        skip_print = false;

        cmd = getchar();
        switch (cmd) {
            case 'q':
                break;

            case 't':
                while(scanf("%s", dev_tx) == 0);
                break;

            case 'r':
                while(scanf("%s", dev_rx) == 0);
                break;


            case '\r':
            case '\n':
                skip_print = true;
                break;

            default:
                break;
        }

        if (strcmp(dev_tx, "\0") != 0 && strcmp(dev_rx, "\0") != 0) {
            user_input_complete = true;

            strcat(test_cmd, " -t \"");
            strcat(test_cmd, dev_tx);
            strcat(test_cmd, "\"");

            strcat(test_cmd, " -r \"");
            strcat(test_cmd, dev_rx);
            strcat(test_cmd, "\"");
        }
    }

    status = system(strcat(cwd, test_cmd));
    if (status == -1) {
        fprintf(stderr, "hwloop test failed to run: system err: %i\n", status);
        status = BLADERF_ERR_UNEXPECTED;
        return status;
    }

    return status;
}

