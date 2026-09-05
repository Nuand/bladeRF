#include <stdint.h>

#include "board/board.h"

#include "platform.h"


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
 * @brief no_os_udelay, no_os_mdelay
 *
 * The current driver revision calls the delays through no_os_delay.h. Only the
 * names and the argument width changed, so these forward to the existing
 * implementations rather than duplicating them.
*******************************************************************************/

void no_os_udelay(uint32_t usecs)
{
    udelay(usecs);
}

void no_os_mdelay(uint32_t msecs)
{
    mdelay(msecs);
}

/***************************************************************************//**
 * @brief msleep_interruptible
*******************************************************************************/

unsigned long msleep_interruptible(unsigned int msecs)
{
    usleep(msecs * 1000);
    return 0;
}

/*
 * The axiadc_* accessors that used to live here are gone: they served the AXI
 * ADC/DAC cores, which this board does not have. config.h now defines
 * AXI_ADC_NOT_PRESENT, so the driver never calls into that path, and
 * adc_core.c / dac_core.c are out of the build for the same reason.
 */
