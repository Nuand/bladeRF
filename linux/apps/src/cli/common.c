#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <pthread.h>

#ifdef INTERACTIVE
#include <libtecla.h>
#endif

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

/* FIXME this function is *nix-specific */
char *to_path(FILE *f)
{
    size_t buff_size = 64;
    char buff[buff_size];
    ssize_t status;
    char *file_path = NULL;

    int fd = fileno(f);

    if (fd < 0) {
        return NULL;
    }

    file_path = calloc(PATH_MAX + 1, 1);
    if (file_path) {
        snprintf(buff, buff_size, "/proc/self/fd/%d", fd);
        status = readlink(buff, file_path, PATH_MAX);

        if (status < 0) {
            free(file_path);
            file_path = NULL;
        }
    }

    return file_path;
}

#ifdef INTERACTIVE
char *expand_file(const char *inpath) {
	ExpandFile *ef = NULL;
	FileExpansion *fe = NULL;
	char *ret = NULL;

	ef = new_ExpandFile();
	if (!ef) 
		return NULL;

	fe = ef_expand_file(ef, inpath, strlen(inpath));

	if (!fe)
		return NULL;

	if (fe->nfile <= 0)
		return NULL;

	ret = strdup(fe->files[0]);

	del_ExpandFile(ef);

	return ret;
}
#else
char *expand_file(const char *inpath) {
	return strdup(inpath);
}
#endif
