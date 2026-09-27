/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2026 Nuand LLC
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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libbladeRF.h>
#include <bladeRF1.h>

#include "backend/backend.h"
#include "board/bladerf1/flash.h"
#include "board/board.h"
#include "host_config.h"
#include "LzmaDec.h"

#define FLASH_BYTES (32u << 17)
#define PAGE_BYTES 256u
#define EB_BYTES (64u << 10)
#define OVERSIZED_LEN 4000000u
#define RAW_EDGE_LEN (FLASH_BYTES - BLADERF_FLASH_ADDR_FPGA - PAGE_BYTES)
#define SMALL_LEN 1024u
#define FPGA_LZMA_BLOCK_BYTES (60u * 1024u)

struct test_ctx {
    uint8_t *flash;
    struct bladerf_version fw_version;
    int erase_count;
    int read_count;
    int write_count;
};

/**
 * Provide the only public libbladeRF symbol required by flash.c error paths.
 */
const char *CALL_CONV bladerf_strerror(int error)
{
    (void)error;
    return "test error";
}

/**
 * Provide the conversion helper required by unused flash.c OTP helpers.
 */
unsigned int str2uint(const char *str,
                      unsigned int min,
                      unsigned int max,
                      bool *ok)
{
    unsigned long value = strtoul(str, NULL, 0);

    if (ok != NULL) {
        *ok = value >= min && value <= max;
    }
    return (unsigned int)value;
}

/**
 * Return the test context attached to the fake device.
 */
static struct test_ctx *ctx_from_dev(struct bladerf *dev)
{
    return (struct test_ctx *)dev->backend_data;
}

/**
 * Return the fake firmware version configured for this test.
 */
static int fake_get_fw_version(struct bladerf *dev,
                               struct bladerf_version *version)
{
    *version = ctx_from_dev(dev)->fw_version;
    return 0;
}

/**
 * Erase the requested fake flash erase blocks.
 */
static int fake_erase_flash_blocks(struct bladerf *dev,
                                   uint32_t eb,
                                   uint16_t count)
{
    struct test_ctx *ctx = ctx_from_dev(dev);
    size_t offset = (size_t)eb * EB_BYTES;
    size_t len = (size_t)count * EB_BYTES;

    if (offset > FLASH_BYTES || len > FLASH_BYTES - offset) {
        return BLADERF_ERR_INVAL;
    }

    memset(ctx->flash + offset, 0xff, len);
    ++ctx->erase_count;
    return 0;
}

/**
 * Read the requested fake flash pages.
 */
static int fake_read_flash_pages(struct bladerf *dev,
                                 uint8_t *buf,
                                 uint32_t page,
                                 uint32_t count)
{
    struct test_ctx *ctx = ctx_from_dev(dev);
    size_t offset = (size_t)page * PAGE_BYTES;
    size_t len = (size_t)count * PAGE_BYTES;

    if (offset > FLASH_BYTES || len > FLASH_BYTES - offset) {
        return BLADERF_ERR_INVAL;
    }

    memcpy(buf, ctx->flash + offset, len);
    ++ctx->read_count;
    return 0;
}

/**
 * Write the requested fake flash pages.
 */
static int fake_write_flash_pages(struct bladerf *dev,
                                  const uint8_t *buf,
                                  uint32_t page,
                                  uint32_t count)
{
    struct test_ctx *ctx = ctx_from_dev(dev);
    size_t offset = (size_t)page * PAGE_BYTES;
    size_t len = (size_t)count * PAGE_BYTES;

    if (offset > FLASH_BYTES || len > FLASH_BYTES - offset) {
        return BLADERF_ERR_INVAL;
    }

    memcpy(ctx->flash + offset, buf, len);
    ++ctx->write_count;
    return 0;
}

static const struct backend_fns fake_backend = {
    FIELD_INIT(.erase_flash_blocks, fake_erase_flash_blocks),
    FIELD_INIT(.read_flash_pages, fake_read_flash_pages),
    FIELD_INIT(.write_flash_pages, fake_write_flash_pages),
};

static const struct board_fns fake_board = {
    FIELD_INIT(.get_fw_version, fake_get_fw_version),
};

/**
 * Initialize fake device state for one compression test case.
 */
