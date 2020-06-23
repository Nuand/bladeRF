/* This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (c) 2018 Nuand LLC
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "foxhunt.h"
#include "morse.h"

#include "debug.h"
#include "devices.h"
#include "devices_rfic.h"

#define BLADERF_DEVICE_NAME "Foxhunt for the Nuand bladeRF 2.0 Micro"

#define SAMPLERATE 768000

#define MAX_MESSAGE_LENGTH 128
#define MAX_TONE_COUNT 128

// uncomment to enable bias tee power (e.g. for BT-100)
#define BIASTEE 1

// Check for buildability
#ifndef TONE_GENERATOR_0_BASE
#error tone generator not present
#endif

/**
 * @brief      Error-catching wrapper
 *
 * @param      _fn   The function to wrap
 *
 * @return     returns 1 on failure, continues if successful
 */
#define CALL(_fn)                                                       \
    do {                                                                \
        if (!_fn) {                                                     \
            DBG("*** FAILED: %s() @ %s:0x%x\n", __FUNCTION__, __FILE__, \
                __LINE__);                                              \
            return 1;                                                   \
        }                                                               \
    } while (0)

bool const REPEAT_MODE = true;

uint32_t const INTER_MESSAGE_GAP = 10; /* seconds */

struct message const MESSAGES[] = {
    {
        /* This should be exactly 100 dot lengths */
        .text       = "PARIS PARIS",
        .wpm        = 5,
        .tone       = 1000,
        .carrier    = 915000000,
        .backoff_dB = 10,
    },
    // {
    //     /* This should be exactly 100 dot lengths */
    //     .text       = "PARIS PARIS",
    //     .wpm        = 5,
    //     .tone       = 1000,
    //     .carrier    = 433920000,
    //     .backoff_dB = 10,
    // },
    // {
    //     .text       = "cq foxhunt de w2xh",
    //     .wpm        = 5,
    //     .tone       = 1000,
    //     .carrier    = 147555000,
    //     .backoff_dB = 0,
    // },
};

size_t queue_status()
{
    return IORD_32DIRECT(TONE_GENERATOR_0_BASE, TONE_GEN_OFFSET_STATUS);
}

size_t queue_len()
{
    return (queue_status() >> 8) & 0xFF;
}

bool queue_full()
{
    return (queue_status() & 0x2);
}

bool queue_running()
{
    return (queue_status() & 0x1);
}

/* dphase is 4096 / (f_clock / f_tone) */
float const DPHASE_COEFF = 4096.0 / (float)SAMPLERATE;

void queue_tone(unsigned int frequency, float duration)
{
    uint32_t dur_clks;
    int32_t dphase;

    dur_clks = __round_int((float)SAMPLERATE * duration);
    dphase   = __round_int((float)frequency * DPHASE_COEFF);

    /* Spin until queue has room */
    while (queue_full()) {
        usleep(1000);
    }

    IOWR_32DIRECT(TONE_GENERATOR_0_BASE, TONE_GEN_OFFSET_DPHASE, dphase);
    IOWR_32DIRECT(TONE_GENERATOR_0_BASE, TONE_GEN_OFFSET_DURATION, dur_clks);

    DBG("%s: read dphase=0x%x\n", __FUNCTION__,
        IORD_32DIRECT(TONE_GENERATOR_0_BASE, TONE_GEN_OFFSET_DPHASE));
    DBG("%s: read dur_clks=0x%x\n", __FUNCTION__,
        IORD_32DIRECT(TONE_GENERATOR_0_BASE, TONE_GEN_OFFSET_DURATION));

    IOWR_32DIRECT(TONE_GENERATOR_0_BASE, TONE_GEN_OFFSET_STATUS,
                  TONE_GEN_OFFSET_CTRL_APPEND);

    DBG("%s: running=%x dphase=0x%x dur_clks=0x%x qlen=0x%x\n", __FUNCTION__,
        queue_running(), dphase, dur_clks, queue_len());
}

/* Generate morse code signal */
void morse_tx(struct message msg)
{
    size_t count;
    element elements[MAX_MESSAGE_LENGTH];
    struct tone tones[MAX_TONE_COUNT];

    /** Dot length calculation:
     *
     * "PARIS" is 50 dot lengths, a "typical" word
     * If it can be sent n times in a minute, speed is n WPM
     *
     * 1 WPM => 60 seconds / 50 dots
     * n WPM => 60 seconds / (n*50) dots
     */
    float const base = 60.0 / 50.0;
    float dot_time   = base / msg.wpm;

    int total_dots = 0;

    count = to_morse(msg.text, elements, MAX_MESSAGE_LENGTH);
    if (0 == count || count >= MAX_MESSAGE_LENGTH) {
        DBG("%s: to_morse ret 0x%x\n", __FUNCTION__, count);
        return;
    }

    count = to_tones(elements, tones, msg.tone, dot_time, MAX_TONE_COUNT);
    if (0 == count || count >= MAX_TONE_COUNT) {
        DBG("%s: to_tones ret 0x%x\n", __FUNCTION__, count);
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        int dots = (int)(tones[i].duration / dot_time);
        // DBG("play len=%x freq=%x\n", dots, tones[i].frequency);
        queue_tone(tones[i].frequency, tones[i].duration);
        total_dots += dots;
    }

    DBG("%s: total count=%x len=%x\n", __FUNCTION__, count, total_dots);
}

