#include "cmd.h"

/* Todo move to cmd_probe.c */
int cmd_probe(struct cli_state *s, int argc, char *argv[])
{
    struct bladerf_devinfo *devices = NULL;
    ssize_t n_devices, i;

    if ((n_devices = bladerf_get_device_list(&devices)) < 0) {
        cli_err(s, argv[0], "Failed to probe for devices");
        s->last_lib_error = n_devices;
        return CMD_RET_LIBBLADERF;
    }

    printf("\n");
    for (i = 0; i < n_devices; i++) {
/*        printf("    Path: %s\n", devices[i].path);
        printf("    Serial: 0x%016lX\n", devices[i].serial);
        printf("    Firmware: v%d.%d\n", devices[i].fw_ver_maj,
               devices[i].fw_ver_min);

        if (devices[i].fpga_configured) {
            printf("    FPGA: v%d.%d\n",
                    devices[i].fpga_ver_maj, devices[i].fpga_ver_min);
        } else {
            printf("    FPGA: not configured\n");
        }
*/
        printf("Kind of stubbed out for now\n");
    }

    bladerf_free_device_list(devices, n_devices);

    return 0;
}