static int init_test_dev(struct bladerf *dev,
                         struct bladerf_flash_arch *arch,
                         struct test_ctx *ctx,
                         uint16_t fw_patch)
{
    memset(dev, 0, sizeof(*dev));
    memset(arch, 0, sizeof(*arch));
    memset(ctx, 0, sizeof(*ctx));

    ctx->flash = malloc(FLASH_BYTES);
    if (ctx->flash == NULL) {
        return BLADERF_ERR_MEM;
    }

    memset(ctx->flash, 0xff, FLASH_BYTES);
    ctx->fw_version.major = 2;
    ctx->fw_version.minor = 6;
    ctx->fw_version.patch = fw_patch;

    arch->status = STATUS_SUCCESS;
    arch->tsize_bytes = FLASH_BYTES;
    arch->psize_bytes = PAGE_BYTES;
    arch->ebsize_bytes = EB_BYTES;
    arch->num_pages = FLASH_BYTES / PAGE_BYTES;
    arch->num_ebs = FLASH_BYTES / EB_BYTES;

    dev->backend = &fake_backend;
    dev->backend_data = ctx;
    dev->board = &fake_board;
    dev->flash_arch = arch;
    return 0;
}

/**
 * Release fake device state after one compression test case.
 */
static void deinit_test_dev(struct test_ctx *ctx)
{
    free(ctx->flash);
    ctx->flash = NULL;
}

/**
 * Decode an unsigned integer from the FPGA metadata page.
 */
static int metadata_uint(struct test_ctx *ctx, char *field, size_t *value)
{
    char text[16];
    int status = binkv_decode_field((char *)ctx->flash + BLADERF_FLASH_ADDR_FPGA,
                                    PAGE_BYTES, field, text, sizeof(text) - 1);

    if (status != 0) {
        return status;
    }

    *value = strtoul(text, NULL, 10);
    return 0;
}

/**
 * Read a little-endian 32-bit value.
 */
static uint32_t load_le32(const uint8_t *in)
{
    return (uint32_t)in[0] |
           ((uint32_t)in[1] << 8) |
           ((uint32_t)in[2] << 16) |
           ((uint32_t)in[3] << 24);
}

/**
 * Allocate memory for the LZMA SDK.
 */
static void *lzma_alloc(ISzAllocPtr p, size_t size)
{
    (void)p;
    return malloc(size);
}

/**
 * Free memory allocated by the LZMA SDK.
 */
static void lzma_free(ISzAllocPtr p, void *addr)
{
    (void)p;
    free(addr);
}

static const ISzAlloc lzma_allocator = { lzma_alloc, lzma_free };

/**
 * Decode LZMA block stream data into a caller-provided output buffer.
 */
static int decode_lzma_blocks(uint8_t *out,
                              size_t out_len,
                              const uint8_t *in,
                              size_t in_len)
{
    size_t src = 0;
    size_t dst = 0;

    for (src = 0; src < in_len && dst < out_len;) {
        uint32_t clen;
        size_t block_len = out_len - dst > FPGA_LZMA_BLOCK_BYTES ?
                           FPGA_LZMA_BLOCK_BYTES : out_len - dst;
        ELzmaStatus status;
        SizeT lzma_src_len;
        SizeT lzma_dst_len;
        SRes lzma_status;

        if (in_len - src < 4u) {
            return BLADERF_ERR_INVAL;
        }

        clen = load_le32(in + src);
        src += 4u;
        if (clen <= LZMA_PROPS_SIZE || clen > in_len - src) {
            return BLADERF_ERR_INVAL;
        }

        lzma_src_len = (SizeT)(clen - LZMA_PROPS_SIZE);
        lzma_dst_len = (SizeT)block_len;
        lzma_status = LzmaDecode(out + dst, &lzma_dst_len,
                                 in + src + LZMA_PROPS_SIZE, &lzma_src_len,
                                 in + src, LZMA_PROPS_SIZE, LZMA_FINISH_END,
                                 &status, &lzma_allocator);
        if (lzma_status != SZ_OK ||
            lzma_src_len != (SizeT)(clen - LZMA_PROPS_SIZE) ||
            lzma_dst_len != (SizeT)block_len) {
            return BLADERF_ERR_INVAL;
        }

        src += clen;
        dst += block_len;
    }

    return src == in_len && dst == out_len ? 0 : BLADERF_ERR_UNEXPECTED;
}

/**
 * Allocate and fill a deterministic FPGA image buffer.
 */
static uint8_t *make_image(size_t len, bool compressible)
{
    uint8_t *image = malloc(len);

    if (image == NULL) {
        return NULL;
    }

    if (compressible) {
        memset(image, 0xa5, len);
    } else {
        uint32_t state = 0x12345678;

        for (size_t i = 0; i < len; ++i) {
            state = state * 1103515245u + 12345u;
            image[i] = (uint8_t)(state >> 24);
        }
    }

    return image;
}

