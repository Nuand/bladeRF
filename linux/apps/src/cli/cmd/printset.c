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

void invalid_argc( char *cmd, char *param, int argc )
{
    printf( "%s %s: Invalid number of arguments (%d)\n", cmd, param, argc );
    return;
}

int print_bandwidth(struct cli_state *state, int argc, char **argv)
{
    /* Usage: print bandwidth [<rx|tx>]*/
    int rv = CMD_RET_OK, ret;
    bladerf_module module = RX ;
    unsigned int bw ;
    if( argc == 2 ) {
        printf( "print bandwidth specialized help\n" ) ;
    }

    else if( argc == 3 ) {
        bool ok;
        module = get_module( argv[2], &ok );
        if( !ok ) {
            printf( "%s %s: %s is not TX or RX - printing both\n", argv[0], argv[1], argv[2] );
        }
    }

    printf( "\n" ) ;

    if( argc == 2 || module == RX ) {
        /* TODO: Check ret */
        ret = bladerf_get_bandwidth( state->curr_device, RX, &bw );
        printf( "  RX Bandwidth: %9uHz\n", bw );
    }

    if( argc == 2 || module == TX ) {
        /* TODO: Check ret */
        ret = bladerf_get_bandwidth( state->curr_device, TX, &bw );
        printf( "  TX Bandwidth: %9uHz\n", bw );
    }

    printf( "\n" );

    return rv;
}

