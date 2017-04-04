#include <stdint.h>
#include <unistd.h>

#include "board/board.h"

#include "platform.h"

/***************************************************************************//**
 * @brief spi_init
*******************************************************************************/

int spi_init(struct ad9361_rf_phy *phy, void *userdata)
{
    phy->spi->userdata = userdata;
    return 0;
}

/***************************************************************************//**
 * @brief spi_write
*******************************************************************************/

int spi_write(struct spi_device *spi, uint16_t cmd, const uint8_t *buf,
              unsigned int len)
{
    struct bladerf *dev = spi->userdata;
    int status;
    uint64_t data;
    unsigned i;

    /* Copy buf to data */
    data = 0;
    for (i = 0; i < len; i++) {
        data |= (((uint64_t)buf[i]) << 8*(7-i));
    }

    /* SPI transaction */
    status = dev->backend->ad9361_spi_write(dev, cmd, data);
    if (status < 0) {
        return -EIO;
    }

    return 0;
}

/***************************************************************************//**
 * @brief spi_read
*******************************************************************************/

#include <inttypes.h>

int spi_read(struct spi_device *spi, uint16_t cmd, uint8_t *buf,
             unsigned int len)
{
    struct bladerf *dev = spi->userdata;
    int status;
    uint64_t data = 0;
    unsigned int i;

    /* SPI transaction */
    status = dev->backend->ad9361_spi_read(dev, cmd, &data);
    if (status < 0) {
        return -EIO;
    }

    /* Copy data to buf */
    for (i = 0; i < len; i++) {
        buf[i] = (data >> 8*(7-i)) & 0xff;
    }

    return 0;
}

/***************************************************************************//**
 * @brief gpio_init
*******************************************************************************/

int gpio_init(struct ad9361_rf_phy *phy, void *userdata)
{
    phy->gpio->userdata = userdata;
    return 0;
}

/***************************************************************************//**
 * @brief gpio_is_valid
*******************************************************************************/

/* Bitwise positions of GPOs in RFFE control register */
#define RFFE_CONTROL_RESET_N  0
#define RFFE_CONTROL_SYNC_IN  4

bool gpio_is_valid(struct gpio_device *gpio, int32_t number)
{
    if (number == RFFE_CONTROL_RESET_N) {
        return true;
    } else if (number == RFFE_CONTROL_SYNC_IN) {
        return true;
    }

    return false;
}

/***************************************************************************//**
 * @brief gpio_set_value
*******************************************************************************/

int gpio_set_value(struct gpio_device *gpio, int32_t number, bool value)
{
    struct bladerf *dev = gpio->userdata;
    int status;
    uint32_t reg;

    /* Read */
    status = dev->backend->rffe_control_read(dev, &reg);
    if (status < 0) {
        return -EIO;
    }

    /* Modify */
    if (value) {
        reg |= (1 << number);
    } else {
        reg &= ~(1 << number);
    }

    /* Write */
    status = dev->backend->rffe_control_write(dev, reg);
    if (status < 0) {
        return -EIO;
    }

    return 0;
}

/***************************************************************************//**
 * @brief udelay
*******************************************************************************/

void udelay(unsigned long usecs)
{
    usleep(usecs);
}

/***************************************************************************//**
 * @brief mdelay
*******************************************************************************/

void mdelay(unsigned long msecs)
{
    usleep(msecs * 1000);
}

/***************************************************************************//**
 * @brief msleep_interruptible
*******************************************************************************/

unsigned long msleep_interruptible(unsigned int msecs)
{
    usleep(msecs * 1000);
    return 0;
}
