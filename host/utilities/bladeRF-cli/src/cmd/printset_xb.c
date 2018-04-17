/*
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2013-2018 Nuand LLC
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
#include "cmd.h"
#include "printset.h"
#include <inttypes.h>

struct xb_gpio_lut {
    const char *name;
    uint32_t bitmask;
};

// clang-format off
static const struct xb_gpio_lut xb_pins[] = {
    { "GPIO_1",          BLADERF_XB_GPIO_01 },
    { "GPIO_2",          BLADERF_XB_GPIO_02 },
    { "GPIO_3",          BLADERF_XB_GPIO_03 },
    { "GPIO_4",          BLADERF_XB_GPIO_04 },
    { "GPIO_5",          BLADERF_XB_GPIO_05 },
    { "GPIO_6",          BLADERF_XB_GPIO_06 },
    { "GPIO_7",          BLADERF_XB_GPIO_07 },
    { "GPIO_8",          BLADERF_XB_GPIO_08 },
    { "GPIO_9",          BLADERF_XB_GPIO_09 },
    { "GPIO_10",         BLADERF_XB_GPIO_10 },
    { "GPIO_11",         BLADERF_XB_GPIO_11 },
    { "GPIO_12",         BLADERF_XB_GPIO_12 },
    { "GPIO_13",         BLADERF_XB_GPIO_13 },
    { "GPIO_14",         BLADERF_XB_GPIO_14 },
    { "GPIO_15",         BLADERF_XB_GPIO_15 },
    { "GPIO_16",         BLADERF_XB_GPIO_16 },
    { "GPIO_17",         BLADERF_XB_GPIO_17 },
    { "GPIO_18",         BLADERF_XB_GPIO_18 },
    { "GPIO_19",         BLADERF_XB_GPIO_19 },
    { "GPIO_20",         BLADERF_XB_GPIO_20 },
    { "GPIO_21",         BLADERF_XB_GPIO_21 },
    { "GPIO_22",         BLADERF_XB_GPIO_22 },
    { "GPIO_23",         BLADERF_XB_GPIO_23 },
    { "GPIO_24",         BLADERF_XB_GPIO_24 },
    { "GPIO_25",         BLADERF_XB_GPIO_25 },
    { "GPIO_26",         BLADERF_XB_GPIO_26 },
    { "GPIO_27",         BLADERF_XB_GPIO_27 },
    { "GPIO_28",         BLADERF_XB_GPIO_28 },
    { "GPIO_29",         BLADERF_XB_GPIO_29 },
    { "GPIO_30",         BLADERF_XB_GPIO_30 },
    { "GPIO_31",         BLADERF_XB_GPIO_31 },
    { "GPIO_32",         BLADERF_XB_GPIO_32 },
};

static const struct xb_gpio_lut xb200_pins[] = {
    { "J7_1",         BLADERF_XB200_PIN_J7_1 },
    { "J7_2",         BLADERF_XB200_PIN_J7_2 },
    { "J7_6",         BLADERF_XB200_PIN_J7_6 },
    { "J13_1",        BLADERF_XB200_PIN_J13_1 },
    { "J13_2",        BLADERF_XB200_PIN_J13_2 },
    { "J16_1",        BLADERF_XB200_PIN_J16_1 },
    { "J16_2",        BLADERF_XB200_PIN_J16_2 },
    { "J16_3",        BLADERF_XB200_PIN_J16_3 },
    { "J16_4",        BLADERF_XB200_PIN_J16_4 },
    { "J16_5",        BLADERF_XB200_PIN_J16_5 },
    { "J16_6",        BLADERF_XB200_PIN_J16_6 },
};

static const struct xb_gpio_lut xb100_pins[] = {
    { "J2_3",         BLADERF_XB100_PIN_J2_3 },
    { "J2_4",         BLADERF_XB100_PIN_J2_4 },
    { "J3_3",         BLADERF_XB100_PIN_J3_3 },
    { "J3_4",         BLADERF_XB100_PIN_J3_4 },
    { "J4_3",         BLADERF_XB100_PIN_J4_3 },
    { "J4_4",         BLADERF_XB100_PIN_J4_4 },
    { "J5_3",         BLADERF_XB100_PIN_J5_3 },
    { "J5_4",         BLADERF_XB100_PIN_J5_4 },
    { "J11_2",        BLADERF_XB100_PIN_J11_2 },
    { "J11_3",        BLADERF_XB100_PIN_J11_3 },
    { "J11_4",        BLADERF_XB100_PIN_J11_4 },
    { "J11_5",        BLADERF_XB100_PIN_J11_5 },
    { "J12_5",        BLADERF_XB100_PIN_J12_5 },
    { "LED_D1",       BLADERF_XB100_LED_D1 },
    { "LED_D2",       BLADERF_XB100_LED_D2 },
    { "LED_D3",       BLADERF_XB100_LED_D3 },
    { "LED_D4",       BLADERF_XB100_LED_D4 },
    { "LED_D5",       BLADERF_XB100_LED_D5 },
    { "LED_D6",       BLADERF_XB100_LED_D6 },
    { "LED_D7",       BLADERF_XB100_LED_D7 },
    { "LED_D8",       BLADERF_XB100_LED_D8 },
    { "TLED_RED",     BLADERF_XB100_TLED_RED },
    { "TLED_GREEN",   BLADERF_XB100_TLED_GREEN },
    { "TLED_BLUE",    BLADERF_XB100_TLED_BLUE },
    { "DIP_SW1",      BLADERF_XB100_DIP_SW1 },
    { "DIP_SW2",      BLADERF_XB100_DIP_SW2 },
    { "DIP_SW3",      BLADERF_XB100_DIP_SW3 },
    { "DIP_SW4",      BLADERF_XB100_DIP_SW4 },
    { "BTN_J6",       BLADERF_XB100_BTN_J6 },
    { "BTN_J7",       BLADERF_XB100_BTN_J7 },
    { "BTN_J8",       BLADERF_XB100_BTN_J8 },
};
// clang-format on

static int get_xb_lut(struct cli_state *state,
                      const struct xb_gpio_lut **lut,
                      size_t *len)
{
    bladerf_xb xb_type;
    int status;

    status = bladerf_expansion_get_attached(state->dev, &xb_type);
    if (status < 0) {
        return status;
    }

    switch (xb_type) {
        case BLADERF_XB_100:
            *lut = xb100_pins;
            *len = ARRAY_SIZE(xb100_pins);
            break;

        case BLADERF_XB_200:
            *lut = xb200_pins;
            *len = ARRAY_SIZE(xb200_pins);
            break;

        default:
            *lut = xb_pins;
            *len = ARRAY_SIZE(xb_pins);
    }

    return 0;
}

static int str2xbgpio(struct cli_state *state,
                      const char *str,
                      const struct xb_gpio_lut **io,
                      bool nnl_on_error)
{
    const struct xb_gpio_lut *lut = NULL;
    size_t lut_len;
    int status;
    size_t i;

    *io = NULL;

    status = get_xb_lut(state, &lut, &lut_len);
    if (status != 0) {
        state->last_lib_error = status;
        return CLI_RET_LIBBLADERF;
    }

    /* Should not occur - would be indicative of a bug */
    if (lut == NULL || lut_len == 0) {
        return CLI_RET_UNKNOWN;
    }

    for (i = 0; i < lut_len; i++) {
        if (!strcasecmp(str, lut[i].name)) {
            *io = &lut[i];
            return 0;
        }
    }

    return CLI_RET_INVPARAM;
}

