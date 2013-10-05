#include "cmd.h"
#include "version.h"
#include <stdio.h>

int cmd_version(struct cli_state *state, int argc, char **argv)
{
    int status;
    char serial[BLADERF_SERIAL_LENGTH] = { 0 };
    bladerf_fpga_size fpga_size;
    uint16_t dac_trim;

    struct bladerf_version fw_version, fpga_version, lib_version;
    bool fpga_loaded = false;

    bladerf_version(&lib_version);

    printf("\n");
    printf("  bladeRF-cli version:        " BLADERF_CLI_VERSION "\n");
    printf("  libbladeRF version:         %s\n", lib_version.describe);
    printf("\n");

    if (state->dev == NULL) {
        printf("  No device attached to retrive version information from.\n\n");
        return 0;
    }

    status = bladerf_is_fpga_configured(state->dev);
    if (status < 0) {
        return status;
    } else if (status) {
        fpga_loaded = true;
        status = bladerf_fpga_version(state->dev, &fpga_version);
        if (status < 0) {
            return status;
        }
    }

    status = bladerf_fw_version(state->dev, &fw_version);
    if (status < 0) {
        return status;
    }

    status = bladerf_get_serial(state->dev, serial);
    if (status < 0) {
        return status;
    }

    status = bladerf_get_fpga_size(state->dev, &fpga_size);
    if (status < 0) {
        return status;
    }

    status = bladerf_get_vctcxo_trim(state->dev, &dac_trim);
    if (status < 0) {
        return status;
    }

    printf("  Firmware version:           %s\n", fw_version.describe);

    if (fpga_loaded) {
        printf("  FPGA version:               %s\n", fpga_version.describe);
    } else {
        printf("  FPGA version:               Unknown (FPGA not loaded)\n");
    }

    /* TODO: Move these into an info command */
    printf("  Serial #:                   %s\n", serial);
    printf("  VCTCXO DAC calibration:     0x%.4x\n", dac_trim);
    if (fpga_size != 0) {
        printf("  FPGA size:                  %d KLE\n", fpga_size);
    } else {
        printf("  FPGA size:                  Unknown\n");
    }
    printf("\n");


    return CMD_RET_OK;
}

