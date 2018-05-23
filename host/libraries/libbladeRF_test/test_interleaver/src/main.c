#include <libbladeRF.h>
#include <stdio.h>

#include "helpers/interleave.h"

#ifndef min
#define min(x, y) x < y ? x : y
#endif  // !min

#ifndef max
#define max(x, y) x > y ? x : y
#endif  // !max

#define PRINT_VERBOSE(...)                 \
    {                                      \
        verbose ? printf(__VA_ARGS__) : 0; \
    }
#define PRINT_INFO(...)                  \
    {                                    \
        quiet ? 0 : printf(__VA_ARGS__); \
    }
#define PRINT_ERROR(...) printf(__VA_ARGS__)

bool verbose = false;
bool quiet   = false;

size_t const CELL_WIDTH  = 4;
size_t const NUM_COLUMNS = 8;

/* Creates a buffer of buflen bytes, containing a counting pattern */
void *create_buf(size_t buflen)
{
    void *retbuf  = NULL;
    uint16_t *ptr = NULL;
    size_t i;

    retbuf = calloc(buflen, sizeof(uint8_t));
    if (NULL == retbuf) {
        PRINT_ERROR("%s: calloc failed\n", __FUNCTION__);
        return retbuf;
    }

    for (i = 0; i < buflen / 2; ++i) {
        ptr  = (uint16_t *)retbuf + i;
        *ptr = (i % 65536);
    }

    return retbuf;
}

/* Checks the contents of a buffer, buf, of buflen bytes for a proper counting
 * pattern (starting at count), checking every stride'th sample */
bool check_buf(void const *buf,
               size_t buflen,
               size_t samplesize,
               size_t stride,
               uint16_t count)
{
    bool retval   = true;
    uint32_t *ptr = NULL;
    uint32_t expect;
    size_t i;

    if (NULL == buf) {
        PRINT_ERROR("%s: buf is null, unable to check\n", __FUNCTION__);
        return false;
    }

    for (i = 0; i < buflen / samplesize; i += stride) {
        ptr = (uint32_t *)buf + i;

        count %= 65536;
        expect = count++;
        count %= 65536;
        expect += (count++ << 16);

        if (expect != *ptr) {
            PRINT_ERROR("%p = %08x instead of %08x\n", ptr, *ptr, expect);
            retval = false;
        } else {
            PRINT_VERBOSE("%p = %08x ok\n", ptr, *ptr);
        }
    }

    return retval;
}

/* Outputs the contents of a buffer, buf, of buflen bytes, with num_columns
 * columns with CELL_WIDTH bytes in each column */
void print_buf(void const *buf, size_t buflen, size_t num_columns)
{
    size_t const columns = min(num_columns, buflen / CELL_WIDTH);
    size_t const rowsize = sizeof(char) * (CELL_WIDTH * 2) * (columns + 1) + 1;
    size_t const rows    = max(buflen / columns / CELL_WIDTH, 1);
    uint8_t const *u8buf = (uint8_t *)buf;

    size_t row, column, byte;
    char *rowstr;

    // short circuit if we're going to print nothing
    if (!verbose) {
        return;
    }


    rowstr = malloc(rowsize);
    if (NULL == rowstr) {
        PRINT_ERROR("%s: couldn't malloc rowstr\n", __FUNCTION__);
        return;
    }

    for (row = 0; row < rows; ++row) {
        uint8_t const *rowptr = u8buf + (row * columns * CELL_WIDTH);
        size_t rowidx         = 0;

        for (column = 0; column < columns; ++column) {
            uint8_t const *colptr = rowptr + (column * CELL_WIDTH);

            if (column > 0) {
                rowstr[rowidx++] = ' ';
            }

            for (byte = 0; byte < CELL_WIDTH; ++byte) {
                snprintf((rowstr + rowidx), 3, "%02x", colptr[byte]);
                rowidx += 2;
            }
        }

        rowstr[rowidx] = '\0';

        if (rowidx >= rowsize) {
            PRINT_ERROR("%s: WARNING: rowidx is %zu but rowsize is %zu\n",
                        __FUNCTION__, rowidx, rowsize);
        }

        PRINT_VERBOSE("  %p = %s\n", rowptr, rowstr);
    }

    free(rowstr);
}

/* Executes a test case with channel layouts rxlay and txlay, expecting
 * num_samples in format */
