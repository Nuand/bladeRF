/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2016 Nuand LLC
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "log.h"

#include "smb_clock.h"
#include "driver/si5338.h"
#include "board/board.h"

struct regvals {
    uint8_t addr;
    uint8_t data;
};

/* Power-on defaults with SMB clock port not in use */
static const struct regvals default_config[] = {
    {  6, 0x08 },
    { 28, 0x0b },
    { 29, 0x08 },
    { 30, 0xb0 },
    { 34, 0xe3 },
    { 39, 0x00 },

    /* Reset Multisynth 3 */
    { 86, 0x00 },
    { 87, 0x00 },
    { 88, 0x00 },
    { 89, 0x00 },
    { 90, 0x00 },
    { 91, 0x00 },
    { 92, 0x00 },
    { 93, 0x00 },
    { 94, 0x00 },
    { 95, 0x00 },
};

static const struct regvals input_config[] = {
    {  6, 0x04 },
    { 28, 0x2b },
    { 29, 0x28 },
    { 30, 0xa8 },
};

static const struct regvals output_config[] = {
    { 34, 0x22 },
};

static int write_regs(struct bladerf *dev, const struct regvals *reg, size_t n)
{
    size_t i;
    int status = 0;

    for (i = 0; i < n && status == 0; i++) {
        status = dev->backend->si5338_write(dev, reg[i].addr, reg[i].data);
    }

    return status;
}

static inline int smb_mode_input(struct bladerf *dev)
{
    int status;
    uint8_t val;

    status = write_regs(dev, input_config, ARRAY_SIZE(input_config));
    if (status != 0) {
        return status;
    }

    /* Turn off any SMB connector output */
    status = dev->backend->si5338_read(dev, 39, &val);
    if (status != 0) {
        return status;
    }

    val &= ~(1);
    status = dev->backend->si5338_write(dev, 39, val);
    if (status != 0) {
        return status;
    }

    return status;
}

static inline int smb_mode_output(struct bladerf *dev)
{
    int status;
    uint8_t val;

    status = dev->backend->si5338_read(dev, 39, &val);
    if (status != 0) {
        return status;
    }

    val |= 1;
    status = dev->backend->si5338_write(dev, 39, val);
    if (status != 0) {
        return status;
    }

    status = write_regs(dev, output_config, ARRAY_SIZE(output_config));
    if (status != 0) {
        return status;
    }

    return status;
}

static inline int smb_mode_disabled(struct bladerf *dev)
{
    return write_regs(dev, default_config, ARRAY_SIZE(default_config));
}

int smb_clock_set_mode(struct bladerf *dev, bladerf_smb_mode mode)
{
    int status;

    /* Reset initial state */
    status = smb_mode_disabled(dev);
    if (status != 0) {
        return status;
    }

    /* Apply changes */
    switch (mode) {
        case BLADERF_SMB_MODE_DISABLED:
            break;

        case BLADERF_SMB_MODE_OUTPUT:
            status = smb_mode_output(dev);
            break;

        case BLADERF_SMB_MODE_INPUT:
            status = smb_mode_input(dev);
            break;

        default:
            log_debug("Invalid SMB clock port mode: %d\n", mode);
            return BLADERF_ERR_INVAL;
    }

    return status;
}

/* For simplicity, we just check a few bits that are indicative of our known
 * register configurations for our SMB clock port configurations.
 *
 * Inconsistent register states (or users manually changing registers) may
 * cause this function to erroneously report the state.
 */
int smb_clock_get_mode(struct bladerf *dev, bladerf_smb_mode *mode)
{
    int status;
    uint8_t val;

    /* Check DRV3_FMT[2:0] for an output configuration */
    status = dev->backend->si5338_read(dev, 39, &val);
    if (status != 0) {
        return status;
    }

    switch (val & 0x7) {
        case 0x00: /* No output */
            break;

        case 0x01: /* CLK3A CMOS/SSTL/HSTL - we're outputting a clock */
            *mode = BLADERF_SMB_MODE_OUTPUT;
            return 0;

        case 0x02: /* CLK3B CMOS/SSTL/HSTL - used by the XB-200. */
            *mode = BLADERF_SMB_MODE_UNAVAILBLE;
            return 0;

        default:
            *mode = BLADERF_SMB_MODE_INVALID;
            log_debug("Si5338[39] in unexpected state: 0x%02x\n", val);
            return BLADERF_ERR_UNSUPPORTED;
    }

    /* Check P2DIV_IN[0] for an input configuration */
    status = dev->backend->si5338_read(dev, 28, &val);
    if (status != 0) {
        return status;
    }

    if ((val & (1 << 5)) != 0) {
        *mode = BLADERF_SMB_MODE_INPUT;
    } else {
        *mode = BLADERF_SMB_MODE_DISABLED;
    }

    return status;
}