int set_bandwidth(struct cli_state *state, int argc, char **argv)
{
    /* Usage: set bandwidth [rx|tx] <bandwidth in Hz> */
    int rv = CMD_RET_OK, ret;
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
            printf( "%s %s: %s is not TX or RX\n", argv[0], argv[1], argv[2] );
            rv = CMD_RET_INVPARAM;
        }

        /* Parse bandwidth */
        bw = str2uint( argv[3], 0, UINT_MAX, &ok );
        if( !ok ) {
            printf( "%s: %s is not a valid bandwidth\n", argv[0], argv[3] );
            rv = CMD_RET_INVPARAM;
        }
    }

    /* No module, just bandwidth */
    else if( argc == 3 ) {
        bool ok;
        bw = str2uint( argv[2], 0, UINT_MAX, &ok );
        if( !ok ) {
            printf( "%s %s: %s is not a valid bandwidth\n", argv[0], argv[1], argv[2] );
            rv = CMD_RET_INVPARAM;
        }
    }

    /* Weird number of arguments */
    else {
        invalid_argc( argv[0], argv[1], argc );
        rv = CMD_RET_INVPARAM;
    }

    /* Problem parsing arguments? */
    if( argc > 2 && rv == CMD_RET_OK ) {

        printf( "\n" );

        /* Lack of option, so set both or RX only */
        if( argc == 3 || module == RX ) {
            /* TODO: Check ret */
            ret = bladerf_set_bandwidth( state->curr_device, RX, bw, &actual );
            printf( "  Setting RX bandwidth - req:%9uHz actual:%9uHz\n",  bw, actual );
        }

        /* Lack of option, so set both or TX only */
        if( argc == 3 || module == TX ) {
            /* TODO: Check ret */
            ret = bladerf_set_bandwidth( state->curr_device, TX, bw, &actual );
            printf( "  Setting TX bandwidth - req:%9uHz actual:%9uHz\n", bw, actual );
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
    unsigned int freq;
    bladerf_module module = RX;
    if( argc == 3 ) {
        /* Parse module */
        bool ok;
        module = get_module( argv[2], &ok );
        if( !ok ) {
            printf( "%s %s: %s is not TX or RX\n", argv[0], argv[1], argv[2] );
            rv = CMD_RET_INVPARAM;
        }
    } else if( argc != 2 ) {
        invalid_argc( argv[0], argv[1], argc );
        rv = CMD_RET_INVPARAM;
    }

    if( rv == CMD_RET_OK ) {
        printf( "\n" );
        if( argc == 2 || module == RX ) {
            /* TODO: Check ret */
            int ret;
            ret = bladerf_get_frequency( state->curr_device, RX, &freq );
            printf( "  RX Frequency: %10uHz\n", freq );
        }

        if( argc == 2 || module == TX ) {
            /* TODO: Check ret */
            int ret;
            ret = bladerf_get_frequency( state->curr_device, TX, &freq );
            printf( "  TX Frequency: %10uHz\n", freq );
        }
        printf( "\n" );
    }

    return rv;
}

int set_frequency(struct cli_state *state, int argc, char **argv)
{
    /* Usage: set frequency [<rx|tx>] <frequency in Hz> */
    int rv = CMD_RET_OK;
    unsigned int freq;
    bladerf_module module = RX;

    if( argc == 2 ) {
        printf( "set frequency specialized help\n" );
    } else if( argc == 4 ) {
        /* Parse module */
        bool ok;
        module = get_module( argv[2], &ok );
        if( !ok ) {
            printf( "%s %s: %s is not TX or RX\n", argv[0], argv[1], argv[2] );
            rv = CMD_RET_INVPARAM;
        }
    } else if( argc != 3 ) {
        invalid_argc( argv[0], argv[1], argc );
        rv = CMD_RET_INVPARAM;
    }

    if( argc > 2 && rv == CMD_RET_OK ) {
        bool ok;
        /* Parse out frequency */
        freq = str2uint( argv[argc-1], 0, UINT_MAX, &ok );

        if( !ok ) {
            printf( "%s %s: %s is not a valid frequency\n", argv[0], argv[1], argv[argc-1] );
        } else {

            printf( "\n" );

            /* Change RX frequency */
            if( argc == 3 || module == RX ) {
                /* TODO: Check ret */
                int ret = bladerf_set_frequency( state->curr_device, RX, freq );
                printf( "  Set RX frequency: %10uHz\n", freq );
            }

            /* Change TX frequency */
            if( argc == 3 || module == TX ) {
                /* TODO: Check ret */
                int ret = bladerf_set_frequency( state->curr_device, TX, freq );
                printf( "  Set TX frequency: %10uHz\n", freq );
            }

            printf( "\n" );
        }
    }

    return rv;
}

int print_gpio(struct cli_state *state, int argc, char **argv)
{
    /* TODO: Can't be implemented until gpio_read() is implemented */
    return CMD_RET_OK;
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
            printf( "%s %s: %s is not a valid value\n", argv[0], argv[1], argv[2] );
            rv = CMD_RET_INVPARAM;
        } else {
            gpio_write( state->curr_device,val );
        }
    } else {
        /* TODO: Fill this in */
        printf( "set gpio specialized help\n" );
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
    int rv = CMD_RET_OK, ret;
    bladerf_lna_gain gain;
    if( argc != 2 ) {
        printf( "%s %s: Ignoring extra arguments\n", argv[0], argv[1] );
    }

    /* TODO: Check ret */
    ret = bladerf_get_lna_gain( state->curr_device, &gain );

    printf( "\n" );
    printf( "  LNA Gain: ");
    switch(gain) {
        case LNA_UNKNOWN: printf( "LNA_UNKNOWN\n" ); break;
        case LNA_MID    : printf( "LNA_MID\n" ); break;
        case LNA_MAX    : printf( "LNA_MAX\n" ); break;
        case LNA_BYPASS : printf( "LNA_BYPASS\n"); break;
    }
    printf( "\n" ) ;
    return rv;
}

