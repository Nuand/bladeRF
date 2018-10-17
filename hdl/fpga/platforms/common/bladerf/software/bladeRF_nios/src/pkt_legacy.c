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
#include <stdint.h>
#include <stdbool.h>
#include "pkt_handler.h"
#include "pkt_legacy.h"
#include "devices.h"
#include "fpga_version.h"
#include "debug.h"

#define PAYLOAD_IDX 2
#define ADDR_IDX    PAYLOAD_IDX
#define DATA_IDX    (ADDR_IDX + 1)

/* Omit debug output from this file. */
#ifdef BLADERF_NIOS_DEBUG
#undef DBG
#define DBG(...)
#endif

/**
 * Configuration options
 *
 * Programmable blocks attached via NIOS II GPIOs are grouped under a single
 * "device address" since they don't have many sub-addresses, and the device
 * address bits in the FX3 UART packet format structure are limited.
 *
 * Historically, this was just for GPIO-based interfaces.  Over time,
 * this has been overloaded with additional functionality.
 */
enum config_param {
    CONFIG_CONTROL_REG,             /* bladeRF FPGA control register */
    CONFIG_IQ_CORR_RX_GAIN,         /* IQ Balance corrections */
    CONFIG_IQ_CORR_RX_PHASE,
    CONFIG_IQ_CORR_TX_GAIN,
    CONFIG_IQ_CORR_TX_PHASE,
    CONFIG_FPGA_VERSION,            /* FPGA version number */
    CONFIG_RX_TIMESTAMP,            /* RX timestamp counter read/clear */
    CONFIG_TX_TIMESTAMP,            /* RX timestamp counter read/clear */
    CONFIG_VCTXCO,                  /* VCTCXO Trim DAC */
    CONFIG_XB200_SYNTH,             /* Control of XB-200 Synthesizer */
    CONFIG_EXPANSION,               /* Expansion port I/Os */
    CONFIG_EXPANSION_DIR,           /* Direction control of these I/Os */

    CONFIG_UNKNOWN,                 /* Reserved for invalid entry */
};

struct config_param_info {
    uint8_t start;
    uint8_t len;
};

/* Lookup-table for the legacy config parameters' address ranges */
static const struct config_param_info config_params[] = {
    [CONFIG_CONTROL_REG]      = { NIOS_PKT_LEGACY_PIO_ADDR_CONTROL,
                                  NIOS_PKT_LEGACY_PIO_LEN_CONTROL },

    [CONFIG_IQ_CORR_RX_GAIN]  = { NIOS_PKT_LEGACY_PIO_ADDR_IQ_RX_GAIN,
                                  NIOS_PKT_LEGACY_PIO_LEN_IQ_RX_PHASE },

    [CONFIG_IQ_CORR_RX_PHASE] = { NIOS_PKT_LEGACY_PIO_ADDR_IQ_RX_PHASE,
                                  NIOS_PKT_LEGACY_PIO_LEN_IQ_RX_PHASE },

    [CONFIG_IQ_CORR_TX_GAIN]  = { NIOS_PKT_LEGACY_PIO_ADDR_IQ_TX_GAIN,
                                  NIOS_PKT_LEGACY_PIO_LEN_IQ_TX_GAIN },

    [CONFIG_IQ_CORR_TX_PHASE] = { NIOS_PKT_LEGACY_PIO_ADDR_IQ_TX_PHASE,
                                  NIOS_PKT_LEGACY_PIO_LEN_IQ_TX_GAIN },

    [CONFIG_FPGA_VERSION]     = { NIOS_PKT_LEGACY_PIO_ADDR_FPGA_VERSION,
                                  NIOS_PKT_LEGACY_PIO_LEN_FPGA_VERSION },

    [CONFIG_RX_TIMESTAMP]     = { NIOS_PKT_LEGACY_PIO_ADDR_RX_TIMESTAMP,
                                  NIOS_PKT_LEGACY_PIO_LEN_RX_TIMESTAMP },

    [CONFIG_TX_TIMESTAMP]     = { NIOS_PKT_LEGACY_PIO_ADDR_TX_TIMESTAMP,
                                  NIOS_PKT_LEGACY_PIO_LEN_TX_TIMESTAMP },

    [CONFIG_VCTXCO]           = { NIOS_PKT_LEGACY_PIO_ADDR_VCTCXO,
                                  NIOS_PKT_LEGACY_PIO_LEN_VCTCXO },

    [CONFIG_XB200_SYNTH]      = { NIOS_PKT_LEGACY_PIO_ADDR_XB200_SYNTH,
                                  NIOS_PKT_LEGACY_PIO_LEN_XB200_SYNTH },

    [CONFIG_EXPANSION]        = { NIOS_PKT_LEGACY_PIO_ADDR_EXP,
                                  NIOS_PKT_LEGACY_PIO_LEN_EXP },

    [CONFIG_EXPANSION_DIR]    = { NIOS_PKT_LEGACY_PIO_ADDR_EXP_DIR,
                                  NIOS_PKT_LEGACY_PIO_LEN_EXP_DIR },

    [CONFIG_UNKNOWN]          = { 255, 0 },
};