static int print_xbio_base(struct cli_state *state,
                           int argc,
                           char **argv,
                           bool is_dir)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;

    const struct xb_gpio_lut *lut = NULL;
    const struct xb_gpio_lut *pin = NULL;

    size_t lut_len;
    uint32_t val;
    int status;
    size_t i;

    enum {
        PRINT_LIST,
        PRINT_ALL,
        PRINT_REGISTER,
        PRINT_PIN,
    } action;


    if (argc <= 2) {
        printf("Usage: print xb_gpio%s <name>\n\n", is_dir ? "_dir" : "");

        printf("<name> describes what to print. "
               "It may be one of the following:\n\n");

        printf("  \"all\" - Print the register value and the state of\n"
               "  individual pins. Note that the pin names shown\n"
               "  is dependent upon which expansion board has been\n"
               "  enabled.\n\n");

        printf(
            "  \"reg\" or \"register\" - Print the GPIO%s register value.\n\n",
            is_dir ? " direction" : "");

        printf("  \"list\" - Display available pins to print.\n\n");

        printf("  One of the pin names from the aforementioned list.\n");

        return 0;

    } else if (argc > 3) {
        return CLI_RET_NARGS;
    }

    if (is_dir) {
        status = bladerf_expansion_gpio_dir_read(state->dev, &val);
    } else {
        status = bladerf_expansion_gpio_read(state->dev, &val);
    }

    if (status < 0) {
        *err = status;
        return CLI_RET_LIBBLADERF;
    }

    status = get_xb_lut(state, &lut, &lut_len);
    if (status < 0) {
        *err = status;
        return CLI_RET_LIBBLADERF;
    }

    /* Should not occur - indicative of a bug */
    if (lut == NULL || lut_len == 0) {
        return CLI_RET_UNKNOWN;
    }

    if (!strcasecmp(argv[2], "all")) {
        action = PRINT_ALL;
    } else if (!strcasecmp(argv[2], "list")) {
        action = PRINT_LIST;
    } else if (!strcasecmp(argv[2], "reg") ||
               !strcasecmp(argv[2], "register")) {
        action = PRINT_REGISTER;
    } else {
        status = str2xbgpio(state, argv[2], &pin, true);
        if (status != 0) {
            cli_err_nnl(state, "Invalid pin name or option", "%s\n", argv[2]);
            return CLI_RET_INVPARAM;
        }

        action = PRINT_PIN;
    }

    if (action == PRINT_ALL || action == PRINT_REGISTER) {
        printf("  Expansion GPIO%s register: 0x%8.8x\n",
               is_dir ? " direction" : "", val);
    }

    if (action == PRINT_ALL) {
        printf("\n");

        for (i = 0; i < lut_len; i++) {
            uint32_t bit = (val & lut[i].bitmask) ? 1 : 0;

            if (is_dir) {
                printf("%12s: %s\n", lut[i].name, bit ? "output" : "input");
            } else {
                printf("%12s: %u\n", lut[i].name, bit);
            }
        }
    } else if (action == PRINT_LIST) {
        for (i = 0; i < lut_len; i++) {
            printf("%12s\n", lut[i].name);
        }
    } else if (action == PRINT_PIN) {
        uint32_t bit = (val & pin->bitmask) ? 1 : 0;

        if (is_dir) {
            printf("%12s: %s\n", pin->name, bit ? "output" : "input");
        } else {
            printf("%12s: %u\n", pin->name, bit);
        }
    }

    return rv;
}

