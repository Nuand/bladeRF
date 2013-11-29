/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2013  Daniel Gröber <dxld AT darkboxed DOT org>
 * Copyright (C) 2013  Nuand, LLC.
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

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <rel_assert.h>
#include <limits.h>
#include <errno.h>

#include "libbladeRF.h"
#include "bladerf_priv.h"
#include "host_config.h"
#include "sha256.h"
#include "log.h"
#include "minmax.h"
#include "file_ops.h"

/* These two are used interchangeably - ensure they're the same! */
#if SHA256_DIGEST_SIZE != BLADERF_IMAGE_CHECKSUM_LEN
#error "Image checksum size mismatch"
#endif

#define CALC_IMAGE_SIZE(len) ((size_t) \
     ( \
        BLADERF_IMAGE_MAGIC_LEN + \
        BLADERF_IMAGE_CHECKSUM_LEN + \
        3 * sizeof(uint16_t) + \
        sizeof(uint64_t) + \
        BLADERF_SERIAL_LENGTH + \
        BLADERF_IMAGE_RESERVED_LEN + \
        3 * sizeof(uint32_t) + \
        len \
     ) \
)

static const char image_magic[] = "bladeRF";

#if BLADERF_OS_WINDOWS
#include <time.h>
static uint64_t get_timestamp()
{
    __time64_t now = _time64(NULL);
    return (uint64_t)now;
}
#else
#include <sys/time.h>
static inline uint64_t get_timestamp()
{
    uint64_t ret;
    struct timeval tv;

    if (gettimeofday(&tv, NULL) == 0) {
        ret = tv.tv_sec;
    } else {
        log_verbose("gettimeofday failed: %s\n", strerror(errno));
        ret = 0;
    }

    return ret;
}
#endif

static void sha256_buffer(const char *buf, size_t len,
                          char digest[SHA256_DIGEST_SIZE])
{
    SHA256_CTX ctx;

    SHA256_Init(&ctx);
    SHA256_Update(&ctx, buf, len);
    SHA256_Final((uint8_t*)digest, &ctx);
}

static int verify_checksum(uint8_t *buf, size_t buf_len)
{
    char checksum_expected[SHA256_DIGEST_SIZE];
    char checksum_calc[SHA256_DIGEST_SIZE];

    if (buf_len <= CALC_IMAGE_SIZE(0)) {
        log_debug("Provided buffer isn't a full image\n");
        return BLADERF_ERR_INVAL;
    }

    /* Backup and clear the expected checksum before we calculate the
     * expected checksum */
    memcpy(checksum_expected, &buf[BLADERF_IMAGE_MAGIC_LEN],
           sizeof(checksum_expected));

    memset(&buf[BLADERF_IMAGE_MAGIC_LEN], 0, SHA256_DIGEST_SIZE);

    sha256_buffer((const char *)buf, buf_len, checksum_calc);

    if (memcmp(checksum_expected, checksum_calc, SHA256_DIGEST_SIZE) != 0) {
        return BLADERF_ERR_CHECKSUM;
    } else {
        /* Restore the buffer's checksum so the caller can still use it */
        memcpy(&buf[BLADERF_IMAGE_MAGIC_LEN], checksum_expected,
               sizeof(checksum_expected));

        return 0;
    }
}

static bool image_type_is_valid(bladerf_image_type type) {
    switch (type) {
        case BLADERF_IMAGE_TYPE_RAW:
        case BLADERF_IMAGE_TYPE_FIRMWARE:
        case BLADERF_IMAGE_TYPE_FPGA_40KLE:
        case BLADERF_IMAGE_TYPE_FPGA_115KLE:
        case BLADERF_IMAGE_TYPE_CALIBRATION:
            return true;

        default:
            return false;
    }
}

