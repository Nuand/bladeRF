#include "cmd.h"
#include <stdio.h>

struct print_parameter {
    int (*exec)(struct cli_state *s);   /*<< Function pointer to thing that prints parameter */
    const char *name;                   /*<< String associated with parameter */
};

#define PARAM(x) { .exec = print_##x, .name = #x }

int print_bandwidth(struct cli_state *state) {
    return CMD_RET_OK;
}

int print_config(struct cli_state *state) {
    return CMD_RET_OK;
}

int print_frequency(struct cli_state *state) {
    return CMD_RET_OK;
}

int print_lmsregs(struct cli_state *state) {
    return CMD_RET_OK;
}

int print_loopback(struct cli_state *state) {
    return CMD_RET_OK;
}

int print_mimo(struct cli_state *state) {
    return CMD_RET_OK;
}

int print_pa(struct cli_state *state) {
    return CMD_RET_OK;
}

int print_pps(struct cli_state *state) {
    return CMD_RET_OK;
}
int print_refclk(struct cli_state *state) {
    return CMD_RET_OK;
}
int print_rxvga1(struct cli_state *state) {
    return CMD_RET_OK;
}
int print_rxvga2(struct cli_state *state) {
    return CMD_RET_OK;
}

int print_samplerate(struct cli_state *state) {
    return CMD_RET_OK;
}

int print_trimdac(struct cli_state *state) {
    return CMD_RET_OK;
}

int print_txvga1(struct cli_state *state) {
    return CMD_RET_OK;
}

int print_txvga2(struct cli_state *state) {
    return CMD_RET_OK;
}

struct print_parameter print_table[] = {
    PARAM(bandwidth),
    PARAM(config),
    PARAM(frequency),
    PARAM(lmsregs),
    PARAM(loopback),
    PARAM(mimo),
    PARAM(pa),
    PARAM(pps),
    PARAM(refclk),
    PARAM(rxvga1),
    PARAM(rxvga2),
    PARAM(samplerate),
    PARAM(trimdac),
    PARAM(txvga1),
    PARAM(txvga2),
    { .exec = NULL, .name = "" }
};

int cmd_print(struct cli_state *state, int argc, char **argv)
{
    /* Valid commands:
        print bandwidth
        print config
        print frequency
        print lmsregs
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
    if( argc == 2 ) {
        int i;
        struct print_parameter *param = NULL;
        for( i = 0; param == NULL && print_table[i].exec != NULL; ++i ) {
            if( strcasecmp( argv[1], print_table[i].name ) == 0 ) {
                param = &print_table[i];
            }
        }
        if( param ) {
            /* Call the parameter printing function */
            rv = param->exec(state);
        } else {
            /* Incorrect parameter to print */
            printf( "%s: %s is not a valid parameter to print\n", argv[0], argv[1] );
            rv = CMD_RET_INVPARAM;
        }
    } else {
        printf( "%s: Invalid number of parameters (%d)\n", argv[0], argc );
        rv = CMD_RET_INVPARAM;
    }
    return rv;
}
