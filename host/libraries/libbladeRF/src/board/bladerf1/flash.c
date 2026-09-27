#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "minmax.h"
#include "misc.h"
#include "conversions.h"
#include "helpers/version.h"

#include "bladeRF.h"
#include "board/board.h"

#include "driver/spi_flash.h"

#include "flash.h"
#include "LzmaEnc.h"

#define OTP_BUFFER_SIZE 256
#define FPGA_LZMA_BLOCK_BYTES (60u * 1024u)
#define FPGA_LZMA_MAX_BLOCK_BYTES 65504u

#define FPGA_LZMA_FW_MAJOR 2
#define FPGA_LZMA_FW_MINOR 6
#define FPGA_LZMA_FW_PATCH 1

int spi_flash_write_fx3_fw(struct bladerf *dev, const uint8_t *image, size_t len)
{
    int status;
    uint8_t *readback_buf;
    uint8_t *padded_image;
    uint32_t padded_image_len;

    /* Pad firwmare data out to a page size */
    const uint32_t page_size = dev->flash_arch->psize_bytes;
    const uint32_t padding_len =
        (len % page_size == 0) ? 0 : page_size - (len % page_size);

    /* Flash page where FX3 firmware starts */
    const uint32_t flash_page_fw = BLADERF_FLASH_ADDR_FIRMWARE /
        dev->flash_arch->psize_bytes;

    /* Flash erase block where FX3 firmware starts */
    const uint32_t flash_eb_fw = BLADERF_FLASH_ADDR_FIRMWARE /
        dev->flash_arch->ebsize_bytes;

    /** Length of firmware region of flash, in erase blocks */
    const uint32_t flash_eb_len_fw = BLADERF_FLASH_BYTE_LEN_FIRMWARE /
        dev->flash_arch->ebsize_bytes;

    if (len >= (UINT32_MAX - padding_len)) {
        return BLADERF_ERR_INVAL;
    }

    padded_image_len = (uint32_t) len + padding_len;

    readback_buf = malloc(padded_image_len);
    if (readback_buf == NULL) {
        return BLADERF_ERR_MEM;
    }

    padded_image = malloc(padded_image_len);
    if (padded_image == NULL) {
        free(readback_buf);
        return BLADERF_ERR_MEM;
    }

    /* Copy image */
    memcpy(padded_image, image, len);

    /* Clear the padded region */
    memset(padded_image + len, 0xFF, padded_image_len - len);

    /* Erase the entire firmware region */
    status = spi_flash_erase(dev, flash_eb_fw, flash_eb_len_fw);
    if (status != 0) {
        log_debug("Failed to erase firmware region: %s\n",
                  bladerf_strerror(status));
        goto error;
    }

    /* Convert the image length to pages */
    padded_image_len /= page_size;

    /* Write the firmware image to flash */
    status = spi_flash_write(dev, padded_image,
                             flash_page_fw, padded_image_len);

    if (status < 0) {
        log_debug("Failed to write firmware: %s\n", bladerf_strerror(status));
        goto error;
    }

    /* Read back and double-check what we just wrote */
    status = spi_flash_verify(dev, readback_buf, padded_image,
                              flash_page_fw, padded_image_len);
    if (status != 0) {
        log_debug("Flash verification failed: %s\n", bladerf_strerror(status));
        goto error;
    }

error:
    free(padded_image);
    free(readback_buf);
    return status;
}

static inline void fill_fpga_metadata_page(struct bladerf *dev,
                                           uint8_t *metadata,
                                           size_t actual_bitstream_len)
{
    char len_str[12];
    int idx = 0;

    memset(len_str, 0, sizeof(len_str));
    memset(metadata, 0xff, dev->flash_arch->psize_bytes);

    snprintf(len_str, sizeof(len_str), "%u",
             (unsigned int)actual_bitstream_len);

    binkv_encode_field((char *)metadata, dev->flash_arch->psize_bytes,
                       &idx, "LEN", len_str);
}

/**
 * Fill FPGA autoload metadata for a compressed bitstream.
 */
