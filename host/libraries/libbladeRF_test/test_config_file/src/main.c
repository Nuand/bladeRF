/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2017 Nuand LLC
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

/**
 * Test program for libbladeRF configuration file parsing
 *
 * Creates a config file with a few different scope restrictions, then
 * makes sure the proper activity happens.
 *
 * Currently, it tests:
 *  Option keys:
 *      frequency
 *  Restrictions:
 *      none
 *      x40
 *      x115
 *      serial number
 */

#include <libbladeRF.h>
#include <stdio.h>
#include <stdlib.h>

#include "bladerf_priv.h"
#include "config.h"
#include "test_common.h"

#ifdef WIN32
#include "mkdtemp.h"
#define MAX_FILENAME_LEN 256
static char const DIR_DELIM[] = "\\";
#else
size_t const MAX_FILENAME_LEN = 256;
static char const DIR_DELIM[] = "/";
#endif  // WIN32

/**
 * Structure for storing test state and conditions
 */
typedef struct {
    struct bladerf_devinfo ident; /**< devinfo struct for this test */
    bladerf_fpga_size fpga_size;  /**< FPGA size */
    uint64_t frequency;           /**< Expected frequency */
} test_state;

/**
 * Test cases
 *
 * .ident and .fpga_size are inputs, and the remainder are expected end
 * states for their respective keys.
 */
// clang-format off
static const test_state states[] = {
    {
        FIELD_INIT(.ident, {
            FIELD_INIT(.backend, BLADERF_BACKEND_DUMMY),
            FIELD_INIT(.serial, "9f9f90dbe3e5ee1218c86b8839db1995"),
            FIELD_INIT(.usb_bus, 1),
            FIELD_INIT(.usb_addr, 2),
            FIELD_INIT(.instance, 0)
        }),
        FIELD_INIT(.fpga_size, BLADERF_FPGA_40KLE),
        FIELD_INIT(.frequency, 2000000000),
    },
    {
        FIELD_INIT(.ident, {
            FIELD_INIT(.backend, BLADERF_BACKEND_DUMMY),
            FIELD_INIT(.serial, "df34f5f71a4e812327ac9b04538386af"),
            FIELD_INIT(.usb_bus, 1),
            FIELD_INIT(.usb_addr, 3),
            FIELD_INIT(.instance, 1)
        }),
        FIELD_INIT(.fpga_size, BLADERF_FPGA_115KLE),
        FIELD_INIT(.frequency, 1800000000),
    },
    {
        FIELD_INIT(.ident, {
            FIELD_INIT(.backend, BLADERF_BACKEND_DUMMY),
            FIELD_INIT(.serial, "742330d6617e449e7bb460e802d50701"),
            FIELD_INIT(.usb_bus, 2),
            FIELD_INIT(.usb_addr, 1),
            FIELD_INIT(.instance, 2)
        }),
        FIELD_INIT(.fpga_size, BLADERF_FPGA_40KLE),
        FIELD_INIT(.frequency, 1600000000),
    },
};
// clang-format on


/**
 * Contents of test configuration file...
 */
static char const *test_config[] = {
    "frequency 1.0G",  // All boards: set frequency to 1.0 GHz
    "frequency 1.4G",  // All boards: set frequency to 1.4 GHz
    "[x40]",
    "frequency 1.6G",  // If x40 FPGA: set frequency to 1.6 GHz
    "[x115]",
    "frequency 1.8G",  // If x115 FPGA: set frequency to 1.8 GHz
    "[*:serial=9f9f90dbe3e5ee1218c86b8839db1995]",
    "frequency 2.0G",  // If serial 9f9f90db..., set frequency to 2.0 GHz
};

/**
 * Global test history variable, used by {update,check}_test_state
 */
static test_state HISTORY[ARRAY_SIZE(states)];

/**
 * Writes a config file
 *
 * @param[in]   filename    Filename to write to
 * @param[in]   config      Array of lines to write to config file
 * @param[in]   len         Number of elements in config
 *
 * @return 0 on success, or value from \ref RETCODES list on failure
 */
