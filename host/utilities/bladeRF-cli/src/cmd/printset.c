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
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include "cmd.h"
#include "conversions.h"

#define PRINTSET_DECL(x) int print_##x(struct cli_state *, int, char **); int set_##x(struct cli_state *, int, char **);
#define PRINTSET_ENTRY(x) { \
        FIELD_INIT(.print, print_##x), \
        FIELD_INIT(.set, set_##x), \
        FIELD_INIT(.name, #x) \
}

/* An entry in the printset table */
struct printset_entry {
    int (*print)(struct cli_state *s, int argc, char **argv);  /*<< Function pointer to thing that prints parameter */
    int (*set)(struct cli_state *s, int argc, char **argv);    /*<< Function pointer to setter */
    const char *name;                   /*<< String associated with parameter */
};

static const struct numeric_suffix FREQ_SUFFIXES[] = {
    { "G",      1000 * 1000 * 1000 },
    { "GHz",    1000 * 1000 * 1000 },
    { "M",      1000 * 1000 },
    { "MHz",    1000 * 1000 },
    { "k",      1000 } ,
    { "kHz",    1000 }
};

static const int NUM_FREQ_SUFFIXES =
        sizeof(FREQ_SUFFIXES) / sizeof(struct numeric_suffix);

/* Declarations */
PRINTSET_DECL(bandwidth)
PRINTSET_DECL(config)
PRINTSET_DECL(frequency)
PRINTSET_DECL(gpio)
PRINTSET_DECL(lmsregs)
PRINTSET_DECL(lna)
PRINTSET_DECL(lnagain)
PRINTSET_DECL(loopback)
PRINTSET_DECL(mimo)
PRINTSET_DECL(pa)
PRINTSET_DECL(pps)
PRINTSET_DECL(refclk)
PRINTSET_DECL(rxvga1)
PRINTSET_DECL(rxvga2)
PRINTSET_DECL(samplerate)
PRINTSET_DECL(sampling)
PRINTSET_DECL(trimdac)
PRINTSET_DECL(xb_spi)
PRINTSET_DECL(xb_gpio)
PRINTSET_DECL(xb_gpio_dir)
PRINTSET_DECL(txvga1)
PRINTSET_DECL(txvga2)

/* print/set parameter table */
struct printset_entry printset_table[] = {
    PRINTSET_ENTRY(bandwidth),
    PRINTSET_ENTRY(config),
    PRINTSET_ENTRY(frequency),
    PRINTSET_ENTRY(gpio),
    PRINTSET_ENTRY(lmsregs),
    PRINTSET_ENTRY(lna),
    PRINTSET_ENTRY(lnagain),
    PRINTSET_ENTRY(loopback),
    PRINTSET_ENTRY(mimo),
    PRINTSET_ENTRY(pa),
    PRINTSET_ENTRY(pps),
    PRINTSET_ENTRY(refclk),
    PRINTSET_ENTRY(rxvga1),
    PRINTSET_ENTRY(rxvga2),
    PRINTSET_ENTRY(samplerate),
    PRINTSET_ENTRY(sampling),
    PRINTSET_ENTRY(trimdac),
    PRINTSET_ENTRY(xb_spi),
    PRINTSET_ENTRY(xb_gpio),
    PRINTSET_ENTRY(xb_gpio_dir),
    PRINTSET_ENTRY(txvga1),
    PRINTSET_ENTRY(txvga2),

    /* End of table marked by entry with NULL/empty fields */
    { FIELD_INIT(.print, NULL), FIELD_INIT(.set, NULL), FIELD_INIT(.name, "") }
};

bladerf_module get_module( char *mod, bool *ok )
{
    bladerf_module rv = BLADERF_MODULE_RX;
    *ok = true;
    if( strcasecmp( mod, "rx" ) == 0 ) {
        rv = BLADERF_MODULE_RX;
    } else if( strcasecmp( mod, "tx" ) == 0 ) {
        rv = BLADERF_MODULE_TX;
    } else {
        *ok = false;
    }
    return rv;
}

static inline void invalid_module(struct cli_state *s, const char *cmd,
                                  const char *module)
{
    cli_err(s, cmd, "Invalid module (%s)", module);
}

static inline void invalid_gain(struct cli_state *s, const char *cmd,
                                const char *param, const char *gain)
{
    cli_err(s, cmd, "Invalid gain setting for %s (%s)", param, gain);
}

static int is_fpga_configured(struct cli_state *state, const char *cmd)
{
    int status;

    status = bladerf_is_fpga_configured(state->dev);
    if (status < 0) {
        cli_err(state, cmd, "Failed to determine if the FPGA is "
                            "configured. Is the FX3 programmed?");

        state->last_lib_error = status;
        status = CMD_RET_LIBBLADERF;
    }

    return status;
}

int print_bandwidth(struct cli_state *state, int argc, char **argv)
{
    /* Usage: print bandwidth [rx|tx]*/
    int rv = CMD_RET_OK, status;
    bladerf_module module = BLADERF_MODULE_RX ;
    unsigned int bw ;

    if( argc == 3 ) {
        bool ok;
        module = get_module( argv[2], &ok );
        if( !ok ) {
            invalid_module(state, argv[0], argv[2]);
            return CMD_RET_INVPARAM;
        }
    } else if (argc != 2) {
        /* Assume both RX & TX if not specified */
        return CMD_RET_NARGS;
    }

    printf( "\n" ) ;

    if( argc == 2 || module == BLADERF_MODULE_RX ) {
        status =  bladerf_get_bandwidth( state->dev, BLADERF_MODULE_RX, &bw );
        if (status < 0) {
            state->last_lib_error = status;
            rv = CMD_RET_LIBBLADERF;
        } else {
            printf( "  RX Bandwidth: %9uHz\n", bw );
        }
    }

    if( argc == 2 || module == BLADERF_MODULE_TX ) {
        status = bladerf_get_bandwidth( state->dev, BLADERF_MODULE_TX, &bw );
        if (status < 0) {
            state->last_lib_error = status;
            rv = CMD_RET_LIBBLADERF;
        } else {
            printf( "  TX Bandwidth: %9uHz\n", bw );
        }
    }

    printf( "\n" );

    return rv;
}

int set_bandwidth(struct cli_state *state, int argc, char **argv)
{
    /* Usage: set bandwidth [rx|tx] <bandwidth in Hz> */
    int rv = CMD_RET_OK;
    int status;
    bladerf_module module = BLADERF_MODULE_RX;
    unsigned int bw = 28000000, actual;

    /* Check for extended help */
    if( argc == 2 ) {
        printf( "\n" );
        printf( "Usage: set bandwidth [module] <bandwidth>\n" );
        printf( "\n" );
        printf( "    module         Optional argument to set single module bandwidth\n" );
        printf( "    bandwidth      Bandwidth in Hz - will be rounded up to closest bandwidth\n" );
        printf( "\n" );
    }

    /* Check for optional module */
    else if( argc == 4 ) {
        /* Parse module */
        bool ok;
        module = get_module( argv[2], &ok );
        if( !ok ) {
            invalid_module(state, argv[0], argv[2]);
            rv = CMD_RET_INVPARAM;
        }

        /* Parse bandwidth */
        bw = str2uint_suffix( argv[3],
                              BLADERF_BANDWIDTH_MIN, BLADERF_BANDWIDTH_MAX,
                              FREQ_SUFFIXES, NUM_FREQ_SUFFIXES, &ok );
        if( !ok ) {
            cli_err(state, argv[0], "Invalid bandwidth (%s)", argv[3]);
            rv = CMD_RET_INVPARAM;
        }
    }

    /* No module, just bandwidth */
    else if( argc == 3 ) {
        bool ok;
        bw = str2uint_suffix( argv[2],
                              BLADERF_BANDWIDTH_MIN, BLADERF_BANDWIDTH_MAX,
                              FREQ_SUFFIXES, NUM_FREQ_SUFFIXES, &ok );
        if( !ok ) {
            cli_err(state, argv[0], "Invalid bandwidth (%s)", argv[2]);
            rv = CMD_RET_INVPARAM;
        }
    }

    /* Weird number of arguments */
    else {
        rv = CMD_RET_NARGS;
    }

    /* Problem parsing arguments? */
    if( argc > 2 && rv == CMD_RET_OK ) {

        printf( "\n" );

        /* Lack of option, so set both or RX only */
        if( argc == 3 || module == BLADERF_MODULE_RX ) {
            status = bladerf_set_bandwidth( state->dev, BLADERF_MODULE_RX,
                                            bw, &actual );

            if (status < 0) {
                state->last_lib_error = status;
                rv = CMD_RET_LIBBLADERF;
            } else {
                printf( "  Set RX bandwidth - req:%9uHz actual:%9uHz\n",
                        bw, actual );
            }
        }

        /* Lack of option, so set both or TX only */
        if( argc == 3 || module == BLADERF_MODULE_TX ) {
            status = bladerf_set_bandwidth( state->dev, BLADERF_MODULE_TX,
                                            bw, &actual );

            if (status < 0) {
                state->last_lib_error = status;
                rv = CMD_RET_LIBBLADERF;
            } else {
                printf( "  Set TX bandwidth - req:%9uHz actual:%9uHz\n",
                        bw, actual );
            }
        }

        printf( "\n" );
    }


    return rv;
}

int print_config(struct cli_state *state, int argc, char **argv)
{
    return CMD_RET_OK;
}

int set_config(struct cli_state *state, int argc, char **argv)
{
    return CMD_RET_OK;
}

int print_frequency(struct cli_state *state, int argc, char **argv)
{
    /* Usage: print frequency [<rx|tx>] */
    int rv = CMD_RET_OK;
    int status;
    unsigned int freq;
    bladerf_module module = BLADERF_MODULE_RX;
    if( argc == 3 ) {
        /* Parse module */
        bool ok;
        module = get_module( argv[2], &ok );
        if( !ok ) {
            invalid_module(state, argv[0], argv[2]);
            rv = CMD_RET_INVPARAM;
        }
    } else if( argc != 2 ) {
        /* Assume both RX & TX if not specified */
        rv = CMD_RET_NARGS;
    }

    if( rv == CMD_RET_OK ) {
        if( argc == 2 || module == BLADERF_MODULE_RX ) {
            status = bladerf_get_frequency( state->dev, BLADERF_MODULE_RX, &freq );
            if (status < 0) {
                state->last_lib_error = status;
                rv = CMD_RET_LIBBLADERF;
            } else {
                printf( "\n" );
                printf( "  RX Frequency: %10uHz\n", freq );
                if( argc == 3 ) {
                    printf( "\n" );
                }
            }
        }

        if( argc == 2 || module == BLADERF_MODULE_TX ) {
            status = bladerf_get_frequency( state->dev, BLADERF_MODULE_TX, &freq );
            if (status < 0) {
                state->last_lib_error = status;
                rv = CMD_RET_LIBBLADERF;
            } else {
                if( argc == 3 ) {
                    printf( "\n" );
                }
                printf( "  TX Frequency: %10uHz\n", freq );
                printf( "\n" );
            }
        }
    }

    return rv;
}

int set_frequency(struct cli_state *state, int argc, char **argv)
{
    /* Usage: set frequency [<rx|tx>] <frequency in Hz> */
    int rv = CMD_RET_OK;
    int status;
    unsigned int freq;
    bladerf_module module = BLADERF_MODULE_RX;

    if( argc == 4 ) {
        /* Parse module */
        bool ok;
        module = get_module( argv[2], &ok );
        if( !ok ) {
            invalid_module(state, argv[0], argv[2]);
            rv = CMD_RET_INVPARAM;
        }
    } else if( argc != 3 ) {
        /* Assume both RX & TX if not specified */
        rv = CMD_RET_NARGS;
    }

    if( argc > 2 && rv == CMD_RET_OK ) {
        bool ok;
        /* Parse out frequency */
        freq = str2uint_suffix( argv[argc-1],
                                BLADERF_FREQUENCY_MIN, BLADERF_FREQUENCY_MAX,
                                FREQ_SUFFIXES, NUM_FREQ_SUFFIXES, &ok );

        if( !ok ) {
            cli_err(state, argv[0], "Invalid frequency (%s)", argv[argc - 1]);
            rv = CMD_RET_INVPARAM;
        } else {

            printf( "\n" );

            /* Change RX frequency */
            if( argc == 3 || module == BLADERF_MODULE_RX ) {
                int status = bladerf_set_frequency( state->dev,
                                                    BLADERF_MODULE_RX, freq );

                if (status < 0) {
                    state->last_lib_error = status;
                    rv = CMD_RET_LIBBLADERF;
                } else {
                    printf( "  Set RX frequency: %10uHz\n", freq );
                }
            }

            /* Change TX frequency */
            if( argc == 3 || module == BLADERF_MODULE_TX ) {
                status = bladerf_set_frequency( state->dev,
                                                BLADERF_MODULE_TX, freq );

                if (status < 0) {
                    state->last_lib_error = status;
                    rv = CMD_RET_LIBBLADERF;
                } else {
                    printf( "  Set TX frequency: %10uHz\n", freq );
                }
            }

            printf( "\n" );
        }
    }

    return rv;
}

int print_gpio(struct cli_state *state, int argc, char **argv)
{
    int rv = CMD_RET_OK, status;
    unsigned int val;

    status = bladerf_config_gpio_read( state->dev, &val );

    if (status < 0) {
        state->last_lib_error = status;
        rv = CMD_RET_LIBBLADERF;
    } else {
        printf( "\n" );
        printf( "  GPIO: 0x%8.8x\n", val );
        printf( "\n" );
        printf( "    %-20s%-10s\n", "LMS Enable:", val&0x01 ? "Enabled" : "Reset" ); // Active low
        printf( "    %-20s%-10s\n", "LMS RX Enable:", val&0x02 ? "Enabled" : "Disabled" );
        printf( "    %-20s%-10s\n", "LMS TX Enable:", val&0x04 ? "Enabled" : "Disabled" );
        printf( "    %-20s", "TX Band:" );
        if( ((val>>3)&3) == 2 ) {
            printf( "Low Band (300M - 1.5GHz)\n" );
        } else if( ((val>>3)&3) == 1 ) {
            printf( "High Band (1.5GHz - 3.8GHz)\n" );
        } else {
            printf( "Invalid Band Selection!\n" );
        }
        printf( "    %-20s", "RX Band:" );
        if( ((val>>5)&3) == 2 ) {
            printf( "Low Band (300M - 1.5GHz)\n" );
        } else if( ((val>>5)&3) == 1 ) {
            printf( "High Band (1.5GHz - 3.8GHz)\n" );
        } else {
            printf( "Invalid Band Selection!\n" );
        }
        printf( "    %-20s", "RX Source:" );
        switch((val&0x300)>>8) {
            case 0: printf( "LMS6002D\n" ); break;
            case 1: printf( "Internal 12-bit counter\n" ); break;
            case 2: printf( "Internal 32-bit counter\n" ); break;
            case 3: printf( "Whitened Entropy\n" ); break;
            default : printf( "????\n" ); break;
        }
        printf( "\n" );
    }

    return rv;
}

int set_gpio(struct cli_state *state, int argc, char **argv)
{
    /* set gpio <value> */
    int rv = CMD_RET_OK;
    uint32_t val;
    bool ok;

    if( argc == 3 ) {
        val = str2uint( argv[2], 0, UINT_MAX, &ok );
        if( !ok ) {
            cli_err(state, argv[0], "Invalid gpio value (%s)", argv[2]);
            rv = CMD_RET_INVPARAM;
        } else {
            bladerf_config_gpio_write( state->dev,val );
        }
    } else {
        rv = CMD_RET_NARGS;
    }
    return rv;
}

int print_xb_gpio(struct cli_state *state, int argc, char **argv) {
    int rv = CMD_RET_OK, status;
    unsigned int val;

    status = bladerf_expansion_gpio_read( state->dev, &val );

    if (status < 0) {
        state->last_lib_error = status;
        rv = CMD_RET_LIBBLADERF;
    } else {
        printf( "\n" );
        printf( "Expansion GPIO: 0x%8.8x\n", val );
        printf( "\n" );
    }
    return rv;
}

int set_xb_gpio(struct cli_state *state, int argc, char **argv)
{
    /* set gpio <value> */
    int rv = CMD_RET_OK;
    uint32_t val;
    bool ok;

    if( argc == 3 ) {
        val = str2uint( argv[2], 0, UINT_MAX, &ok );
        if( !ok ) {
            cli_err(state, argv[0], "Invalid xb gpio value (%s)", argv[2]);
            rv = CMD_RET_INVPARAM;
        } else {
            bladerf_expansion_gpio_write ( state->dev,val );
        }
    } else {
        rv = CMD_RET_NARGS;
    }
    return rv;
}

int print_xb_gpio_dir(struct cli_state *state, int argc, char **argv) {
    int rv = CMD_RET_OK, status;
    unsigned int val;

    status = bladerf_expansion_gpio_dir_read( state->dev, &val );

    if (status < 0) {
        state->last_lib_error = status;
        rv = CMD_RET_LIBBLADERF;
    } else {
        printf( "\n" );
        printf( "Expansion GPIO direction (1=output): 0x%8.8x\n", val );
        printf( "\n" );
    }
    return rv;
}

int set_xb_gpio_dir(struct cli_state *state, int argc, char **argv)
{
    /* set gpio <value> */
    int rv = CMD_RET_OK;
    uint32_t val;
    bool ok;

    if( argc == 3 ) {
        val = str2uint( argv[2], 0, UINT_MAX, &ok );
        if( !ok ) {
            cli_err(state, argv[0], "Invalid xb gpio dir value (%s)", argv[2]);
            rv = CMD_RET_INVPARAM;
        } else {
            bladerf_expansion_gpio_dir_write ( state->dev,val );
        }
    } else {
        rv = CMD_RET_NARGS;
    }
    return rv;
}

int print_lmsregs(struct cli_state *state, int argc, char **argv)
{
    return CMD_RET_OK;
}

int set_lmsregs(struct cli_state *state, int argc, char **argv)
{
    return CMD_RET_OK;
}

int print_lna(struct cli_state *state, int argc, char **argv)
{
    return CMD_RET_OK;
}

int set_lna(struct cli_state *state, int argc, char **argv)
{
    return CMD_RET_OK;
}

int print_lnagain(struct cli_state *state, int argc, char **argv)
{
    int rv = CMD_RET_OK, status;
    bladerf_lna_gain gain;

    status = bladerf_get_lna_gain( state->dev, &gain );
    if (status < 0) {
        state->last_lib_error = status;
        rv = CMD_RET_LIBBLADERF;
    } else {
        printf( "\n" );
        printf( "  LNA Gain: ");
        switch(gain) {
            case BLADERF_LNA_GAIN_UNKNOWN:
                    printf( "BLADERF_LNA_GAIN_UNKNOWN\n" );
                    break;

            case BLADERF_LNA_GAIN_MID:
                    printf( "BLADERF_LNA_GAIN_MID\n" );
                    break;

            case BLADERF_LNA_GAIN_MAX:
                    printf( "BLADERF_LNA_GAIN_MAX\n" );
                    break;

            case BLADERF_LNA_GAIN_BYPASS:
                    printf( "BLADERF_LNA_GAIN_BYPASS\n");
                    break;
        }
        printf( "\n" ) ;
    }

    return rv;
}

int set_lnagain(struct cli_state *state, int argc, char **argv)
{
    int rv = CMD_RET_OK;
    int status;

    if( argc != 3 ) {
        rv = CMD_RET_NARGS;
    } else {
        bladerf_lna_gain gain = BLADERF_LNA_GAIN_UNKNOWN;
        if( strcasecmp( argv[2], "max" ) == 0 ) {
            gain = BLADERF_LNA_GAIN_MAX;
        } else if( strcasecmp( argv[2], "mid" ) == 0 ) {
            gain = BLADERF_LNA_GAIN_MID;
        } else if( strcasecmp( argv[2], "bypass" ) == 0 ) {
            gain = BLADERF_LNA_GAIN_BYPASS;
        } else {
            invalid_gain(state, argv[0], argv[1], argv[2]);
        }

        if (gain != BLADERF_LNA_GAIN_UNKNOWN) {
            status = bladerf_set_lna_gain( state->dev, gain );
            if (status < 0) {
                state->last_lib_error = status;
                rv = CMD_RET_LIBBLADERF;
            }
        } else {
            rv = CMD_RET_INVPARAM;
        }
    }

    return rv;
}

int print_loopback(struct cli_state *state, int argc, char **argv)
{
    int status;
    bladerf_loopback loopback;

    status = bladerf_get_loopback(state->dev, &loopback);
    if (status != 0) {
        state->last_lib_error = status;
        return CMD_RET_LIBBLADERF;
    }

    switch (loopback) {
        case BLADERF_LB_BB_TXLPF_RXVGA2:
            printf("Loopback mode: bb_txlpf_rxvga2\n");
            break;

        case BLADERF_LB_BB_TXLPF_RXLPF:
            printf("Loopback mode: bb_txlpf_rxlpf\n");
            break;

        case BLADERF_LB_BB_TXVGA1_RXVGA2:
            printf("Loopback mode: bb_txvga1_rxvga2\n");
            break;

        case BLADERF_LB_BB_TXVGA1_RXLPF:
            printf("Loopback mode: bb_txvga1_rxlpf\n");
            break;

        case BLADERF_LB_RF_LNA1:
            printf("Loopback mode: rf_lna1\n");
            break;

        case BLADERF_LB_RF_LNA2:
            printf("Loopback mode: rf_lna2\n");
            break;

        case BLADERF_LB_RF_LNA3:
            printf("Loopback mode: rf_lna3\n");
            break;

        case BLADERF_LB_NONE:
            printf("Loopback mode: none\n");
            break;

        default:
            cli_err(state, argv[0], "Read back unexpected loopback mode: %d\n",
                    (int)loopback);
            return CMD_RET_INVPARAM;
    }

    return 0;
}

int set_loopback(struct cli_state *state, int argc, char **argv)
{
    int status;
    bladerf_loopback loopback;

    if (argc == 2) {
        printf("\n");
        printf("Usage: %s %s <mode>, where <mode> is one of the following:\n",
               argv[0], argv[1]);
        printf("\n");
        printf("  bb_txlpf_rxvga2   Baseband loopback: TXLPF output --> RXVGA2 input\n");
        printf("  bb_txlpf_rxlpf    Baseband loopback: TXLPF output --> RXLPF input\n");
        printf("  bb_txvga1_rxvga2  Baseband loopback: TXVGA1 output --> RXVGA2 input.\n");
        printf("  bb_txvga1_rxlpf   Baseband loopback: TXVGA1 output --> RXLPF input\n");
        printf("  rx_lna1           RF loopback: TXMIX --> RXMIX via LNA1 path.\n");
        printf("  rx_lna2           RF loopback: TXMIX --> RXMIX via LNA2 path.\n");
        printf("  rx_lna3           RF loopback: TXMIX --> RXMIX via LNA3 path.\n");
        printf("  none              Loopback disabled - Normal operation.\n");
        printf("\n");

        return CMD_RET_INVPARAM;

    } else if (argc != 3) {
        return CMD_RET_NARGS;
    }

    if (!strcasecmp(argv[2], "bb_txlpf_rxvga2")) {
        loopback = BLADERF_LB_BB_TXLPF_RXVGA2;
    } else if (!strcasecmp(argv[2], "bb_txlpf_rxlpf")) {
        loopback = BLADERF_LB_BB_TXLPF_RXLPF;
    } else if (!strcasecmp(argv[2], "bb_txvga1_rxvga2")) {
        loopback = BLADERF_LB_BB_TXVGA1_RXVGA2;
    } else if (!strcasecmp(argv[2], "bb_txvga1_rxlpf")) {
        loopback = BLADERF_LB_BB_TXVGA1_RXLPF;
    } else if (!strcasecmp(argv[2], "rf_lna1")) {
        loopback = BLADERF_LB_RF_LNA1;
    } else if (!strcasecmp(argv[2], "rf_lna2")) {
        loopback = BLADERF_LB_RF_LNA2;
    } else if (!strcasecmp(argv[2], "rf_lna3")) {
        loopback = BLADERF_LB_RF_LNA3;
    } else if (!strcasecmp(argv[2], "none")) {
        loopback = BLADERF_LB_NONE;
    } else {
        cli_err(state, argv[0],
                "Invalid loopback mode provided: %s\n", argv[2]);
        return CMD_RET_INVPARAM;
    }

    status = bladerf_set_loopback(state->dev, loopback);
    if (status != 0) {
        state->last_lib_error = status;
        return CMD_RET_LIBBLADERF;
    } else {
        return CMD_RET_OK;
    }
}

int print_mimo(struct cli_state *state, int argc, char **argv)
{
    return CMD_RET_OK;
}

int set_mimo(struct cli_state *state, int argc, char **argv)
{
    return CMD_RET_OK;
}

int print_pa(struct cli_state *state, int argc, char **argv)
{
    return CMD_RET_OK;
}

int set_pa(struct cli_state *state, int argc, char **argv)
{
    return CMD_RET_OK;
}

int print_pps(struct cli_state *state, int argc, char **argv)
{
    return CMD_RET_OK;
}

int set_pps(struct cli_state *state, int argc, char **argv)
{
    return CMD_RET_OK;
}

int print_refclk(struct cli_state *state, int argc, char **argv)
{
    return CMD_RET_OK;
}

int set_refclk(struct cli_state *state, int argc, char **argv)
{
    return CMD_RET_OK;
}

int print_rxvga1(struct cli_state *state, int argc, char **argv)
{
    int rv = CMD_RET_OK, gain, status;

    status = bladerf_get_rxvga1( state->dev, &gain );
    if (status < 0) {
        state->last_lib_error = status;
        rv = CMD_RET_LIBBLADERF;
    } else {
        printf( "\n" );
        printf( "  RXVGA1 Gain: %3ddB\n", gain );
        printf( "\n" );
    }

    return rv;
}

int set_rxvga1(struct cli_state *state, int argc, char **argv)
{
    int rv = CMD_RET_OK, gain, status;
    if( argc != 3 ) {
        rv = CMD_RET_NARGS;
    } else {
        bool ok;
        gain = str2int( argv[2], BLADERF_RXVGA1_GAIN_MIN,
                        BLADERF_RXVGA1_GAIN_MAX, &ok );
        if( !ok ) {
            invalid_gain(state, argv[0], argv[1], argv[2]);
            rv = CMD_RET_INVPARAM;
        } else {
            status = bladerf_set_rxvga1( state->dev, gain );
            if (status < 0) {
                state->last_lib_error = status;
                rv = CMD_RET_LIBBLADERF;
            }
        }
    }
    return rv;
}

int print_rxvga2(struct cli_state *state, int argc, char **argv)
{
    int rv = CMD_RET_OK, gain, status;

    status = bladerf_get_rxvga2( state->dev, &gain );
    if (status < 0) {
        state->last_lib_error = status;
        rv = CMD_RET_LIBBLADERF;
    } else {
        printf( "\n" );
        printf( "  RXVGA2 Gain: %3ddB\n", gain );
        printf( "\n" );
    }

    return rv;
}

int set_rxvga2(struct cli_state *state, int argc, char **argv)
{
    int rv = CMD_RET_OK, gain, status;

    if( argc != 3 ) {
        rv = CMD_RET_NARGS;
    } else {
        bool ok;
        gain = str2int( argv[2], BLADERF_RXVGA2_GAIN_MIN,
                        BLADERF_RXVGA2_GAIN_MAX, &ok );

        if( !ok ) {
            invalid_gain(state, argv[0], argv[1], argv[2]);
            rv = CMD_RET_INVPARAM;
        } else {
            status = bladerf_set_rxvga2( state->dev, gain );
            if (status < 0) {
                state->last_lib_error = status;
                rv = CMD_RET_LIBBLADERF;
            }
        }
    }
    return rv;
}

int print_samplerate(struct cli_state *state, int argc, char **argv)
{
    int status;
    struct bladerf_rational_rate rate;
    bladerf_module module;
    bool ok;

    if (argc == 3) {
        module = get_module(argv[2], &ok);
        if (!ok) {
            invalid_module(state, argv[0], argv[2]);
            status = CMD_RET_INVPARAM;
        } else {
            status = bladerf_get_rational_sample_rate(state->dev, module, &rate);
            printf("  %s sample rate: %"PRIu64" %"PRIu64"/%"PRIu64"\n",
                    module == BLADERF_MODULE_RX ? "RX" : "TX", rate.integer, rate.num, rate.den);
        }
    } else if (argc == 2) {
        struct bladerf_rational_rate tx_rate;
        status = bladerf_get_rational_sample_rate(state->dev, BLADERF_MODULE_RX, &rate);

        if (!status) {
            status = bladerf_get_rational_sample_rate(state->dev,
                                                BLADERF_MODULE_TX, &tx_rate);

            if (!status) {
                printf("  RX sample rate: %"PRIu64" %"PRIu64"/%"PRIu64"\n", rate.integer, rate.num, rate.den);
                printf("  TX sample rate: %"PRIu64" %"PRIu64"/%"PRIu64"\n", tx_rate.integer, tx_rate.num, tx_rate.den);
            } else {
                state->last_lib_error = status;
                status = CMD_RET_LIBBLADERF;
            }
        } else {
            state->last_lib_error = status;
            status = CMD_RET_LIBBLADERF;
        }

    } else {
        return CMD_RET_NARGS;
    }

    return status;
}

int set_samplerate(struct cli_state *state, int argc, char **argv)
{
    /* Usage: set samplerate [rx|tx] [integer [numerator denominator]]*/
    int rv = CMD_RET_OK;
    bladerf_module module = BLADERF_MODULE_RX;

    if( argc == 4 || argc == 6 ) {
        /* Parse module */
        bool ok;
        module = get_module( argv[2], &ok );
        if( !ok ) {
            invalid_module(state, argv[0], argv[2]);
            rv = CMD_RET_INVPARAM;
        }
    } else if( argc != 3 && argc != 5 ) {
        printf( "Usage:\n" );
        printf( "\n" );
        printf( "  set samplerate [rx|tx] [integer [numerator denominator]]\n" );
        printf( "\n" );
        printf( "Setting the sample rate first addresses an optional RX or TX module.\n" );
        printf( "Omitting the module will set both of the modules.\n" );
        printf( "\n" );
        printf( "In the case of a whole integer sample rate, only the integer portion\n" );
        printf( "is required in the command.  In the case of a fractional rate, the\n" );
        printf( "numerator follows the integer portion followed by the denominator.\n" );
        printf( "\n" );
        printf( "Examples:\n" );
        printf( "\n" );
        printf( "  Set the sample rate for both RX and TX to 4MHz\n" );
        printf( "\n" );
        printf( "    set samplerate 4M\n" );
        printf( "\n" );
        printf( "  Set the sample rate for both RX and TX to GSM 270833 1/3Hz.\n" );
        printf( "\n" );
        printf( "    set samplerate 270833 1 3\n" );
        printf( "\n" );
        printf( "  Set the sample rate for RX to 40MHz\n" );
        printf( "\n" );
        printf( "    set samplerate rx 40M\n" );
        printf( "\n" );
        rv = CMD_RET_NARGS;
    }

    if( argc > 2 && rv == CMD_RET_OK ) {
        bool ok;
        uint8_t idx = 0;
        struct bladerf_rational_rate rate, actual;

        /* Initialize the samplerate */
        rate.integer = 0;
        rate.num = 0;
        rate.den = 1;

        /* Figure out the integer index */
        if( argc == 3 || argc == 5 ) {
            idx = 2;
        } else {
            idx = 3;
        }

        rate.integer = str2uint_suffix( argv[idx],
                                        BLADERF_SAMPLERATE_MIN,
                                        UINT_MAX,
                                        FREQ_SUFFIXES, NUM_FREQ_SUFFIXES, &ok );

        /* Integer portion didn't make it */
        if( !ok ) {
            cli_err(state, argv[0], "Invalid sample rate (%s)", argv[idx]);
            rv = CMD_RET_INVPARAM;
        }

        /* Take in num/den if they are present */
        if( rv == CMD_RET_OK && (argc == 5 || argc == 6) ) {
            idx++;
            rate.num = str2uint_suffix( argv[idx], 0, 999999999,
                FREQ_SUFFIXES, NUM_FREQ_SUFFIXES, &ok );
            if( !ok ) {
                cli_err(state, argv[0], "Invalid sample rate (%s %s/%s)",
                    argv[idx-1], argv[idx], argv[idx+1] );
                rv = CMD_RET_INVPARAM;
            }

            if( ok ) {
                idx++;
                rate.den = str2uint_suffix( argv[idx], 1, 1000000000,
                    FREQ_SUFFIXES, NUM_FREQ_SUFFIXES, &ok );
                if( !ok ) {
                    cli_err(state, argv[0], "Invalid sample rate (%s %s/%s)",
                        argv[idx-2], argv[idx-1], argv[idx] ) ;
                    rv = CMD_RET_INVPARAM;
                }
            }
        }
        if( rv == CMD_RET_OK ) {
            int status;
            bladerf_dev_speed usb_speed = bladerf_device_speed(state->dev);

            /* Discontinuities have been reported for 2.0 on Intel controllers
             * above 6MHz. */
            if (usb_speed != BLADERF_DEVICE_SPEED_SUPER && rate.integer > 6000000) {
                printf("\n  Warning: The provided sample rate may "
                       "result in timeouts with the current\n"
                       "           %s connection. "
                       "A SuperSpeed connection or a lower sample\n"
                       "           rate are recommended.\n",
                       devspeed2str(usb_speed));
            }

            printf( "\n" );

            if( argc == 3 || argc == 5 || module == BLADERF_MODULE_RX ) {
                status = bladerf_set_rational_sample_rate( state->dev,
                                                           BLADERF_MODULE_RX,
                                                           &rate, &actual );

                if (status < 0) {
                    state->last_lib_error = status;
                    rv = CMD_RET_LIBBLADERF;
                } else {
                    printf( "  Setting RX sample rate - req: %9"PRIu64" %"PRIu64"/%"PRIu64"Hz, "
                            "actual: %9"PRIu64" %"PRIu64"/%"PRIu64"Hz\n", rate.integer, rate.num, rate.den, actual.integer, actual.num, actual.den );
                }
            }

            if( argc == 3 || argc == 5 || module == BLADERF_MODULE_TX ) {
                status = bladerf_set_rational_sample_rate( state->dev,
                                                           BLADERF_MODULE_TX,
                                                           &rate, &actual );
                if (status < 0) {
                    state->last_lib_error = status;
                    rv = CMD_RET_LIBBLADERF;
                } else {
                    printf( "  Setting TX sample rate - req: %9"PRIu64" %"PRIu64"/%"PRIu64"Hz, "
                            "actual: %9"PRIu64" %"PRIu64"/%"PRIu64"Hz\n", rate.integer, rate.num, rate.den, actual.integer, actual.num, actual.den );
                }
            }
            printf( "\n" );

        }
    }

    return rv;
}

int set_sampling(struct cli_state *state, int argc, char **argv)
{
    /* Usage: set sampling [internal|external] */
    int rv = CMD_RET_OK;
    int status = 0;
    if( argc != 3 ) {
        rv = CMD_RET_NARGS ;
    } else {
        if( strcasecmp( "internal", argv[2] ) == 0 ) {
            status = bladerf_set_sampling( state->dev, BLADERF_SAMPLING_INTERNAL );
            if (status) {
                state->last_lib_error = status;
                rv = CMD_RET_LIBBLADERF;
            }
        } else if( strcasecmp( "external", argv[2] ) == 0 ) {
            status = bladerf_set_sampling( state->dev, BLADERF_SAMPLING_EXTERNAL );
            if (status) {
                state->last_lib_error = status;
                rv = CMD_RET_LIBBLADERF;
            }
        } else {
            cli_err(state, argv[0], "Invalid sampling mode (%s)", argv[2] );
            rv = CMD_RET_INVPARAM;
        }
    }
    return rv;
}

int print_sampling( struct cli_state *state, int argc, char **argv)
{
    int rv = CMD_RET_OK, status = 0;
    bladerf_sampling mode;
    if( argc != 2 ) {
        rv = CMD_RET_NARGS;
    } else {
        /* Read the ADC input mux */
        status = bladerf_get_sampling( state->dev, &mode);
        if (status) {
            state->last_lib_error = status;
            rv = CMD_RET_LIBBLADERF;
        } else {
            printf( "  %-20s%-20s\n", "Sampling:", mode == BLADERF_SAMPLING_EXTERNAL ? "External" : mode == BLADERF_SAMPLING_INTERNAL ? "Internal" : "UNKNOWN" ) ;
        }
    }

    return rv;
}

int print_trimdac(struct cli_state *state, int argc, char **argv)
{
    /* TODO: Can't be implemented until dac_read() is written */
    return CMD_RET_OK;
}

int set_trimdac(struct cli_state *state, int argc, char **argv)
{
    int rv = CMD_RET_OK;
    unsigned int val;

    if( argc != 3 ) {
        rv = CMD_RET_NARGS;
    } else {
        bool ok;
        val = str2uint( argv[2], 0, 65535, &ok );
        if( !ok ) {
            cli_err(state, argv[0], "Invalid VCTCXO DAC value (%s)", argv[2]);
            rv = CMD_RET_INVPARAM;
        } else {
            int status = bladerf_dac_write( state->dev, val );
            if (status < 0) {
                state->last_lib_error = status;
                rv = CMD_RET_LIBBLADERF;
            }
        }
    }
    return rv;
}

int print_xb_spi(struct cli_state *state, int argc, char **argv)
{
    /* All of the SPI devices so far are write-only */
    return CMD_RET_OK;
}

int set_xb_spi(struct cli_state *state, int argc, char **argv)
{
    int rv = CMD_RET_OK;
    unsigned int val;

    if( argc != 3) {
        rv = CMD_RET_NARGS;
    } else {
        bool ok;
        val = str2uint( argv[2], 0, -1, &ok );
        if( !ok ) {
            cli_err(state, argv[0], "Invalid XB SPI value (%s)", argv[2]);
            rv = CMD_RET_INVPARAM;
        } else {
            int status = bladerf_xb_spi_write( state->dev, val );
            if (status < 0) {
                state->last_lib_error = status;
                rv = CMD_RET_LIBBLADERF;
            }
        }
    }
    return rv;
}

int print_txvga1(struct cli_state *state, int argc, char **argv)
{
    /* Usage: print txvga1 */
    int rv = CMD_RET_OK, gain, status;

    status = bladerf_get_txvga1( state->dev, &gain );
    if (status < 0) {
        state->last_lib_error = status;
        rv = CMD_RET_LIBBLADERF;
    } else {
        printf( "\n" );
        printf( "  TXVGA1 Gain: %ddB\n", gain ) ;
        printf( "\n" );
    }

    return rv;
}

int set_txvga1(struct cli_state *state, int argc, char **argv)
{
    /* Usage: set txvga1 <gain> */
    int rv = CMD_RET_OK, gain;

    if( argc == 3 ) {
        bool ok ;
        gain = str2int( argv[2], BLADERF_TXVGA1_GAIN_MIN,
                        BLADERF_TXVGA1_GAIN_MAX, &ok );

        if( !ok ) {
            invalid_gain(state, argv[0], argv[1], argv[2]);
            rv = CMD_RET_INVPARAM;
        } else {
            int status = bladerf_set_txvga1( state->dev, gain );
            if (status < 0) {
                state->last_lib_error = status;
                rv = CMD_RET_LIBBLADERF;
            } else {
                printf( "  Set TXVGA1 to %d\n", gain );
            }
        }
    } else {
        rv = CMD_RET_NARGS;
    }

    return rv;
}

int print_txvga2(struct cli_state *state, int argc, char **argv)
{
    /* Usage: print txvga2 */
    int rv = CMD_RET_OK;
    int status, gain;

    status = bladerf_get_txvga2( state->dev, &gain );
    if (status < 0) {
        state->last_lib_error = status;
        rv = CMD_RET_LIBBLADERF;
    } else {
        printf( "\n" );
        printf( "  TXVGA2 Gain: %ddB\n", gain );
        printf( "\n" );
    }
    return rv;
}

int set_txvga2(struct cli_state *state, int argc, char **argv)
{
    /* Usage: set txvga2 <gain> */
    int rv = CMD_RET_OK;

    if( argc != 3 ) {
        rv = CMD_RET_NARGS;
    } else {
        bool ok;
        int gain;
        gain = str2int( argv[2], BLADERF_TXVGA2_GAIN_MIN,
                        BLADERF_TXVGA2_GAIN_MAX, &ok );
        if( !ok ) {
            invalid_gain(state, argv[0], argv[1], argv[2]);
            rv = CMD_RET_INVPARAM;
        } else {
            int status = bladerf_set_txvga2( state->dev, gain );
            if (status < 0) {
                state->last_lib_error = status;
                rv = CMD_RET_LIBBLADERF;
            }
        }
    }
    return rv;
}

struct printset_entry *get_printset_entry( char *name)
{
    int i;
    struct printset_entry *entry = NULL;
    for( i = 0; entry == NULL && printset_table[i].print != NULL; ++i ) {
        if( strcasecmp( name, printset_table[i].name ) == 0 ) {
            entry = &(printset_table[i]);
        }
    }
    return entry;
}

/* Set command */
int cmd_set(struct cli_state *state, int argc, char **argv)
{
    /* Valid commands:
        set bandwidth <rx|tx> <bw in Hz>
        set frequency <rx|tx> <frequency in Hz>
        set gpio <value>
        set loopback [mode]
        set mimo <master|slave|off>
        set pa <lb|hb>
        set pps <on|off>
        set refclk <reference frequency> <comparison frequency>
        set rxvga1 <gain in dB>
        set rxvga2 <gain in dB>
        set samplerate <integer Hz> [<num frac> <denom frac>]
        set trimdac <value>
        set txvga1 <gain in dB>
        set txvga2 <gain in dB>

       NOTE: Using set <parameter> with no other entries should print
       a nice usage note for that specific setting.
    */
    int rv = CMD_RET_OK;
    int fpga_configured;

    if (!cli_device_is_opened(state)) {
        return  CMD_RET_NODEV;
    }

    fpga_configured = is_fpga_configured(state, argv[0]);

    if ( fpga_configured < 0) {
        rv = fpga_configured;
    } else if ( !fpga_configured ) {
        rv = CMD_RET_NOFPGA;
    } else if( argc > 1 ) {
        struct printset_entry *entry = NULL;

        entry = get_printset_entry( argv[1] );

        if( entry ) {
            /* Call the parameter setting function */
            rv = entry->set(state, argc, argv);
        } else {
            /* Incorrect parameter to print */
            cli_err(state, argv[0], "Invalid parameter (%s)", argv[1]);
            rv = CMD_RET_INVPARAM;
        }
    } else {
        rv = CMD_RET_NARGS;
    }
    return rv;
}

/* Print command */
int cmd_print(struct cli_state *state, int argc, char **argv)
{
    /* Valid commands:
        print bandwidth <rx|tx>
        print config
        print frequency
        print gpio
        print lmsregs
        print lna
        print loopback
        print mimo
        print pa
        print pps
        print refclk
        print rxvga1
        print rxvga2
        print samplerate
        print trimdac
        print txvga1
        print txvga2
    */
    int rv = CMD_RET_OK;
    struct printset_entry *entry = NULL;
    int fpga_configured;

    if (!cli_device_is_opened(state)) {
        return  CMD_RET_NODEV;
    }

    fpga_configured = is_fpga_configured( state, argv[0] );

    if ( fpga_configured < 0 ) {
        rv = fpga_configured;
    } else if( !fpga_configured ) {
        rv = CMD_RET_NOFPGA;
    } else if( argc > 1 ) {

        entry = get_printset_entry( argv[1] );

        if( entry ) {
            /* Call the parameter printing function */
            rv = entry->print(state, argc, argv);
        } else {
            /* Incorrect parameter to print */
            cli_err(state, argv[0], "Invalid parameter (%s)", argv[1]);
            rv = CMD_RET_INVPARAM;
        }
    } else {
        rv = CMD_RET_NARGS;
    }
    return rv;
}

