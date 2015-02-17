/*
 * This file implements functionality for reading and validating an FX3 firmware
 * image, and providing access to the image contents.
 *
 * Details about the image format can be found and FX3 bootloader can be found
 * in Cypress AN76405: EZ-USB (R) FX3 (TM) Boot Options:
 *  http://www.cypress.com/?docID=49862
 *
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
#include <string.h>
#include <stdint.h>

#include "fx3_fw.h"
#include "host_config.h"
#include "file_ops.h"
#include "log.h"
#include "rel_assert.h"

#define FX3_IMAGE_TYPE_NORMAL    0xb0    /* "Normal" image with checksum */

#define FX3_HDR_SIG_IDX          0x00
#define FX3_HDR_IMAGE_CTL_IDX    0x02
#define FX3_HDR_IMAGE_TYPE_IDX   0x03
#define FX3_HDR_IMAGE_LEN0_IDX   0x04
#define FX3_HDR_IMAGE_ADDR0_IDX  0x08
#define FX3_HDR_IMAGE_DATA0_IDX  0x0c

#define FX3_HDR_LEN             FX3_HDR_IMAGE_DATA0_IDX

#define FX3_RAM_SIZE_WORDS      (256 * 1024 / sizeof(uint32_t))

struct fx3_firmware {
    uint8_t  *data;
    uint32_t data_len;

    uint32_t entry_addr;

    uint32_t num_sections;
    uint32_t curr_section;
    uint32_t section_offset;
};

static inline uint32_t to_uint32(struct fx3_firmware *fw, uint32_t offset)
{
    uint32_t ret;

    assert((offset + sizeof(uint32_t)) <= fw->data_len);

    memcpy(&ret, &fw->data[offset], sizeof(ret));

    return LE32_TO_HOST(ret);
}

static inline bool is_valid_fx3_ram_addr(uint32_t addr, uint32_t len) {
    bool valid = true;

    /* If you're doing something fun, wild, and crazy with the FX3 and your
     * modifications of linker scripts has changed the firmware entry point,
     * you'll need to add this compile-time definition to suppress this check.
     *
     * One potential improvement here would be to define the I-TCM and SYSMEM
     * addresses at configuration/compilation-time to ensure they match
     * what's in the FX3's linker script. The default values are assumed here.
     */
#   ifndef BLADERF_SUPPRESS_FX3_FW_ENTRY_POINT_CHECK
    const uint32_t itcm_base = 0x00000000;
    const uint32_t itcm_len  = 0x4000;
    const uint32_t itcm_end  = itcm_base + itcm_len;

    const uint32_t sysmem_base = 0x40000000;
    const uint32_t sysmem_len  = 0x80000;
    const uint32_t sysmem_end  = sysmem_base + sysmem_len;

    const bool in_itcm   = (addr <  itcm_end) &&
                           (len  <= itcm_len) &&
                           ((addr + len) < itcm_end);

    const bool in_sysmem = (addr >= sysmem_base) &&
                           (addr <  sysmem_end) &&
                           (len  <= sysmem_len) &&
                           ((addr + len) < sysmem_end);

    /* In lieu of compilers issuing warnings over the fact that the condition
     * (addr >= itcm_base) is always true, this condition has been removed.
     *
     * Instead, an assertion has been added to catch the attention of anyone
     * making a change to the above itcm_base definition, albeit a *very*
     * unlikely change to make. */
    assert(itcm_base == 0); /* (addr >= itcm_base) guaranteed */

    valid = in_itcm || in_sysmem;
#   endif

    return valid;
}

static int scan_fw_sections(struct fx3_firmware *fw)
{
    int status = 0;
    bool done = false;      /* Have we read all the sections? */
    uint32_t checksum = 0;

    uint32_t offset, i;                 /* In bytes */
    uint32_t next_section;              /* Section offset in bytes */
    uint32_t section_len_words;         /* FW uses units of 32-bit words */
    uint32_t section_len_bytes;         /* Section length converted to bytes */

    /* Byte offset where the checksum is expected to be */
    const uint32_t checksum_off = fw->data_len - sizeof(uint32_t);

    /* These assumptions should have been verified earlier */
    assert(checksum_off > FX3_HDR_IMAGE_DATA0_IDX);
    assert((checksum_off % 4) == 0);

    offset = FX3_HDR_IMAGE_LEN0_IDX;

    while (!done) {

        /* Fetch the length of the current section */
        section_len_words = to_uint32(fw, offset);

        if (section_len_words > FX3_RAM_SIZE_WORDS) {
            log_debug("Firmware section %u is unexpectedly large.\n",
                      fw->num_sections);
            status = BLADERF_ERR_INVAL;
            goto error;
        } else {
            section_len_bytes = (uint32_t)(section_len_words * sizeof(uint32_t));
            offset += sizeof(uint32_t);
        }

        /* The list of sections is terminated by a 0 section length field  */
        if (section_len_bytes == 0) {
            fw->entry_addr = to_uint32(fw, offset);
            if (!is_valid_fx3_ram_addr(fw->entry_addr, 0)) {
                status = BLADERF_ERR_INVAL;
                goto error;
            }

            offset += sizeof(uint32_t);
            done = true;
        } else {
#           if LOGGING_ENABLED
            /* Just a value to print in verbose output */
            uint32_t section_start_offset = offset - sizeof(uint32_t);
#           endif

            uint32_t addr = to_uint32(fw, offset);
            if (!is_valid_fx3_ram_addr(addr, section_len_bytes)) {
                status = BLADERF_ERR_INVAL;
                goto error;
            }

            offset += sizeof(uint32_t);
            if (offset >= checksum_off) {
                log_debug("Firmware truncated after section address.\n");
                status = BLADERF_ERR_INVAL;
                goto error;
            }

            next_section = offset + section_len_bytes;

            if (next_section >= checksum_off) {
                log_debug("Firmware truncated in section %u\n",
                          fw->num_sections);
                status = BLADERF_ERR_INVAL;
                goto error;
            }

            for (i = offset; i < next_section; i += sizeof(uint32_t)) {
                checksum += to_uint32(fw, i);
            }

            offset = next_section;
            log_verbose("Scanned section %u at offset 0x%08x: "
                        "addr=0x%08x, len=0x%08x\n",
                        fw->num_sections, section_start_offset,
                        addr, section_len_words);

            fw->num_sections++;
        }
    }

    if (offset != checksum_off) {
        log_debug("Invalid offset or junk at the end of the firmware image.\n");
        status = BLADERF_ERR_INVAL;
    } else {
        const uint32_t expected_checksum = to_uint32(fw, checksum_off);

        if (checksum != expected_checksum) {
            log_debug("Bad checksum. Expected 0x%08x, got 0x%08x\n",
                      expected_checksum, checksum);

            status = BLADERF_ERR_INVAL;
        } else {
            log_verbose("Firmware checksum OK.\n");
            fw->section_offset = FX3_HDR_IMAGE_LEN0_IDX;
        }
    }

error:
    return status;
}

