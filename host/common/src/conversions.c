/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (c) 2017-2018 Nuand LLC.
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
    tmp   = strtoul(start, &end, 10);
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
    tmp   = strtoul(start, &end, 10);
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
    tmp   = strtoul(start, &end, 10);
    if (errno != 0 || tmp > UINT16_MAX || end == start ||
        (*end != '-' && *end != '\0')) {
        return -1;
    }
    version->patch = (uint16_t)tmp;

    version->describe = str;

    return 0;
}

const char *devspeed2str(bladerf_dev_speed speed)
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
    bool valid              = true;

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

const char *module2str(bladerf_module m)
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

const char *channel2str(bladerf_channel ch)
{
    switch (ch) {
        case BLADERF_CHANNEL_RX(0):
            return "RX1";
        case BLADERF_CHANNEL_TX(0):
            return "TX1";
        case BLADERF_CHANNEL_RX(1):
            return "RX2";
        case BLADERF_CHANNEL_TX(1):
            return "TX2";
        default:
            return "Unknown";
    }
}

bladerf_channel str2channel(char const *str)
{
    bladerf_channel rv;

    if (strcasecmp(str, "rx") == 0 || strcasecmp(str, "rx1") == 0) {
        rv = BLADERF_CHANNEL_RX(0);
    } else if (strcasecmp(str, "rx2") == 0) {
        rv = BLADERF_CHANNEL_RX(1);
    } else if (strcasecmp(str, "tx") == 0 || strcasecmp(str, "tx1") == 0) {
        rv = BLADERF_CHANNEL_TX(0);
    } else if (strcasecmp(str, "tx2") == 0) {
        rv = BLADERF_CHANNEL_TX(1);
    } else {
        rv = BLADERF_CHANNEL_INVALID;
    }

    return rv;
}

const char *direction2str(bladerf_direction dir)
{
    switch (dir) {
        case BLADERF_RX:
            return "RX";
        case BLADERF_TX:
            return "TX";
        default:
            return "Unknown";
    }
}

