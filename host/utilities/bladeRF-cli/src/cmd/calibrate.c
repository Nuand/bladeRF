#include <stdio.h>
#include <string.h>
#include <libbladeRF.h>

#include "common.h"
#include "cmd.h"

int cmd_calibrate(struct cli_state *state, int argc, char **argv)
{
    /* Valid commands:
        calibrate [module]
    */
    int rv = CMD_RET_OK;
    if (!cli_device_is_opened(state)) {
        return CMD_RET_NODEV;
    }

    if( argc == 1 ) {
        /* Calibrate LPF Tuning Module */
        bladerf_calibrate_dc(state->dev, BLADERF_DC_CAL_LPF_TUNING);

        /* Calibrate TX LPF Filter */
        bladerf_calibrate_dc(state->dev, BLADERF_DC_CAL_TX_LPF);

        /* Calibrate RX LPF Filter */
        bladerf_calibrate_dc(state->dev, BLADERF_DC_CAL_RX_LPF);

        /* Calibrate RX VGA2 */
        bladerf_calibrate_dc(state->dev, BLADERF_DC_CAL_RXVGA2);

    } else if( argc == 2 ) {
        bladerf_cal_module module;
        /* Figure out which module we are calibrating */
        if (strcasecmp(argv[1], "tuning") == 0) {
            module = BLADERF_DC_CAL_LPF_TUNING;
        } else if (strcasecmp(argv[1], "txlpf") == 0) {
            module = BLADERF_DC_CAL_TX_LPF;
        } else if (strcasecmp(argv[1], "rxlpf") == 0) {
            module = BLADERF_DC_CAL_RX_LPF;
        } else if (strcasecmp(argv[1], "rxvga2") == 0) {
            module = BLADERF_DC_CAL_RXVGA2;
        } else {
            cli_err(state, argv[0], "Invalid module provided (%s)", argv[1]);
            return CMD_RET_INVPARAM;
        }
        /* Calibrate it */
        bladerf_calibrate_dc(state->dev, module);
    } else {
        return CMD_RET_INVPARAM;
    }

    return rv ;
}
