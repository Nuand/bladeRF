#include <stdint.h>

#include "board/board.h"

#include "platform.h"

#include "adc_core.h"
#include "dac_core.h"

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
    unsigned int i;

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

/***************************************************************************//**
 * @brief axiadc_init
*******************************************************************************/

int axiadc_init(struct ad9361_rf_phy *phy, void *userdata)
{
    int status;

    phy->adc_state->userdata = userdata;

    status = adc_init(phy);
    if (status < 0) {
        return status;
    }

    status = dac_init(phy, DATA_SEL_DMA, 0);
    if (status < 0) {
        return status;
    }

    return 0;
}

/***************************************************************************//**
 * @brief axiadc_post_setup
*******************************************************************************/

int axiadc_post_setup(struct ad9361_rf_phy *phy)
{
    return ad9361_post_setup(phy);
}

/***************************************************************************//**
 * @brief axiadc_read
*******************************************************************************/

int axiadc_read(struct axiadc_state *st, uint32_t addr, uint32_t *data)
{
    struct bladerf *dev = st->userdata;
    int status;

    /* Read */
    status = dev->backend->adi_axi_read(dev, addr, data);
    if (status < 0) {
        return -EIO;
    }

    return 0;
}

/***************************************************************************//**
 * @brief axiadc_write
*******************************************************************************/

int axiadc_write(struct axiadc_state *st, uint32_t addr, uint32_t data)
{
    struct bladerf *dev = st->userdata;
    int status;

    /* Write */
    status = dev->backend->adi_axi_write(dev, addr, data);
    if (status < 0) {
        return -EIO;
    }

    return 0;
}

/***************************************************************************//**
 * @brief axiadc_set_pnsel
*******************************************************************************/

int axiadc_set_pnsel(struct axiadc_state *st, unsigned int channel, enum adc_pn_sel sel)
{
    int status;
    uint32_t reg;

    if (PCORE_VERSION_MAJOR(st->pcore_version) > 7) {
        status = axiadc_read(st, ADI_REG_CHAN_CNTRL_3(channel), &reg);
        if (status != 0)
            return status;

        reg &= ~ADI_ADC_PN_SEL(~0);
        reg |= ADI_ADC_PN_SEL(sel);

        status = axiadc_write(st, ADI_REG_CHAN_CNTRL_3(channel), reg);
        if (status != 0)
            return status;
    } else {
        status = axiadc_read(st, ADI_REG_CHAN_CNTRL(channel), &reg);
        if (status != 0)
            return status;

        if (sel == ADC_PN_CUSTOM) {
            reg |= ADI_PN_SEL;
        } else if (sel == ADC_PN9) {
            reg &= ~ADI_PN23_TYPE;
            reg &= ~ADI_PN_SEL;
        } else {
            reg |= ADI_PN23_TYPE;
            reg &= ~ADI_PN_SEL;
        }

        status = axiadc_write(st, ADI_REG_CHAN_CNTRL(channel), reg);
        if (status != 0)
            return status;
    }

    return 0;
}

/***************************************************************************//**
 * @brief axiadc_idelay_set
*******************************************************************************/

int axiadc_idelay_set(struct axiadc_state *st, unsigned int lane, unsigned int val)
{
    int status;

    if (PCORE_VERSION_MAJOR(st->pcore_version) > 8) {
        status = axiadc_write(st, ADI_REG_DELAY(lane), val);
        if (status != 0)
            return status;
    } else {
        status = axiadc_write(st, ADI_REG_DELAY_CNTRL, 0);
        if (status != 0)
            return status;

        status = axiadc_write(st, ADI_REG_DELAY_CNTRL,
                                ADI_DELAY_ADDRESS(lane) | ADI_DELAY_WDATA(val) | ADI_DELAY_SEL);
        if (status != 0)
            return status;
    }

    return 0;
}