struct fx3_firmware * fx3_fw_init()
{
    return calloc(1, sizeof(struct fx3_firmware));
}

int fx3_fw_read(const char *file, struct fx3_firmware **fw_out)
{
    int status;
    size_t buf_len;
    struct fx3_firmware *fw;

    *fw_out = NULL;

    fw = calloc(1, sizeof(fw[0]));
    if (fw == NULL) {
        return BLADERF_ERR_MEM;
    }

    status = file_read_buffer(file, &fw->data, &buf_len);
    if (status != 0) {
        free(fw);
        return status;
    }

    if ((buf_len > UINT32_MAX)) {
        /* This is just intended to catch a crazy edge case, since we're casting
         * to 32-bits below. If this passes, the data length might still be well
         * over the 512 KiB RAM limit. */
        log_debug("Size of provided image is too large.\n");
        status = BLADERF_ERR_INVAL;
        goto error;
    } else {
        fw->data_len = (uint32_t) buf_len;
    }

    if (fw->data_len < FX3_HDR_LEN) {
        log_debug("Provided file is too short.");
        status = BLADERF_ERR_INVAL;
        goto error;
    }

    if ((fw->data_len % 4) != 0) {
        log_debug("Size of provided image is not a multiple of 4 bytes.\n");
        status = BLADERF_ERR_INVAL;
        goto error;
    }

    if (fw->data[FX3_HDR_SIG_IDX]     != 'C' &&
        fw->data[FX3_HDR_SIG_IDX + 1] != 'Y') {

        log_debug("FX3 firmware does have 'CY' marker.\n");
        status = BLADERF_ERR_INVAL;
        goto error;
    }

    if (fw->data[3] != FX3_IMAGE_TYPE_NORMAL) {
        log_debug("FX3 firmware header contained unexpected image type: "
                  "0x%02x\n", fw->data[FX3_HDR_IMAGE_TYPE_IDX]);
        status = BLADERF_ERR_INVAL;
        goto error;
    }

    status = scan_fw_sections(fw);
    if (status != 0) {
        goto error;
    }

error:

    if (status != 0) {
        free(fw->data);
        free(fw);
    } else {
        *fw_out = fw;
    }

    return status;
}

void fx3_fw_deinit(struct fx3_firmware *fw)
{
    free(fw->data);
    memset(fw, 0, sizeof(fw[0]));
    free(fw);
}

bool fx3_fw_next_section(struct fx3_firmware *fw, uint32_t *section_addr,
                        uint8_t **section_data, uint32_t *section_len)
{
    uint32_t len;
    uint32_t addr;
    uint8_t *data;

    /* Max offset is the checksum address */
    const uint32_t max_offset = fw->data_len - sizeof(uint32_t);

    assert(fw != NULL);
    assert(fw->data != NULL);

    *section_addr = 0;
    *section_data = NULL;
    *section_len = 0;

    if (fw->curr_section >= fw->num_sections) {
        return false;
    }

    /* Length in bytes (as converted from 32-bit words) */
    len = to_uint32(fw, fw->section_offset) * sizeof(uint32_t);
    if (len == 0) {
        return false;
    }

    /* Advance to address field */
    fw->section_offset += sizeof(uint32_t);
    assert(fw->section_offset < max_offset);
    addr = to_uint32(fw, fw->section_offset);

    /* Advance to data field */
    fw->section_offset += sizeof(uint32_t);
    assert(fw->section_offset < max_offset);
    data = &fw->data[fw->section_offset];

    /* Advance to the next section for the next call */
    fw->section_offset += len;
    assert(fw->section_offset < max_offset);
    fw->curr_section++;

    *section_addr = addr;
    *section_data = data;
    *section_len = len;
    return true;
}

uint32_t fx3_fw_entry_point(struct fx3_firmware *fw)
{
    assert(fw != NULL);
    return fw->entry_addr;
}

#ifdef TEST_FX3_FW_VALIDATION
int main(int argc, char *argv[])
{
    int status;
    struct fx3_firmware *fw;

    log_set_verbosity(BLADERF_LOG_LEVEL_VERBOSE);

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <firmware file>\n", argv[0]);
        return 1;
    }

    status = fx3_fw_read(argv[1], &fw);
    if (status == 0) {
        fx3_fw_deinit(fw);
    }

    return status;
}
#endif
