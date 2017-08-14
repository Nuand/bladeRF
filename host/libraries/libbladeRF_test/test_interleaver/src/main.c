#include <libbladeRF.h>
#include <stdio.h>

#include "log.h"

#ifndef min
#define min(x, y) x < y ? x : y
#endif  // !min

#ifndef max
#define max(x, y) x > y ? x : y
#endif  // !max

#define PRINT_VERBOSE(args...) verbose ? printf(args) : 0
#define PRINT_INFO(args...) quiet ? 0 : printf(args)
#define PRINT_ERROR(args...) printf(args)

bool verbose = false;
bool quiet   = false;

size_t const BYTES_PER_SAMPLE = sizeof(uint32_t);
size_t const CELL_WIDTH       = 4;
size_t const NUM_COLUMNS      = 8;

/* Creates a buffer of buflen bytes, containing a counting pattern */
void *create_buf(size_t buflen)
{
    void *retbuf  = NULL;
    uint16_t *ptr = NULL;

    retbuf = malloc(buflen);
    if (NULL == retbuf) {
        PRINT_ERROR("%s: malloc failed\n", __FUNCTION__);
        return retbuf;
    }

    for (size_t i = 0; i < buflen / 2; ++i) {
        ptr  = (uint16_t *)retbuf + i;
        *ptr = (i % 65536);
    }

    return retbuf;
}

/* Checks the contents of a buffer, buf, of buflen bytes for a proper counting
 * pattern (starting at count), checking every stride'th sample */
