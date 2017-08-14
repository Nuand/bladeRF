#include <stdio.h>
#include <libbladeRF.h>

#include "log.h"

#ifndef min
#define min(x, y) x < y ? x : y
#endif // !min

#ifndef max
#define max(x, y) x > y ? x : y
#endif // !max

#define PRINT_VERBOSE(args...) verbose ? printf(args) : 0
#define PRINT_INFO(args...) printf(args)

bool verbose = false;

size_t const BYTES_PER_SAMPLE = 4;
size_t const NUM_SAMPLES = 1024;

// for print_buf
size_t const CELL_WIDTH = 4;
size_t const NUM_COLUMNS = 8;

void *create_buf(size_t buflen)
{
    void *retbuf = NULL;

    retbuf = malloc(buflen);
    if (NULL == retbuf) {
        PRINT_INFO("%s: malloc failed\n", __FUNCTION__);
        return retbuf;
    }

    for (size_t i = 0; i < buflen/2; ++i) {
        uint16_t *ptr = (uint16_t *)retbuf + i;
        *ptr = (i % 65536);
    }

    return retbuf;
}

bool check_buf(void *buf, size_t buflen)
{
    bool retval = true;

    for (size_t i = 0; i < buflen/2; ++i) {
        uint16_t *ptr = (uint16_t *)buf + i;
        if ((i % 65536) != *ptr) {
            retval = false;
        }
    }

    return retval;
}

void print_buf(void *buf, size_t buflen, size_t num_columns)
{
    size_t const columns = min(num_columns, buflen / CELL_WIDTH);
    size_t const rows = max(buflen / columns / CELL_WIDTH, 1);
    size_t const rowlen = sizeof(char) * (CELL_WIDTH*2) * (columns+1) + 1;

    char *rowstr;
    size_t rowidx = 0;
    size_t bufidx = 0;
    size_t bufidx_start = 0;

    rowstr = malloc(rowlen);
    if (NULL == rowstr) {
        PRINT_INFO("couldn't malloc rowstr\n");
        return;
    }

    for (size_t row = 0; row < rows; ++row) {
        rowidx = 0;
        bufidx_start = bufidx;
        for (size_t column = 0; column < columns; ++column) {
            if (column > 0) {
                rowstr[rowidx++] = ' ';
            }
            for (size_t byte = 0; byte < CELL_WIDTH; ++byte) {
                snprintf((rowstr + rowidx), rowlen - rowidx, "%02x", *((uint8_t*)buf + bufidx));
                rowidx += 2;
                bufidx += 1;
            }
        }
        rowstr[rowidx] = '\0';
        PRINT_VERBOSE("  %p = %s\n", buf + bufidx_start, rowstr);
    }

    free(rowstr);
}

int test(bladerf_channel_layout rx_layout, bladerf_channel_layout tx_layout, bladerf_format format, size_t num_samples)
{
    void *buf;
    int status;
    size_t const bytes = BYTES_PER_SAMPLE * num_samples;

    PRINT_INFO("beginning test: rx_layout = %d, tx_layout = %d, format = %d, num_samples = %lu\n",
        rx_layout, tx_layout, format, num_samples);

    PRINT_INFO("creating test buffer... ");

    buf = create_buf(bytes);
    if (NULL == buf) {
        PRINT_INFO("failed to create_buf\n");
        return -1;
    } else {
        PRINT_INFO("ok!\n");
    }

    print_buf(buf, bytes, NUM_COLUMNS);

    status = bladerf_interleave_stream_buffer(tx_layout, format, num_samples, buf);
    if (status != 0) {
        PRINT_INFO("ERROR: interleaver returned %d\n", status);
        goto error;
    }

    print_buf(buf, bytes, NUM_COLUMNS);

    if (BLADERF_FORMAT_SC16_Q11_META == format) {
        // special case check
        PRINT_INFO("checking metadata... ");
        print_buf(buf, 16, NUM_COLUMNS);
        if (!check_buf(buf, 16)) {
            PRINT_INFO("check_buf returned FALSE!\n");
            status = -1;
            goto error;
        } else {
            PRINT_INFO("ok!\n");
        }
    }

    if (BLADERF_RX_X1 == rx_layout || BLADERF_TX_X1 == tx_layout) {
        // special case check
        PRINT_INFO("not a MIMO layout, verifying no interleaving... ");
        if (!check_buf(buf, bytes)) {
            PRINT_INFO("check_buf returned FALSE!\n");
            status = -1;
            goto error;
        } else {
            PRINT_INFO("ok!\n");
        }
    } else if (!verbose) {
        verbose = true;
        PRINT_VERBOSE(" please visually verify interleaver output:\n");
        print_buf(buf, min(bytes, 64), 2);
        if (bytes > 64) {
            PRINT_VERBOSE(" ...\n");
            print_buf(buf + bytes - 64, 64, 2);
        }
        verbose = false;
    }

    status = bladerf_deinterleave_stream_buffer(rx_layout, format, num_samples, buf);
    if (status != 0) {
        PRINT_INFO("ERROR: deinterleaver returned %d\n", status);
        goto error;
    }

    print_buf(buf, bytes, NUM_COLUMNS);

    PRINT_INFO("checking deinterleaved data... ");

    if (!check_buf(buf, bytes)) {
        PRINT_INFO("check_buf returned FALSE!\n");
        status = -1;
        goto error;
    } else {
        PRINT_INFO("ok!\n");
    }

error:
    free(buf);
    return status;
}

int main(int argc, char *argv[])
{
    int status = 0;

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
        PRINT_INFO("test returned %d, failing\n", status);
    }

    return status;
}

