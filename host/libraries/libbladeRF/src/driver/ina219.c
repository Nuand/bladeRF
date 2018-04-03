/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2017 Nuand LLC
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

#include <libbladeRF.h>

#include "log.h"

#include "ina219.h"

#define INA219_REG_CONFIGURATION    0x00
#define INA219_REG_SHUNT_VOLTAGE    0x01
#define INA219_REG_BUS_VOLTAGE      0x02
#define INA219_REG_POWER            0x03
#define INA219_REG_CURRENT          0x04
#define INA219_REG_CALIBRATION      0x05

int ina219_init(struct bladerf *dev, float r_shunt)
{
    int status;
    uint16_t value;

    /* Soft-reset INA219 */
    value = 0x8000;
    status = dev->backend->ina219_write(dev, INA219_REG_CONFIGURATION, value);
    if (status < 0) {
        log_error("INA219 soft reset error: %d\n", status);
        return status;
    }

    /* Poll until we're out of reset */
    while (value & 0x8000) {
        status = dev->backend->ina219_read(dev, INA219_REG_CONFIGURATION, &value);
        if (status < 0) {
            log_error("INA219 soft reset poll error: %d\n", status);
            return status;
        }
    }

    /* Write configuration register */
    /* BRNG   (13) = 0 for 16V FSR
       PG  (12-11) = 00 for 40mV
       BADC (10-7) = 0011 for 12-bit / 532uS
       SADC  (6-3) = 0011 for 12-bit / 532uS
       MODE  (2-0) = 111 for continuous shunt & bus */
    value = 0x019f;
    status = dev->backend->ina219_write(dev, INA219_REG_CONFIGURATION, value);
    if (status < 0) {
        log_error("INA219 configuration error: %d\n", status);
        return status;
    }

    log_debug("Configuration register: 0x%04x\n", value);

    /* Write calibration register */
    /* Current_LSB = 0.001 A / LSB */
    /* Calibration = 0.04096 / (Current_LSB * r_shunt) */
    value = (uint16_t)((0.04096 / (0.001 * r_shunt)) + 0.5);
    status = dev->backend->ina219_write(dev, INA219_REG_CALIBRATION, value);
    if (status < 0) {
        log_error("INA219 calibration error: %d\n", status);
        return status;
    }

    log_debug("Calibration register: 0x%04x\n", value);

    return 0;
}

int ina219_read_shunt_voltage(struct bladerf *dev, float *voltage)
{
    int status;
    uint16_t data;

    status = dev->backend->ina219_read(dev, INA219_REG_SHUNT_VOLTAGE, &data);
    if (status < 0) {
        return status;
    }

    /* Scale by 1e-5 LSB / Volt */
    *voltage = ((float)((int16_t)data)) * 1e-5F;

    return 0;
}

int ina219_read_bus_voltage(struct bladerf *dev, float *voltage)
{
    int status;
    uint16_t data;

    status = dev->backend->ina219_read(dev, INA219_REG_BUS_VOLTAGE, &data);
    if (status < 0) {
        return status;
    }

    /* If overflow flag is set */
    if (data & 0x1) {
        return BLADERF_ERR_UNEXPECTED;
    }

    /* Scale by 0.004 LSB / Volt */
    *voltage = ((float)(data >> 3)) * 0.004F;

    return 0;
}

int ina219_read_current(struct bladerf *dev, float *current)
{
    int status;
    uint16_t data;

    status = dev->backend->ina219_read(dev, INA219_REG_CURRENT, &data);
    if (status < 0) {
        return status;
    }

    /* Scale by 0.001 LSB / Ampere */
    *current = ((float)((int16_t)data)) * 0.001F;

    return 0;
}

int ina219_read_power(struct bladerf *dev, float *power)
{
    int status;
    uint16_t data;

    status = dev->backend->ina219_read(dev, INA219_REG_POWER, &data);
    if (status < 0) {
        return status;
    }

    /* Scale by 0.020 LSB / Watt */
    *power = ((float)((int16_t)data)) * 0.020F;

    return 0;
}