int set_xbio_base(struct cli_state *state, int argc, char **argv, bool is_dir)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;

    const struct xb_gpio_lut *pin;
    const char *suffix = is_dir ? "_dir" : "";

    uint32_t val;
    bool ok;

    switch (argc) {
        case 0:
        case 1:
        case 2:
            printf("Usage: set xb_gpio%s <name> <value>\n\n", suffix);

            printf("<name> describes what to set. It may be one of the "
                   "following:\n\n");

            printf("  \"reg\" or \"register\" - Set the entire GPIO%s register "
                   "value.\n\n",
                   is_dir ? " direction" : "");

            printf("  One of the pin names listed by: \"print xb_gpio%s "
                   "list\"\n\n",
                   suffix);

            if (is_dir) {
                printf("Specify <value> as \"input\" (0) or \"output\" (1).\n");
            }

            break;

        case 4:
            /* Specifying a pin by name */
            rv = str2xbgpio(state, argv[2], &pin, false);
            if (rv == 0) {
                val = UINT32_MAX;

                if (is_dir) {
                    if (!strcasecmp(argv[3], "in") ||
                        !strcasecmp(argv[3], "input")) {
                        val = 0;
                    } else if (!strcasecmp(argv[3], "out") ||
                               !strcasecmp(argv[3], "output")) {
                        val = 1;
                    }
                }

                if (!is_dir || val == UINT32_MAX) {
                    val = str2uint(argv[3], 0, 1, &ok);
                    if (!ok) {
                        cli_err_nnl(state, "Invalid pin value", "%s\n",
                                    argv[3]);
                        return CLI_RET_INVPARAM;
                    }
                }

                if (val == 1) {
                    val = pin->bitmask;
                }

                if (is_dir) {
                    rv = bladerf_expansion_gpio_dir_masked_write(
                        state->dev, pin->bitmask, val);
                } else {
                    rv = bladerf_expansion_gpio_masked_write(state->dev,
                                                             pin->bitmask, val);
                }

                if (rv < 0) {
                    *err = rv;
                    rv   = CLI_RET_LIBBLADERF;
                } else {
                    rv = print_xbio_base(state, 3, argv, is_dir);
                }
            } else if (!strcasecmp("reg", argv[2]) ||
                       !strcasecmp("register", argv[2])) {
                /* Specifying a write of the entire XB GPIO (Direction) reg */
                val = str2uint(argv[3], 0, UINT32_MAX, &ok);
                if (!ok) {
                    cli_err_nnl(state, "Invalid register value", "%s\n",
                                argv[3]);
                    return CLI_RET_INVPARAM;
                }

                if (is_dir) {
                    rv = bladerf_expansion_gpio_dir_write(state->dev, val);
                } else {
                    rv = bladerf_expansion_gpio_write(state->dev, val);
                }

                if (rv < 0) {
                    *err = rv;
                    rv   = CLI_RET_LIBBLADERF;
                } else {
                    rv = print_xbio_base(state, 3, argv, is_dir);
                }

            } else {
                cli_err_nnl(state, "Invalid pin name or option", "%s\n",
                            argv[2]);
                rv = CLI_RET_INVPARAM;
            }
            break;


        default:
            rv = CLI_RET_NARGS;
    }

    return rv;
}

