#ifndef PEEKPOKE_H__
#define PEEKPOKE_H__

#define DAC_MAX_ADDRESS 127
#define LMS_MAX_ADDRESS 127
#define SI_MAX_ADDRESS  255

#define MAX_NUM_ADDRESSES 256
#define MAX_VALUE 255

/* Convenience shared function */
void invalid_address( char *fn, char *str );

#endif /* PEEKPOKE_H__ */

