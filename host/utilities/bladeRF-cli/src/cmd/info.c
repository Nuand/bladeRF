#include <stdio.h>
#include "cmd.h"

int cmd_info(struct cli_state *state, int argc, char **argv)
{
    int status;
    bladerf_fpga_size fpga_size;
    uint16_t dac_trim;
    bool fpga_loaded;
    struct bladerf_devinfo info;

    if (state->dev == NULL) {
        printf("  No device is currently opened\n");
        return 0;
    }

    status = bladerf_get_devinfo(state->dev, &info);
    if (status < 0) {
        state->last_lib_error = status;
        return CMD_RET_LIBBLADERF;
    }

    status = bladerf_is_fpga_configured(state->dev);
    if (status < 0) {
        state->last_lib_error = status;
        return CMD_RET_LIBBLADERF;
    }
    fpga_loaded = status != 0;

    status = bladerf_get_fpga_size(state->dev, &fpga_size);
    if (status < 0) {
        state->last_lib_error = status;
        return CMD_RET_LIBBLADERF;
    }

    status = bladerf_get_vctcxo_trim(state->dev, &dac_trim);
    if (status < 0) {
        state->last_lib_error = status;
        return CMD_RET_LIBBLADERF;
    }

    printf("\n");
    printf("  Serial #:                 %s\n", info.serial);
    printf("  VCTCXO DAC calibration:   0x%.4x\n", dac_trim);
    if (fpga_size != 0) {
        printf("  FPGA size:                %d KLE\n", fpga_size);
    } else {
        printf("  FPGA size:                Unknown\n");
    }
    printf("  FPGA loaded:              %s\n", fpga_loaded ? "yes" : "no");
    printf("  USB bus:                  %d\n", info.usb_bus);
    printf("  USB address:              %d\n", info.usb_addr);

    switch(info.backend) {
        case BLADERF_BACKEND_LIBUSB:
            printf("  Backend:                  libusb\n");
            break;

        case BLADERF_BACKEND_LINUX:
            printf("  Backend:                  Linux driver\n");
            break;

        default:
            printf("  Backend:                  Unknown\n");
    }
    printf("  Instance:                 %d\n", info.instance);

    printf("\n");
    return 0;
}