static inline enum config_param lookup_param(uint8_t addr)
{
    uint8_t i;

    DBG("Perip lookup for addr=0x%x\n", addr);

    for (i = 0; i < ARRAY_SIZE(config_params); i++) {
        if (config_params[i].start <= addr &&
            (config_params[i].start + config_params[i].len) > addr) {
            DBG("Found match at entry 0x%x\n", i);
            return (enum config_param) i;
        }
    }

    DBG("UNKNOWN PARAM.\n");
    return CONFIG_UNKNOWN;
}

static uint64_t perform_config_read(enum config_param param)
{
    uint64_t payload;

    switch (param) {
        case CONFIG_CONTROL_REG:
            DBG("%s: Performing control reg read.\n", __FUNCTION__);
            payload = control_reg_read();
            break;

        case CONFIG_IQ_CORR_RX_GAIN:
            DBG("%s: Performing RX IQ gain read.\n", __FUNCTION__);
            payload = iqbal_get_gain(BLADERF_MODULE_RX);
            break;

        case CONFIG_IQ_CORR_RX_PHASE:
            DBG("%s: Performing RX IQ phase read.\n", __FUNCTION__);
            payload = iqbal_get_phase(BLADERF_MODULE_RX);
            break;

        case CONFIG_IQ_CORR_TX_GAIN:
            DBG("%s: Performing TX IQ gain read.\n", __FUNCTION__);
            payload = iqbal_get_gain(BLADERF_MODULE_TX);
            break;

        case CONFIG_IQ_CORR_TX_PHASE:
            DBG("%s: Performing TX IQ phase read.\n", __FUNCTION__);
            payload = iqbal_get_phase(BLADERF_MODULE_TX);
            break;

        case CONFIG_FPGA_VERSION:
            DBG("%s: Performing FPGA version read.\n", __FUNCTION__);
            payload = fpga_version();
            break;

        case CONFIG_RX_TIMESTAMP:
            DBG("%s: Performing RX timestamp read.\n", __FUNCTION__);
            payload = time_tamer_read(BLADERF_MODULE_RX);
            break;

        case CONFIG_TX_TIMESTAMP:
            DBG("%s: Performing TX timestamp read.\n", __FUNCTION__);
            payload = time_tamer_read(BLADERF_MODULE_TX);
            break;

        case CONFIG_VCTXCO:
            /* TODO Implement VCTCXO trim DAC read */
            DBG("%s: Attempted VCTCXO read.\n", __FUNCTION__);
            payload = 0x00;
            break;

        case CONFIG_XB200_SYNTH:
            DBG("%s: Attempted XB-200 synth read from write-only device.\n",
                __FUNCTION__);
            payload = 0x00;
            break;

        case CONFIG_EXPANSION:
            DBG("%s: Performing expansion port read.\n", __FUNCTION__);
            payload = expansion_port_read();
            break;

        case CONFIG_EXPANSION_DIR:
            DBG("%s: Performing expansion port direction read.\n", __FUNCTION__);
            payload = expansion_port_get_direction();
            break;

        default:
            DBG("Invalid config read parameter: 0x%x\n", param);
            payload = (uint64_t) -1;
    }

    return payload;
}

/* Perform read on first request and return a byte from the payload
 * on each successive request.
 *
 * Although this legacy format includes a (addr, data) tuple per request, we
 * always request data "in order" from the host, from LSB to MSB, so we won't
 * bother checking the successive addresses, which should just be incrementing.
 */
