/**
 * @filename rxtx.c
 *
 * Supported commands:
 *      rx <options>
 *      tx <options>
 *
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <limits.h>
#include <endian.h>
#include <errno.h>
#include <assert.h>
#include "cmd.h"

#define RXTX_CMDSTR_START "start"
#define RXTX_CMDSTR_STOP  "stop"
#define RXTX_CMDSTR_CFG   "config"
#define RXTX_DELIM        " \t\r\n"

#define RXTX_PARAM_FILE         "file"
#define RXTX_PARAM_NUMSAMPLES   "n"
#define RXTX_PARAM_FILEFORMAT   "format"
#define RXTX_PARAM_REPEAT       "repeat"
#define RXTX_PARAM_REPEATDLY    "delay"


#define RXTX_ERRMSG_VALUE(param, value) \
    "Invalid value for \"%s\" (%s)", param, value

/* TODO fetch this from the library (or even better, remove this restriction
 *      from the library?) */
#define LIBBLADERF_SAMPLE_BLOCK_SIZE    1024

/* Size of TX buffer, in elements (NOT bytes) */
#ifndef RXTX_TX_BUFF_SIZE
#   define RXTX_TX_BUFF_SIZE  (2 * LIBBLADERF_SAMPLE_BLOCK_SIZE)
#endif


/* Size of RX buffer, in elements (NOT bytes) */
#ifndef RXTX_RX_BUFF_SIZE
#   define RXTX_RX_BUFF_SIZE  (2 * LIBBLADERF_SAMPLE_BLOCK_SIZE)
#endif

#ifndef EOL
#   define EOL "\n"
#endif

#if defined(__BIG_ENDIAN__)
#   define RXTX_FMT_BINHOST_C16 RXTX_FMT_BINBE_C16
#elif defined(__LITTLE_ENDIAN)
#   define RXTX_FMT_BINHOST_C16 RXTX_FMT_BINLE_C16
#else
#   error "Compiler did not define __BIG/LITTLE_ENDIAN__ - required here"
#endif

#define TMP_FILE_NAME "/tmp/bladeRF-XXXXXX"

enum rxtx_fmt {
    RXTX_FMT_INVALID = -1,
    RXTX_FMT_CSV_C16,   /* CSV (Comma-separated, one entry per line) c16 I,Q */
    RXTX_FMT_BINLE_C16, /* Binary (little-endian), c16 I,Q */
    RXTX_FMT_BINBE_C16, /* Binary (big-endian), c16 I,Q */
};

enum rxtx_cmd {
    RXTX_CMD_INVALID,
    RXTX_CMD_START,
    RXTX_CMD_STOP,
    RXTX_CMD_CFG,
};

enum rxtx_state {
    RXTX_STATE_IDLE,
    RXTX_STATE_RUNNING,
    RXTX_STATE_SHUTDOWN,
    RXTX_STATE_ERROR,
};

struct common_cfg
{

    /* Once a task transitions into the RUNNING state, it takes ownership
     * of the file by locking file_lock. */
    FILE    *file;              /* File to read from or write to. */
    pthread_mutex_t file_lock;  /* Lock access to file, file_path */



    int16_t *buff;
    size_t buff_size;      /* Size in elements, NOT bytes */

    struct cli_error error; /* Last error the task encountered. Use
                               routines to read/manipulte - do not edit
                               directly. */

    pthread_t thread;
    enum rxtx_state task_state; /* Task state. Access should be locked */
    pthread_mutex_t task_lock;
    pthread_cond_t  task_state_changed;

    /* This lock should be used when accesing the below items,
     * and those items in the rx_cfg and tx_cfg structures */
    pthread_mutex_t param_lock;
    char *file_path;            /* Track which file we've opened */
    enum rxtx_fmt file_fmt;     /* Format of file data */
};

struct rx_cfg
{
    struct common_cfg common;
    unsigned int n_samples;

    /* Should not be touched while rx task is running */
    int (*write_samples)(struct cli_state *s, size_t n_samples);
};

struct tx_cfg
{
    struct common_cfg common;
    unsigned int repeat;
    unsigned int repeat_delay;

    /* Should not be touched while tx task is running */
    ssize_t (*read_samples)(struct cli_state *s, unsigned int *repeats_left);
};

struct rxtx_data
{
    struct rx_cfg rx;
    struct tx_cfg tx;
};

static inline void set_state(struct common_cfg *cfg,
                                enum rxtx_state state, bool notify)
{
    pthread_mutex_lock(&cfg->task_lock);
    cfg->task_state = state;
    pthread_mutex_unlock(&cfg->task_lock);

    if (notify) {
        pthread_cond_signal(&cfg->task_state_changed);
    }
}

static inline enum rxtx_state get_state(struct common_cfg *cfg)
{
    enum rxtx_state ret;

    pthread_mutex_lock(&cfg->task_lock);
    ret = cfg->task_state;
    pthread_mutex_unlock(&cfg->task_lock);

    return ret;
}

static int set_sample_file_path(struct common_cfg *c, const char *file_path)
{
    int status = 0;

    pthread_mutex_lock(&c->file_lock);

    if (c->file_path) {
        free(c->file_path);
    }

    /* c->file_path = strdup(file_path); */
	c->file_path = expand_file(file_path);
    if (!c->file_path) {
        status = CMD_RET_MEM;
    }

    pthread_mutex_unlock(&c->file_lock);

    return status;
}

