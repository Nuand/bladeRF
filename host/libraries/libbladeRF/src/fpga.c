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

static inline bool valid_fpga_size(size_t len)
{
    if (len < (1 * 1024 * 1024)) {
        return false;
    } else if (len > BLADERF_FLASH_BYTE_LEN_FPGA) {
        return false;
    } else {
        return true;
    }
}

int fpga_load_from_file(struct bladerf *dev, const char *fpga_file)
{
    uint8_t *buf = NULL;
    size_t  buf_size;
    int status;

    /* TODO sanity check FPGA:
     *  - Check for x40 vs x115 and verify FPGA image size
     *  - Known header/footer on images?
     *  - Checksum/hash?
     */
    status = file_read_buffer(fpga_file, &buf, &buf_size);
    if (status != 0) {
        goto error;
    }

    if (!valid_fpga_size(buf_size)) {
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
    const char env_override[] = "BLADERF_SKIP_FPGA_SIZE_CHECK";

    status = file_read_buffer(fpga_file, &buf, &buf_size);
    if (status == 0) {
        if (!getenv(env_override) && !valid_fpga_size(buf_size)) {
            log_warning("Detected potentially invalid firmware file.\n");

            /* You probably don't want to do this unless you know what you're
             * doing. Only show this to users who have gone hunting for
             * more information... */
            log_debug("Define BLADERF_SKIP_FPGA_SIZE_CHECK in your evironment "
                      "to skip this check.\n");
            status = BLADERF_ERR_INVAL;
        } else {
            status = flash_write_fpga_bitstream(dev, &buf, buf_size);
        }
    }

    free(buf);
    return status;
}
