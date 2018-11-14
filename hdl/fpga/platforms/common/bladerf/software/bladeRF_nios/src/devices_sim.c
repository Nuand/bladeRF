/* This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (c) 2015 Nuand LLC
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
#ifdef BLADERF_NIOS_PC_SIMULATION

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

#include "pkt_handler.h"
#include "devices.h"
#include "fpga_version.h"
#include "debug.h"

#include "sim_test_cases.h"

/* This global is used to flag the end of a simulation */
bool run_nios = true;

static size_t test_case_idx = 0;

const char *module2str(bladerf_module m)
{
    switch (m) {
        case BLADERF_MODULE_RX:
            return "RX";

        case BLADERF_MODULE_TX:
            return "TX";

        default:
            return "Invalid";
    }
}

void bladerf_nios_init(struct pkt_buf *pkt)
{
    DBG("%s()\n", __FUNCTION__);
}

uint8_t lms6_read(uint8_t addr) {
    const uint8_t ret = 0x17;
    DBG("%s: addr=0x%02x, returning 0x%02x\n", __FUNCTION__, addr, ret);
    ASSERT(addr == 0x2f);
    return ret;
}

void lms6_write(uint8_t addr, uint8_t data)
{
    DBG("%s: addr=0x%02x, data=0x%02x\n", __FUNCTION__, addr, data);
    ASSERT(addr == 0x07);
    ASSERT(data == 0x09);
}

uint64_t adi_spi_read(uint16_t addr) {
    const uint64_t ret = 0x17;
    DBG("%s: addr=0x%04x, returning 0x%04x\n", __FUNCTION__, addr, ret);
    ASSERT(addr == 0x2f2f);
    return ret;
}

void adi_spi_write(uint16_t addr, uint64_t data)
{
    DBG("%s: addr=0x%04x, data=0x%04x\n", __FUNCTION__, addr, data);
    ASSERT(addr == 0x0707);
    ASSERT(data == 0x09);
}

uint8_t si5338_read(uint8_t addr)
{
    const uint8_t ret = 0x88;
    DBG("%s: addr=0x%02x, returning 0x%02x\n", __FUNCTION__, addr, ret);
    ASSERT(addr == 0x3);
    return ret;
}

void si5338_write(uint8_t addr, uint8_t data)
{
    DBG("%s: addr=0x%02x, data=0x%02x\n", __FUNCTION__, addr, data);
    ASSERT(addr == 0x05);
    ASSERT(data == 0xab);
}

void vctcxo_trim_dac_write(uint8_t cmd, uint16_t val)
{
    DBG("%s: cmd=0x%02x, val=0x%04x\n", __FUNCTION__, cmd, val);
    ASSERT(cmd == 0x28   || cmd == 0x08);
    ASSERT(val == 0x0000 || val == 0x8012);
}

void vctcxo_trim_dac_read(uint8_t cmd, uint16_t *val)
{
    *val = 0x1234;

    DBG("%s: cmd=0x%02x, val=0x%04x\n", __FUNCTION__, cmd, *val);

    ASSERT(cmd == 0x98);
}

void adf4351_write(uint32_t val)
{
    DBG("%s: val=0x%08x\n", __FUNCTION__, val);
    ASSERT(0x580005);
}

uint32_t control_reg_read(void)
{
    uint32_t ret = 0x8abcde57;
    DBG("%s: returning 0x%08x\n", __FUNCTION__, ret);
    return ret;
}

void control_reg_write(uint32_t value)
{
    DBG("%s: value=0x%08x\n", __FUNCTION__, value);
    ASSERT(value == 0x80402057);
}

uint16_t iqbal_get_gain(bladerf_module m)
{
    uint16_t ret = 0xffff;

    switch (m) {
        case BLADERF_MODULE_RX:
            ret = 0x14d2;
            break;

        case BLADERF_MODULE_TX:
            ret = 0x0812;
            break;

        default:
            DBG("%s: Invalid module %u\n", __FUNCTION__, m);
            ASSERT(0);
    }

    DBG("%s: module=%s, returning 0x%04x\n",
        __FUNCTION__, module2str(m), ret);

    return ret;
}

void iqbal_set_gain(bladerf_module m, uint16_t value)
{
    switch (m) {
        case BLADERF_MODULE_RX:
            ASSERT(value == 0x14d2);
            break;

        case BLADERF_MODULE_TX:
            ASSERT(value == 0x0812);
            break;

        default:
            DBG("%s: Invalid module %u\n", __FUNCTION__, m);
            ASSERT(0);
    }

    DBG("%s: module=%s, value=0x%04x\n", __FUNCTION__, module2str(m), value);
}

uint16_t iqbal_get_phase(bladerf_module m)
{
    uint16_t ret = 0xffff;

    switch (m) {
        case BLADERF_MODULE_RX:
            ret = 0x101c;
            break;

        case BLADERF_MODULE_TX:
            ret = 0x021f;
            break;

        default:
            DBG("%s: Invalid module %u\n", __FUNCTION__, m);
            ASSERT(0);
    }

    DBG("%s: module=%s, returning 0x%04x\n",
        __FUNCTION__, module2str(m), ret);

    return ret;
}

