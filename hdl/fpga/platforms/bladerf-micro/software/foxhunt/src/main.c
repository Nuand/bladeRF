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

/* Define this to run this code on a PC instead of the NIOS II */
//#define BLADERF_NULL_HARDWARE

/* Will send debug alt_printf info to JTAG console while running on NIOS.
 * This can slow performance and cause timing issues... be careful. */
//#define BLADERF_NIOS_DEBUG

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

#define SAMPLERATE 3840000

#define MAX_MESSAGE_LENGTH 128
#define MAX_TONE_COUNT 128

#define CHANNEL BLADERF_CHANNEL_TX(1)

struct message const MESSAGES[] = {
    {
        .text       = "PARIS PARIS",
        .wpm        = 10,
        .tone       = 1000,
        .carrier    = 147555000,
        .backoff_dB = 10,
    },
    {
        .text       = "cq foxhunt de w2xh",
        .wpm        = 12,
        .tone       = 1000,
        .carrier    = 147555000,
        .backoff_dB = 10,
    },
};

void test_morse_sequencer(struct message msg)
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

    count = to_morse(msg.text, elements, MAX_MESSAGE_LENGTH);
    if (count == 0) {
        DBG("%s: to_morse did nothing\n", __FUNCTION__);
        return;
    } else if (count >= MAX_MESSAGE_LENGTH) {
        DBG("%s: to_morse overflowed\n", __FUNCTION__);
        return;
    }

    count = to_tones(elements, tones, msg.tone, dot_time, MAX_TONE_COUNT);
    if (count == 0) {
        DBG("%s: to_tones did nothing\n", __FUNCTION__);
        return;
    } else if (count >= MAX_TONE_COUNT) {
        DBG("%s: to_tones overflowed\n", __FUNCTION__);
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        DBG("Tone f=%x d=%x\n", tones[i].frequency,
            (int)(tones[i].duration / dot_time));
    }
}

void test_morse_simple(void)
{
    char const message[] = "Hello World";
    element out[MAX_MESSAGE_LENGTH];
    size_t count;

    count = to_morse(message, out, MAX_MESSAGE_LENGTH);

    DBG("Count: %x\n", count);

    DBG("Contents:");
    for (size_t i = 0; i < MAX_MESSAGE_LENGTH; ++i) {
        DBG(" %x", out[i]);
    }
    DBG("\n");
    DBG("Message: ");

    for (size_t i = 0; i < count; ++i) {
        switch (out[i]) {
            case T_END:
                DBG(" ");
                break;
            case T_INVALID:
                DBG("<ERROR>");
                break;
            case T_DONE:
                DBG("<EOM>");
                break;
            case T_DOT:
                DBG(". ");
                break;
            case T_DASH:
                DBG("_ ");
                break;
            case T_WORD_GAP:
                DBG("/ ");
                break;
        }
    }

    DBG("\n");
}

#define RFIC_ADDRESS(cmd, ch) ((cmd & 0xFF) + ((ch & 0xF) << 8))

bool queue_is_empty(void)
{
    uint64_t read_data;
    bool rv;
    rv = rfic_command_read(RFIC_ADDRESS(BLADERF_RFIC_COMMAND_STATUS, -1),
                           &read_data);

    return (rv && 0 == ((read_data >> BLADERF_RFIC_STATUS_WQLEN_SHIFT) &
                        BLADERF_RFIC_STATUS_WQLEN_MASK));
}

int main(void)
{
    bool run_nios    = true;
    size_t run_line  = 0;
    size_t msg_index = 0;

    DBG(BLADERF_DEVICE_NAME " FPGA v%x.%x.%x\n", FPGA_VERSION_MAJOR,
        FPGA_VERSION_MINOR, FPGA_VERSION_PATCH);
    DBG("Built " __DATE__ " " __TIME__ " with love <3\n");
#ifdef BLADERF_NIOS_LIBAD936X
    DBG("libad936x found: This FPGA image has magic transgirl powers\n");
#endif  // BLADERF_NIOS_LIBAD936X

#ifndef BLADERF_NIOS_LIBAD936X
    DBG("FATAL ERROR: libad936x is required for this application.\n");
    do {
    } while (true);
#endif  // BLADERF_NIOS_LIBAD936X

    /* Initialize the structures for RFIC control */
    rfic_command_init();

    DBG("=== System Ready ===\n");

    /**
     * Main program loop.
     *
     * This implements a rudimentary "program counter" kind of thing, to
     * sequence commands to the RFIC, FPGA, etc, while still doing background
     * tasks as required.
     */
    while (run_nios) {
        struct message msg = MESSAGES[msg_index];

        /* Run any queued work. It is important to run this in the main loop, as
         * RFIC control tasks are queued. */
        rfic_command_work();

        /* run_line is, fundamentally, a "program counter" and steps through the
         * switch cases below. */
        switch (run_line) {
            case 0:
                /* Initialize the RFIC. This command takes a little while. */
                rfic_command_write(RFIC_ADDRESS(BLADERF_RFIC_COMMAND_INIT, -1),
                                   true);
                break;

            case 1:
                /* Block until queue is empty */
                if (!queue_is_empty()) {
                    run_line -= 1;
                }
                break;

            case 100:
                /* BEGIN TRANSMITTING MESSAGE */
                break;

            case 101:
                /* Set Frequency */
                rfic_command_write(
                    RFIC_ADDRESS(BLADERF_RFIC_COMMAND_FREQUENCY, CHANNEL),
                    msg.carrier);
                break;

            case 102:
                /* Set Sample Rate */
                rfic_command_write(
                    RFIC_ADDRESS(BLADERF_RFIC_COMMAND_SAMPLERATE, CHANNEL),
                    SAMPLERATE);
                break;

            case 103:
                /* Set Gain */
                rfic_command_write(
                    RFIC_ADDRESS(BLADERF_RFIC_COMMAND_GAIN, CHANNEL),
                    msg.backoff_dB * 1000);
                break;

            case 104:
                /* Begin TX on this channel */
                rfic_command_write(
                    RFIC_ADDRESS(BLADERF_RFIC_COMMAND_ENABLE, CHANNEL), true);
                break;

            case 105:
                /* Block until queue is empty */
                if (!queue_is_empty()) {
                    run_line -= 1;
                }
                break;

            case 110:
                /* Do signal transmission */
                test_morse_sequencer(msg);
                break;

            case 120:
                /* Stop transmitting */
                rfic_command_write(
                    RFIC_ADDRESS(BLADERF_RFIC_COMMAND_ENABLE, CHANNEL), false);
                break;

            case 150:
                /* If we have more messages, transmit them */
                if (msg_index < ARRAY_SIZE(MESSAGES) - 1) {
                    ++msg_index;
                    run_line = 100;
                } else {
                    // Delay and repeat??
                }
                break;

            case 1000:
                /* End of commands... */
                DBG("=== Command sequence completed ===\n");
                break;

            case 1001:
                /* Halt here */
                run_line -= 1;
                break;

            default:
                break;
        };

        ++run_line;
    }

    return 0;
}
