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
#include "bladerf_priv.h"
#include "fpga.h"
#include "version_compat.h"
#include "file_ops.h"
#include "log.h"
#include "flash.h"

int fpga_check_version(struct bladerf *dev)
{
    int status = version_check_fpga(dev);

#if LOGGING_ENABLED
    const unsigned int fw_maj = dev->fw_version.major;
    const unsigned int fw_min = dev->fw_version.minor;
    const unsigned int fw_pat = dev->fw_version.patch;
    const unsigned int fpga_maj = dev->fpga_version.major;
    const unsigned int fpga_min = dev->fpga_version.minor;
    const unsigned int fpga_pat = dev->fpga_version.patch;
    unsigned int req_maj, req_min, req_pat;
    struct bladerf_version req;

    if (status == BLADERF_ERR_UPDATE_FPGA) {
        version_required_fpga(dev, &req);
        req_maj = req.major;
        req_min = req.minor;
        req_pat = req.patch;

        log_warning("FPGA v%u.%u.%u was detected. Firmware v%u.%u.%u "
                    "requires FPGA v%u.%u.%u or later. Please load a "
                    "different FPGA version before continuing.\n\n",
                    fpga_maj, fpga_min, fpga_pat,
                    fw_maj, fw_min, fw_pat,
                    req_maj, req_min, req_pat);
    } else if (status == BLADERF_ERR_UPDATE_FW) {
        version_required_fw(dev, &req, true);
        req_maj = req.major;
        req_min = req.minor;
        req_pat = req.patch;

        log_warning("FPGA v%u.%u.%u was detected, which requires firmware "
                    "v%u.%u.%u or later. The device firmware is currently "
                    "v%u.%u.%u. Please upgrade the device firmware before "
                    "continuing.\n\n",
                    fpga_maj, fpga_min, fpga_pat,
                    req_maj, req_min, req_pat,
                    fw_maj, fw_min, fw_pat);
    }
#endif

    return status;
}

/* We do not build FPGAs with compression enabled. Therfore, they
 * will always have a fixed file size.
 */
#define FPGA_SIZE_X40   (1191788)
#define FPGA_SIZE_X115  (3571462)

static bool valid_fpga_size(bladerf_fpga_size fpga, size_t len)
{
    static const char env_override[] = "BLADERF_SKIP_FPGA_SIZE_CHECK";
    bool valid;

    switch (fpga) {
        case BLADERF_FPGA_40KLE:
            valid = (len == FPGA_SIZE_X40);
            break;

        case BLADERF_FPGA_115KLE:
            valid = (len == FPGA_SIZE_X115);
            break;

        default:
            log_debug("Unknown FPGA type (%d). Using relaxed size criteria.\n",
                      fpga);

            if (len < (1 * 1024 * 1024)) {
                valid = false;
            } else if (len > BLADERF_FLASH_BYTE_LEN_FPGA) {
                valid = false;
            } else {
                valid = true;
            }
    }

    /* Provide a means to override this check. This is intended to allow
     * folks who know what they're doing to work around this quickly without
     * needing to make a code change. (e.g., someone building a custom FPGA
     * image that enables compressoin) */
    if (getenv(env_override)) {
        log_info("Overriding FPGA size check per %s\n", env_override);
        valid = true;
    }

    if (!valid) {
        log_warning("Detected potentially incorrect FPGA file.\n");

        log_debug("If you are certain this file is valid, you may define\n"
                  "BLADERF_SKIP_FPGA_SIZE_CHECK in your environment to skip this check.\n\n");
    }

    return valid;
}

int fpga_load_from_file(struct bladerf *dev, const char *fpga_file)
{
    uint8_t *buf = NULL;
    size_t  buf_size;
    int status;

    status = file_read_buffer(fpga_file, &buf, &buf_size);
    if (status != 0) {
        goto error;
    }

    if (!valid_fpga_size(dev->fpga_size, buf_size)) {
        status = BLADERF_ERR_INVAL;
        goto error;
    }

    status = dev->fn->load_fpga(dev, buf, buf_size);
    if (status != 0) {
        goto error;
    }

    status = fpga_check_version(dev);
    if (status != 0) {
        goto error;
    }

    status = init_device(dev);
    if (status != 0) {
        goto error;
    }

error:
    free(buf);
    return status;
}

int fpga_write_to_flash(struct bladerf *dev, const char *fpga_file)
{
    int status;
    size_t buf_size;
    uint8_t *buf = NULL;

    status = file_read_buffer(fpga_file, &buf, &buf_size);
    if (status == 0) {
        if (!valid_fpga_size(dev->fpga_size, buf_size)) {
            status = BLADERF_ERR_INVAL;
        } else {
            status = flash_write_fpga_bitstream(dev, &buf, buf_size);
        }
    }

    free(buf);
    return status;
}
