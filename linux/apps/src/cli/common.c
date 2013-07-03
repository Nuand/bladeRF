#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include "cmd.h"

int str2int(const char *str, int min, int max, bool *ok)
{
    long value;
    char *endptr;

    errno = 0;
    value = strtol(str, &endptr, 0);

    if (errno != 0 || value < (long)min || value > (long)max ||
        endptr == str || *endptr != '\0') {

        if (ok) {
            *ok = false;
        }

        return 0;
    }

    if (ok) {
        *ok = true;
    }
    return (int)value;
}

unsigned int str2uint(const char *str,
                        unsigned int min, unsigned int max, bool *ok)
{
    long long value;
    char *endptr;

    errno = 0;
    value = strtoll(str, &endptr, 0);

    if (errno != 0 || value < (long long)min || value > (long long)max ||
        endptr == str || *endptr != '\0') {

        if (ok) {
            *ok = false;
        }

        return 0;
    }

    if (ok) {
        *ok = true;
    }

    return (unsigned int)value;
}

double str2double(const char *str, double min, double max, bool *ok)
{
    double value;
    char *endptr;

    errno = 0;
    value = strtod(str, &endptr);

    if (errno != 0 || value < min || value > max ||
        endptr == str || *endptr != '\0') {

        if (ok) {
            *ok = false;
        }

        return 0;
    }

    if (ok) {
        *ok = true;
    }

    return value;
}

void cli_err(struct cli_state *s, const char *pfx, const char *format, ...)
{
    va_list arg_list;
    const size_t lbuf_sz = 32;
    char lbuf[lbuf_sz];
    char *err;

    memset(lbuf, 0, lbuf_sz);

    /* If we're in a script, we can provide line number info */
    if (s->script) {
        snprintf(lbuf, lbuf_sz, " (line %d)", s->lineno);
    }

    err = calloc(strlen(lbuf) + strlen(pfx) + strlen(format) + 5, 1);
    if (err) {
        strcat(err, "\n");
        strcat(err, pfx);
        strcat(err, lbuf);
        strcat(err, ": ");
        strcat(err, format);
        strcat(err, "\n\n");
    } else {
        /* Just do the best we can if a memory allocation error occurs */
        fprintf(stderr, "\nYikes! Multiple errors occurred!\n");
    }

    va_start(arg_list, format);
    vfprintf(stderr, err, arg_list);
    va_end(arg_list);

    free(err);
}
