/*
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2015-2016 Nuand LLC
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
#include "conversions.h"
#include <string.h>

int print_trigger(struct cli_state *state,
                  bladerf_module module, bladerf_trigger_signal signal)
{
    int status;
    struct bladerf_trigger t;
    bool master, armed, fired, fire_req;
    const char *sig_str, *role_str, *module_str;

    status = bladerf_trigger_init(state->dev, module, signal, &t);
    if (status != 0) {
        goto out;
    }

    status = bladerf_trigger_state(state->dev, &t,
                                   &armed, &fired, &fire_req, NULL, NULL);
    if (status != 0) {
        goto out;
    }

    printf("\n");

    master = (t.role == BLADERF_TRIGGER_ROLE_MASTER);

    role_str    = triggerrole2str(t.role);
    sig_str     = trigger2str(signal);
    module_str  = module2str(module);

    printf("  %s %s trigger\n", sig_str, module_str);
    printf("    State:          %s\n", armed ? "Armed" : "Disarmed");
    printf("    Role:           %s\n", role_str);

    if (master) {
        printf("    Fire Requested: %s\n", fire_req ? "Yes" : "No");
    }

    printf("    Fired:          %s\n", fired ? "Yes" : "No");

    printf("\n");

out:
    if (status != 0) {
        state->last_lib_error = status;
        return CLI_RET_LIBBLADERF;
    }

    return status;
}

static inline int config_trigger(struct cli_state *state,
                                 bladerf_module module,
                                 bladerf_trigger_signal signal,
                                 const char *cmd, const char *argv0)
{
    int status;
    struct bladerf_trigger trigger;
    bool armed, fired, fire_req, master;
    const char *module_str, *sig_str;


    /* Dissect current value to print certain warnings */
    status = bladerf_trigger_init(state->dev, module, signal, &trigger);
    if (status != 0) {
        goto out;
    }

    status = bladerf_trigger_state(state->dev, &trigger,
                                   &armed, &fired, &fire_req, NULL, NULL);
    if (status != 0) {
        goto out;
    }

    master      = (trigger.role == BLADERF_TRIGGER_ROLE_MASTER);

    module_str  = module2str(module);
    sig_str     = trigger2str(signal);


    if (!strcasecmp(cmd, "off") || !strcasecmp(cmd, "disable") || !strcasecmp(cmd, "disabled")) {

        /* Disarm trigger and restore it to a slave role */
        trigger.role = BLADERF_TRIGGER_ROLE_DISABLED;
        status = bladerf_trigger_arm(state->dev, &trigger, false, 0, 0);
        if (status != 0) {
            goto out;
        }

        printf("\n %s %s trigger disabled\n\n", sig_str, module_str);

    } else if (!strcasecmp(cmd, "slave")) {
        if (fired) {
            printf("\n  WARNING: Trigger signal is already asserted; trigger will fire immediately.\n");
        }

        if (master && fire_req) {
            printf("\n  WARNING: Fire request on this master was active. Changing from master to slave will stop other triggered slaves.\n");
        }

        trigger.role = BLADERF_TRIGGER_ROLE_SLAVE;
        status = bladerf_trigger_arm(state->dev, &trigger, true, 0, 0);
        if (status != 0) {
            goto out;
        }

        printf("\n  %s %s trigger armed as a slave.\n\n",
               sig_str, module_str);

    } else if (!strcasecmp(cmd, "master")) {
        if (armed && !master) {
            printf("\n  WARNING: Changing from slave to master. Ensure there is only one master in the trigger chain!\n");
        }

        trigger.role = BLADERF_TRIGGER_ROLE_MASTER;
        status = bladerf_trigger_arm(state->dev, &trigger, true, 0, 0);
        if (status != 0) {
            goto out;
        }

        printf("\n  %s %s trigger armed as master.\n\n",
               sig_str, module_str);

    } else if (!strcasecmp(cmd, "fire")) {
        if (!(armed && master)) {
            cli_err(state, argv0,
                    "%s %s trigger not configured as master, can't fire\n",
                    sig_str, module_str);

            return CLI_RET_INVPARAM;
        }

        if (fired) {
            printf("\n  WARNING: Trigger already fired. Ignoring fire request.\n\n");
            return 0;
        }

        status = bladerf_trigger_fire(state->dev, &trigger);
        if (status != 0) {
            goto out;
        }

        printf("\n %s %s trigger fire request submitted successfully.\n\n",
               sig_str, module_str);

    } else {
        cli_err(state, cmd, "Invalid trigger command.\n");
        return CLI_RET_INVPARAM;
    }

out:
    if (status != 0) {
        state->last_lib_error = status;
        status = CLI_RET_LIBBLADERF;
    }

    return status;
}

static inline int print_all_triggers(struct cli_state *state)
{
    int status;
    char const *board_name = bladerf_get_board_name(state->dev);
    bladerf_trigger_signal signal;

    if (strcmp(board_name, "bladerf1") == 0) {
        signal = BLADERF_TRIGGER_J71_4;
    } else if (strcmp(board_name, "bladerf2") == 0) {
        signal = BLADERF_TRIGGER_J51_1;
    } else {
        cli_err(state, "print_all_triggers", "Unknown board name");
        return CLI_RET_INVPARAM;
    }

    status = print_trigger(state, BLADERF_MODULE_RX, signal);
    if (status != 0) {
        return status;
    }

    status = print_trigger(state, BLADERF_MODULE_TX, signal);

    return status;
}

int cmd_trigger(struct cli_state *state, int argc, char **argv)
{
    int status = 0;
    bladerf_module m;
    bladerf_trigger_signal s;

    switch (argc) {
        case 1:
            status = print_all_triggers(state);
            break;

        case 3:
        case 4:
            s = str2trigger(argv[1]);
            if (s == BLADERF_TRIGGER_INVALID) {
                cli_err(state, "Invalid trigger signal", "%s\n", argv[1]);
                return CLI_RET_INVPARAM;
            }

            m = str2module(argv[2]);
            if (m == BLADERF_MODULE_INVALID) {
                cli_err(state, "Invalid module", "%s\n", argv[2]);
                return CLI_RET_INVPARAM;
            }

            if (argc == 3) {
                status = print_trigger(state, m, s);
            } else {
                status = config_trigger(state, m, s, argv[3], argv[0]);
            }
            break;

        default:
            status = CLI_RET_NARGS;
    }

    return status;
}