int test(bladerf_channel_layout rxlay,
         bladerf_channel_layout txlay,
         bladerf_format format,
         size_t num_samples)
{
    void *buf;
    int status;
    size_t i;

    size_t const samplesize = _interleave_calc_bytes_per_sample(format);
    size_t const offset     = _interleave_calc_metadata_bytes(format);
    size_t const num_chan   = _interleave_calc_num_channels(rxlay);
    size_t const bytes      = samplesize * num_samples;

    if (num_chan != _interleave_calc_num_channels(txlay)) {
        PRINT_ERROR("incompatible channel layouts: %d and %d\n", rxlay, txlay);
        return -1;
    }

    if (bytes < offset) {
        PRINT_ERROR("bytes (%zu) cannot be less than offset (%zu)\n", bytes,
                    offset);
        return -1;
    }

    if (num_chan < 1) {
        PRINT_ERROR("num_chan (%zu) cannot be less than 1\n", num_chan);
        return -1;
    }

    PRINT_INFO("beginning test: rxlay = %d, txlay = %d, format = %d, "
               "num_samples = %zu\n",
               rxlay, txlay, format, num_samples);

    PRINT_INFO("creating test buffer... ");

    buf = create_buf(bytes);
    if (NULL == buf) {
        PRINT_ERROR("failed to create_buf\n");
        return -1;
    } else {
        PRINT_INFO("ok!\n");
    }

    print_buf(buf, bytes, NUM_COLUMNS);

    status = _interleave_interleave_buf(txlay, format,
                                        (unsigned int)num_samples, buf);
    if (status != 0) {
        PRINT_ERROR("interleaver returned %d\n", status);
        goto error;
    }

    print_buf(buf, bytes, NUM_COLUMNS);

    if (offset > 0) {
        // special case check
        PRINT_INFO("checking metadata (%zu bytes starting at %p)... ", offset,
                   buf);
        print_buf(buf, offset, NUM_COLUMNS);
        if (!check_buf(buf, offset, samplesize, 1, 0)) {
            PRINT_ERROR("check_buf returned FALSE!\n");
            status = -1;
            goto error;
        } else {
            PRINT_INFO("good!\n");
        }
    }

    if (num_chan == 1) {
        // special case check
        PRINT_INFO("not a MIMO layout, verifying no interleaving occurred... ");
        if (!check_buf(buf, bytes, samplesize, 1, 0)) {
            PRINT_ERROR("check_buf returned FALSE!\n");
            status = -1;
            goto error;
        } else {
            PRINT_INFO("good!\n");
        }
    }

    for (i = 0; i < num_chan; ++i) {
        // get a pointer to the first interleaved sample for this channel
        void *bufptr = (uint8_t *)buf + offset + (samplesize * i);
        // len is still the whole buffer
        size_t buflen = bytes - offset;
        // start the counting pattern at the right place
        // offset the start value accordingly for channels > 0
        uint16_t startval = ((offset + i * (buflen / num_chan)) / 2) % 65536;

        if (!verbose && !quiet && 0 == i) {
            verbose = true;
            PRINT_VERBOSE("memory dump %p -> %p\n", buf,
                          (uint8_t *)buf + bytes);
            if (bytes > 64 * 2) {
                print_buf(buf, 48, 2);
                PRINT_VERBOSE(" ...\n");
                print_buf((uint8_t *)buf + bytes - 48, 48, 2);
            } else {
                print_buf(buf, bytes, 2);
            }
            verbose = false;
        }

        PRINT_INFO("checking interleaved data for ch %zu (*bufptr %p buflen "
                   "%zu num_chan %zu startval %04x)... ",
                   i, bufptr, buflen, num_chan, startval);

        if (!check_buf(bufptr, buflen, samplesize, num_chan, startval)) {
            PRINT_ERROR("check_buf returned FALSE!\n");
            status = -1;
            goto error;
        } else {
            PRINT_INFO("good!\n");
        }
    }

    status = _interleave_deinterleave_buf(rxlay, format,
                                          (unsigned int)num_samples, buf);
    if (status != 0) {
        PRINT_ERROR("deinterleaver returned %d\n", status);
        goto error;
    }

    print_buf(buf, bytes, NUM_COLUMNS);

    PRINT_INFO("checking deinterleaved data... ");

    if (!check_buf(buf, bytes, samplesize, 1, 0)) {
        PRINT_ERROR("check_buf returned FALSE!\n");
        status = -1;
        goto error;
    } else {
        PRINT_INFO("good!\n");
    }

error:
    free(buf);
    return status;
}

/* it's main */
int main(int argc, char *argv[])
{
    int status               = 0;
    size_t const NUM_SAMPLES = 16384;

    PRINT_INFO("*** BEGINNING 1-CHANNEL TESTS: interleaving should be noop\n");

    status = test(BLADERF_RX_X1, BLADERF_TX_X1, BLADERF_FORMAT_SC16_Q11,
                  NUM_SAMPLES);
    if (status < 0) {
        goto error;
    }

    status = test(BLADERF_RX_X1, BLADERF_TX_X1, BLADERF_FORMAT_SC16_Q11_META,
                  NUM_SAMPLES);
    if (status < 0) {
        goto error;
    }

    PRINT_INFO("*** BEGINNING 2-CHANNEL TESTS\n");

    status = test(BLADERF_RX_X2, BLADERF_TX_X2, BLADERF_FORMAT_SC16_Q11,
                  NUM_SAMPLES);
    if (status < 0) {
        goto error;
    }

    status = test(BLADERF_RX_X2, BLADERF_TX_X2, BLADERF_FORMAT_SC16_Q11_META,
                  NUM_SAMPLES);
    if (status < 0) {
        goto error;
    }

error:
    if (status < 0) {
        PRINT_ERROR("test returned %d, failing\n", status);
    }

    return status;
}