/**
 * Allocate and fill a deterministic image that needs LZMA compression.
 */
static uint8_t *make_lzma_image(size_t len)
{
    uint8_t *image = malloc(len);
    uint32_t state = 0x12345678;

    if (image == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < len; ++i) {
        state = state * 1103515245u + 12345u;
        image[i] = (uint8_t)(state >> 25);
    }

    return image;
}

/**
 * Verify integer equality and return one failure on mismatch.
 */
static int expect_int(const char *name, int actual, int expected)
{
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr, "%s: got %d, expected %d\n", name, actual, expected);
    return 1;
}

/**
 * Verify size equality and return one failure on mismatch.
 */
static int expect_size(const char *name, size_t actual, size_t expected)
{
    if (actual == expected) {
        return 0;
    }

    fprintf(stderr, "%s: got %zu, expected %zu\n", name, actual, expected);
    return 1;
}

/**
 * Verify a condition and return one failure on false.
 */
static int expect_true(const char *name, bool condition)
{
    if (condition) {
        return 0;
    }

    fprintf(stderr, "%s: condition was false\n", name);
    return 1;
}

/**
 * Verify legacy FX3 firmware still uses the raw LEN metadata path.
 */
static int test_raw_fit_with_old_firmware(void)
{
    struct bladerf dev;
    struct bladerf_flash_arch arch;
    struct test_ctx ctx;
    uint8_t *image = make_image(SMALL_LEN, false);
    size_t len = 0;
    char fmt[5];
    int failures = 0;
    int status;

    if (image == NULL || init_test_dev(&dev, &arch, &ctx, 0) != 0) {
        free(image);
        return 1;
    }

    status = spi_flash_write_fpga_bitstream(&dev, image, SMALL_LEN);
    failures += expect_int("raw status", status, 0);
    failures += expect_int("raw LEN field",
                           metadata_uint(&ctx, "LEN", &len), 0);
    failures += expect_size("raw LEN value", len, SMALL_LEN);
    failures += expect_int("raw FMT absent",
                           binkv_decode_field((char *)ctx.flash +
                                              BLADERF_FLASH_ADDR_FPGA,
                                              PAGE_BYTES, "FMT", fmt, 4),
                           BLADERF_ERR_INVAL);
    failures += expect_true("raw writes flash",
                            ctx.erase_count == 1 &&
                            ctx.write_count == 2 &&
                            ctx.read_count == 2);

    deinit_test_dev(&ctx);
    free(image);
    return failures;
}

/**
 * Verify legacy FX3 firmware can still write the largest raw image that fits.
 */
static int test_raw_edge_fit_with_old_firmware(void)
{
    struct bladerf dev;
    struct bladerf_flash_arch arch;
    struct test_ctx ctx;
    uint8_t *image = make_image(RAW_EDGE_LEN, false);
    size_t len = 0;
    char fmt[5];
    int failures = 0;
    int status;

    if (image == NULL || init_test_dev(&dev, &arch, &ctx, 0) != 0) {
        free(image);
        return 1;
    }

    status = spi_flash_write_fpga_bitstream(&dev, image, RAW_EDGE_LEN);
    failures += expect_int("raw edge status", status, 0);
    failures += expect_int("raw edge LEN field",
                           metadata_uint(&ctx, "LEN", &len), 0);
    failures += expect_size("raw edge LEN value", len, RAW_EDGE_LEN);
    failures += expect_int("raw edge FMT absent",
                           binkv_decode_field((char *)ctx.flash +
                                              BLADERF_FLASH_ADDR_FPGA,
                                              PAGE_BYTES, "FMT", fmt, 4),
                           BLADERF_ERR_INVAL);
    failures += expect_int("raw edge round trip",
                           memcmp(ctx.flash + BLADERF_FLASH_ADDR_FPGA +
                                  PAGE_BYTES, image, RAW_EDGE_LEN),
                           0);

    deinit_test_dev(&ctx);
    free(image);
    return failures;
}

/**
 * Verify oversized compressible FPGA images write LZMA metadata and data.
 */
