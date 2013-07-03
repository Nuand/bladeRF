#ifndef PEEKPOKE_H__
#define PEEKPOKE_H__

#include "common.h"

#define DAC_MAX_ADDRESS 127
#define LMS_MAX_ADDRESS 127
#define SI_MAX_ADDRESS  255

#define MAX_NUM_ADDRESSES 256
#define MAX_VALUE 255


/* Convenience shared function */
void invalid_address(struct cli_state *s, char *fn, char *addr);

#endif /* PEEKPOKE_H__ */