const char *trigger2str(bladerf_trigger_signal trigger)
{
    switch (trigger) {
        case BLADERF_TRIGGER_J71_4:
            return "J71-4";

        case BLADERF_TRIGGER_J51_1:
            return "J51-1";

        case BLADERF_TRIGGER_MINI_EXP_1:
            return "MiniExp-1";

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
    } else if (!strcasecmp("J51-1", str)) {
        return BLADERF_TRIGGER_J51_1;
    } else if (!strcasecmp("Miniexp-1", str)) {
        return BLADERF_TRIGGER_MINI_EXP_1;
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

bladerf_trigger_role str2triggerrole(const char *str)
{
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

    if (!strcasecmp("firmware", str)) {
        *loopback = BLADERF_LB_FIRMWARE;
    } else if (!strcasecmp("bb_txlpf_rxvga2", str)) {
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
    } else if (!strcasecmp("rfic_bist", str)) {
        *loopback = BLADERF_LB_RFIC_BIST;
    } else if (!strcasecmp("none", str)) {
        *loopback = BLADERF_LB_NONE;
    } else {
        status = -1;
    }

    return status;
}

char const *loopback2str(bladerf_loopback loopback)
{
    switch (loopback) {
        case BLADERF_LB_BB_TXLPF_RXVGA2:
            return "bb_txlpf_rxvga2";

        case BLADERF_LB_BB_TXLPF_RXLPF:
            return "bb_txlpf_rxlpf";

        case BLADERF_LB_BB_TXVGA1_RXVGA2:
            return "bb_txvga1_rxvga2";

        case BLADERF_LB_BB_TXVGA1_RXLPF:
            return "bb_txvga1_rxlpf";

        case BLADERF_LB_RF_LNA1:
            return "rf_lna1";

        case BLADERF_LB_RF_LNA2:
            return "rf_lna2";

        case BLADERF_LB_RF_LNA3:
            return "rf_lna3";

        case BLADERF_LB_FIRMWARE:
            return "firmware";

        case BLADERF_LB_NONE:
            return "none";

        case BLADERF_LB_RFIC_BIST:
            return "rfic_bist";

        default:
            return "unknown";
    }
}

int str2lnagain(const char *str, bladerf_lna_gain *gain)
{
    *gain = BLADERF_LNA_GAIN_MAX;

    if (!strcasecmp("max", str) || !strcasecmp("BLADERF_LNA_GAIN_MAX", str)) {
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

char const *tuningmode2str(bladerf_tuning_mode mode)
{
    switch (mode) {
        case BLADERF_TUNING_MODE_HOST:
            return "Host";
        case BLADERF_TUNING_MODE_FPGA:
            return "FPGA";
        default:
            return "Unknown";
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
        out[i]     = (float)in[i] * (1.0f / 2048.0f);
        out[i + 1] = (float)in[i + 1] * (1.0f / 2048.0f);
    }
}

void float_to_sc16q11(const float *in, int16_t *out, unsigned int n)
{
    unsigned int i;

    for (i = 0; i < (2 * n); i += 2) {
        out[i]     = (int16_t)(in[i] * 2048.0f);
        out[i + 1] = (int16_t)(in[i + 1] * 2048.0f);
    }
}

bladerf_cal_module str_to_bladerf_cal_module(const char *str)
{
    bladerf_cal_module module = BLADERF_DC_CAL_INVALID;

    if (!strcasecmp(str, "lpf_tuning") || !strcasecmp(str, "lpftuning") ||
        !strcasecmp(str, "tuning")) {
        module = BLADERF_DC_CAL_LPF_TUNING;
    } else if (!strcasecmp(str, "tx_lpf") || !strcasecmp(str, "txlpf")) {
        module = BLADERF_DC_CAL_TX_LPF;
    } else if (!strcasecmp(str, "rx_lpf") || !strcasecmp(str, "rxlpf")) {
        module = BLADERF_DC_CAL_RX_LPF;
    } else if (!strcasecmp(str, "rx_vga2") || !strcasecmp(str, "rxvga2")) {
        module = BLADERF_DC_CAL_RXVGA2;
    }

    return module;
}

const char *smb_mode_to_str(bladerf_smb_mode mode)
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

bladerf_tuning_mode str_to_tuning_mode(const char *str)
{
    if (!strcasecmp(str, "fpga")) {
        return BLADERF_TUNING_MODE_FPGA;
    } else if (!strcasecmp(str, "host")) {
        return BLADERF_TUNING_MODE_HOST;
    } else {
        return BLADERF_TUNING_MODE_INVALID;
    }
}

unsigned int str2uint(const char *str,
                      unsigned int min,
                      unsigned int max,
                      bool *ok)
{
    unsigned int ret;
    char *optr;

    errno = 0;
    ret   = strtoul(str, &optr, 0);

    if (errno == ERANGE || (errno != 0 && ret == 0)) {
        *ok = false;
        return 0;
    }

    if (str == optr) {
        *ok = false;
        return 0;
    }

    if (ret >= min && ret <= max) {
        *ok = true;
        return (unsigned int)ret;
    }

    *ok = false;
    return 0;
}

int str2int(const char *str, int min, int max, bool *ok)
{
    long int ret;
    char *optr;

    errno = 0;
    ret   = strtol(str, &optr, 0);

    if ((errno == ERANGE && (ret == LONG_MAX || ret == LONG_MIN)) ||
        (errno != 0 && ret == 0)) {
        *ok = false;
        return 0;
    }

    if (str == optr) {
        *ok = false;
        return 0;
    }

    if (ret >= min && ret <= max) {
        *ok = true;
        return (int)ret;
    }

    *ok = false;
    return 0;
}

uint64_t str2uint64(const char *str, uint64_t min, uint64_t max, bool *ok)
{
    uint64_t ret;
    char *optr;

    errno = 0;
    ret   = (uint64_t)strtod(str, &optr);

    if ((errno == ERANGE && ret == ULONG_MAX) || (errno != 0 && ret == 0)) {
        *ok = false;
        return 0;
    }

    if (str == optr) {
        *ok = false;
        return 0;
    }

    if (ret >= min && ret <= max) {
        *ok = true;
        return ret;
    }

    *ok = false;
    return 0;
}

double str2double(const char *str, double min, double max, bool *ok)
{
    double ret;
    char *optr;

    errno = 0;
    ret   = strtod(str, &optr);

    if (errno == ERANGE || (errno != 0 && ret == 0)) {
        *ok = false;
        return NAN;
    }

    if (str == optr) {
        *ok = false;
        return NAN;
    }

    if (ret >= min && ret <= max) {
        *ok = true;
        return ret;
    }

    *ok = false;
    return NAN;
}

static uint64_t suffix_multiplier(const char *str,
                                  const struct numeric_suffix *suffixes,
                                  const size_t num_suff,
                                  bool *ok)
{
    unsigned i;

    *ok = true;

    if (!strlen(str))
        return 1;

    for (i = 0; i < num_suff; i++) {
        if (!strcasecmp(suffixes[i].suffix, str)) {
            return suffixes[i].multiplier;
        }
    }

    *ok = false;
    return 0;
}

unsigned int str2uint_suffix(const char *str,
                             unsigned int min,
                             unsigned int max,
                             const struct numeric_suffix *suffixes,
                             const size_t num_suff,
                             bool *ok)
{
    uint64_t mult;
    unsigned int rv;
    double val;
    char *optr;

    errno = 0;
    val   = strtod(str, &optr);

    if (errno == ERANGE || (errno != 0 && val == 0)) {
        *ok = false;
        return 0;
    }

    if (str == optr) {
        *ok = false;
        return 0;
    }

    mult = suffix_multiplier(optr, suffixes, num_suff, ok);
    if (!*ok)
        return false;

    rv = (unsigned int)(val * mult);

    if (rv >= min && rv <= max) {
        return rv;
    }

    *ok = false;
    return 0;
}

uint64_t str2uint64_suffix(const char *str,
                           uint64_t min,
                           uint64_t max,
                           const struct numeric_suffix *suffixes,
                           const size_t num_suff,
                           bool *ok)
{
    uint64_t mult, rv;
    long double val;
    char *optr;

    errno = 0;
    val   = strtold(str, &optr);

    if (errno == ERANGE && (errno != 0 && val == 0)) {
        *ok = false;
        return 0;
    }

    if (str == optr) {
        *ok = false;
        return 0;
    }

    mult = suffix_multiplier(optr, suffixes, num_suff, ok);
    if (!*ok)
        return false;

    rv = (uint64_t)(val * mult);
    if (rv >= min && rv <= max) {
        return rv;
    }

    *ok = false;
    return 0;
}

int str2bool(const char *str, bool *val)
{
    unsigned int i;

    char *str_true[]  = { "true", "t", "one", "1", "enable", "en", "on" };
    char *str_false[] = { "false", "f", "zero", "0", "disable", "dis", "off" };

    for (i = 0; i < ARRAY_SIZE(str_true); i++) {
        if (!strcasecmp(str_true[i], str)) {
            *val = true;
            return 0;
        }
    }

    for (i = 0; i < ARRAY_SIZE(str_false); i++) {
        if (!strcasecmp(str_false[i], str)) {
            *val = false;
            return 0;
        }
    }

    return BLADERF_ERR_INVAL;
}