void iqbal_set_phase(bladerf_module m, uint16_t value)
{
    switch (m) {
        case BLADERF_MODULE_RX:
            ASSERT(value == 0x101c);
            break;

        case BLADERF_MODULE_TX:
            ASSERT(value == 0x021f);
            break;

        default:
            DBG("%s: Invalid module %u\n", __FUNCTION__, m);
            ASSERT(0);
    }

    DBG("%s: module=%s, value=0x%04x\n", __FUNCTION__, module2str(m), value);
}

uint32_t expansion_port_read(void)
{
    uint32_t ret = 0xffffc7ff;
    DBG("%s: returning 0x%08x\n", __FUNCTION__, ret);
    return ret;
}

void expansion_port_write(uint32_t value)
{
    DBG("%s: value=0x%08x\n", __FUNCTION__, value);
    ASSERT(value == 0x3c000800);
}

uint32_t expansion_port_get_direction()
{
    uint32_t ret = 0x0a1b2c3d;
    DBG("%s: returning 0x%08x\n", __FUNCTION__, ret);
    return ret;
}

void expansion_port_set_direction(uint32_t dir)
{
    DBG("%s: dir=0x%08x\n", __FUNCTION__, dir);
}

uint64_t time_tamer_read(bladerf_module m)
{
    uint64_t ret = (uint64_t) -1;

    switch (m) {
        case BLADERF_MODULE_RX:
            ret = 0x123456780a1b2c3d;
            break;

        case BLADERF_MODULE_TX:
            ret = 0x87654321a1b2c3d4;
            break;

        default:
            DBG("%s: Invalid module %u\n", __FUNCTION__, m);
            ASSERT(0);
            break;
    }

    DBG("%s: module=%s, returning 0x%016"PRIx64"\n",
         __FUNCTION__, module2str(m), ret);

    return ret;
}

void time_tamer_reset(bladerf_module m)
{
    switch (m) {
        case BLADERF_MODULE_RX:
        case BLADERF_MODULE_TX:
            break;

        default:
            DBG("%s: Invalid module %u\n", __FUNCTION__, m);
            ASSERT(0);
            break;
    }

    DBG("%s: module=%s\n", __FUNCTION__, module2str(m));
}

void time_tamer_clear_interrupt(bladerf_module m)
{
    switch (m) {
        case BLADERF_MODULE_RX:
        case BLADERF_MODULE_TX:
            break;

        default:
            DBG("%s: Invalid module %u\n", __FUNCTION__, m);
            ASSERT(0);
            break;
    }

    DBG("%s: module=%s\n", __FUNCTION__, module2str(m));
}

void tamer_schedule(bladerf_module m, uint64_t time) {
    DBG("%s: module=%s, time=%"PRIu64"\n", __FUNCTION__, module2str(m), time);
}

int lms_set_precalculated_frequency(struct bladerf *dev, bladerf_module mod,
                                    struct lms_freq *f)
{
    DBG("%s: module=%s, nint=0x%04x, nfrac=0x%08x, freqsel=0x%02x, flags=0x%02x\n",
        __FUNCTION__, module2str(mod), f->nint, f->nfrac, f->freqsel, f->flags);

    return 0;
}

int lms_select_band(struct bladerf *dev, bladerf_module module, bool low_band)
{
    DBG("%s: module=%s, low_band=%s\n", __FUNCTION__, module2str(module),
        low_band ? "true" : "false");

    return 0;
}

void command_uart_read_request(uint8_t *req) {

    printf("\nTest case %-2zd: %s\n", test_case_idx + 1,
            test_cases[test_case_idx].desc);
    printf("--------------------------------------------------------\n");

    if (test_case_idx < ARRAY_SIZE(test_cases)) {
        memcpy(req, &test_cases[test_case_idx].req, NIOS_PKT_LEN);
        test_case_idx++;

        if (test_case_idx == ARRAY_SIZE(test_cases)) {
            run_nios = false;
        }
    } else {
        memset(req, 0xff, NIOS_PKT_LEN);
        DBG("Read past test case array.\n");
        return;
    }
}

void command_uart_write_response(uint8_t *resp) {
    print_bytes("Response data:", resp, NIOS_PKT_LEN);

    /* At this point, we'll already have incremented the test_case_idx past
     * this test. */
    ASSERT(test_case_idx > 0);

    if (memcmp(resp, test_cases[test_case_idx - 1].resp, NIOS_PKT_LEN)) {
        printf("Failed.\n");
        if (!getenv("CONTINUE_ON_FAIL")) {
                ASSERT(!"Aborting due to failed test case.\n");
        }
    } else {
        printf("Pass.\n\n");
    }
}

bool rfic_command_write(uint16_t addr, uint64_t data)
{
    DBG("%s: addr=%02x data=%08" PRIx64 "\n", __FUNCTION__, addr, data);
    return true;
}

bool rfic_command_read(uint16_t addr, uint64_t *data)
{
    DBG("%s: addr=%02x *data=%08" PRIx64 "\n", __FUNCTION__, addr, *data);
    return true;
}

#endif
