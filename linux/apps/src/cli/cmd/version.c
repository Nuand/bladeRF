#include "cmd.h"
#include <stdio.h>

int cmd_version(struct cli_state *state, int argc, char **argv)
{
    int status;
    unsigned int fw_major, fw_minor;
    unsigned int fpga_major, fpga_minor;
    bool fpga_loaded = false;

    status = bladerf_is_fpga_configured(state->dev);
    if (status < 0) {
        return status;
    } else if (status) {
        fpga_loaded = true;
        status = bladerf_get_fpga_version(state->dev, &fpga_major, &fpga_minor);
        if (status < 0) {
            return status;
        }
    }

    status = bladerf_get_fw_version(state->dev, &fw_major, &fw_minor);
    if (status < 0) {
        return status;
    }

    printf("Firmware version: %u.%u\n", fw_major, fw_minor);
    if (fpga_loaded) {
        printf("FPGA version:     %u.%u\n", fpga_major, fpga_minor);
    } else {
        printf("FPGA version:     Unknown (FPGA not loaded)\n");
    }


    return CMD_RET_OK;
}