static int test_compressed_write_round_trip(void)
{
    struct bladerf dev;
    struct bladerf_flash_arch arch;
    struct test_ctx ctx;
    uint8_t *image = make_image(OVERSIZED_LEN, true);
    uint8_t *decoded = malloc(OVERSIZED_LEN);
    size_t ulen = 0;
    size_t clen = 0;
    char fmt[5];
    int failures = 0;
    int status;

    if (image == NULL || decoded == NULL ||
        init_test_dev(&dev, &arch, &ctx, 1) != 0) {
        free(decoded);
        free(image);
        return 1;
    }

    status = spi_flash_write_fpga_bitstream(&dev, image, OVERSIZED_LEN);
    failures += expect_int("compressed status", status, 0);
    failures += expect_int("compressed FMT field",
                           binkv_decode_field((char *)ctx.flash +
                                              BLADERF_FLASH_ADDR_FPGA,
                                              PAGE_BYTES, "FMT", fmt, 4),
                           0);
    failures += expect_int("compressed FMT value", strcmp(fmt, "LZMA"), 0);
    failures += expect_int("compressed ULEN field",
                           metadata_uint(&ctx, "ULEN", &ulen), 0);
    failures += expect_int("compressed CLEN field",
                           metadata_uint(&ctx, "CLEN", &clen), 0);
    failures += expect_size("compressed ULEN value", ulen, OVERSIZED_LEN);
    failures += expect_true("compressed data is smaller", clen < OVERSIZED_LEN);
    failures += expect_int("compressed decode",
                           decode_lzma_blocks(decoded, OVERSIZED_LEN,
                                              ctx.flash + BLADERF_FLASH_ADDR_FPGA +
                                              PAGE_BYTES, clen),
                           0);
    failures += expect_int("compressed round trip",
                           memcmp(decoded, image, OVERSIZED_LEN), 0);

    deinit_test_dev(&ctx);
    free(decoded);
    free(image);
    return failures;
}

/**
 * Verify new FX3 firmware writes LZMA metadata even when raw fits.
 */
static int test_default_compressed_write_round_trip(void)
{
    struct bladerf dev;
    struct bladerf_flash_arch arch;
    struct test_ctx ctx;
    uint8_t *image = make_image(SMALL_LEN, true);
    uint8_t *decoded = malloc(SMALL_LEN);
    size_t ulen = 0;
    size_t clen = 0;
    char fmt[5];
    int failures = 0;
    int status;

    if (image == NULL || decoded == NULL ||
        init_test_dev(&dev, &arch, &ctx, 1) != 0) {
        free(decoded);
        free(image);
        return 1;
    }

    status = spi_flash_write_fpga_bitstream(&dev, image, SMALL_LEN);
    failures += expect_int("default compressed status", status, 0);
    failures += expect_int("default compressed FMT field",
                           binkv_decode_field((char *)ctx.flash +
                                              BLADERF_FLASH_ADDR_FPGA,
                                              PAGE_BYTES, "FMT", fmt, 4),
                           0);
    failures += expect_int("default compressed FMT value", strcmp(fmt, "LZMA"), 0);
    failures += expect_int("default compressed ULEN field",
                           metadata_uint(&ctx, "ULEN", &ulen), 0);
    failures += expect_int("default compressed CLEN field",
                           metadata_uint(&ctx, "CLEN", &clen), 0);
    failures += expect_size("default compressed ULEN value", ulen, SMALL_LEN);
    failures += expect_int("default compressed decode",
                           decode_lzma_blocks(decoded, SMALL_LEN,
                                              ctx.flash + BLADERF_FLASH_ADDR_FPGA +
                                              PAGE_BYTES, clen),
                           0);
    failures += expect_int("default compressed round trip",
                           memcmp(decoded, image, SMALL_LEN), 0);

    deinit_test_dev(&ctx);
    free(decoded);
    free(image);
    return failures;
}

/**
 * Verify new FX3 firmware rejects writes when LZMA does not fit.
 */
static int test_new_firmware_rejects_lzma_expansion(void)
{
    struct bladerf dev;
    struct bladerf_flash_arch arch;
    struct test_ctx ctx;
    uint8_t *image = make_image(RAW_EDGE_LEN, false);
    int failures = 0;
    int status;

    if (image == NULL || init_test_dev(&dev, &arch, &ctx, 1) != 0) {
        free(image);
        return 1;
    }

    status = spi_flash_write_fpga_bitstream(&dev, image, RAW_EDGE_LEN);
    failures += expect_int("new firmware expanded LZMA status",
                           status, BLADERF_ERR_INVAL);
    failures += expect_true("new firmware expanded LZMA does not touch flash",
                            ctx.erase_count == 0 &&
                            ctx.write_count == 0 &&
                            ctx.read_count == 0);

    deinit_test_dev(&ctx);
    free(image);
    return failures;
}

/**
 * Verify LZMA handles FPGA images that raw storage cannot fit.
 */
