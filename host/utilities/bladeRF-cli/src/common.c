#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>

#include "cmd.h"
#include "cmd/rxtx.h"

struct cli_state *cli_state_create()
{
    struct cli_state *ret = malloc(sizeof(*ret));

    if (ret) {
        ret->dev = NULL;
        ret->last_lib_error = 0;
        ret->script = NULL;
        ret->lineno = 0;

        ret->rxtx_data = rxtx_data_alloc(ret);
        if (!ret->rxtx_data) {
            fprintf(stderr, "Failed to allocate RX/TX task data\n");
            free(ret);
            ret = NULL;
        }
    }

    return ret;
}

void cli_state_destroy(struct cli_state *s)
{
    if (s) {
        if (s->dev) {
            bladerf_close(s->dev);
        }

        rxtx_data_free(s->rxtx_data);
        free(s);
    }
}

bool cli_device_is_opened(struct cli_state *s)
{
    return s->dev != NULL;
}

bool cli_device_in_use(struct cli_state *s)
{
    return cli_device_is_opened(s) &&
            (rxtx_tx_task_running(s) || rxtx_rx_task_running(s));
}

void cli_err(struct cli_state *s, const char *pfx, const char *format, ...)
{
    va_list arg_list;
    char lbuf[32];
    char *err;
	int ret;

    memset(lbuf, 0, sizeof(lbuf));

    /* If we're in a script, we can provide line number info */
    if (s->script) {
        ret = snprintf(lbuf, sizeof(lbuf), " (line %d)", s->lineno);
        if(ret < 0) {
            lbuf[0] = '\0';
        } else if(((size_t)ret) >= sizeof(lbuf)) {
            lbuf[sizeof(lbuf)-1] = '\0';
        }
    }

    /* +6 --> 3 newlines, 2 chars padding, NUL terminator */
    err = calloc(strlen(lbuf) + strlen(pfx) + strlen(format) + 6, 1);
    if (err) {
        strcat(err, "\n");
        strcat(err, pfx);
        strcat(err, lbuf);
        strcat(err, ": ");
        strcat(err, format);
        strcat(err, "\n\n");

        va_start(arg_list, format);
        vfprintf(stderr, err, arg_list);
        va_end(arg_list);
        free(err);
    } else {
        /* Just do the best we can if a memory allocation error occurs */
        fprintf(stderr, "\nYikes! Multiple errors occurred!\n");
    }
}

void set_last_error(struct cli_error *e, enum error_type type, int error)
{
    pthread_mutex_lock(&e->lock);
    e->type = type;
    e->value = error;
    pthread_mutex_unlock(&e->lock);
}

void get_last_error(struct cli_error *e, enum error_type *type, int *error)
{
    pthread_mutex_lock(&e->lock);
    *type = e->type;
    *error = e->value;
    pthread_mutex_unlock(&e->lock);
}
