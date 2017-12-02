/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (c) 2017 Nuand LLC.
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

#include "conversions.h"

int str2version(const char *str, struct bladerf_version *version)
{
    unsigned long tmp;
    const char *start = str;
    char *end;

    /* Major version */
    errno = 0;
    tmp = strtoul(start, &end, 10);
    if (errno != 0 || tmp > UINT16_MAX || end == start || *end != '.') {
        return -1;
    }
    version->major = (uint16_t)tmp;

    /* Minor version */
    if (end[0] == '\0' || end[1] == '\0') {
        return -1;
    }
    errno = 0;
    start = &end[1];
    tmp = strtoul(start, &end, 10);
    if (errno != 0 || tmp > UINT16_MAX || end == start || *end != '.') {
        return -1;
    }
    version->minor = (uint16_t)tmp;

    /* Patch version */
    if (end[0] == '\0' || end[1] == '\0') {
        return -1;
    }
    errno = 0;
    start = &end[1];
    tmp = strtoul(start, &end, 10);
    if (errno != 0 || tmp > UINT16_MAX || end == start ||
            (*end != '-' && *end != '\0')) {
        return -1;
    }
    version->patch = (uint16_t)tmp;

    version->describe = str;

    return 0;
}

const char * devspeed2str(bladerf_dev_speed speed)
{
    switch (speed) {
        case BLADERF_DEVICE_SPEED_HIGH:
            /* Yeah, the USB IF actually spelled it "Hi" instead of "High".
             * I know. It hurts me too. */
            return "Hi-Speed";

        case BLADERF_DEVICE_SPEED_SUPER:
            /* ...and no hyphen :( */
            return "SuperSpeed";

        default:
            return "Unknown";
    }
}

bladerf_log_level str2loglevel(const char *str, bool *ok)
{
    bladerf_log_level level = BLADERF_LOG_LEVEL_ERROR;
    bool valid = true;

    if (!strcasecmp(str, "critical")) {
        level = BLADERF_LOG_LEVEL_CRITICAL;
    } else if (!strcasecmp(str, "error")) {
        level = BLADERF_LOG_LEVEL_ERROR;
    } else if (!strcasecmp(str, "warning")) {
        level = BLADERF_LOG_LEVEL_WARNING;
    } else if (!strcasecmp(str, "info")) {
        level = BLADERF_LOG_LEVEL_INFO;
    } else if (!strcasecmp(str, "debug")) {
        level = BLADERF_LOG_LEVEL_DEBUG;
    } else if (!strcasecmp(str, "verbose")) {
        level = BLADERF_LOG_LEVEL_VERBOSE;
    } else {
        valid = false;
    }

    *ok = valid;
    return level;
}

const char * module2str(bladerf_module m)
{
    switch (m) {
        case BLADERF_MODULE_RX:
            return "RX";
        case BLADERF_MODULE_TX:
            return "TX";
        default:
            return "Unknown";
    }
}

bladerf_module str2module(const char *str)
{
    if (!strcasecmp(str, "RX")) {
        return BLADERF_MODULE_RX;
    } else if (!strcasecmp(str, "TX")) {
        return BLADERF_MODULE_TX;
    } else {
        return BLADERF_MODULE_INVALID;
    }
}

const char * trigger2str(bladerf_trigger_signal trigger)
{
    switch (trigger) {
        case BLADERF_TRIGGER_J71_4:
            return "J71-4";

        case BLADERF_TRIGGER_USER_0:
            return "User-0";

        case BLADERF_TRIGGER_USER_1:
            return "User-1";

        case BLADERF_TRIGGER_USER_2:
            return "User-2";

        case BLADERF_TRIGGER_USER_3:
            return "User-3";

        case BLADERF_TRIGGER_USER_4:
            return "User-4";

        case BLADERF_TRIGGER_USER_5:
            return "User-5";

        case BLADERF_TRIGGER_USER_6:
            return "User-6";

        case BLADERF_TRIGGER_USER_7:
            return "User-7";

        default:
            return "Unknown";
    }
}

bladerf_trigger_signal str2trigger(const char *str)
{
    if (!strcasecmp("J71-4", str)) {
        return BLADERF_TRIGGER_J71_4;
    } else if (!strcasecmp("User-0", str)) {
        return BLADERF_TRIGGER_USER_0;
    } else if (!strcasecmp("User-1", str)) {
        return BLADERF_TRIGGER_USER_1;
    } else if (!strcasecmp("User-2", str)) {
        return BLADERF_TRIGGER_USER_2;
    } else if (!strcasecmp("User-3", str)) {
        return BLADERF_TRIGGER_USER_3;
    } else if (!strcasecmp("User-4", str)) {
        return BLADERF_TRIGGER_USER_4;
    } else if (!strcasecmp("User-5", str)) {
        return BLADERF_TRIGGER_USER_5;
    } else if (!strcasecmp("User-6", str)) {
        return BLADERF_TRIGGER_USER_6;
    } else if (!strcasecmp("User-7", str)) {
        return BLADERF_TRIGGER_USER_7;
    } else {
        return BLADERF_TRIGGER_INVALID;
    }
}

const char *triggerrole2str(bladerf_trigger_role role)
{
    switch (role) {
        case BLADERF_TRIGGER_ROLE_MASTER:
            return "Master";
        case BLADERF_TRIGGER_ROLE_SLAVE:
            return "Slave";
        case BLADERF_TRIGGER_ROLE_DISABLED:
            return "Disabled";
        default:
            return "Unknown";
    }
}

