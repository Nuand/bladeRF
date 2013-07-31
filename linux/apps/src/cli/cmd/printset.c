#include <limits.h>
#include <stdint.h>
#include "cmd.h"

#define PRINTSET_DECL(x) int print_##x(struct cli_state *, int, char **); int set_##x(struct cli_state *, int, char **);
#define PRINTSET_ENTRY(x) { .print = print_##x, .set = set_##x, .name = #x }

/* An entry in the printset table */
struct printset_entry {
    int (*print)(struct cli_state *s, int argc, char **argv);  /*<< Function pointer to thing that prints parameter */
    int (*set)(struct cli_state *s, int argc, char **argv);    /*<< Function pointer to setter */
    const char *name;                   /*<< String associated with parameter */
};

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
    PRINTSET_ENTRY(txvga1),
    PRINTSET_ENTRY(txvga2),
    { .print = NULL, .set = NULL, .name = "" }
};

bladerf_module get_module( char *mod, bool *ok )
{
    bladerf_module rv = RX;
    *ok = true;
    if( strcasecmp( mod, "rx" ) == 0 ) {
        rv = RX;
    } else if( strcasecmp( mod, "tx" ) == 0 ) {
        rv = TX;
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
    bladerf_module module = RX ;
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

    if( argc == 2 || module == RX ) {
        status =  bladerf_get_bandwidth( state->dev, RX, &bw );
        if (status < 0) {
            state->last_lib_error = status;
            rv = CMD_RET_LIBBLADERF;
        } else {
            printf( "  RX Bandwidth: %9uHz\n", bw );
        }
    }

    if( argc == 2 || module == TX ) {
        status = bladerf_get_bandwidth( state->dev, TX, &bw );
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
    bladerf_module module = RX;
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
        bw = str2uint( argv[3], 0, UINT_MAX, &ok );
        if( !ok ) {
            cli_err(state, argv[0], "Invalid bandwidth (%s)", argv[3]);
            rv = CMD_RET_INVPARAM;
        }
    }

    /* No module, just bandwidth */
    else if( argc == 3 ) {
        bool ok;
        bw = str2uint( argv[2], 0, UINT_MAX, &ok );
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
        if( argc == 3 || module == RX ) {
            status = bladerf_set_bandwidth( state->dev, RX, bw, &actual );
            if (status < 0) {
                state->last_lib_error = status;
                rv = CMD_RET_LIBBLADERF;
            } else {
                printf( "  Set RX bandwidth - req:%9uHz actual:%9uHz\n",  bw, actual );
            }
        }

        /* Lack of option, so set both or TX only */
        if( argc == 3 || module == TX ) {
            status = bladerf_set_bandwidth( state->dev, TX, bw, &actual );
            if (status < 0) {
                state->last_lib_error = status;
                rv = CMD_RET_LIBBLADERF;
            } else {
                printf( "  Set TX bandwidth - req:%9uHz actual:%9uHz\n", bw, actual );
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
    bladerf_module module = RX;
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
        if( argc == 2 || module == RX ) {
            status = bladerf_get_frequency( state->dev, RX, &freq );
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

        if( argc == 2 || module == TX ) {
            status = bladerf_get_frequency( state->dev, TX, &freq );
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
    bladerf_module module = RX;

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
        freq = str2uint( argv[argc-1], 225000000, 3900000000u, &ok );

        if( !ok ) {
            cli_err(state, argv[0], "Invalid frequency (%s)", argv[argc - 1]);
            rv = CMD_RET_INVPARAM;
        } else {

            printf( "\n" );

            /* Change RX frequency */
            if( argc == 3 || module == RX ) {
                int status = bladerf_set_frequency( state->dev, RX, freq );
                if (status < 0) {
                    state->last_lib_error = status;
                    rv = CMD_RET_LIBBLADERF;
                } else {
                    printf( "  Set RX frequency: %10uHz\n", freq );
                }
            }

            /* Change TX frequency */
            if( argc == 3 || module == TX ) {
                status = bladerf_set_frequency( state->dev, TX, freq );
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

    status = gpio_read( state->dev, &val );

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
            /* TODO: Should this be exposed at this level? */
            gpio_write( state->dev,val );
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
            case LNA_UNKNOWN: printf( "LNA_UNKNOWN\n" ); break;
            case LNA_MID    : printf( "LNA_MID\n" ); break;
            case LNA_MAX    : printf( "LNA_MAX\n" ); break;
            case LNA_BYPASS : printf( "LNA_BYPASS\n"); break;
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
        bladerf_lna_gain gain = LNA_UNKNOWN;
        if( strcasecmp( argv[2], "max" ) == 0 ) {
            gain = LNA_MAX;
        } else if( strcasecmp( argv[2], "mid" ) == 0 ) {
            gain = LNA_MID;
        } else if( strcasecmp( argv[2], "bypass" ) == 0 ) {
            gain = LNA_BYPASS;
        } else {
            invalid_gain(state, argv[0], argv[1], argv[2]);
        }

        if (gain != LNA_UNKNOWN) {
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
    return CMD_RET_OK;
}

int set_loopback(struct cli_state *state, int argc, char **argv)
{
    return CMD_RET_OK;
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
        printf( "  RXVGA1 Gain: %3d\n", gain );
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
        gain = str2int( argv[2], INT_MIN, INT_MAX, &ok );
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
        gain = str2int( argv[2], INT_MIN, INT_MAX, &ok );
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
    unsigned int rate;
    bladerf_module module;
    bool ok;

    if (argc == 3) {
        module = get_module(argv[2], &ok);
        if (!ok) {
            invalid_module(state, argv[0], argv[2]);
            status = CMD_RET_INVPARAM;
        } else {
            status = bladerf_get_sample_rate(state->dev, module, &rate);
            printf("  %s sample rate: %u\n", module == RX ? "RX" : "TX", rate);
        }
    } else if (argc == 2) {
        unsigned int tx_rate;
        status = bladerf_get_sample_rate(state->dev, RX, &rate);

        if (!status) {
            status = bladerf_get_sample_rate(state->dev, TX, &tx_rate);

            if (!status) {
                printf("  RX sample rate: %u\n", rate);
                printf("  TX sample rate: %u\n", tx_rate);
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
    /* Usage: set samplerate [rx|tx] <samplerate in Hz> */
    int rv = CMD_RET_OK;
    bladerf_module module  = RX;

    if( argc == 4 ) {
        /* Parse module */
        bool ok;
        module = get_module( argv[2], &ok );
        if( !ok ) {
            invalid_module(state, argv[0], argv[2]);
            rv = CMD_RET_INVPARAM;
        }
    } else if( argc != 3 ) {
        rv = CMD_RET_NARGS;
    }

    if( argc > 2 && rv == CMD_RET_OK ) {
        bool ok;
        unsigned int rate, actual;
        rate = str2uint( argv[argc-1], 160000, 40000000, &ok );

        if( !ok ) {
            cli_err(state, argv[0], "Invalid sample rate (%s)", argv[2]);
            rv = CMD_RET_INVPARAM;
        } else {
            printf( "\n" );
            if( argc == 3 || module == RX ) {
                int status = bladerf_set_sample_rate( state->dev,
                                                      RX, rate, &actual );

                if (status < 0) {
                    state->last_lib_error = status;
                    rv = CMD_RET_LIBBLADERF;
                } else {
                    printf( "  Setting RX sample rate - req: %9uHz, "
                            "actual: %9uHz\n", rate, actual );
                }
            }

            if( argc == 3 || module == TX ) {
                int status = bladerf_set_sample_rate( state->dev, TX,
                                                      rate, &actual );
                if (status < 0) {
                    state->last_lib_error = status;
                    rv = CMD_RET_LIBBLADERF;
                } else {
                    printf( "  Setting TX sample rate - req: %9uHz, "
                            "actual: %9uHz\n", rate, actual );
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
    uint8_t val = 0;
    if( argc != 3 ) {
        rv = CMD_RET_NARGS ;
    } else {
        if( strcasecmp( "internal", argv[2] ) == 0 ) {
            /* Disconnect the ADC input from the outside world */
            lms_spi_read( state->dev, 0x09, &val );
            val &= ~(1<<7) ;
            lms_spi_write( state->dev, 0x09, val );

            /* Turn on RXVGA2 */
            lms_spi_read( state->dev, 0x64, &val );
            val |= (1<<1) ;
            lms_spi_write( state->dev, 0x64, val );

        } else if( strcasecmp( "external", argv[2] ) == 0 ) {
            /* Turn off RXVGA2 */
            lms_spi_read( state->dev, 0x64, &val );
            val &= ~(1<<1) ;
            lms_spi_write( state->dev, 0x64, val );

            /* Connect the external ADC pins to the internal ADC input */
            lms_spi_read( state->dev, 0x09, &val );
            val |= (1<<7) ;
            lms_spi_write( state->dev, 0x09, val );
        } else {
            cli_err(state, argv[0], "Invalid sampling mode (%s)", argv[2] );
        }
    }
    return rv;
}

int print_sampling( struct cli_state *state, int argc, char **argv)
{
    int rv = CMD_RET_OK;
    uint8_t val = 0;
    int external = 0 ;
    if( argc != 2 ) {
        rv = CMD_RET_NARGS;
    } else {
        /* Read the ADC input mux */
        lms_spi_read( state->dev, 0x09, &val );
        external = val&(1<<7) ? 1 : 0;
        printf( "  %-20s%-20s\n", "RX ADC Switch:", external ? "Closed" : "Open" );

        /* Read the RXVGA2 module state */
        lms_spi_read( state->dev, 0x64, &val );
        external |= val&(1<<1) ? 0 : 2 ;
        printf( "  %-20s%-20s\n", "RXVGA2 Module:", external&2 ? "Powered down" : "Powered up" );

        /* If external has both those bits set, we are external */
        printf( "  %-20s%-20s\n", "Sampling:", external == 3 ? "External" : external == 0 ? "Internal" : "Weirdness" ) ;
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
            int status = dac_write( state->dev, val );
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
        gain = str2int( argv[2], INT_MIN, INT_MAX, &ok );

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
        gain = str2int( argv[2], INT_MIN, INT_MAX, &ok );
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
        set loopback <mode> [options]
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