int create_config(char const *filename, char const **config, size_t len)
{
    size_t i;

    FILE *f = fopen(filename, "w");
    if (NULL == f) {
        perror("fopen failed");
        return BLADERF_ERR_IO;
    }

    for (i = 0; i < len; ++i) {
        fprintf(f, "%s\n", config[i]);
    }

    fclose(f);

    printf("Created config file: %s\n", filename);

    return 0;
}

/**
 * Creates a fake test bladeRF device
 *
 * @param[out]  device      Update with device handle on success
 * @param[in]   devinfo     Device specification. If NULL, any available
 *                          device will be opened.
 *
 * @return 0 on success, or value from \ref RETCODES list on failure
 */
int create_test_device(struct bladerf **device,
                       struct bladerf_devinfo const *devinfo)
{
    struct bladerf *dev;

    dev = (struct bladerf *)calloc(1, sizeof(struct bladerf));
    if (dev == NULL) {
        return BLADERF_ERR_MEM;
    }

    MUTEX_INIT(&dev->ctrl_lock);
    MUTEX_INIT(&dev->sync_lock[BLADERF_MODULE_RX]);
    MUTEX_INIT(&dev->sync_lock[BLADERF_MODULE_TX]);

    dev->fpga_version.describe = calloc(1, BLADERF_VERSION_STR_MAX + 1);
    if (dev->fpga_version.describe == NULL) {
        free(dev);
        return BLADERF_ERR_MEM;
    }

    dev->fw_version.describe = calloc(1, BLADERF_VERSION_STR_MAX + 1);
    if (dev->fw_version.describe == NULL) {
        free((void *)dev->fpga_version.describe);
        free(dev);
        return BLADERF_ERR_MEM;
    }

    dev->capabilities = 0;
    dev->ident        = *devinfo;

    *device = dev;

    return 0;
}

/**
 * Update the test state for a given device and key
 *
 * @param[in]   dev     Device handle
 * @param[in]   key     Key to update
 * @param[in]   value   New value
 *
 * @return 0 on success, or value from \ref RETCODES list on failure
 */
int update_test_state(struct bladerf *dev, char const *key, uint64_t value)
{
    size_t i;

    if (NULL == dev || NULL == key) {
        return BLADERF_ERR_INVAL;
    }

    for (i = 0; i < ARRAY_SIZE(states); ++i) {
        test_state const *t = &states[i];

        if (bladerf_devinfo_matches(&dev->ident, &t->ident)) {
            if (0 == strcmp(key, "frequency")) {
                HISTORY[i].frequency = value;
                return 0;
            }

            return BLADERF_ERR_INVAL;
        }
    }

    return BLADERF_ERR_NODEV;
}

/**
 * Checks the current test value for a given device and key against the
 * expected values.
 *
 * @param[in]   dev     Device handle
 * @param[in]   key     Key to test
 *
 * @return true if the value is as expected, false otherwise
 */
bool check_test_state(struct bladerf *dev, char const *key)
{
    size_t i;

    if (NULL == dev || NULL == key) {
        return false;
    }

    for (i = 0; i < ARRAY_SIZE(states); ++i) {
        test_state const *t = &states[i];

        if (bladerf_devinfo_matches(&dev->ident, &t->ident)) {
            if (0 == strcmp(key, "frequency")) {
                return states[i].frequency == HISTORY[i].frequency;
            }
        }
    }

    return false;
}

/**
 * Surrogate bladerf_set_frequency handler
 *
 * @param       dev         Device handle
 * @param       module      Module to configure
 * @param       frequency   Desired frequency
 *
 * @return 0 on success, or value from \ref RETCODES list on failure
 */
int bladerf_set_frequency(struct bladerf *dev,
                          bladerf_module module,
                          unsigned int frequency)
{
    return update_test_state(dev, "frequency", frequency);
}

/**
 * Surrogate bladerf_close handler
 *
 * @param       dev         Device handle
 */
void bladerf_close(struct bladerf *dev)
{
    if (dev) {
        free((void *)dev->fpga_version.describe);
        free((void *)dev->fw_version.describe);

        free(dev);
    }
}