int main(void)
{
    /* Standard output text */
    DBG(BLADERF_DEVICE_NAME " FPGA v%x.%x.%x\n", FPGA_VERSION_MAJOR,
        FPGA_VERSION_MINOR, FPGA_VERSION_PATCH);
    DBG("Built " __DATE__ " " __TIME__ " with love <3\n");
#ifdef BLADERF_NIOS_LIBAD936X
    DBG("libad936x found: This FPGA image has magic transgirl powers\n");
#else
    DBG("FATAL ERROR: libad936x is required for this application.\n");
    while (true) {
    };
#endif  // BLADERF_NIOS_LIBAD936X

    /* Hardcoded channel */
    bladerf_channel channel = BLADERF_CHANNEL_TX(0);

    /* State */
    bool run_nios    = true; /* Set 'false' to drop out of the loop */
    size_t msg_index = 0;    /* Counter for message index */

    bladerf_frequency prev_freq = 0;
    bladerf_gain prev_gain      = 0;

    /* Using a guestimate here... */
    ad56x1_vctcxo_trim_dac_write(0x1ec1);
    usleep(1000000);

    IOWR_32DIRECT(TONE_GENERATOR_0_BASE, TONE_GEN_OFFSET_STATUS,
                  TONE_GEN_OFFSET_CTRL_CLEAR);

    DBG("=== Initializing RFIC ===\n");

    /* Initialize RFIC.
     *
     * The CALL() macro will print an error and return 1 if the function returns
     * false.
     */
    CALL(rfic_command_write_immed(BLADERF_RFIC_COMMAND_INIT,
                                  RFIC_SYSTEM_CHANNEL,
                                  BLADERF_RFIC_INIT_STATE_ON));

    /* Set up filters */
    for (size_t i = 0; i < 2; ++i) {
        CALL(rfic_command_write_immed(BLADERF_RFIC_COMMAND_FILTER,
                                      BLADERF_CHANNEL_RX(i),
                                      BLADERF_RFIC_RXFIR_DEC4));
        CALL(rfic_command_write_immed(BLADERF_RFIC_COMMAND_FILTER,
                                      BLADERF_CHANNEL_TX(i),
                                      BLADERF_RFIC_TXFIR_INT4));
    }

    /* Set Sample Rate */
    CALL(rfic_command_write_immed(BLADERF_RFIC_COMMAND_SAMPLERATE, channel,
                                  SAMPLERATE));

    DBG("=== System Ready ===\n");

    /**
     * Main program loop.
     */
    while (run_nios) {
        struct message msg = MESSAGES[msg_index];

        DBG("--> Message %x ('%s')\n", msg_index, msg.text);

        /* BEGIN TRANSMITTING MESSAGE */

        /* Set Frequency */
        if (msg.carrier != prev_freq) {
            CALL(rfic_command_write_immed(BLADERF_RFIC_COMMAND_FREQUENCY,
                                          channel, msg.carrier));
            prev_freq = msg.carrier;
        }

        /* Set Gain */
        if (msg.backoff_dB != prev_gain) {
            CALL(rfic_command_write_immed(BLADERF_RFIC_COMMAND_GAIN, channel,
                                          msg.backoff_dB * 1000));
            prev_gain = msg.backoff_dB;
        }

        /* Begin TX on this channel if required */
        CALL(rfic_command_write_immed(BLADERF_RFIC_COMMAND_ENABLE, channel,
                                      true));

#ifdef BIASTEE
        /* Enable bias tee on TX side */
        uint32_t rffe_reg;

        rffe_reg = rffe_csr_read();

        rffe_reg |= (1 << 10);  // tx_bias_en

        rffe_csr_write(rffe_reg);
#endif // BIASTEE

        /* Pause 1 second */
        queue_tone(0, 1);

        /* Do signal transmission */
        morse_tx(msg);

        /* If we have more messages, transmit them */
        if ((msg_index + 1) < ARRAY_SIZE(MESSAGES)) {
            ++msg_index;
        } else {
            msg_index = 0;
            run_nios  = REPEAT_MODE;
        }

        if (run_nios) {
            /* Pause 1 second */
            queue_tone(0, 1);

            /* Wait for completion */
            while (queue_running()) {
                usleep(1000);
            }

            /* Stop transmitting */
            CALL(rfic_command_write_immed(BLADERF_RFIC_COMMAND_ENABLE, channel,
                                          false));

#ifdef BIASTEE
        /* Disable bias tee on TX side */
        rffe_reg = rffe_csr_read();

        rffe_reg &= ~(1 << 10);  // tx_bias_en

        rffe_csr_write(rffe_reg);
#endif // BIASTEE

            usleep(INTER_MESSAGE_GAP * 1e6);
        }
    }

    DBG("=== RFIC standby ===\n");

    CALL(rfic_command_write_immed(BLADERF_RFIC_COMMAND_INIT,
                                  RFIC_SYSTEM_CHANNEL,
                                  BLADERF_RFIC_INIT_STATE_STANDBY));

    DBG("=== Time to die... ===\n");
    return 0;
}