bool check_buf(void const *buf, size_t buflen, size_t stride, uint16_t count)
{
    bool retval   = true;
    uint32_t *ptr = NULL;
    uint32_t expect;

    if (NULL == buf) {
        PRINT_ERROR("%s: buf is null, unable to check\n", __FUNCTION__);
        return false;
    }

    for (size_t i = 0; i < buflen / BYTES_PER_SAMPLE; i += stride) {
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

    char *rowstr = malloc(rowsize);
    if (NULL == rowstr) {
        PRINT_ERROR("%s: couldn't malloc rowstr\n", __FUNCTION__);
        return;
    }

    for (size_t row = 0; row < rows; ++row) {
        void const *bufptr = buf;
        size_t rowidx      = 0;

        for (size_t column = 0; column < columns; ++column) {
            if (column > 0) {
                rowstr[rowidx++] = ' ';
            }
            for (size_t byte = 0; byte < CELL_WIDTH; ++byte) {
                snprintf((rowstr + rowidx), rowsize - rowidx, "%02x",
                         *((uint8_t *)buf++));
                        //*((uint8_t *)buf + bufidx));
                rowidx += 2;
            }
        }

        rowstr[rowidx] = '\0';

        if (rowidx >= rowsize) {
            PRINT_ERROR("%s: WARNING: rowidx is %lu but rowsize is %lu\n",
                        __FUNCTION__, rowidx, rowsize);
        }

        PRINT_VERBOSE("  %p = %s\n", bufptr, rowstr);
    }

    free(rowstr);
}

/* Executes a test case with channel layouts rx_layout and tx_layout, expecting
 * num_samples in format */
int test(bladerf_channel_layout rx_layout, bladerf_channel_layout tx_layout, bladerf_format format, size_t num_samples)
{
    void *buf;
    int status;

    size_t const bytes = BYTES_PER_SAMPLE * num_samples;
    size_t const stride = (BLADERF_RX_X1 == rx_layout || BLADERF_TX_X1 == tx_layout) ? 1 : 2;
    size_t const offset = BLADERF_FORMAT_SC16_Q11_META == format ? 16 : 0;

    PRINT_INFO("beginning test: rx_layout = %d, tx_layout = %d, format = %d, num_samples = %lu\n",
        rx_layout, tx_layout, format, num_samples);

    PRINT_INFO("creating test buffer... ");

    buf = create_buf(bytes);
    if (NULL == buf) {
        PRINT_ERROR("failed to create_buf\n");
        return -1;
    } else {
        PRINT_INFO("ok!\n");
    }

    print_buf(buf, bytes, NUM_COLUMNS);

    status = bladerf_interleave_stream_buffer(tx_layout, format, num_samples, buf);
    if (status != 0) {
        PRINT_ERROR("interleaver returned %d\n", status);
        goto error;
    }

    print_buf(buf, bytes, NUM_COLUMNS);

    if (offset > 0) {
        // special case check
        PRINT_INFO("checking metadata (%lu bytes starting at %p)... ", offset, buf);
        print_buf(buf, offset, NUM_COLUMNS);
        if (!check_buf(buf, offset, 1, 0)) {
            PRINT_ERROR("check_buf returned FALSE!\n");
            status = -1;
            goto error;
        } else {
            PRINT_INFO("good!\n");
        }
    }

    if (1 == stride) {
        // special case check
        PRINT_INFO("not a MIMO layout, verifying no interleaving occurred... ");
        if (!check_buf(buf, bytes, 1, 0)) {
            PRINT_ERROR("check_buf returned FALSE!\n");
            status = -1;
            goto error;
        } else {
            PRINT_INFO("good!\n");
        }
    }

    for (size_t i = 0; i < stride; ++i) {
        // get a pointer to the first interleaved sample for this channel
        void *bufptr = buf + offset + (BYTES_PER_SAMPLE * i);
        // len is still the whole buffer
        size_t bufptrlen = bytes - offset;
        // start the counting pattern at the right place
        // offset the start value accordingly for channels > 0
        uint16_t startval = ((offset + i * (bufptrlen/stride)) / sizeof(uint16_t)) % 65536;

        if (!verbose && !quiet && 0 == i) {
            verbose = true;
            PRINT_VERBOSE("memory dump %p -> %p\n", buf, buf + bytes);
            if (bytes > 64*2) {
                print_buf(buf, 48, 2);
                PRINT_VERBOSE(" ...\n");
                print_buf(buf + bytes - 48, 48, 2);
            } else {
                print_buf(buf, bytes, 2);
            }
            verbose = false;
        }

        PRINT_INFO("checking interleaved data for ch %lu (*bufptr %p bufptrlen %lu stride %lu startval %04x)... ", i, bufptr, bufptrlen, stride, startval);

        if (!check_buf(bufptr, bufptrlen, stride, startval)) {
            PRINT_ERROR("check_buf returned FALSE!\n");
            status = -1;
            goto error;
        } else {
            PRINT_INFO("good!\n");
        }
    }


    status = bladerf_deinterleave_stream_buffer(rx_layout, format, num_samples, buf);
    if (status != 0) {
        PRINT_ERROR("deinterleaver returned %d\n", status);
        goto error;
    }

    print_buf(buf, bytes, NUM_COLUMNS);

    PRINT_INFO("checking deinterleaved data... ");

    if (!check_buf(buf, bytes, 1, 0)) {
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
    int status = 0;
size_t const NUM_SAMPLES = 16384;

    PRINT_INFO("*** BEGINNING 1-CHANNEL TESTS: interleaving should be noop\n");

    status = test(BLADERF_RX_X1, BLADERF_TX_X1, BLADERF_FORMAT_SC16_Q11, NUM_SAMPLES);
    if (status < 0) {
        goto error;
    }

    status = test(BLADERF_RX_X1, BLADERF_TX_X1, BLADERF_FORMAT_SC16_Q11_META, NUM_SAMPLES);
    if (status < 0) {
        goto error;
    }

    PRINT_INFO("*** BEGINNING 2-CHANNEL TESTS\n");

    status = test(BLADERF_RX_X2, BLADERF_TX_X2, BLADERF_FORMAT_SC16_Q11, NUM_SAMPLES);
    if (status < 0) {
        goto error;
    }

    status = test(BLADERF_RX_X2, BLADERF_TX_X2, BLADERF_FORMAT_SC16_Q11_META, NUM_SAMPLES);
    if (status < 0) {
        goto error;
    }


error:
    if (status < 0) {
        PRINT_ERROR("test returned %d, failing\n", status);
    }

    return status;
}

