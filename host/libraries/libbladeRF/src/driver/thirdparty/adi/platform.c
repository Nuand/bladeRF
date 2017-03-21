#include <stdint.h>
#include <unistd.h>

#include "board/board.h"

#include "platform.h"

/***************************************************************************//**
 * @brief spi_init
*******************************************************************************/

int spi_init(struct ad9361_rf_phy *phy, void *userdata)
{
    return 0;
}

/***************************************************************************//**
 * @brief spi_write_then_read
*******************************************************************************/

int spi_write_then_read(struct spi_device *spi,
                        const unsigned char *txbuf, unsigned n_tx,
                        unsigned char *rxbuf, unsigned n_rx)
{
	return 0;
}

/***************************************************************************//**
 * @brief gpio_init
*******************************************************************************/

int gpio_init(struct ad9361_rf_phy *phy, void *userdata)
{
    return 0;
}

/***************************************************************************//**
 * @brief gpio_is_valid
*******************************************************************************/

bool gpio_is_valid(struct gpio_device *gpio, int32_t number)
{
	return 0;
}

/***************************************************************************//**
 * @brief gpio_set_value
*******************************************************************************/

int gpio_set_value(struct gpio_device *gpio, int32_t number, bool value)
{

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