/* Serialize image contents and fill in checksum */
static size_t pack_image(struct bladerf_image *img, uint8_t *buf)
{
    size_t i = 0;
    uint16_t ver_field;
    uint32_t type, len, addr;
    uint64_t timestamp;
    char checksum[BLADERF_IMAGE_CHECKSUM_LEN];

    memcpy(&buf[i], img->magic, BLADERF_IMAGE_MAGIC_LEN);
    i += BLADERF_IMAGE_MAGIC_LEN;

    memset(&buf[i], 0, BLADERF_IMAGE_CHECKSUM_LEN);
    i += BLADERF_IMAGE_CHECKSUM_LEN;

    ver_field = HOST_TO_BE16(img->version.major);
    memcpy(&buf[i], &ver_field, sizeof(ver_field));
    i += sizeof(ver_field);

    ver_field = HOST_TO_BE16(img->version.minor);
    memcpy(&buf[i], &ver_field, sizeof(ver_field));
    i += sizeof(ver_field);

    ver_field = HOST_TO_BE16(img->version.patch);
    memcpy(&buf[i], &ver_field, sizeof(ver_field));
    i += sizeof(ver_field);

    timestamp = HOST_TO_BE64(img->timestamp);
    memcpy(&buf[i], &timestamp, sizeof(timestamp));
    i += sizeof(timestamp);

    memcpy(&buf[i], &img->serial, BLADERF_SERIAL_LENGTH);
    i += BLADERF_SERIAL_LENGTH;

    memset(&buf[i], 0, BLADERF_IMAGE_RESERVED_LEN);
    i += BLADERF_IMAGE_RESERVED_LEN;

    type = HOST_TO_BE32((uint32_t)img->type);
    memcpy(&buf[i], &type, sizeof(type));
    i += sizeof(type);

    addr = HOST_TO_BE32(img->address);
    memcpy(&buf[i], &addr, sizeof(addr));
    i += sizeof(addr);

    len = HOST_TO_BE32(img->length);
    memcpy(&buf[i], &len, sizeof(len));
    i += sizeof(len);

    memcpy(&buf[i], img->data, img->length);
    i += img->length;

    sha256_buffer((const char *)buf, i, checksum);
    memcpy(&buf[BLADERF_IMAGE_MAGIC_LEN], checksum, BLADERF_IMAGE_CHECKSUM_LEN);

    return i;
}

/* Unpack flash image from file and validate fields */
static int unpack_image(struct bladerf_image *img, uint8_t *buf, size_t len)
{
    size_t i = 0;
    uint32_t type;

    /* Ensure we have at least a full set of metadata */
    if (len < CALC_IMAGE_SIZE(0)) {
        return BLADERF_ERR_INVAL;
    }

    memcpy(img->magic, &buf[i], BLADERF_IMAGE_MAGIC_LEN);
    img->magic[BLADERF_IMAGE_MAGIC_LEN] = '\0';
    if (strncmp(img->magic, image_magic, BLADERF_IMAGE_MAGIC_LEN)) {
        return BLADERF_ERR_INVAL;
    }
    i += BLADERF_IMAGE_MAGIC_LEN;

    memcpy(img->checksum, &buf[i], BLADERF_IMAGE_CHECKSUM_LEN);
    i += BLADERF_IMAGE_CHECKSUM_LEN;

    memcpy(&img->version.major, &buf[i], sizeof(img->version.major));
    i += sizeof(img->version.major);
    img->version.major = BE16_TO_HOST(img->version.major);

    memcpy(&img->version.minor, &buf[i], sizeof(img->version.minor));
    i += sizeof(img->version.minor);
    img->version.minor = BE16_TO_HOST(img->version.minor);

    memcpy(&img->version.patch, &buf[i], sizeof(img->version.patch));
    i += sizeof(img->version.patch);
    img->version.patch = BE16_TO_HOST(img->version.patch);

    memcpy(&img->timestamp, &buf[i], sizeof(img->timestamp));
    i += sizeof(img->timestamp);
    img->timestamp = BE64_TO_HOST(img->timestamp);

    memcpy(img->serial, &buf[i], BLADERF_SERIAL_LENGTH);
    img->serial[BLADERF_SERIAL_LENGTH] = '\0';
    i += BLADERF_SERIAL_LENGTH;

    memcpy(img->reserved, &buf[i], BLADERF_IMAGE_RESERVED_LEN);
    i += BLADERF_IMAGE_RESERVED_LEN;

    memcpy(&type, &buf[i], sizeof(type));
    i += sizeof(type);
    type = BE32_TO_HOST(type);

    if (!image_type_is_valid((bladerf_image_type)type)) {
        log_debug("Invalid type value in image: %d\n", (int)type);
        return BLADERF_ERR_INVAL;
    } else {
        img->type = (bladerf_image_type)type;
    }

    memcpy(&img->address, &buf[i], sizeof(img->address));
    i += sizeof(img->address);
    img->address = BE32_TO_HOST(img->address);

    memcpy(&img->length, &buf[i], sizeof(img->length));
    i += sizeof(img->length);
    img->length = BE32_TO_HOST(img->length);

    if (len != CALC_IMAGE_SIZE(img->length)) {
        log_debug("Image contains more or less data than expected\n");
        return BLADERF_ERR_INVAL;
    }

    /* Just slide the data over */
    memmove(&buf[0], &buf[i], img->length);
    img->data = buf;

    return 0;
}