int set_lnagain(struct cli_state *state, int argc, char **argv)
{
    int rv = CMD_RET_OK;
    if ( argc == 2 ) {
        printf( "set lnagain specialized help\n" );
    } else if( argc != 3 ) {
        printf( "%s %s: Invalid number of arguments (%d)\n", argv[0], argv[1], argc );
        rv = CMD_RET_INVPARAM;
    } else {
        int ret ;
        bladerf_lna_gain gain;
        if( strcasecmp( argv[2], "max" ) == 0 ) {
            gain = LNA_MAX;
        } else if( strcasecmp( argv[2], "mid" ) == 0 ) {
            gain = LNA_MID;
        } else if( strcasecmp( argv[2], "bypass" ) == 0 ) {
            gain = LNA_BYPASS;
        } else {
            printf( "%s %s: %s is not a valid gain setting, so using MAX instead\n", argv[0], argv[1], argv[2] );
            gain = LNA_MAX;
        }
        /* TODO: Check ret */
        ret = bladerf_set_lna_gain( state->curr_device, gain );
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
    int rv = CMD_RET_OK, ret, gain;
    if( argc != 2 ) {
        printf( "%s %s: Ignoring extra arguments\n", argv[0], argv[1] );
    }

    /* TODO: Check gain */
    ret = bladerf_get_rxvga1( state->curr_device, &gain );

    printf( "\n" );
    printf( "  RXVGA1 Gain: %3d\n", gain );
    printf( "\n" );
    return rv;
}

int set_rxvga1(struct cli_state *state, int argc, char **argv)
{
    int rv = CMD_RET_OK, ret, gain;
    if( argc == 2 ) {
        printf( "set rxvga1 specialized help\n" );
    } else if( argc != 3 ) {
        printf( "%s %s: Invalid number of arguments (%d)\n", argv[0], argv[1], argc );
        rv = CMD_RET_INVPARAM;
    } else {
        bool ok;
        gain = str2int( argv[2], INT_MIN, INT_MAX, &ok );
        if( !ok ) {
            printf( "%s %s: %s is not a valid gain\n", argv[0], argv[1], argv[2] );
            rv = CMD_RET_INVPARAM;
        } else {
            /* TODO: Check ret */
            ret = bladerf_set_rxvga1( state->curr_device, gain );
        }
    }
    return rv;
}

int print_rxvga2(struct cli_state *state, int argc, char **argv)
{
    int rv = CMD_RET_OK, ret, gain;
    if( argc != 2 ) {
        printf( "%s %s: Ignoring extra arguments\n", argv[0], argv[1] );
    }

    /* TODO: Check ret */
    ret = bladerf_get_rxvga2( state->curr_device, &gain );

    printf( "\n" );
    printf( "  RXVGA2 Gain: %3ddB\n", gain );
    printf( "\n" );

    return rv;
}

int set_rxvga2(struct cli_state *state, int argc, char **argv)
{
    int rv = CMD_RET_OK, ret, gain;
    if( argc == 2 ) {
        printf( "set rxvga2 specialized help\n" );
    } else if( argc != 3 ) {
        printf( "%s %s: Invalid number of arguments (%d)\n", argv[0], argv[1], argc );
        rv = CMD_RET_INVPARAM;
    } else {
        bool ok;
        gain = str2int( argv[2], INT_MIN, INT_MAX, &ok );
        if( !ok ) {
            printf( "%s %s: %s is not a valid gain\n", argv[0], argv[1], argv[2] );
            rv = CMD_RET_INVPARAM;
        } else {
            /* TODO: Check ret */
            ret = bladerf_set_rxvga2( state->curr_device, gain );
        }
    }
    return rv;
}

int print_samplerate(struct cli_state *state, int argc, char **argv)
{
    /* TODO: Can't do this until we can read back from the si5338 */
    return CMD_RET_OK;
}

int set_samplerate(struct cli_state *state, int argc, char **argv)
{
    /* Usage: set samplerate [rx|tx] <samplerate in Hz> */
    int rv = CMD_RET_OK;
    bladerf_module module  = RX;

    if( argc == 2 ) {
        printf( "set samplerate specific help\n" );
    } else if( argc == 4 ) {
        /* Parse module */
        bool ok;
        module = get_module( argv[2], &ok );
        if( !ok ) {
            printf( "%s %s: %s is not TX or RX\n", argv[0], argv[1], argv[2] );
            rv = CMD_RET_INVPARAM;
        }
    } else if( argc != 3 ) {
        invalid_argc( argv[0], argv[1], argc );
        rv = CMD_RET_INVPARAM;
    }

    if( argc > 2 && rv == CMD_RET_OK ) {
        bool ok;
        unsigned int rate, actual;
        rate = str2uint( argv[argc-1], 160000, 40000000, &ok );

        if( !ok ) {
            printf( "%s %s: %s is not a valid samplerate\n", argv[0], argv[1], argv[2] );
            rv = CMD_RET_INVPARAM;
        } else {
            printf( "\n" );
            if( argc == 3 || module == RX ) {
                /* TODO: Check ret */
                int ret;
                ret = bladerf_set_sample_rate( state->curr_device, RX, rate, &actual );
                printf( "  Setting RX sample rate - req: %9uHz actual: %9uHz\n", rate, actual );
            }

            if( argc == 3 || module == TX ) {
                /* TODO: Check ret */
                int ret;
                ret = bladerf_set_sample_rate( state->curr_device, TX, rate, &actual );
                printf( "  Setting TX sample rate - req: %9uHz actual: %9uHz\n", rate, actual );
            }
            printf( "\n" );

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
    /* TODO: Can't be implemented until dac_write() is written */
    return CMD_RET_OK;
}

int print_txvga1(struct cli_state *state, int argc, char **argv)
{
    /* Usage: print txvga1 */
    int rv = CMD_RET_OK, ret, gain;
    if( argc != 2 ) {
        printf( "%s: Ignoring %d extra arguments\n", argv[0], argc-2 );
    }

    ret = bladerf_get_txvga1( state->curr_device, &gain );

    printf( "\n" );
    printf( "  TXVGA1 Gain: %ddB\n", gain ) ;
    printf( "\n" );

    return rv;
}

int set_txvga1(struct cli_state *state, int argc, char **argv)
{
    /* Usage: set txvga1 <gain> */
    int rv = CMD_RET_OK, gain;
    if( argc == 2 ) {
        /* TODO: Write this better */
        printf( "set txvga1 specialized help\n" );
    }

    else if( argc == 3 ) {
        bool ok ;
        gain = str2int( argv[2], INT_MIN, INT_MAX, &ok );

        if( !ok ) {
            printf( "%s %s: %s is not a valid gain setting\n", argv[0], argv[1], argv[2] );
            rv = CMD_RET_INVPARAM;
        } else {
            int ret ;
            /* TODO: Check ret */
            printf( "  Setting TXVGA1 to %d\n", gain );
            ret = bladerf_set_txvga1( state->curr_device, gain );
        }
    }

    else {
        invalid_argc( argv[0], argv[1], argc );
        rv = CMD_RET_INVPARAM;
    }

    return rv;
}

int print_txvga2(struct cli_state *state, int argc, char **argv)
{
    /* Usage: print txvga2 */
    int rv = CMD_RET_OK;
    if( argc != 2 ) {
        invalid_argc( argv[0], argv[1], argc );
        rv = CMD_RET_INVPARAM;
    } else {
        int ret, gain;
        ret = bladerf_get_txvga2( state->curr_device, &gain );
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
    if( argc == 2 ) {
        printf( "specialized set_txvga2 help\n" );
    } else if( argc != 3 ) {
        invalid_argc( argv[0], argv[1], argc );
        rv = CMD_RET_INVPARAM;
    } else {
        bool ok;
        int gain;
        gain = str2int( argv[2], INT_MIN, INT_MAX, &ok );
        if( !ok ) {
            printf( "%s %s: %s is not a valid gain value\n", argv[0], argv[1], argv[2] );
            rv = CMD_RET_INVPARAM;
        } else {
            /* TODO: Check ret */
            int ret;
            ret = bladerf_set_txvga2( state->curr_device, gain );
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
    if( state->curr_device == NULL ) {
        rv = CMD_RET_NODEV;
    }
    else if( argc > 1 ) {
        struct printset_entry *entry = NULL;

        entry = get_printset_entry( argv[1] );

        if( entry ) {
            /* Call the parameter setting function */
            rv = entry->set(state, argc, argv);
        } else {
            /* Incorrect parameter to print */
            printf( "%s: %s is not a valid parameter to print\n", argv[0], argv[1] );
            rv = CMD_RET_INVPARAM;
        }
    } else {
        invalid_argc( argv[0], argv[1], argc);
        rv = CMD_RET_INVPARAM;
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

    if( state->curr_device == NULL ) {
        rv = CMD_RET_NODEV;
    } else {

        entry = get_printset_entry( argv[1] );

        if( entry ) {
            /* Call the parameter printing function */
            rv = entry->print(state, argc, argv);
        } else {
            /* Incorrect parameter to print */
            printf( "%s: %s is not a valid parameter to print\n", argv[0], argv[1] );
            rv = CMD_RET_INVPARAM;
        }
    }
    return rv;
}