static inline void fill_fpga_compressed_metadata_page(struct bladerf *dev,
                                                     uint8_t *metadata,
                                                     size_t uncompressed_len,
                                                     size_t compressed_len,
                                                     const char *format)
{
    char clen_str[12];
    char ulen_str[12];
    int idx = 0;

    memset(clen_str, 0, sizeof(clen_str));
    memset(ulen_str, 0, sizeof(ulen_str));
    memset(metadata, 0xff, dev->flash_arch->psize_bytes);

    snprintf(ulen_str, sizeof(ulen_str), "%u", (unsigned int)uncompressed_len);
    snprintf(clen_str, sizeof(clen_str), "%u", (unsigned int)compressed_len);

    binkv_encode_field((char *)metadata, dev->flash_arch->psize_bytes,
                       &idx, "FMT", format);
    binkv_encode_field((char *)metadata, dev->flash_arch->psize_bytes,
                       &idx, "ULEN", ulen_str);
    binkv_encode_field((char *)metadata, dev->flash_arch->psize_bytes,
                       &idx, "CLEN", clen_str);
}

/**
 * Store a 32-bit value in little-endian order.
 */
static inline void store_le32(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
    out[2] = (uint8_t)(value >> 16);
    out[3] = (uint8_t)(value >> 24);
}

/**
 * Return true when an FPGA autoload payload fits after metadata and padding.
 */