/* If the provided filename is NULL, attempt to use the one cached in the
 * current state information */
static int open_samples_file(struct common_cfg *c, const char *filename,
                                const char *mode)
{
    int ret = 0;

    pthread_mutex_lock(&c->file_lock);

    if (c->file == NULL) {
        if (filename == NULL && c->file_path != NULL) {
            c->file = fopen(c->file_path, mode);

            if (!c->file) {
                ret = CMD_RET_FILEOP;
            } else {
                ret = 0;
            }

        } else if (filename != NULL) {

            if (c->file_path) {
                free(c->file_path);
            }

            c->file = fopen(filename, mode);
            if (c->file) {
                c->file_path = strdup(filename);

                if (!c->file_path)
                {
                    fclose(c->file);
                    c->file = NULL;
                    ret = CMD_RET_MEM;
                }
            } else {
                ret = CMD_RET_FILEOP;
            }

        } else {
            /* This condition is indicative of a bug */
            assert(0);
            ret = CMD_RET_UNKNOWN;
        }

    } else {
        /* This condition is indicative of a bug --
         * Attempted to open another file while the current one is still open */
        assert(0);
        ret = CMD_RET_UNKNOWN;
    }

    pthread_mutex_unlock(&c->file_lock);

    return ret;
}

static int close_samples_file(struct common_cfg *c, bool lock)
{
    int ret;

    if (lock) {
        pthread_mutex_lock(&c->file_lock);
    }

    if (c->file) {
        fclose(c->file);
        c->file = NULL;
        ret = 0;
    } else {
        /* This case shouldn't happen */
        assert(0);
        ret = CMD_RET_UNKNOWN;
    }

    if (lock) {
        pthread_mutex_unlock(&c->file_lock);
    }

    return ret;
}

/*
 * n    - # samples
 * fmt  - binle or binbe
 * in   - Is this data being TX'd (RX'd assumed otherwise)
 */
static void c16_sample_fixup(int16_t *buff, size_t n,
                                enum rxtx_fmt fmt, bool tx)
{
    size_t i;

    /* For each sample, we need to:
     *  (1) Convert to appropriate endianness
     *  (2, RX only) Mask and sign-extend.
     *      FIXME  this will soon be done in libbladeRF
     *
     *  The 4 permutations are unrolled here intentionally, to keep
     *  the amount of massaging on these samples to a minimum...
     */

    if (tx) {
        if (fmt == RXTX_FMT_BINLE_C16) {
            for (i = 0; i < n; i++) {
                /* I - Correct sign extension is assumed */
                *buff = (htole16(*buff) & 0x0fff);
                buff++;

                /* Q - Correct sign extention is assumed*/
                *buff = (htole16(*buff) & 0x0fff);
                buff++;
            }
        } else {
            for (i = 0; i < n; i++) {
                /* I - Correct sign extension is assumed */
                *buff = (htobe16(*buff) & 0x0fff);
                buff++;

                /* Q - Correct sign extention is assumed*/
                *buff = (htobe16(*buff) & 0x0fff);
                buff++;
            }
        }
    } else {
        if (fmt == RXTX_FMT_CSV_C16) {
            fmt = RXTX_FMT_BINHOST_C16;
        }

        if (fmt == RXTX_FMT_BINLE_C16) {
            for (i = 0; i < n; i++) {
                /* I - Mask off the marker and sign extend */
                *buff = htole16(*buff) & 0x0fff;
                if (*buff & 0x800) {
                    *buff |= 0xf000;
                }
                buff++;

                /* Q - Mask off the marker and sign extend */
                *buff = htole16(*buff) & 0x0fff;
                if (*buff & 0x800) {
                    *buff |= 0xf000;
                }
                buff++;

            }
        } else {
            for (i = 0; i < n; i++) {
                /* I - Mask off the marker and sign extend */
                *buff = htobe16(*buff) & 0x0fff;
                if (*buff & 0x800) {
                    *buff |= 0xf000;
                }
                buff++;

                /* Q - Mask off the marker and sign extend */
                *buff = htobe16(*buff) & 0x0fff;
                if (*buff & 0x800) {
                    *buff |= 0xf000;
                }
                buff++;
            }
        }
    }
}

/* returns 0 on success, -1 on failure (and calls set_last_error()) */
static int rxtx_write_bin_c16(struct cli_state *s, size_t n_samples)
{
    size_t status;
    struct rx_cfg *rx = &s->rxtx_data->rx;
    struct common_cfg *common = &rx->common;

    /* We assume we have pairs... */
    assert((common->buff_size & 1) == 0);

    c16_sample_fixup(common->buff, common->buff_size/2, common->file_fmt, rx);

    status = fwrite(common->buff, sizeof(int16_t),
                    common->buff_size, common->file);

    if (status < common->buff_size) {
        set_last_error(&common->error, ETYPE_ERRNO, errno);
        return -1;
    } else {
        return 0;
    }
}

