#include <stdio.h>

void invalid_address( char *fn, char *str ) {
    printf( "%s: %s is not a valid address to write\n", fn, str );
    return;
}

