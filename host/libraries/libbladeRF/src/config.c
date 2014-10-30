/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014 Nuand LLC
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include <stdlib.h>
#include "bladerf_priv.h"
#include "dc_cal_table.h"
#include "fpga.h"
#include "file_ops.h"
#include "log.h"
#include "config.h"

static inline void load_dc_cal(struct bladerf *dev, const char *file)
{
    int status;
    struct bladerf_image *img;

    img = bladerf_alloc_image(BLADERF_IMAGE_TYPE_INVALID, 0, 0);
    if (img == NULL) {
        return;
    }

    status = bladerf_image_read(img, file);
    if (status != 0) {
        log_debug("Failed to open image file (%s): %s\n",
                  file, bladerf_strerror(status));
        goto out;
    }

    switch (img->type) {
        case BLADERF_IMAGE_TYPE_RX_DC_CAL:
            dc_cal_tbl_free(&dev->cal.dc_rx);
            dev->cal.dc_rx = dc_cal_tbl_load(img->data, img->length);
            break;

        case BLADERF_IMAGE_TYPE_TX_DC_CAL:
            dc_cal_tbl_free(&dev->cal.dc_tx);
            dev->cal.dc_tx = dc_cal_tbl_load(img->data, img->length);
            break;

        default:
            log_debug("%s is not an RX DC calibration table.\n", file);
    }

out:
    bladerf_free_image(img);
}

int config_load_fpga(struct bladerf *dev)
{
    int status = 0;
    char *filename = NULL;

    if (dev->fpga_size == BLADERF_FPGA_40KLE) {
        filename = file_find("hostedx40.rbf");
    } else if (dev->fpga_size == BLADERF_FPGA_115KLE) {
        filename = file_find("hostedx115.rbf");
    } else {
        /* Unexpected, but this will be complained about elsewhere */
        return 0;
    }

    if (filename != NULL) {
        log_debug("Loading FPGA from: %s\n", filename);
        status = fpga_load_from_file(dev, filename);
    }

    free(filename);
    return status;
}

int config_load_dc_cals(struct bladerf *dev)
{
    char *filename;
    char *full_path;

    filename = calloc(1, FILENAME_MAX + 1);
    if (filename == NULL) {
        return BLADERF_ERR_MEM;
    }

    strncat(filename, dev->ident.serial, FILENAME_MAX);
    strncat(filename, "_dc_rx.tbl", FILENAME_MAX - BLADERF_SERIAL_LENGTH);
    full_path = file_find(filename);
    if (full_path != NULL) {
        log_debug("Loading %s\n", full_path);
        load_dc_cal(dev, full_path);
        free(full_path);
    }

    memset(filename, 0, FILENAME_MAX + 1);
    strncat(filename, dev->ident.serial, FILENAME_MAX);
    strncat(filename, "_dc_tx.tbl", FILENAME_MAX - BLADERF_SERIAL_LENGTH);
    full_path = file_find(filename);
    if (full_path != NULL) {
        log_debug("Loading %s\n", full_path);
        load_dc_cal(dev, full_path);
        free(full_path);
    }

    free(filename);
    return 0;
}