/* returns 0 on success, -1 on failure (and calls set_last_error()) */
static int rxtx_write_csv_c16(struct cli_state *s, size_t n_samples)
{
    size_t i;
    struct rx_cfg *rx = &s->rxtx_data->rx;
    struct common_cfg *common = &rx->common;
    const size_t line_sz = 32;
    const size_t to_write = n_samples * 2;
    char line[line_sz];

    /* We assume we have pairs... */
    assert((common->buff_size & 1) == 0);

    c16_sample_fixup(common->buff, common->buff_size/2, common->file_fmt, false);

    for (i = 0; i < to_write; i += 2) {
        snprintf(line, line_sz, "%d, %d" EOL,
                 common->buff[i], common->buff[i + 1]);

        if (fputs(line, common->file) < 0) {
            set_last_error(&common->error, ETYPE_ERRNO, errno);
            return -1;
        }
    }

    return 0;
}

/*
 * Fill buffer with samples from file contents, 0-padding if appropriate
 *
 * returns: # IQ values on success,
 *          -1 on error (and calls set_last_error())
 */
static ssize_t rxtx_read_bin_c16(struct cli_state *s,
                                    unsigned int *repeats_left)
{
    ssize_t ret = 0;
    struct tx_cfg *tx;
    size_t rd_ret, n_read = 0;
    size_t rd_size;

    tx = &s->rxtx_data->tx;

    /* We'll keep gobbling up samples from the file until one of the following:
     *   (1) We fill our buffer
     *   (2) We hit an error
     *   (3) We hit an EOF and we have no repeats left
     *   (4) We hit an EOF and repeat-delay > 0 ("burst mode")
     *
     * In cases (3) and (4) we must zero pad the buffer do to interface reqts.
     */

    while (n_read < tx->common.buff_size) {
        rd_size = tx->common.buff_size - n_read;
        rd_ret = fread(tx->common.buff + n_read, sizeof(int16_t),
                        rd_size, tx->common.file);

        if (rd_ret != rd_size && ferror(tx->common.file)) {
            set_last_error(&tx->common.error, ETYPE_ERRNO, errno);
            ret = -1;
            break;
        }

        /* We require IQ *pairs*, so throw away last value if we have
         * an odd number of values. Otherwise, this can cause problems
         * when we wrap back around in the file... */
        n_read += rd_ret & ~0x1;

        if (n_read < tx->common.buff_size) {

            /* This should be the only way we got here... */
            assert(feof(tx->common.file));

            *repeats_left -= 1;

            /* Cases (3) - our work here is done */
            if (*repeats_left == 0) {
                ret = n_read;
                break;
            } else if (repeats_left != 0 ) {
                /* Wrap back around to start of file if we're repeating */
                if (fseek(tx->common.file, 0, SEEK_SET) < 0) {
                    set_last_error(&tx->common.error, ETYPE_ERRNO, errno);
                    ret = -1;
                    break;
                } else if (tx->repeat_delay != 0) {
                    /* Case (4) */
                    ret = n_read;
                    break;
                } /* else, we'll keep looping to fill out buffer since we
                   * have no repeat delay */
            }
        } else {
            /* Case (1) */
            ret = n_read;
        }
    }

    if (ret > 0) {
        /* Zero padding for cases (3) and (4) */
        memset(tx->common.buff + n_read, 0, tx->common.buff_size - n_read);
        c16_sample_fixup(tx->common.buff, tx->common.buff_size/2,
                            tx->common.file_fmt, true);
        ret = tx->common.buff_size;
    }

    return ret;
}

/* Create a temp (binary) file from a CSV so we don't have to waste time
 * parsing it in between sending samples.
 *
 * Preconditions:  Assumes CLI state's TX cfg data has a valid file descriptor
 *
 * Postconditions: TX cfg's file descriptor, filename, and format will be
 *                  changed.
 *
 * return 0 on success, CMD_RET_* on failure
 */
static int tx_csv_to_c16(struct cli_state *s)
{
    const char delim[] = " \r\n\t,.:";
    const size_t buff_size = 81;
    char buff[buff_size];
    enum rxtx_fmt fmt = RXTX_FMT_BINHOST_C16;
    struct tx_cfg *tx = &s->rxtx_data->tx;
    char *token, *saveptr;
    int tmp_int;
    int16_t tmp_iq[2];
    bool ok;
    int ret;
    FILE *bin;
    int bin_fd;
    char *bin_path;
    char bin_name[] = TMP_FILE_NAME;

    ret = 0;


    bin_fd = mkstemp(bin_name);
    if (!bin_fd) {
        return CMD_RET_FILEOP;
    }

    bin = fdopen(bin_fd, "wb");
    if (!bin) {
        return CMD_RET_FILEOP;
    }

    bin_path = to_path(bin);
    if (!bin_path) {
        return CMD_RET_FILEOP;
    }

    while (fgets(buff, buff_size, tx->common.file))
    {
        /* I */
        token = strtok_r(buff, delim, &saveptr);

        if (token) {
            tmp_int = str2int(token, INT16_MIN, INT16_MAX, &ok);

            if (ok) {
                tmp_iq[0] = tmp_int;
            } else {
                cli_err(s, "tx", "Invalid I value encountered in CSV file");
                ret = CMD_RET_INVPARAM;
                break;
            }

            /* Q */
            token = strtok_r(NULL, delim, &saveptr);

            if (token) {
                tmp_int = str2int(token, INT16_MIN, INT16_MAX, &ok);

                if (ok) {
                    tmp_iq[1] = tmp_int;
                } else {
                    cli_err(s, "tx", "Invalid Q value encountered in CSV file");
                    ret = CMD_RET_INVPARAM;
                    break;
                }

            } else {
                cli_err(s, "tx", "Missing Q value in CSV file");
                ret = CMD_RET_INVPARAM;
                break;
            }

            /* Check for extraneous tokens */
            token = strtok_r(NULL, delim, &saveptr);
            if (!token) {
                if (fwrite(tmp_iq, sizeof(tmp_iq[0]), 2, bin) != 2) {
                    ret = CMD_RET_FILEOP;
                    break;
                }
            } else {
                cli_err(s, "tx",
                        "Encountered extra token after Q value in CSV file");

                ret = CMD_RET_INVPARAM;
                break;
            }
        }
    }

    if (ret >= 0 && feof(tx->common.file)) {

        fclose(tx->common.file);
        tx->common.file = bin;
        tx->common.file_fmt = fmt;

        free(tx->common.file_path);
        tx->common.file_path = bin_path;

    } else {
        ret = -1;
        fclose(bin);
        free(bin_path);
    }

    return ret;
}