int bladerf_image_write(struct bladerf_image *img, const char *file)
{
    int rv;
    FILE *f = NULL;
    uint8_t *buf = NULL;
    size_t buf_len;

    /* Ensure the format identifier is correct */
    if (memcmp(img->magic, image_magic, BLADERF_IMAGE_MAGIC_LEN) != 0) {
#ifdef LOGGING_ENABLED
        char badmagic[BLADERF_IMAGE_MAGIC_LEN + 1];
        memset(badmagic, 0, sizeof(badmagic));
        memcpy(&badmagic, &img->magic, BLADERF_IMAGE_MAGIC_LEN);
        log_debug("Invalid file format magic value: %s\n", badmagic);
#endif
        return BLADERF_ERR_INVAL;
    }

    /* Check for a valid image type */
    if (!image_type_is_valid(img->type)) {
        log_debug("Invalid image type: %d\n", img->type);
        return BLADERF_ERR_INVAL;
    }

    /* Just to be tiny bit paranoid... */
    if (!img->data) {
        log_debug("Image data pointer is NULL\n");
        return BLADERF_ERR_INVAL;
    }

    buf_len = CALC_IMAGE_SIZE(img->length);
    buf = (uint8_t *)calloc(1, buf_len);
    if (!buf) {
        log_verbose("calloc failed: %s\n", strerror(errno));
        return BLADERF_ERR_MEM;
    }

    pack_image(img, buf);

    f = fopen(file, "wb");
    if (!f) {
        log_debug("Failed to open \"%s\": %s\n", file, strerror(errno));
        rv = BLADERF_ERR_IO;
        goto bladerf_image_write_out;
    }

    rv = file_write(f, buf, buf_len);

bladerf_image_write_out:
    if (f) {
        fclose(f);
    }
    free(buf);
    return rv;
}

int bladerf_image_read(struct bladerf_image *img, const char *file)
{
    int rv = -1;
    uint8_t *buf = NULL;
    size_t buf_len;

    rv = file_read_buffer(file, &buf, &buf_len);
    if (rv < 0) {
        goto bladerf_image_read_out;
    }

    rv = verify_checksum(buf, buf_len);
    if (rv < 0) {
        goto bladerf_image_read_out;
    }

    /* Note: On success, buf->data = buf, with the data memmove'd over.
     *       Static analysis tools might indicate a false postive leak when
     *       buf goes out of scope with rv == 0 */
    rv = unpack_image(img, buf, buf_len);

bladerf_image_read_out:
    if (rv != 0) {
        free(buf);
    }

    return rv;
}

struct bladerf_image * bladerf_alloc_image(bladerf_image_type type,
                                           uint32_t address,
                                           uint32_t length)
{
    struct bladerf_image *image;

    assert(BLADERF_IMAGE_MAGIC_LEN == (sizeof(image_magic) - 1));

    image = (struct bladerf_image *)calloc(1, sizeof(*image));

    if (!image) {
        return NULL;
    }

    if (length) {
        image->data = (uint8_t *)calloc(1, length);
        if (!image->data) {
            free(image);
            return NULL;
        }
    }

    memcpy(image->magic, &image_magic, BLADERF_IMAGE_MAGIC_LEN);

    image->version.major = 0;
    image->version.minor = 1;
    image->version.patch = 0;
    image->timestamp = get_timestamp();
    image->address = address;
    image->length = length;
    image->type = type;

    return image;
}

static int make_cal_region(bladerf_fpga_size size, uint16_t vctcxo_trim,
                           uint8_t *buf, size_t len)
{
    int rv;
    static const char fpga_size_40[] = "40";
    static const char fpga_size_115[] = "115";
    const char *fpga_size;
    char dac[7] = {0};

    if (size == BLADERF_FPGA_40KLE) {
        fpga_size = fpga_size_40;
    } else if (size == BLADERF_FPGA_115KLE) {
        fpga_size = fpga_size_115;
    } else {
        assert(0); /* Bug catcher */
        return BLADERF_ERR_INVAL;
    }

    memset(buf, 0xff, len);

    assert(len < INT_MAX);
    rv = add_field((char*)buf, (int)len, "B", fpga_size);

    if (rv < 0) {
        return rv;
    }

    sprintf(dac, "%u", vctcxo_trim);

    rv = add_field((char*)buf, (int)len, "DAC", dac);
    if (rv < 0) {
        return rv;
    }

    return 0;
}

struct bladerf_image * bladerf_alloc_cal_image(bladerf_fpga_size fpga_size,
                                               uint16_t vctcxo_trim)
{
    struct bladerf_image *image;
    int status;

    image = bladerf_alloc_image(BLADERF_IMAGE_TYPE_CALIBRATION,
                                BLADERF_FLASH_ADDR_CALIBRATION,
                                BLADERF_FLASH_LEN_CALIBRATION);

    if (!image) {
        return NULL;
    }

    status = make_cal_region(fpga_size, vctcxo_trim,
                             image->data, image->length);

    if (status != 0) {
        bladerf_free_image(image);
        image = NULL;
    }

    return image;
}

void bladerf_free_image(struct bladerf_image *image)
{
    if (image) {
        free(image->data);
        free(image);
    }
}

