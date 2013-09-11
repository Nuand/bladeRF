/*
 * bladeRF flashing utility
 */
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <libbladeRF.h>
#include "version.h"


int main(int argc, char *argv[])
{
    /* Arguments:
     * - Firmware image
     * - Device string (optional)
     * - Load RAM only (option)
     * - Issue RESET before JUMP_TO_BOOTLOADER (option)
     */
    
    /* Get initial device or scan */
    /* If normal bladeRF, perform protocol to get to FX3 bootloader */
    /* Perform RAM bootloader */
    /* Optionally flash new image to SPI flash */

    return 0;
}