/* FIXME Clean up this function up. It's very unreadable.
 *       Suggestions include moving to switch() and possibly adding some
 *       intermediary states. (More smaller, simpler states);
 */
static void *rx_task(void *arg) {
    int lib_ret, write_ret;
    enum rxtx_state state, prev_state;
    struct cli_state *s = (struct cli_state *) arg;
    struct rx_cfg *rx = &s->rxtx_data->rx;
    unsigned int n_samples_left = 0;
    bool inf;
    size_t to_write = 0;

    /* We expect to be in the IDLE state when this task is kicked off */
    state = prev_state = get_state(&rx->common);
    assert(state == RXTX_STATE_IDLE);

    while (state != RXTX_STATE_SHUTDOWN) {
        if (state == RXTX_STATE_RUNNING) {

            /* Transitioning from IDLE/ERROR -> RUNNING */
            if (prev_state == RXTX_STATE_IDLE ||
                prev_state == RXTX_STATE_ERROR) {

                pthread_mutex_lock(&rx->common.param_lock);
                n_samples_left = rx->n_samples;
                pthread_mutex_unlock(&rx->common.param_lock);

                inf = n_samples_left == 0;

                /* Task owns file while running */
                pthread_mutex_lock(&rx->common.file_lock);

                /* Flush out any old samples before recording any data */
                lib_ret = bladerf_read_c16(s->dev, rx->common.buff,
                                                   rx->common.buff_size / 2);

                if (lib_ret < 0) {
                    set_last_error(&rx->common.error, ETYPE_BLADERF, lib_ret);
                    state = RXTX_STATE_ERROR;
                }
            } else {
                lib_ret = bladerf_read_c16(s->dev, rx->common.buff,
                                           rx->common.buff_size / 2);

                if (lib_ret < 0) {
                    set_last_error(&rx->common.error, ETYPE_BLADERF, lib_ret);
                    state = RXTX_STATE_ERROR;
                } else {

                    /* Bug catcher */
                    assert((size_t)lib_ret <= (rx->common.buff_size / 2));

                    if (!inf) {
                        to_write = uint_min(n_samples_left, lib_ret);
                    } else {
                        to_write = rx->common.buff_size / 2;
                    }

                    write_ret = rx->write_samples(s, to_write);

                    if (write_ret < 0) {
                        /* write_samples will have set the last error info */
                        state = RXTX_STATE_ERROR;
                    } else if (!inf) {
                        n_samples_left -= to_write;
                        if (n_samples_left == 0) {
                            state = RXTX_STATE_IDLE;
                        }
                    }
                }
            }

        }

        if (state != RXTX_STATE_RUNNING && prev_state == RXTX_STATE_RUNNING) {
            /* Clean up as we transition from RUNNING -> <any other state> */
            lib_ret = bladerf_enable_module(s->dev, RX, false);
            close_samples_file(&rx->common, false);
            pthread_mutex_unlock(&rx->common.file_lock);

            if (lib_ret < 0) {
                set_last_error(&rx->common.error, ETYPE_BLADERF, lib_ret);
                state = RXTX_STATE_ERROR;
            }

            set_state(&rx->common, state, false);
        }


        prev_state = state;

        /* Wait here while if we're in the IDLE/ERROR states */
        pthread_mutex_lock(&rx->common.task_lock);
        while (state == RXTX_STATE_IDLE || state == RXTX_STATE_ERROR) {
            pthread_cond_wait(&rx->common.task_state_changed,
                                &rx->common.task_lock);

            state = rx->common.task_state;
        }

        state = rx->common.task_state;
        pthread_mutex_unlock(&rx->common.task_lock);
    }

    return NULL;
}

/* FIXME Clean up this function up. It's very unreadable.
 *       Suggestions include moving to switch() and possibly adding some
 *       intermediary states. (More smaller, simpler states);
 */