static bool fpga_autoload_fits(size_t len,
                               size_t padding_len,
                               uint32_t page_size,
                               uint32_t flash_len)
{
    return flash_len > page_size &&
           len < UINT32_MAX - padding_len &&
           len + padding_len <= (size_t)flash_len - page_size;
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
 * Compress FPGA autoload data into independent LZMA blocks.
 */
static int lzma_encode_blocks(const uint8_t *in, size_t len,
                              uint8_t **out, size_t *out_len)
{
    CLzmaEncProps props;
    size_t max_len;
    size_t i;
    size_t o = 0;
    uint8_t *buf;

    if (len > UINT32_MAX) {
        return BLADERF_ERR_INVAL;
    }

    max_len = 0;
    for (i = 0; i < len;) {
        if (max_len > SIZE_MAX - 4u - FPGA_LZMA_MAX_BLOCK_BYTES) {
            return BLADERF_ERR_INVAL;
        }

        max_len += 4u + FPGA_LZMA_MAX_BLOCK_BYTES;
        i += min_sz(len - i, FPGA_LZMA_BLOCK_BYTES);
    }

    buf = malloc(max_len);
    if (buf == NULL) {
        return BLADERF_ERR_MEM;
    }

    LzmaEncProps_Init(&props);
    props.level = 9;
    props.dictSize = FPGA_LZMA_BLOCK_BYTES;
    props.lc = 3;
    props.lp = 0;
    props.pb = 2;
    props.algo = 1;
    props.fb = 273;
    props.btMode = 1;
    props.numHashBytes = 4;
    props.numThreads = 1;
    props.writeEndMark = 0;

    for (i = 0; i < len;) {
        const size_t block_len = min_sz(len - i, FPGA_LZMA_BLOCK_BYTES);
        Byte props_encoded[LZMA_PROPS_SIZE];
        SizeT props_size = LZMA_PROPS_SIZE;
        SizeT clen = FPGA_LZMA_MAX_BLOCK_BYTES - LZMA_PROPS_SIZE;
        const SRes lzma_status =
            LzmaEncode(buf + o + 4u + LZMA_PROPS_SIZE, &clen,
                       in + i, block_len, &props, props_encoded, &props_size,
                       0, NULL, &lzma_allocator, &lzma_allocator);
        const size_t frame_len = (size_t)props_size + (size_t)clen;

        if (lzma_status != SZ_OK) {
            free(buf);
            return lzma_status == SZ_ERROR_MEM ?
                   BLADERF_ERR_MEM : BLADERF_ERR_UNEXPECTED;
        } else if (props_size != LZMA_PROPS_SIZE ||
                   frame_len > FPGA_LZMA_MAX_BLOCK_BYTES) {
            free(buf);
            return BLADERF_ERR_UNEXPECTED;
        }

        store_le32(buf + o, (uint32_t)frame_len);
        memcpy(buf + o + 4u, props_encoded, LZMA_PROPS_SIZE);
        o += 4u + frame_len;
        i += block_len;
    }

    *out = buf;
    *out_len = o;
    return 0;
}

static inline size_t get_flash_eb_len_fpga(struct bladerf *dev)
{
    int status;
    size_t fpga_bytes;
    size_t eb_count;
    size_t max_eb_count;

    status = dev->board->get_fpga_bytes(dev, &fpga_bytes);
    if (status < 0) {
        return status;
    }

    eb_count = fpga_bytes / dev->flash_arch->ebsize_bytes;

    if ((fpga_bytes % dev->flash_arch->ebsize_bytes) > 0) {
        // Round up to nearest full block
        ++eb_count;
    }

    max_eb_count = dev->flash_arch->num_ebs -
                   (BLADERF_FLASH_ADDR_FPGA / dev->flash_arch->ebsize_bytes);
    if (eb_count > max_eb_count) {
        eb_count = max_eb_count;
    }

    return eb_count;
}

#define METADATA_LEN 256

int spi_flash_write_fpga_bitstream(struct bladerf *dev,
                                   const uint8_t *bitstream,
                                   size_t len)
{
    const uint32_t page_size = dev->flash_arch->psize_bytes;

    /** Flash page where FPGA metadata and bitstream start */
    const uint32_t flash_page_fpga =
        BLADERF_FLASH_ADDR_FPGA / dev->flash_arch->psize_bytes;

    /** Flash erase block where FPGA metadata and bitstream start */
    const uint32_t flash_eb_fpga =
        BLADERF_FLASH_ADDR_FPGA / dev->flash_arch->ebsize_bytes;

    assert(METADATA_LEN <= page_size);

    int status;
    bool compressed = false;
    uint32_t flash_len;
    uint32_t flash_eb_len_fpga;
    uint8_t *readback_buf;
    uint8_t *compressed_bitstream = NULL;
    uint8_t *padded_bitstream;
    const uint8_t *stored_bitstream = bitstream;
    uint8_t metadata[METADATA_LEN];
    struct bladerf_version fw_version;
    size_t stored_len = len;
    size_t padding_len;
    size_t raw_padding_len;
    uint32_t padded_bitstream_len;
    bool raw_fits;

    if (dev->flash_arch->tsize_bytes <= BLADERF_FLASH_ADDR_FPGA) {
        return BLADERF_ERR_INVAL;
    }
    flash_len = dev->flash_arch->tsize_bytes - BLADERF_FLASH_ADDR_FPGA;

    raw_padding_len = (len % page_size == 0) ? 0 : page_size - (len % page_size);
    padding_len = raw_padding_len;
    raw_fits = fpga_autoload_fits(len, raw_padding_len, page_size, flash_len);
    status = dev->board->get_fw_version(dev, &fw_version);
    if (status != 0) {
        return status;
    }

    if (version_fields_less_than(&fw_version,
                                 FPGA_LZMA_FW_MAJOR,
                                 FPGA_LZMA_FW_MINOR,
                                 FPGA_LZMA_FW_PATCH)) {
        if (!raw_fits) {
            log_error("LZMA FPGA autoload requires FX3 firmware "
                      "v%u.%u.%u or later; device has v%u.%u.%u\n",
                      FPGA_LZMA_FW_MAJOR, FPGA_LZMA_FW_MINOR,
                      FPGA_LZMA_FW_PATCH,
                      fw_version.major, fw_version.minor, fw_version.patch);
            return BLADERF_ERR_UPDATE_FW;
        }
    } else {
        status = lzma_encode_blocks(bitstream, len, &compressed_bitstream,
                                    &stored_len);
        if (status != 0) {
            return status;
        }

        stored_bitstream = compressed_bitstream;
        compressed = true;
        padding_len = (stored_len % page_size == 0) ?
                      0 : page_size - (stored_len % page_size);
    }

    if (!fpga_autoload_fits(stored_len, padding_len, page_size, flash_len)) {
        free(compressed_bitstream);
        return BLADERF_ERR_INVAL;
    }

    padded_bitstream_len = (uint32_t)(stored_len + padding_len);
    if (compressed) {
        log_info("FPGA bitstream compression (LZMA): %zu bytes -> %zu bytes; "
                 "%zu bytes left in flash after padding\n",
                 len, stored_len,
                 (size_t)flash_len - page_size - padded_bitstream_len);
    }

    flash_eb_len_fpga =
        (uint32_t)((padded_bitstream_len + page_size +
                    dev->flash_arch->ebsize_bytes - 1) /
                   dev->flash_arch->ebsize_bytes);

    /* Fill in metadata with the *actual* FPGA bitstream length */
    if (compressed) {
        fill_fpga_compressed_metadata_page(dev, metadata, len, stored_len,
                                           "LZMA");
    } else {
        fill_fpga_metadata_page(dev, metadata, len);
    }

    readback_buf = malloc(padded_bitstream_len);
    if (readback_buf == NULL) {
        free(compressed_bitstream);
        return BLADERF_ERR_MEM;
    }

    padded_bitstream = malloc(padded_bitstream_len);
    if (padded_bitstream == NULL) {
        free(readback_buf);
        free(compressed_bitstream);
        return BLADERF_ERR_MEM;
    }

    /* Copy bitstream */
    memcpy(padded_bitstream, stored_bitstream, stored_len);

    /* Clear the padded region */
    memset(padded_bitstream + stored_len, 0xFF,
           padded_bitstream_len - stored_len);

    /* Erase FPGA metadata and bitstream region */
    status = spi_flash_erase(dev, flash_eb_fpga, flash_eb_len_fpga);
    if (status != 0) {
        log_debug("Failed to erase FPGA meta & bitstream regions: %s\n",
                  bladerf_strerror(status));
        goto error;
    }

    /* Write the metadata page */
    status = spi_flash_write(dev, metadata, flash_page_fpga, 1);
    if (status != 0) {
        log_debug("Failed to write FPGA metadata page: %s\n",
                  bladerf_strerror(status));
        goto error;
    }

    /* Convert the padded bitstream length to pages */
    padded_bitstream_len /= page_size;

    /* Write the padded bitstream */
    status = spi_flash_write(dev, padded_bitstream, flash_page_fpga + 1,
                             padded_bitstream_len);
    if (status != 0) {
        log_debug("Failed to write bitstream: %s\n", bladerf_strerror(status));
        goto error;
    }

    /* Read back and verify metadata */
    status = spi_flash_verify(dev, readback_buf, metadata, flash_page_fpga, 1);
    if (status != 0) {
        log_debug("Failed to verify metadata: %s\n", bladerf_strerror(status));
        goto error;
    }

    /* Read back and verify the bitstream data */
    status = spi_flash_verify(dev, readback_buf, padded_bitstream,
                              flash_page_fpga + 1, padded_bitstream_len);
    if (status != 0) {
        log_debug("Failed to verify bitstream data: %s\n",
                  bladerf_strerror(status));
        goto error;
    }

error:
    free(padded_bitstream);
    free(compressed_bitstream);
    free(readback_buf);
    return status;
}

int spi_flash_erase_fpga(struct bladerf *dev)
{
    int status;
    size_t fpga_bytes;

    status = dev->board->get_fpga_bytes(dev, &fpga_bytes);
    if (status < 0) {
        return status;
    }

    /** Flash erase block where FPGA metadata and bitstream start */
    const uint32_t flash_eb_fpga =
        BLADERF_FLASH_ADDR_FPGA / dev->flash_arch->ebsize_bytes;

    /** Length of entire FPGA region, in units of erase blocks */
    const uint32_t flash_eb_len_fpga = (uint32_t)get_flash_eb_len_fpga(dev);

    /* Erase the entire FPGA region, including both autoload metadata and the
     * actual bitstream data */
    return spi_flash_erase(dev, flash_eb_fpga, flash_eb_len_fpga);
}

int spi_flash_read_otp(struct bladerf *dev, char *field,
                       char *data, size_t data_size)
{
    int status;
    char otp[OTP_BUFFER_SIZE];

    memset(otp, 0xff, OTP_BUFFER_SIZE);

    status = dev->backend->get_otp(dev, otp);
    if (status < 0)
        return status;
    else
        return binkv_decode_field(otp, OTP_BUFFER_SIZE, field, data, data_size);
}

int spi_flash_read_cal(struct bladerf *dev, char *field,
                       char *data, size_t data_size)
{
    int status;
    char cal[CAL_BUFFER_SIZE];

    status = dev->backend->get_cal(dev, cal);
    if (status < 0)
        return status;
    else
        return binkv_decode_field(cal, CAL_BUFFER_SIZE, field, data, data_size);
}

int spi_flash_read_serial(struct bladerf *dev, char *serial_buf)
{
    int status;

    status = spi_flash_read_otp(dev, "S", serial_buf, BLADERF_SERIAL_LENGTH - 1);

    if (status < 0) {
        log_info("Unable to fetch serial number. Defaulting to 0's.\n");
        memset(dev->ident.serial, '0', BLADERF_SERIAL_LENGTH - 1);

        /* Treat this as non-fatal */
        status = 0;
    }

    serial_buf[BLADERF_SERIAL_LENGTH - 1] = '\0';

    return status;
}

int spi_flash_read_vctcxo_trim(struct bladerf *dev, uint16_t *dac_trim)
{
    int status;
    bool ok;
    int16_t trim;
    char tmp[7] = { 0 };

    status = spi_flash_read_cal(dev, "DAC", tmp, sizeof(tmp) - 1);
    if (status < 0) {
        return status;
    }

    trim = str2uint(tmp, 0, 0xffff, &ok);
    if (ok == false) {
        return BLADERF_ERR_INVAL;
    }

    *dac_trim = trim;

    return 0;
}

int spi_flash_read_fpga_size(struct bladerf *dev, bladerf_fpga_size *fpga_size)
{
    int status;
    char tmp[7] = { 0 };

    status = spi_flash_read_cal(dev, "B", tmp, sizeof(tmp) - 1);
    if (status < 0) {
        return status;
    }

    if (!strcmp("40", tmp)) {
        *fpga_size = BLADERF_FPGA_40KLE;
    } else if(!strcmp("115", tmp)) {
        *fpga_size = BLADERF_FPGA_115KLE;
    } else if(!strcmp("A4", tmp)) {
        *fpga_size = BLADERF_FPGA_A4;
    } else if(!strcmp("A5", tmp)) {
        *fpga_size = BLADERF_FPGA_A5;
    } else if(!strcmp("A9", tmp)) {
        *fpga_size = BLADERF_FPGA_A9;
    } else {
        *fpga_size = BLADERF_FPGA_UNKNOWN;
    }

    return status;
}

int spi_flash_read_flash_id(struct bladerf *dev, uint8_t *mid, uint8_t *did)
{
    int status;

    status = dev->backend->get_flash_id(dev, mid, did);

    return status;
}

int spi_flash_decode_flash_architecture(struct bladerf *dev,
                                        bladerf_fpga_size  *fpga_size)
{
    int status;
    struct bladerf_flash_arch *flash_arch;

    status     = 0;
    flash_arch = dev->flash_arch;

    /* Fill in defaults */
    flash_arch->tsize_bytes  = 32 << 17; /* 32 Mbit */
    flash_arch->psize_bytes  = 256;
    flash_arch->ebsize_bytes = 64 << 10; /* 64 Kbyte */
    flash_arch->status       = STATUS_ASSUMED;

    /* First try to decode the MID/DID of the flash chip */
    switch( flash_arch->manufacturer_id ) {
        case 0xC2: /* MACRONIX */
            log_verbose( "Found SPI flash manufacturer: MACRONIX.\n" );
            switch( flash_arch->device_id ) {
                case 0x36:
                    log_verbose( "Found SPI flash device: MX25U3235E (32 Mbit).\n" );
                    flash_arch->tsize_bytes = 32 << 17;
                    flash_arch->status      = STATUS_SUCCESS;
                    break;
                default:
                    log_debug( "Unknown Macronix flash device ID.\n" );
                    status = BLADERF_ERR_UNEXPECTED;
            }
            break;

        case 0xEF: /* WINBOND */
            log_verbose( "Found SPI flash manufacturer: WINBOND.\n" );
            switch( flash_arch->device_id ) {
                case 0x15:
                    log_verbose( "Found SPI flash device: W25Q32JV (32 Mbit).\n" );
                    flash_arch->tsize_bytes = 32 << 17;
                    flash_arch->status      = STATUS_SUCCESS;
                    break;
                case 0x16:
                    log_verbose( "Found SPI flash device: W25Q64JV (64 Mbit).\n" );
                    flash_arch->tsize_bytes = 64 << 17;
                    flash_arch->status      = STATUS_SUCCESS;
                    break;
                case 0x17:
                    log_verbose( "Found SPI flash device: W25Q128JV (128 Mbit).\n" );
                    flash_arch->tsize_bytes = 128 << 17;
                    flash_arch->status      = STATUS_SUCCESS;
                    break;
                default:
                    log_debug( "Unknown Winbond flash device ID [0x%02X].\n" , flash_arch->device_id );
                    status = BLADERF_ERR_UNEXPECTED;
            }
            break;

        case 0x1F: /* RENESAS */
            log_verbose( "Found SPI flash manufacturer: RENESAS.\n" );
            switch( flash_arch->device_id ) {
                case 0x47:
                    log_verbose( "Found SPI flash device: AT25FF321A"
                                 " (32 Mbit).\n" );
                    flash_arch->tsize_bytes = 32 << 17;
                    flash_arch->status      = STATUS_SUCCESS;
                    break;
                default:
                    log_debug( "Unknown Renesas flash device ID"
                               " [0x%02X].\n",
                               flash_arch->device_id );
                    status = BLADERF_ERR_UNEXPECTED;
            }
            break;

        default:
            log_debug( "Unknown flash manufacturer ID.\n" );
            status = BLADERF_ERR_UNEXPECTED;
    }

    /* Could not decode flash MID/DID, so assume based on FPGA size */
    if( status < 0 || flash_arch->status != STATUS_SUCCESS ) {
        if( (fpga_size == NULL) || (*fpga_size == BLADERF_FPGA_UNKNOWN) ) {
            log_debug( "Could not decode flash manufacturer/device ID and have "
                       "an unknown FPGA size. Assume default flash "
                       "architecture.\n" );
        } else {
            switch( *fpga_size ) {
                case BLADERF_FPGA_A9:
                    flash_arch->tsize_bytes = 128 << 17;
                    break;
                default:
                    flash_arch->tsize_bytes = 32 << 17;
            }
            log_debug( "Could not decode flash manufacturer/device ID, but "
                       "found a %u kLE FPGA. Setting the most probable "
                       "flash architecture.\n", *fpga_size );
        }
    }

    flash_arch->num_pages = flash_arch->tsize_bytes / flash_arch->psize_bytes;
    flash_arch->num_ebs   = flash_arch->tsize_bytes / flash_arch->ebsize_bytes;

    log_verbose("SPI flash total size = %u Mbit\n", (flash_arch->tsize_bytes >> 17));
    log_verbose("SPI flash page size = %u bytes\n", flash_arch->psize_bytes);
    log_verbose("SPI flash erase block size = %u bytes\n", flash_arch->ebsize_bytes);
    log_verbose("SPI flash number of pages = %u\n", flash_arch->num_pages);
    log_verbose("SPI flash number of erase blocks = %u pages\n", flash_arch->num_ebs);

    return status;
}


int binkv_decode_field(char *ptr, int len, char *field,
                       char *val, size_t  maxlen)
{
    int c;
    unsigned char *ub, *end;
    unsigned short a1, a2;
    size_t flen, wlen;

    flen = strlen(field);

    ub = (unsigned char *)ptr;
    end = ub + len;
    while (ub < end) {
        c = *ub;

        if (c == 0xff) // flash and OTP are 0xff if they've never been written to
            break;

        a1 = LE16_TO_HOST(*(unsigned short *)(&ub[c+1]));  // read checksum
        a2 = zcrc(ub, c+1);  // calculate checksum

        if (a1 == a2) {
            if (!strncmp((char *)ub + 1, field, flen)) {
                wlen = min_sz(c - flen, maxlen);
                strncpy(val, (char *)ub + 1 + flen, wlen);
                val[wlen] = 0;
                return 0;
            }
        } else {
            log_debug( "%s: Field checksum mismatch\n", __FUNCTION__);
            return BLADERF_ERR_INVAL;
        }
        ub += c + 3; //skip past `c' bytes, 2 byte CRC field, and 1 byte len field
    }
    return BLADERF_ERR_INVAL;
}

int binkv_encode_field(char *ptr, int len, int *idx,
                       const char *field, const char *val)
{
    int vlen, flen, tlen;
    flen = (int)strlen(field);
    vlen = (int)strlen(val);
    tlen = flen + vlen + 1;

    if (tlen >= 256 || *idx + tlen >= len)
        return BLADERF_ERR_MEM;

    ptr[*idx] = flen + vlen;
    strcpy(&ptr[*idx + 1], field);
    strcpy(&ptr[*idx + 1 + flen], val);
    *(unsigned short *)(&ptr[*idx + tlen ]) = HOST_TO_LE16(zcrc((uint8_t *)&ptr[*idx ], tlen));
    *idx += tlen + 2;
    return 0;
}

int binkv_add_field(char *buf, int buf_len, const char *field_name, const char *val)
{
    int dummy_idx = 0;
    int i = 0;
    int rv;

    /* skip to the end, ignoring crc (don't want to further corrupt partially
     * corrupt data) */
    while(i < buf_len) {
        uint8_t field_len = buf[i];

        if(field_len == 0xff)
            break;

        /* skip past `field_len' bytes, 2 byte CRC field, and 1 byte len
         * field */
        i += field_len + 3;
    }

    rv = binkv_encode_field(buf + i, buf_len - i, &dummy_idx, field_name, val);
    if(rv < 0)
        return rv;

    return 0;
}
