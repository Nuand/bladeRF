#include <stdio.h>
#include "common.h"
#include "conversions.h"
#include <string.h>

int print_trigger(struct cli_state *state,
                  bladerf_module module, bladerf_trigger_signal trigger)
{
    int status = 0;
    uint8_t trigger_value;
    bool armed, fire, master, linestate;

    status = bladerf_read_trigger(state->dev, module, trigger, &trigger_value);
    if (status != 0) {
        return status;
    }

    armed = (trigger_value & BLADERF_TRIGGER_REG_ARM);
    fire = (trigger_value & BLADERF_TRIGGER_REG_FIRE);
    master = (trigger_value & BLADERF_TRIGGER_REG_MASTER);
    linestate = (trigger_value & BLADERF_TRIGGER_REG_LINE);

    printf("  %s %s trigger register value: 0x%02x\n",
           trigger2str(trigger), module2str(module), trigger_value);
    if (!armed) {
        printf("   |- Trigger system is disabled\n");
    } else {
        printf("   |- Trigger system is enabled\n");
        if (!master) {
            printf("   |- Trigger configured as slave\n");
        } else {
            printf("   |- Trigger configured as master\n");
            if (!fire) {
                printf("   |- Fire request is not set\n");
            } else {
                printf("   |- Fire request is set\n");
            }
        }
        if (linestate) {
            printf("   |- mini_exp1 line HIGH, trigger NOT FIRED\n");
        } else {
            printf("   |- mini_exp1 line LOW, trigger FIRED\n");
        }
    }
    return 0;
}

static inline int config_trigger(struct cli_state *state,
                                 bladerf_module module,
                                 bladerf_trigger_signal trigger,
                                 const char *cmd, const char *argv0)
{
    int status;
    uint8_t trigger_value;
    bool armed, fire, master, linestate;

    /* Dissect current value to print certain warnings */
    status = bladerf_read_trigger(state->dev, module, trigger, &trigger_value);
    if (status != 0) {
        goto out;
    }

    armed       = (trigger_value & BLADERF_TRIGGER_REG_ARM);
    fire        = (trigger_value & BLADERF_TRIGGER_REG_FIRE);
    master      = (trigger_value & BLADERF_TRIGGER_REG_MASTER);
    linestate   = (trigger_value & BLADERF_TRIGGER_REG_LINE);

    if (!strcasecmp(cmd, "off")) {
        /* Disarm trigger */
        status = bladerf_write_trigger(state->dev, module, trigger, 0x00);
        if (status != 0) {
            goto out;
        }

        printf(" %s %s trigger successfully disabled\n",
               trigger2str(trigger), module2str(module));

    } else if (!strcasecmp(cmd, "slave")) {
        if (!linestate) {
            printf("  WARNING: Trigger signal is already asserted; trigger will fire immediately\n");
        }

        if (master && fire) {
            printf("  WARNING: Fire request on master was active, triggered slaves will stop.\n");
        }

        status = bladerf_write_trigger(state->dev, module, trigger,
                                       BLADERF_TRIGGER_REG_ARM);
        if (status != 0) {
            goto out;
        }

        printf("  %s %s trigger successfully configured as slave\n",
               trigger2str(trigger), module2str(module));

    } else if (!strcasecmp(cmd, "master")) {
        if (armed && !master) {
            printf("  WARNING: Changing from slave to master. Ensure there is only one master in the trigger chain.\n");
        }

        status = bladerf_write_trigger(state->dev, module, trigger,
                                       BLADERF_TRIGGER_REG_ARM | BLADERF_TRIGGER_REG_MASTER);
        if (status != 0) {
            goto out;
        }

        printf(" %s %s trigger successfully configured as master.\n",
               trigger2str(trigger), module2str(module));

    } else if (!strcasecmp(cmd, "fire")) {
        if (!(armed && master)) {
            cli_err(state, argv0,
                    "%s %s trigger not configured as master, can't fire\n",
                    trigger2str(trigger), module2str(module));
            return CLI_RET_CMD_HANDLED;
        }

        if (fire) {
            printf("  WARNING: Trigger already fired, ignoring request\n");
            goto out;
        }

        status = bladerf_write_trigger(state->dev, module, trigger,
                                       trigger_value | BLADERF_TRIGGER_REG_FIRE);
        if (status != 0) {
            goto out;
        }

        printf("  Successfully set fire-request on %s-trigger\n",
               module2str(module));

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

    status = print_trigger(state, BLADERF_MODULE_RX, BLADERF_TRIGGER_J71_4);
    if (status != 0) {
        goto out;
    }

    status = print_trigger(state, BLADERF_MODULE_TX, BLADERF_TRIGGER_J71_4);

out:
    if (status != 0) {
        state->last_lib_error = status;
        status = CLI_RET_LIBBLADERF;
    }

    return status;
}

int cmd_trigger(struct cli_state *state, int argc, char **argv)
{
    int status = 0;
    bladerf_module m;
    bladerf_trigger_signal t;

    switch (argc) {
        case 1:
            status = print_all_triggers(state);
            break;

        case 3:
        case 4:
            m = str2module(argv[1]);
            if (m == BLADERF_MODULE_INVALID) {
                cli_err(state, argv[1], "Invalid module\n");
                return CLI_RET_INVPARAM;
            }

            t = str2trigger(argv[2]);
            if (t == BLADERF_TRIGGER_INVALID) {
                cli_err(state, argv[2], "Invalid trigger\n");
                return CLI_RET_INVPARAM;
            }

            if (argc == 3) {
                status = print_trigger(state, m, t);
            } else {
                status = config_trigger(state, m, t, argv[3], argv[0]);
            }
            break;

        default:
            status = CLI_RET_NARGS;
    }

    return status;
}