static void *tx_task(void *arg) {
    enum rxtx_state state, prev_state;
    struct cli_state *s = (struct cli_state *) arg;
    struct tx_cfg *tx = &s->rxtx_data->tx;
    ssize_t read_ret;
    int lib_ret;
    unsigned int last_repeats_left;
    unsigned int repeats_left = 0;
    unsigned int repeat_delay = 0;
    bool repeats_inf = false;


    /* We expect to be in the IDLE state when this task is kicked off */
    state = prev_state = get_state(&tx->common);
    assert(state == RXTX_STATE_IDLE);

    while (state != RXTX_STATE_SHUTDOWN) {

        if (state == RXTX_STATE_RUNNING) {

            /* Transitioning from IDLE/ERROR -> RUNNING */
            if (prev_state == RXTX_STATE_IDLE ||
                prev_state == RXTX_STATE_ERROR) {

                pthread_mutex_lock(&tx->common.param_lock);
                repeats_left = tx->repeat;
                repeat_delay = tx->repeat_delay;
                pthread_mutex_unlock(&tx->common.param_lock);

                repeats_inf = repeats_left == 0;

                /* Task owns the file while running */
                pthread_mutex_lock(&tx->common.file_lock);

            } else {

                /* Fetch samples to write */
                last_repeats_left = repeats_left;
                read_ret = tx->read_samples(s, &repeats_left);

                if (read_ret == (2 * LIBBLADERF_SAMPLE_BLOCK_SIZE) ||
                    read_ret == 0) {

                    if (read_ret != 0) {
                        lib_ret = bladerf_send_c16(s->dev,
                                                   tx->common.buff,
                                                   tx->common.buff_size / 2);

                        if (lib_ret < 0) {
                            set_last_error(&tx->common.error,
                                    ETYPE_BLADERF, lib_ret);
                            state = RXTX_STATE_ERROR;
                        }
                    }

                    if (repeats_inf || repeats_left != 0) {
                        if (repeats_left != last_repeats_left &&
                            repeat_delay) {

                            /* Currently seeing timeouts from the driver when
                             * attempting to disable/enable TX here...
                             *
                             * FIXME re-enable this after driver issues are
                             *       resolved...
                             */
#if 0
                            lib_ret = bladerf_enable_module(s->dev, TX, false);
                            if (lib_ret < 0) {
                                set_last_error(&tx->common.error,
                                                ETYPE_BLADERF, lib_ret);
                                state = RXTX_STATE_ERROR;
                            } else {
#endif
                                /* TODO replace with 0-send here for better
                                 * accuracy, as usleep isn't sufficient here */

                                usleep(repeat_delay);
#if 0

                                lib_ret = bladerf_enable_module(s->dev, TX,
                                                                true);
                                if (lib_ret < 0) {
                                    set_last_error(&tx->common.error,
                                            ETYPE_BLADERF, lib_ret);
                                    state = RXTX_STATE_ERROR;
                                }
                            }
#endif
                        }
                    } else {
                        state = RXTX_STATE_IDLE;
                    }

                } else {
                    /* Reads of an in-between size are a bug */
                    assert(read_ret <= 0);

                    /* read-samples will have filled out "last error" */
                    state = RXTX_STATE_ERROR;
                }
            }

        }

        /* Clean up as we transition from RUNNING -> <any other state> */
        if (state != RXTX_STATE_RUNNING && prev_state == RXTX_STATE_RUNNING) {
            lib_ret = bladerf_enable_module(s->dev, TX, false);
            close_samples_file(&tx->common, false);
            pthread_mutex_unlock(&tx->common.file_lock);

            if (lib_ret < 0) {
                set_last_error(&tx->common.error, ETYPE_BLADERF, lib_ret);
                state = RXTX_STATE_ERROR;
            }

            set_state(&tx->common, state, false);
        }

        prev_state = state;

        /* Wait here while if we're in the IDLE/ERROR states */
        pthread_mutex_lock(&tx->common.task_lock);
        while (state == RXTX_STATE_IDLE || state == RXTX_STATE_ERROR) {
            pthread_cond_wait(&tx->common.task_state_changed,
                                &tx->common.task_lock);

            state = tx->common.task_state;
        }

        state = tx->common.task_state;
        pthread_mutex_unlock(&tx->common.task_lock);
    }

    return NULL;
}

static int validate_config(struct cli_state *s, const char *cmd,
                            struct rxtx_data *d, bool is_tx)
{
    int status = CMD_RET_OK;
    struct common_cfg *common = is_tx ? &d->tx.common : &d->rx.common;
    bool file_path_set;

    pthread_mutex_lock(&common->file_lock);
    file_path_set = common->file_path != NULL;
    pthread_mutex_unlock(&common->file_lock);

    if (!file_path_set) {
        cli_err(s, cmd, "File parameter has not been configured");
        status = CMD_RET_INVPARAM;
    } else if (common->file_fmt == RXTX_FMT_INVALID) {
        cli_err(s, cmd, "File format parameter has not been configured");
        status = CMD_RET_INVPARAM;
    }

    return status;
}

static void print_rx_config(struct rx_cfg *c)
{
    if (c->n_samples != 0) {
        printf("    # Samples: %u\n", c->n_samples);
    } else {
        printf("    # Samples: inf\n");
    }
}

static void print_tx_config(struct tx_cfg *c)
{
    if (c->repeat == 0) {
        printf("    Repeat: until stopped\n");
    } else {
        printf("    Repeat: %u iterations\n", c->repeat);
    }

    if (c->repeat_delay == 0) {
        printf("    Repeat delay: none\n");
    } else {
        printf("    Repeat delay: %u us\n", c->repeat_delay);
    }
}