int print_xb_gpio(struct cli_state *state, int argc, char **argv)
{
    return print_xbio_base(state, argc, argv, false);
}

int set_xb_gpio(struct cli_state *state, int argc, char **argv)
{
    return set_xbio_base(state, argc, argv, false);
}

int print_xb_gpio_dir(struct cli_state *state, int argc, char **argv)
{
    return print_xbio_base(state, argc, argv, true);
}

int set_xb_gpio_dir(struct cli_state *state, int argc, char **argv)
{
    return set_xbio_base(state, argc, argv, true);
}

int print_xb_spi(struct cli_state *state, int argc, char **argv)
{
    cli_err_nnl(state, "Error", "XB SPI is currently write-only.\n");
    return CLI_RET_OK;
}

int set_xb_spi(struct cli_state *state, int argc, char **argv)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    unsigned int val;

    if (argc != 3) {
        rv = CLI_RET_NARGS;
    } else {
        bool ok;
        val = str2uint(argv[2], 0, -1, &ok);
        if (!ok) {
            cli_err_nnl(state, argv[0], "Invalid XB SPI value (%s)\n", argv[2]);
            rv = CLI_RET_INVPARAM;
        } else {
            int status = bladerf_xb_spi_write(state->dev, val);
            if (status < 0) {
                *err = status;
                rv   = CLI_RET_LIBBLADERF;
            }
        }
    }
    return rv;
}

/* gpio */
static void _print_gpio_bladerf1(uint32_t val)
{
    printf("    %-20s%-10s\n",
           "LMS Enable:", val & 0x01 ? "Enabled" : "Reset");  // Active low
    printf("    %-20s%-10s\n",
           "LMS RX Enable:", val & 0x02 ? "Enabled" : "Disabled");
    printf("    %-20s%-10s\n",
           "LMS TX Enable:", val & 0x04 ? "Enabled" : "Disabled");

    printf("    %-20s", "TX Band:");
    switch ((val >> 3) & 3) {
        case 2:
            printf("Low Band (300M - 1.5GHz)\n");
            break;
        case 1:
            printf("High Band (1.5GHz - 3.8GHz)\n");
            break;
        default:
            printf("Invalid Band Selection!\n");
            break;
    }

    printf("    %-20s", "RX Band:");
    switch ((val >> 5) & 3) {
        case 2:
            printf("Low Band (300M - 1.5GHz)\n");
            break;
        case 1:
            printf("High Band (1.5GHz - 3.8GHz)\n");
            break;
        default:
            printf("Invalid Band Selection!\n");
            break;
    }

    printf("    %-20s", "RX Source:");
    switch ((val & 0x300) >> 8) {
        case 0:
            printf("Baseband\n");
            break;
        case 1:
            printf("Internal 12-bit counter\n");
            break;
        case 2:
            printf("Internal 32-bit counter\n");
            break;
        case 3:
            printf("Whitened Entropy\n");
            break;
        default:
            printf("????\n");
            break;
    }
}

