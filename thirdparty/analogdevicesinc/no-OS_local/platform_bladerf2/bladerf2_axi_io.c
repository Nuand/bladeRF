/*
 * no_os_axi_io implementation for the bladeRF2.
 *
 * The no-OS axi_adc/axi_dac cores expect memory-mapped register access
 * through no_os_axi_io_read/write(base, offset, ...). On this board the ADI
 * AXI interface core lives behind the NIOS (NIOS_PKT_32x32_TARGET_ADI_AXI),
 * so the accessors forward to the backend adi_axi_read/write. The API has no
 * context argument, so the device handle is a file-static set by
 * bladerf2_axi_io_set_dev() right before ad9361_init() (mirrors how the SPI
 * and GPIO descriptors receive the handle through .extra).
 */
#include <errno.h>
#include <stdint.h>

#include "board/board.h"

#include "no_os_axi_io.h"

static struct bladerf *axi_io_dev = NULL;

void bladerf2_axi_io_set_dev(struct bladerf *dev)
{
    axi_io_dev = dev;
}

int32_t no_os_axi_io_read(uint32_t base, uint32_t offset, uint32_t *data)
{
    if (NULL == axi_io_dev || NULL == data) {
        return -ENODEV;
    }

    if (axi_io_dev->backend->adi_axi_read(axi_io_dev, base + offset, data) < 0) {
        return -EIO;
    }

    return 0;
}

int32_t no_os_axi_io_write(uint32_t base, uint32_t offset, uint32_t data)
{
    if (NULL == axi_io_dev) {
        return -ENODEV;
    }

    if (axi_io_dev->backend->adi_axi_write(axi_io_dev, base + offset, data) < 0) {
        return -EIO;
    }

    return 0;
}