static void print_state(enum rxtx_state state, int error, int error_type)
{
    switch (state) {
        case RXTX_STATE_IDLE:
            printf("    State: Idle\n");
            break;

        case RXTX_STATE_ERROR:
            if (error_type == ETYPE_ERRNO) {
                printf("    State: Error -- %s\n", strerror(error));
            } else if (error_type == ETYPE_BLADERF) {
                printf("    State: Error -- %s\n", bladerf_strerror(error));
            } else if (error_type == ETYPE_CLI) {
                printf("    State: Error -- %s\n", cmd_strerror(error, 0));
            } else {
                printf("    State: Error -- (%d)\n", error);
            }
            break;

        case RXTX_STATE_SHUTDOWN:
            printf("    State: Shutting down\n");
            break;

        case RXTX_STATE_RUNNING:
            printf("    State: Running\n");
            break;

        default:
            printf("    State: Invalid/Unknown (BUG)\n");
    }
}

static void print_config(struct rxtx_data *d, bool is_tx)
{
    enum rxtx_state state;
    int last_error;
    enum error_type last_error_type;
    struct common_cfg *common = is_tx ? &d->tx.common : &d->rx.common;

    state = get_state(common);
    get_last_error(&common->error, &last_error_type, &last_error);


    printf("\n");
    print_state(state, last_error, last_error_type);

    /* FIXME We generally lock file_lock, but currently we can get away
     *       with this since the tasks don't touch this. Revisit this
     *       and the associated lock */
    printf("    File: %s\n",
            common->file_path != NULL ? common->file_path : "Not configured");

    switch (common->file_fmt) {
        case RXTX_FMT_CSV_C16:
            printf("    Format: C16, CSV\n");
            break;
        case RXTX_FMT_BINLE_C16:
            printf("    Format: C16, Binary (Little Endian)\n");
            break;
        case RXTX_FMT_BINBE_C16:
            printf("    Format: C16, Binary (Big Endian)\n");
            break;
        default:
            printf("    Format: Not configured\n");
    }

    if (is_tx) {
        print_tx_config(&d->tx);
    } else {
        print_rx_config(&d->rx);
    }

    printf("\n");
}


static enum rxtx_cmd get_cmd(const char *cmd)
{
    int ret = RXTX_CMD_INVALID;

    if (!strcasecmp(cmd, RXTX_CMDSTR_START)) {
        ret = RXTX_CMD_START;
    } else if (!strcasecmp(cmd, RXTX_CMDSTR_STOP)) {
        ret = RXTX_CMD_STOP;
    } else if (!strcasecmp(cmd, RXTX_CMDSTR_CFG)) {
        ret = RXTX_CMD_CFG;
    }

    return ret;
}

static enum rxtx_fmt str2fmt(const char *str)
{
    enum rxtx_fmt ret = RXTX_FMT_INVALID;

    if (!strcasecmp("csv", str)) {
        ret = RXTX_FMT_CSV_C16;
    } else if (!strcasecmp("binl", str)) {
        ret = RXTX_FMT_BINLE_C16;
    } else if (!strcasecmp("binb", str)) {
        ret = RXTX_FMT_BINBE_C16;
    } else if (!strcasecmp("bin", str)) {
        ret = RXTX_FMT_BINHOST_C16;
    }

    return ret;
}

static int handle_tx_param(struct cli_state *s,
                                const char *param, const char *value)
{
    bool ok = false;
    unsigned int tmp_uint;

    if (!strcasecmp(RXTX_PARAM_REPEAT, param)) {
        tmp_uint = str2uint(value, 0, UINT_MAX, &ok);
        if (ok) {
            s->rxtx_data->tx.repeat = tmp_uint;
        }

    } else if (!strcasecmp(RXTX_PARAM_REPEATDLY, param)) {
        tmp_uint = str2uint(value, 0, UINT_MAX, &ok);
        if (ok) {
            s->rxtx_data->tx.repeat_delay = tmp_uint;
        }
    }

    if (!ok) {
        cli_err(s, "tx", RXTX_ERRMSG_VALUE(param, value));
        return CMD_RET_INVPARAM;
    } else {
        return 0;
    }
}

static int handle_rx_param(struct cli_state *s,
                                const char *param, const char *value)
{
    unsigned int tmp_uint;
    bool ok = false;

    if (!strcasecmp(RXTX_PARAM_NUMSAMPLES, param)) {
        tmp_uint = str2uint(value, 0, INT_MAX, &ok);
        if (ok) {
            s->rxtx_data->rx.n_samples = tmp_uint;
        }
    }

    if (!ok) {
        cli_err(s, "rx", RXTX_ERRMSG_VALUE(param, value));
        return CMD_RET_INVPARAM;
    } else {
        return 0;
    }

}

