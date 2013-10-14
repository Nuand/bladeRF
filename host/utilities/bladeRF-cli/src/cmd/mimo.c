#include <stdio.h>
#include "common.h"

int cmd_mimo(struct cli_state *state, int argc, char **argv)
{
    int status = 0;

    if (state->dev == NULL) {
        printf("  No device is currently opened\n");
        return 0;
    }

	if (argc >= 2) {
		if (argc >= 3 && !strcmp(argv[1], "clk")) {
			if (!strcmp(argv[2], "slave")) {
				status |= bladerf_si5338_write(state->dev, 6, 4);
				status |= bladerf_si5338_write(state->dev, 28, 0x2b);
				status |= bladerf_si5338_write(state->dev, 29, 0x28);
				status |= bladerf_si5338_write(state->dev, 30, 0xa8);
				if (status)
					printf("Could not set device to slave MIMO mode\n");
				else
					printf("Successfully set device to slave MIMO mode\n");
			} else if (!strcmp(argv[2], "master")) {
				status |= bladerf_si5338_write(state->dev, 39, 1);
				status |= bladerf_si5338_write(state->dev, 34, 0x22);
				if (status)
					printf("Could not set device to master MIMO mode\n");
				else
					printf("Successfully set device to master MIMO mode\n");
			}
		}
	}
    return status;
}