static int test_lzma_write_round_trip(void)
{
    struct bladerf dev;
    struct bladerf_flash_arch arch;
    struct test_ctx ctx;
    uint8_t *image = make_lzma_image(OVERSIZED_LEN);
    uint8_t *decoded = malloc(OVERSIZED_LEN);
    size_t ulen = 0;
    size_t clen = 0;
    char fmt[5];
    int failures = 0;
    int status;

    if (image == NULL || decoded == NULL ||
        init_test_dev(&dev, &arch, &ctx, 1) != 0) {
        free(decoded);
        free(image);
        return 1;
    }

    status = spi_flash_write_fpga_bitstream(&dev, image, OVERSIZED_LEN);
    failures += expect_int("lzma status", status, 0);
    failures += expect_int("lzma FMT field",
                           binkv_decode_field((char *)ctx.flash +
                                              BLADERF_FLASH_ADDR_FPGA,
                                              PAGE_BYTES, "FMT", fmt, 4),
                           0);
    failures += expect_int("lzma FMT value", strcmp(fmt, "LZMA"), 0);
    failures += expect_int("lzma ULEN field",
                           metadata_uint(&ctx, "ULEN", &ulen), 0);
    failures += expect_int("lzma CLEN field",
                           metadata_uint(&ctx, "CLEN", &clen), 0);
    failures += expect_size("lzma ULEN value", ulen, OVERSIZED_LEN);
    failures += expect_true("lzma data fits",
                            clen <= RAW_EDGE_LEN);
    failures += expect_int("lzma decode",
                           decode_lzma_blocks(decoded, OVERSIZED_LEN,
                                              ctx.flash + BLADERF_FLASH_ADDR_FPGA +
                                              PAGE_BYTES, clen),
                           0);
    failures += expect_int("lzma round trip",
                           memcmp(decoded, image, OVERSIZED_LEN), 0);

    deinit_test_dev(&ctx);
    free(decoded);
    free(image);
    return failures;
}

/**
 * Verify LZMA writes are gated on the newer FX3 decoder version.
 */
static int test_lzma_requires_newer_firmware(void)
{
    struct bladerf dev;
    struct bladerf_flash_arch arch;
    struct test_ctx ctx;
    uint8_t *image = make_lzma_image(OVERSIZED_LEN);
    int failures = 0;
    int status;

    if (image == NULL || init_test_dev(&dev, &arch, &ctx, 0) != 0) {
        free(image);
        return 1;
    }

    status = spi_flash_write_fpga_bitstream(&dev, image, OVERSIZED_LEN);
    failures += expect_int("lzma old firmware status",
                           status, BLADERF_ERR_UPDATE_FW);
    failures += expect_true("lzma old firmware does not touch flash",
                            ctx.erase_count == 0 &&
                            ctx.write_count == 0 &&
                            ctx.read_count == 0);

    deinit_test_dev(&ctx);
    free(image);
    return failures;
}

/**
 * Verify a wrong or too-large RBF is rejected when LZMA still cannot fit it.
 */
static int test_wrong_rbf_still_too_large(void)
{
    struct bladerf dev;
    struct bladerf_flash_arch arch;
    struct test_ctx ctx;
    uint8_t *image = make_image(OVERSIZED_LEN, false);
    int failures = 0;
    int status;

    if (image == NULL || init_test_dev(&dev, &arch, &ctx, 1) != 0) {
        free(image);
        return 1;
    }

    status = spi_flash_write_fpga_bitstream(&dev, image, OVERSIZED_LEN);
    failures += expect_int("wrong RBF status", status, BLADERF_ERR_INVAL);
    failures += expect_true("wrong RBF does not touch flash",
                            ctx.erase_count == 0 &&
                            ctx.write_count == 0 &&
                            ctx.read_count == 0);

    deinit_test_dev(&ctx);
    free(image);
    return failures;
}

/**
 * Run the FPGA flash compression tests.
 */
int main(void)
{
    int failures = 0;

    failures += test_raw_fit_with_old_firmware();
    failures += test_raw_edge_fit_with_old_firmware();
    failures += test_compressed_write_round_trip();
    failures += test_default_compressed_write_round_trip();
    failures += test_new_firmware_rejects_lzma_expansion();
    failures += test_lzma_write_round_trip();
    failures += test_lzma_requires_newer_firmware();
    failures += test_wrong_rbf_still_too_large();

    if (failures != 0) {
        fprintf(stderr, "%d FPGA flash compression checks failed\n", failures);
        return EXIT_FAILURE;
    }

    printf("FPGA flash compression checks passed\n");
    return EXIT_SUCCESS;
}
