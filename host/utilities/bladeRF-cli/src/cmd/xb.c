/*
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2013 Nuand LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <stdio.h>
#include "common.h"
#include <string.h>

int cmd_xb(struct cli_state *state, int argc, char **argv)
{
    int status = 0;

    if (state->dev == NULL) {
        printf("  No device is currently opened\n");
        return 0;
    }

    if (argc >= 2) {
      if (argc >= 3) {
        // xb enable 200
        if (!strcmp(argv[1], "enable")) {
          if (!strcmp(argv[2], "200")) {
            printf(" Enabling transverter board\n");
            status = bladerf_expansion_attach(state->dev, BLADERF_XB_200);
            if (status != 0) {
              state->last_lib_error = status;
              return CLI_RET_LIBBLADERF;
            }
            printf(" Transverter board successfully enabled\n");
          }
        }
        // xb filter 200 xx yy  - where xx is tx or rx and yy is the filter to be selected
        else if ( (5 == argc) && !strcmp(argv[1], "filter") && !strcmp(argv[2], "200") ) {
          bladerf_xb200_filter filter;
          bladerf_module mod;

          // get module to set filter for
          if (!strcmp(argv[3], "rx"))
            mod = BLADERF_MODULE_RX;
          else if (!strcmp(argv[3], "tx"))
            mod = BLADERF_MODULE_TX;
          else return CLI_RET_INVPARAM;

          // get which filter to set
          if (!strcmp(argv[4], "50"))
            filter = BLADERF_XB200_50M;
          else if (!strcmp(argv[4], "144"))
            filter = BLADERF_XB200_144M;
          else if (!strcmp(argv[4], "222"))
            filter = BLADERF_XB200_222M;
          else if (!strcmp(argv[4], "custom"))
            filter = BLADERF_XB200_CUSTOM;
          else if (!strcmp(argv[4], "auto_1db"))
            filter = BLADERF_XB200_AUTO_1DB;
          else if (!strcmp(argv[4], "auto_3db"))
            filter = BLADERF_XB200_AUTO_3DB;
          else return CLI_RET_INVPARAM;

          status = bladerf_xb200_set_filterbank(state->dev, mod, filter);

          if (status != 0) {
            state->last_lib_error = status;
            return CLI_RET_LIBBLADERF;
          }
          printf(" Transverter board %s filter successfully set to %s\n", argv[3], argv[4]);
        }
      }
    }

    return status;
}