static inline void legacy_config_read(uint8_t count, struct pkt_buf *b)

{
    uint8_t i;
    static uint8_t n = 0;
    static uint64_t payload = 0;
    static enum config_param param = CONFIG_UNKNOWN;
    static enum config_param last_param = CONFIG_UNKNOWN;

    const uint8_t *req_data = &b->req[PAYLOAD_IDX];
    uint8_t *resp_data = &b->resp[PAYLOAD_IDX];

    param = lookup_param(b->req[PAYLOAD_IDX]);

    for (i = 0; i < count; i++) {

        /* Reset if we're at the beggining of a requst, or if it appears
         * that we got a different request while in the middle of the previous
         * series of accesses. */
        if (n == 0 || (param != last_param)) {
            DBG("Resetting read state for param=0x%x with n=0x%x\n", param, n);
            n = 0;
            payload = perform_config_read(param);
            last_param = param;
        }

        /* Copy address offset from request to response buffer */
        *resp_data++ = *req_data++ - config_params[param].start;

        /* Write read data into response buffer */
        *resp_data++ = (uint8_t) (payload >> (n * 8));

        req_data++;
        n++;

        /* We've finished returning data for this request - reset and quit . */
        if (n >= config_params[param].len) {
            n = 0;
            param = CONFIG_UNKNOWN;
            break;
        }
    }

}

static inline void legacy_pkt_read(uint8_t dev_id, uint8_t count,
                                   struct pkt_buf *b)
{

    switch (dev_id) {
        case NIOS_PKT_LEGACY_DEV_LMS:
            DBG("%s: Performing LMS6 read.\n", __FUNCTION__);
            b->resp[ADDR_IDX] = b->req[ADDR_IDX];
            b->resp[DATA_IDX] = lms6_read(b->req[ADDR_IDX]);
            break;

        case NIOS_PKT_LEGACY_DEV_SI5338:
            DBG("%s: Performing SI5338 read.\n", __FUNCTION__);
            b->resp[ADDR_IDX] = b->req[ADDR_IDX];
            b->resp[DATA_IDX] = si5338_read(b->req[ADDR_IDX]);
            break;

        case NIOS_PKT_LEGACY_DEV_CONFIG:
            DBG("%s: Performing config read.\n", __FUNCTION__);
            legacy_config_read(count, b);
            break;

        default:
            DBG("Got invalid device ID: 0x%x\n", dev_id);
            break;
    }
}

static inline void perform_config_write(enum config_param p, uint64_t payload)
{
    switch (p) {
        case CONFIG_CONTROL_REG:
            control_reg_write((uint32_t) payload);
            break;

        case CONFIG_IQ_CORR_RX_GAIN:
            iqbal_set_gain(BLADERF_MODULE_RX, (uint16_t) payload);
            break;

        case CONFIG_IQ_CORR_RX_PHASE:
            iqbal_set_phase(BLADERF_MODULE_RX, (uint16_t) payload);
            break;

        case CONFIG_IQ_CORR_TX_GAIN:
            iqbal_set_gain(BLADERF_MODULE_TX, (uint16_t) payload);
            break;

        case CONFIG_IQ_CORR_TX_PHASE:
            iqbal_set_phase(BLADERF_MODULE_TX, (uint16_t) payload);
            break;

        case CONFIG_FPGA_VERSION:
            DBG("Error: attempted to write to FPGA version parameter.\n");
            break;

        case CONFIG_RX_TIMESTAMP:
            time_tamer_reset(BLADERF_MODULE_RX);
            break;

        case CONFIG_TX_TIMESTAMP:
            time_tamer_reset(BLADERF_MODULE_TX);
            break;

        case CONFIG_VCTXCO:
            /* The legacy packet format only supported writing a value,
             * yielding writes for these specific commands:
             *
             * Command 0x28: Set device to write-through mode
             * Command 0x08: Write value to channel 0
             */
            vctcxo_trim_dac_write(0x28, 0);
            vctcxo_trim_dac_write(0x08, (uint16_t) payload);
            break;

        case CONFIG_XB200_SYNTH:
            adf4351_write((uint32_t) payload);
            break;

        case CONFIG_EXPANSION:
            expansion_port_write(payload);
            break;

        case CONFIG_EXPANSION_DIR:
            expansion_port_set_direction(payload);
            break;

        default:
            DBG("Invalid config param write: 0x%x\n", p);
            break;
    }
}


