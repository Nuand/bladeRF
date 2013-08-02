#ifndef DEBUG_H
#define DEBUG_H
#ifdef DEBUG

#include <stdio.h>

#define _DEBUG_STRFY__(x) #x
#define _DEBUG_STRFY_(x) _DEBUG_STRFY__(x)


#define dbg_printf(...) do { \
    fprintf(stderr, \
        "[DEBUG " __FILE__ ":" _DEBUG_STRFY_(__LINE__) "] " __VA_ARGS__ ); \
} while(0)

#else

#define dbg_printf(...) do {} while (0)

#endif /* DEBUG */
#endif /* DEBUG_H */