/**
 * Main program
 */
int main(int argc, char *argv[])
{
    char filename[MAX_FILENAME_LEN];
    char tmpdir[MAX_FILENAME_LEN];
    char tmpdirname[] = "libbladeRF_test_config_file.XXXXXX";
    char *tmpbase     = NULL;
    bool result       = true;
    int rv;
    size_t i;

    /* Get the TEMP variable from the environment, if it exists */
    tmpbase = getenv("TEMP");

    if (NULL == tmpbase) {
        /* It doesn't, so let's use the current working directory */
        tmpbase = malloc(MAX_FILENAME_LEN);

        if (NULL == tmpbase) {
            perror("malloc failed");
            exit(EXIT_FAILURE);
        }

        if (NULL == getcwd(tmpbase, MAX_FILENAME_LEN - 1)) {
            perror("getcwd failed");
            exit(EXIT_FAILURE);
        }
    }

    /* Concatenate directory names */
    rv = snprintf(tmpdir, MAX_FILENAME_LEN - 1, "%s%s%s", tmpbase, DIR_DELIM,
                  tmpdirname);

    if (rv < 0 || rv >= (int)(MAX_FILENAME_LEN - 1)) {
        perror("snprintf failed");
        exit(EXIT_FAILURE);
    }

    /* Create a temporary directory for testing */
    if (NULL == mkdtemp(tmpdir)) {
        perror("mkdtemp failed");
        exit(EXIT_FAILURE);
    }

    /* Create a sample config file there */
    rv = snprintf(filename, MAX_FILENAME_LEN - 1, "%s%s%s", tmpdir, DIR_DELIM,
                  "bladeRF.conf");

    if (rv < 0 || rv >= (int)(MAX_FILENAME_LEN - 1)) {
        perror("snprintf failed");
        exit(EXIT_FAILURE);
    }

    if (0 > create_config(filename, test_config, ARRAY_SIZE(test_config))) {
        perror("create_config failed");
        exit(EXIT_FAILURE);
    }

    /* Change working directory */
    if (0 > chdir(tmpdir)) {
        perror("chdir failed");
        exit(EXIT_FAILURE);
    }

    bladerf_log_set_verbosity(BLADERF_LOG_LEVEL_DEBUG);

    /* Iterate through the test cases */
    for (i = 0; i < ARRAY_SIZE(states); ++i) {
        test_state const *t = &states[i];
        struct bladerf *dev = NULL;
        bool check          = false;

        int status;

        status = create_test_device(&dev, &t->ident);
        if (status != 0) {
            fprintf(stderr, "Unable to open device: %s\n",
                    bladerf_strerror(status));
            exit(EXIT_FAILURE);
        }

        dev->fpga_size = states[i].fpga_size;

        printf("%s: fpga=%i backend=%d device=%d:%d instance=%d\n",
               dev->ident.serial, dev->fpga_size, dev->ident.backend,
               dev->ident.usb_bus, dev->ident.usb_addr, dev->ident.instance);

        status = config_load_options_file(dev);
        if (status != 0) {
            fprintf(stderr, "Unable to load options file: %s\n",
                    bladerf_strerror(status));
            exit(EXIT_FAILURE);
        }

        check = check_test_state(dev, "frequency");
        printf("\tfrequency=%s\n", check ? "pass" : "FAIL");
        if (!check) {
            result = false;
        }

        printf("\n");

        bladerf_close(dev);
    }

    /* Remove the temporary directory */
    if (0 > unlink(filename)) {
        perror("unlink failed");
        exit(EXIT_FAILURE);
    }

    /* Change working directory */
    if (0 > chdir(tmpbase)) {
        perror("chdir failed");
        exit(EXIT_FAILURE);
    }

    if (0 > rmdir(tmpdir)) {
        perror("rmdir failed");
        exit(EXIT_FAILURE);
    }

    printf("\nEnd Result: %s\n", result ? "PASS" : "FAIL");
    exit(result ? 0 : EXIT_FAILURE);

    return 0;
}