/* In the legacy format, we receive write data as (addr, data) tuples,
 * where addr just advances by one in each successive tuple.
 *
 * Since we know we don't do any weird ordering with these from the
 * host side, we're just assuming that we write these from LSB to MSB.
 *
 * Therefore, we'll just use the address from the first request.
 */
static inline void legacy_config_write(uint8_t count, struct pkt_buf *b)
{
    uint8_t i;
    static uint64_t payload = 0;
    static uint8_t n = 0;
    static enum config_param param = CONFIG_UNKNOWN;
    static enum config_param last_param = CONFIG_UNKNOWN;

    const uint8_t *req_data = &b->req[PAYLOAD_IDX];
    uint8_t *resp_data = &b->resp[PAYLOAD_IDX];

    param = lookup_param(b->req[PAYLOAD_IDX]);

    /* Reset if we're at the beggining of a requst, or if it appears
     * that we got a different request while in the middle of the previous
     * series of accesses. */
    if (n == 0 || (param != last_param)) {
        DBG("Resetting write state for param=0x%x with n=0x%x\n", param, n);
        payload = 0;
        n = 0;
    }

    for (i = 0; i < count && n < config_params[param].len; i++) {
        /* Copy over address offset into response, and zero out data*/
        *resp_data++ = *req_data++ - config_params[param].start;
        *resp_data++ = 0;

        /* Shift data into our aggregated payload word */
        payload |= (*req_data) << (n * 8);

        req_data++;
        n++;
    }

    /* We aggregated all the data we need - perform the write and reset */
    if (n >= config_params[param].len) {
        perform_config_write(param, payload);
        n = 0;
        param = CONFIG_UNKNOWN;
    }

    last_param = param;
}


static inline void legacy_pkt_write(uint8_t dev_id, uint8_t count,
                                   struct pkt_buf *b)

{
    switch (dev_id) {
        case NIOS_PKT_LEGACY_DEV_LMS:
            lms6_write(b->req[ADDR_IDX], b->req[DATA_IDX]);

            b->resp[ADDR_IDX] = b->req[ADDR_IDX];
            b->resp[DATA_IDX] = 0;
            break;

        case NIOS_PKT_LEGACY_DEV_SI5338:
            si5338_write(b->req[ADDR_IDX], b->req[DATA_IDX]);

            b->resp[ADDR_IDX] = b->req[ADDR_IDX];
            b->resp[DATA_IDX] = 0;
            break;

        case NIOS_PKT_LEGACY_DEV_CONFIG:
            legacy_config_write(count, b);
            break;

        default:
            DBG("Got invalid device ID: 0x%x\n", dev_id);
            break;
    }
}

void pkt_legacy(struct pkt_buf *b)
{
    /* Parse configuration word */
    const uint8_t cfg = b->req[PKT_CFG_IDX];
    const bool is_read = (cfg & (NIOS_PKT_LEGACY_MODE_DIR_READ)) != 0;
    const bool is_write = (cfg & (NIOS_PKT_LEGACY_MODE_DIR_WRITE)) != 0;
    const uint8_t dev_id = (cfg & NIOS_PKT_LEGACY_MODE_DEV_MASK);
    const uint8_t count = (cfg & NIOS_PKT_LEGACY_MODE_CNT_MASK);

    /* Response must start with same magic value and config word */
    b->resp[PKT_MAGIC_IDX] = b->req[PKT_MAGIC_IDX];
    b->resp[PKT_CFG_IDX]   = b->req[PKT_CFG_IDX];

    DBG("%s: read=%s, write=%s, dev_id=0x%x, cfg=0x%x, count=0x%x\n", __FUNCTION__,
        is_read ? "true" : "false", is_write ? "true" : "false", dev_id, cfg, count);

    if (is_read) {
        legacy_pkt_read(dev_id, count, b);
    } else if (is_write) {
        legacy_pkt_write(dev_id, count, b);
    } else {
        DBG("Config word did not have R/W: 0x%x\n", cfg);
    }
}
