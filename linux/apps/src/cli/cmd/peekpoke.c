#include "common.h"

void invalid_address(struct cli_state *s, char *fn, char *str) {
    cli_err(s, fn, "%s is an invalid address", str);
}