bladerf_trigger_role str2triggerrole(const char *str) {
    if (!strcasecmp("Master", str)) {
        return BLADERF_TRIGGER_ROLE_MASTER;
    } else if (!strcasecmp("Slave", str)) {
        return BLADERF_TRIGGER_ROLE_SLAVE;
    } else if (!strcasecmp("Disabled", str) || !strcasecmp("Off", str)) {
        return BLADERF_TRIGGER_ROLE_DISABLED;
    } else {
        return BLADERF_TRIGGER_ROLE_INVALID;
    }
}

int str2loopback(const char *str, bladerf_loopback *loopback)
{
    int status = 0;

    if (!strcasecmp("bb_txlpf_rxvga2", str)) {
        *loopback = BLADERF_LB_BB_TXLPF_RXVGA2;
    } else if (!strcasecmp("bb_txlpf_rxlpf", str)) {
        *loopback = BLADERF_LB_BB_TXLPF_RXLPF;
    } else if (!strcasecmp("bb_txvga1_rxvga2", str)) {
        *loopback = BLADERF_LB_BB_TXVGA1_RXVGA2;
    } else if (!strcasecmp("bb_txvga1_rxlpf", str)) {
        *loopback = BLADERF_LB_BB_TXVGA1_RXLPF;
    } else if (!strcasecmp("rf_lna1", str)) {
        *loopback = BLADERF_LB_RF_LNA1;
    } else if (!strcasecmp("rf_lna2", str)) {
        *loopback = BLADERF_LB_RF_LNA2;
    } else if (!strcasecmp("rf_lna3", str)) {
        *loopback = BLADERF_LB_RF_LNA3;
    } else if (!strcasecmp("none", str)) {
        *loopback = BLADERF_LB_NONE;
    } else {
        status = -1;
    }

    return status;
}

int str2lnagain(const char *str, bladerf_lna_gain *gain)
{
    *gain = BLADERF_LNA_GAIN_MAX;

    if (!strcasecmp("max", str) ||
        !strcasecmp("BLADERF_LNA_GAIN_MAX", str)) {
        *gain = BLADERF_LNA_GAIN_MAX;
        return 0;
    } else if (!strcasecmp("mid", str) ||
               !strcasecmp("BLADERF_LNA_GAIN_MID", str)) {
        *gain = BLADERF_LNA_GAIN_MID;
        return 0;
    } else if (!strcasecmp("bypass", str) ||
               !strcasecmp("BLADERF_LNA_GAIN_BYPASS", str)) {
        *gain = BLADERF_LNA_GAIN_BYPASS;
        return 0;
    } else {
        *gain = BLADERF_LNA_GAIN_UNKNOWN;
        return -1;
    }
}

const char *backend_description(bladerf_backend b)
{
    switch (b) {
        case BLADERF_BACKEND_ANY:
            return "Any";

        case BLADERF_BACKEND_LINUX:
            return "Linux kernel driver";

        case BLADERF_BACKEND_LIBUSB:
            return "libusb";

        case BLADERF_BACKEND_CYPRESS:
            return "Cypress driver";

        case BLADERF_BACKEND_DUMMY:
            return "Dummy";

        default:
            return "Unknown";
    }
}

void sc16q11_to_float(const int16_t *in, float *out, unsigned int n)
{
    unsigned int i;

    for (i = 0; i < (2 * n); i += 2) {
        out[i]   = (float) in[i]   * (1.0f / 2048.0f);
        out[i+1] = (float) in[i+1] * (1.0f / 2048.0f);
    }
}

void float_to_sc16q11(const float *in, int16_t *out, unsigned int n)
{
    unsigned int i;

    for (i = 0; i < (2 * n); i += 2) {
        out[i]   = (int16_t) (in[i]   * 2048.0f);
        out[i+1] = (int16_t) (in[i+1] * 2048.0f);
    }
}

bladerf_cal_module str_to_bladerf_cal_module(const char *str)
{
    bladerf_cal_module module = BLADERF_DC_CAL_INVALID;

    if (!strcasecmp(str, "lpf_tuning") || !strcasecmp(str, "lpftuning") ||
        !strcasecmp(str, "tuning")) {
        module = BLADERF_DC_CAL_LPF_TUNING;
    } else if (!strcasecmp(str, "tx_lpf")  || !strcasecmp(str, "txlpf")) {
        module = BLADERF_DC_CAL_TX_LPF;
    } else if (!strcasecmp(str, "rx_lpf")  || !strcasecmp(str, "rxlpf")) {
        module = BLADERF_DC_CAL_RX_LPF;
    } else if (!strcasecmp(str, "rx_vga2") || !strcasecmp(str, "rxvga2")) {
        module = BLADERF_DC_CAL_RXVGA2;
    }

    return module;
}

const char * smb_mode_to_str(bladerf_smb_mode mode)
{
    switch (mode) {
        case BLADERF_SMB_MODE_DISABLED:
            return "Disabled";

        case BLADERF_SMB_MODE_OUTPUT:
            return "Output";

        case BLADERF_SMB_MODE_INPUT:
            return "Input";

        case BLADERF_SMB_MODE_UNAVAILBLE:
            return "Unavailable";

        default:
            return "Unknown";
    }
};

bladerf_smb_mode str_to_smb_mode(const char *str)
{
    if (!strcasecmp(str, "disabled") || !strcasecmp(str, "off")) {
        return BLADERF_SMB_MODE_DISABLED;
    } else if (!strcasecmp(str, "output")) {
        return BLADERF_SMB_MODE_OUTPUT;
    } else if (!strcasecmp(str, "input")) {
        return BLADERF_SMB_MODE_INPUT;
    } else if (!strcasecmp(str, "unavailable")) {
        return BLADERF_SMB_MODE_UNAVAILBLE;
    } else {
        return BLADERF_SMB_MODE_INVALID;
    }
}
