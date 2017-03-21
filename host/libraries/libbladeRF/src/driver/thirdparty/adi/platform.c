#include <stdint.h>
#include <unistd.h>

#include "board/board.h"

#include "platform.h"

/* Bitwise positions of GPIOs in configuration register */
#define CONFIG_GPIO_RESETB  0
#define CONFIG_GPIO_SYNC    1

/***************************************************************************//**
 * @brief spi_init
*******************************************************************************/

int spi_init(struct ad9361_rf_phy *phy, void *userdata)
{
    phy->spi->userdata = userdata;
    return 0;
}

/***************************************************************************//**
 * @brief spi_write_then_read
*******************************************************************************/

int spi_write_then_read(struct spi_device *spi,
                        const unsigned char *txbuf, unsigned n_tx,
                        unsigned char *rxbuf, unsigned n_rx)
{
    const unsigned int num = max(n_tx, n_rx) - 2;
    struct bladerf *dev = spi->userdata;
    uint16_t cmd;
    uint8_t buf[num];
    int status;

    /* Reconstruct 16-bit cmd from txbuf */
    cmd = (txbuf[0] << 8) | (txbuf[1]);

    /* Copy txbuf to buf */
    for (unsigned int i = 2; i < n_tx - 2; i++) {
        buf[i-2] = txbuf[i];
    }

    /* SPI transaction */
    status = dev->backend->ad9361_spi(dev, cmd, buf, num);
    if (status < 0) {
        return - EIO;
    }

    /* Copy buf to rxbuf */
    for (unsigned int i = 0; i < n_rx; i++) {
        rxbuf[i] = buf[i];
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

bool gpio_is_valid(struct gpio_device *gpio, int32_t number)
{
    if (number == CONFIG_GPIO_RESETB) {
        return true;
    } else if (number == CONFIG_GPIO_SYNC) {
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
    status = dev->backend->config_gpio_read(dev, &reg);
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
    status = dev->backend->config_gpio_write(dev, reg);
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