static void _print_gpio_bladerf2(uint32_t val)
{
    int usb_speed       = (val >> 7) & 0x01;   // 7
    int rx_mux_sel      = (val >> 8) & 0x07;   // 10 downto 8
    int adf_chip_enable = (val >> 11) & 0x01;  // 11
    int leds            = (val >> 12) & 0x07;  // 14 downto 12
    int led_mode        = (val >> 15) & 0x01;  // 15
    int meta_sync       = (val >> 16) & 0x01;  // 16
    int xb_mode         = (val >> 30) & 0x03;  // 31 downto 30

    printf("    %-20s%-10s\n", "USB Speed:", usb_speed ? "1" : "0");
    printf("    %-20s0x%08x ", "RX Mux:", rx_mux_sel);
    switch (rx_mux_sel) {
        case 0:
            printf("(Baseband)\n");
            break;
        case 1:
            printf("(Internal 12-bit counter)\n");
            break;
        case 2:
            printf("(Internal 32-bit counter)\n");
            break;
        case 3:
            printf("(Whitened Entropy)\n");
            break;
        default:
            printf("( ???? )\n");
            break;
    }

    printf("    %-20s%-10s\n", "ADF Chip Enable:", adf_chip_enable ? "1" : "0");
    printf("    %-20s%-10s\n", "LED Mode:", led_mode ? "Manual" : "Default");

    if (led_mode) {
        printf("        %-16s%-10s\n", "LED 1:", leds & 0x01 ? "On" : "Off");
        printf("        %-16s%-10s\n", "LED 2:", leds & 0x02 ? "On" : "Off");
        printf("        %-16s%-10s\n", "LED 3:", leds & 0x04 ? "On" : "Off");
    }

    printf("    %-20s%-10s\n", "Meta Sync:", meta_sync ? "1" : "0");
    printf("    %-20s0x%08x\n", "XB Mode:", xb_mode);
}

int print_gpio(struct cli_state *state, int argc, char **argv)
{
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;
    int status;

    uint32_t val;

    status = bladerf_config_gpio_read(state->dev, &val);
    if (status < 0) {
        *err = status;
        rv   = CLI_RET_LIBBLADERF;
    }

    if (rv == CLI_RET_OK) {
        printf("  GPIO: 0x%8.8x\n", val);

        if (ps_is_board(state->dev, BOARD_BLADERF1)) {
            _print_gpio_bladerf1(val);
        }

        if (ps_is_board(state->dev, BOARD_BLADERF2)) {
            _print_gpio_bladerf2(val);
        }
    }

    return rv;
}

int set_gpio(struct cli_state *state, int argc, char **argv)
{
    /* set gpio <value> */
    int rv   = CLI_RET_OK;
    int *err = &state->last_lib_error;

    uint32_t val;
    bool ok;

    if (argc == 3) {
        val = str2uint(argv[2], 0, UINT_MAX, &ok);
        if (!ok) {
            cli_err_nnl(state, argv[0], "Invalid gpio value (%s)\n", argv[2]);
            rv = CLI_RET_INVPARAM;
        } else {
            rv = bladerf_config_gpio_write(state->dev, val);
            if (rv != 0) {
                *err = rv;
                rv   = CLI_RET_LIBBLADERF;
            } else {
                rv = print_gpio(state, argc, argv);
            }
        }
    } else {
        rv = CLI_RET_NARGS;
    }
    return rv;
}