static int handle_params(struct cli_state *s, int argc, char **argv, bool is_tx)
{
    int i, status;
    char *val;
    char file_mode[3];

    struct common_cfg *common;

    if (is_tx) {
        strcpy(file_mode, "r");
        common = &s->rxtx_data->tx.common;
    } else {
        strcpy(file_mode, "w");
        common = &s->rxtx_data->rx.common;
    }

    for(i = 2; i < argc; i++) {
        val = strchr(argv[i], '=');
        if (!val) {
            cli_err(s, argv[0],
                    "No value provided for parameter \"%s\"", argv[i]);

            return CMD_RET_INVPARAM;
        }

        *val++ = '\0';

        if (!strcasecmp(RXTX_PARAM_FILE, argv[i])) {
            status = set_sample_file_path(common, val);
        } else if (!strcasecmp(RXTX_PARAM_FILEFORMAT, argv[i])) {
            common->file_fmt = str2fmt(val);
            if (common->file_fmt == RXTX_FMT_INVALID) {
                cli_err(s, argv[0], "Invalid format provided (%s)", val);
                return CMD_RET_INVPARAM;
            }
        } else {
            if (is_tx) {
                status = handle_tx_param(s, argv[i], val);
                if (status)
                    return status;
            } else {
                status = handle_rx_param(s, argv[i], val);
                if (status)
                    return status;
            }
        }
    }

    return 0;
}

static int rx_start_init(struct cli_state *s)
{
    enum rxtx_state state;
    int status = CMD_RET_UNKNOWN;
    struct rx_cfg *rx = &s->rxtx_data->rx;

    switch(s->rxtx_data->rx.common.file_fmt) {
        case RXTX_FMT_CSV_C16:
            rx->write_samples = rxtx_write_csv_c16;
            break;

        case RXTX_FMT_BINLE_C16:
        case RXTX_FMT_BINBE_C16:
            rx->write_samples = rxtx_write_bin_c16;
            break;

        /* This shouldn't happen...*/
        default:
            assert(0);
            return CMD_RET_UNKNOWN;
    }

    status = bladerf_enable_module(s->dev, RX, true);
    if (status >= 0) {
        set_state(&rx->common, RXTX_STATE_RUNNING, true);

        while ((state = get_state(&rx->common)) == RXTX_STATE_IDLE) {
            usleep(100000);
        }

        if (state == RXTX_STATE_RUNNING) {
            status = 0;
        }
    }

    return status;
}

static int rx_stop_cleanup(struct cli_state *s)
{
    set_state(&s->rxtx_data->rx.common, RXTX_STATE_IDLE, true);
    return 0;
}

static int tx_start_init(struct cli_state *s)
{
    int status = CMD_RET_UNKNOWN;
    enum rxtx_state state;

    switch(s->rxtx_data->tx.common.file_fmt) {
        case RXTX_FMT_CSV_C16:
            status = tx_csv_to_c16(s);
            if (status < 0)
                return status;

            printf("\nConverted CSV file to binary file. Using %s\n",
                    s->rxtx_data->tx.common.file_path);
            printf("Note that this program will not delete the temporary file.\n\n");
            /* Fall through - if we hit this line we're now using
             * a temporary binary file */

        case RXTX_FMT_BINLE_C16:
        case RXTX_FMT_BINBE_C16:
            s->rxtx_data->tx.read_samples = rxtx_read_bin_c16;
            break;

        /* This shouldn't happen...*/
        default:
            assert(0);
            return CMD_RET_UNKNOWN;
    }

    status = bladerf_enable_module(s->dev, TX, true);
    if (status >= 0) {
        set_state(&s->rxtx_data->tx.common, RXTX_STATE_RUNNING, true);

        /* Don't expect to poll here long... replace with cond var? */
        while ((state = get_state(&s->rxtx_data->tx.common)) == RXTX_STATE_IDLE) {
            usleep(100000);
        }

        if (state == RXTX_STATE_RUNNING) {
            status = 0;
        }
    }

    return status;
}

static int tx_stop_cleanup(struct cli_state *s)
{
    set_state(&s->rxtx_data->tx.common, RXTX_STATE_IDLE, true);
    return 0;
}

int cmd_rxtx(struct cli_state *s, int argc, char **argv)
{
    int ret = CMD_RET_OK;
    int fpga_loaded;
    enum rxtx_cmd cmd;
    struct common_cfg *common;
    bool is_tx;
    int (*start_init)(struct cli_state *s);
    int (*stop_cleanup)(struct cli_state *s);

        if (!strcasecmp("tx", argv[0])) {
            is_tx = true;
            common = &s->rxtx_data->tx.common;
            start_init = tx_start_init;
            stop_cleanup = tx_stop_cleanup;
        } else if (!strcasecmp("rx", argv[0])) {
            is_tx = false;
            common = &s->rxtx_data->rx.common;
            start_init = rx_start_init;
            stop_cleanup = rx_stop_cleanup;
        } else {
            /* Bug */
            assert(0);
            return CMD_RET_UNKNOWN;
        }

    /* Just <rx|tx> is supported shorthand for <rx|tx> config */
    if (argc == 1) {
        cmd = RXTX_CMD_CFG;
    } else {
        cmd = get_cmd(argv[1]);
    }

    switch (cmd) {
        case RXTX_CMD_START:
            if (!cli_device_is_opened(s)) {
                ret = CMD_RET_NODEV;
            } else if (get_state(common) == RXTX_STATE_RUNNING) {
                ret = CMD_RET_STATE;
            } else {
                fpga_loaded = bladerf_is_fpga_configured(s->dev);
                if (fpga_loaded < 0) {
                    s->last_lib_error = fpga_loaded;
                    ret = CMD_RET_LIBBLADERF;
                } else if (!fpga_loaded) {
                    cli_err(s, argv[0], "FPGA is not configured");
                    ret = CMD_RET_INVPARAM;
                } else {
                    ret = validate_config(s, argv[0], s->rxtx_data, is_tx);
                    if (!ret) {
                        ret = open_samples_file(common, NULL, is_tx ? "r" : "w");
                        if (!ret) {
                            ret = start_init(s);
                        }
                    }
                }
            }
            break;

        case RXTX_CMD_STOP:
            if (!cli_device_is_opened(s)) {
                ret = CMD_RET_NODEV;
            } else if (get_state(common) != RXTX_STATE_RUNNING) {
                ret = CMD_RET_STATE;
            } else {
                ret = stop_cleanup(s);
            }
            break;

        case RXTX_CMD_CFG:
            if (argc > 2) {
                if (get_state(common) != RXTX_STATE_RUNNING) {
                    ret = handle_params(s, argc, argv, is_tx);
                } else {
                    ret = CMD_RET_STATE;
                }
            } else {
                print_config(s->rxtx_data, is_tx);
            }
            break;

        default:
            cli_err(s, argv[0], "Invalid command (%s)", argv[1]);
            ret = CMD_RET_INVPARAM;
    }


    return ret;
}

/* Buffer size is in units of # elements, not bytes */
static int common_cfg_init(struct common_cfg *c, size_t buff_size)
{
    int status = -1;

    c->buff_size = buff_size;
    c->buff = calloc(buff_size, sizeof(c->buff[0]));

    if (c->buff) {
        c->file = NULL;
        c->file_path = NULL;
        c->file_fmt = RXTX_FMT_BINHOST_C16;
        c->task_state = RXTX_STATE_IDLE;
        c->error.type = ETYPE_ERRNO;
        c->error.value = 0;;

        if (pthread_mutex_init(&c->error.lock, NULL) < 0) {
            fprintf(stderr, "Failed to initialize error.lock\n");
        } else if (pthread_mutex_init(&c->task_lock, NULL) < 0) {
            fprintf(stderr, "Failed to initialize task_lock\n");
        } else if (pthread_mutex_init(&c->file_lock, NULL) < 0) {
            fprintf(stderr, "Failed to initialize file_lock\n");
        } else if (pthread_mutex_init(&c->param_lock, NULL) < 0) {
            fprintf(stderr, "Failed to initialize param_lock\n");
        } else if (pthread_cond_init(&c->task_state_changed, NULL) < 0) {
            fprintf(stderr, "Failed to initialize state_changed\n");
        } else {
            status = 0;
        }
    }

    return status;
}

static void common_cfg_deinit(struct common_cfg *c)
{
    if (c->file) {
        fclose(c->file);
        c->file = NULL;
    }

    if (c->file_path) {
        free(c->file_path);
        c->file_path = NULL;
    }

    free(c->buff);
    c->buff = NULL;
}

struct rxtx_data * rxtx_data_alloc(struct cli_state *s)
{
    struct rxtx_data *ret = NULL;
    int status;

    ret = malloc(sizeof(*ret));
    if (ret) {

        /* Initialize RX task configuration */
        ret->rx.n_samples = 1024;
        status = common_cfg_init(&ret->rx.common, RXTX_RX_BUFF_SIZE);
        if (status < 0) {
            free(ret);
            ret = NULL;
        } else {
            /* Initialize TX task configuration */
            ret->tx.repeat = 1;
            ret->tx.repeat_delay = 0;
            status = common_cfg_init(&ret->tx.common, RXTX_TX_BUFF_SIZE);

            if (status < 0) {
                common_cfg_deinit(&ret->rx.common);
                free(ret);
                ret = NULL;
            }
        }
    }

    return ret;
}

int rxtx_start_tasks(struct cli_state *s)
{
    int status;
    struct rxtx_data *d = s->rxtx_data;

    status = pthread_create(&d->rx.common.thread, NULL, rx_task, s);

    if (status == 0) {
        status = pthread_create(&d->tx.common.thread, NULL, tx_task, s);

        if (status < 0) {
            fprintf(stderr, "Faild to create TX task: %s\n", strerror(errno));
            pthread_cancel(d->rx.common.thread);
            pthread_join(d->rx.common.thread, NULL);
        }
    } else {
        fprintf(stderr, "Faild to create RX task: %s\n", strerror(errno));
    }

    return status;
}

void rxtx_stop_tasks(struct rxtx_data *d)
{
    set_state(&d->rx.common, RXTX_STATE_SHUTDOWN, true);
    set_state(&d->tx.common, RXTX_STATE_SHUTDOWN, true);
    pthread_join(d->rx.common.thread, NULL);
    pthread_join(d->tx.common.thread, NULL);
}

void rxtx_data_free(struct rxtx_data *d)
{
    if (d) {
        common_cfg_deinit(&d->rx.common);
        common_cfg_deinit(&d->tx.common);
        free(d);
    }
}

bool rxtx_tx_task_running(struct cli_state *s)
{
    enum rxtx_state state = get_state(&s->rxtx_data->tx.common);
    return state == RXTX_STATE_RUNNING;
}

bool rxtx_rx_task_running(struct cli_state *s)
{
    enum rxtx_state state = get_state(&s->rxtx_data->rx.common);
    return state == RXTX_STATE_RUNNING;
}
