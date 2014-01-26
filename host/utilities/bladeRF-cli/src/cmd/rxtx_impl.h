/**
 * @file rxtx_impl.h
 *
 * @brief definitions for the implementation of the rx and tx commands.
 *
 * Nothing outside of rxtx_impl.c, rx.c, and tx.c should require anything
 * defined in this file.
 *
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2013 Nuand LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef RXTX_IMPL_H__
#define RXTX_IMPL_H__

#include <libbladeRF.h>
#include "cmd.h"
#include "conversions.h"

#define RXTX_ERRMSG_VALUE(param, value) \
    "Invalid value for \"%s\" (%s)", param, value

/* Minimum required unit of sample acceses */
#define LIBBLADERF_SAMPLE_BLOCK_SIZE    1024

#define RXTX_TASK_REQ_START     (1 << 0)    /* Request to start task */
#define RXTX_TASK_REQ_STOP      (1 << 1)    /* Request to stop task */
#define RXTX_TASK_REQ_SHUTDOWN  (1 << 2)    /* Request to shutdown */
#define RXTX_TASK_REQ_ALL (\
            RXTX_TASK_REQ_START | \
            RXTX_TASK_REQ_STOP  | \
            RXTX_TASK_REQ_SHUTDOWN)

#define RXTX_CMD_START "start"
#define RXTX_CMD_STOP "stop"
#define RXTX_CMD_CONFIG "config"
#define RXTX_CMD_WAIT "wait"

#define TMP_FILE_NAME "bladeRF_samples_from_csv.bin"

enum rxtx_fmt {
    RXTX_FMT_INVALID = -1,
    RXTX_FMT_CSV_SC16Q11,   /* CSV (Comma-separated, one entry per line) */
    RXTX_FMT_BIN_SC16Q11    /* Binary (big-endian), c16 I,Q */
};

enum rxtx_state {
    RXTX_STATE_FAIL,        /* Failed to create task */
    RXTX_STATE_INIT,        /* Task is initializing */
    RXTX_STATE_IDLE,        /* Awaiting rx/tx "start" command */
    RXTX_STATE_START,       /* Perform an pre-run initializations.
                             *   (Transition between IDLE -> RUNNING) */
    RXTX_STATE_RUNNING,     /* Actively running */
    RXTX_STATE_STOP,        /* Got "stop" command. Transition state between
                             *    RUNNING -> IDLE */
    RXTX_STATE_SHUTDOWN,    /* Clean up and deallocate; the program's exiting */
};

/* Items pertaining to obtaining and managing sample data.
 * If acquiring locks from multiple *_mgmt structures, the data_mgmt.lock
 * should be acquired first.*/
struct data_mgmt
{
    /* These two items should not be modified outside of the running task */
    struct bladerf_stream *stream;  /* Stream handle */
    void **buffers;                 /* SC16 Q11 sample buffers*/
    size_t next_idx;                /* Index of next buffer to use */

    pthread_mutex_t lock;           /* Should be acquired to change the
                                     *    the following items */
    size_t num_buffers;             /* # of buffers in 'buffers' list */
    size_t samples_per_buffer;      /* Size of each buffer (in samples) */
    size_t num_transfers;           /* # of transfers to use in the stream */
    unsigned int timeout_ms;        /* Stream timeout, in ms */
};

/* Input/Ouput file and related metadata.
 * If acuiring both the locks, acquire file_meta_lock first, then file_lock */
struct file_mgmt
{
    FILE *file;                 /* File to read/write samples from/to */
    pthread_mutex_t file_lock;  /* Thread using 'file' must hold this lock */


    pthread_mutex_t file_meta_lock; /* Should be acquired when accessing any
                                     * of the following file metadata items */
    char *path;                     /* Path associated with 'file'. */
    enum rxtx_fmt format;           /* File format */
};


/* Task and thread management */
struct task_mgmt
{
    pthread_t thread;           /* Handle to thread in which the task runs */

    enum rxtx_state state;      /* Task state */
    uint8_t req;                /* Requests for state change. See
                                 *   RXTX_TASK_REQ_* bitmasks */
    pthread_mutex_t lock;       /* Must be held to access 'req' or 'state' */
    pthread_cond_t signal_req;  /* Signal when a request has been made */
    pthread_cond_t signal_done; /* Signal when task finishes work */
    pthread_cond_t signal_state_change; /* Signal after state change */
    bool main_task_waiting;     /* Main task is blocked waiting */
};

/* RX or TX-specific parameters */
struct params;

struct rxtx_data
{
    bladerf_module module;

    struct data_mgmt data_mgmt;
    struct file_mgmt file_mgmt;
    struct task_mgmt task_mgmt;
    struct cli_error last_error;

    /* Must be held to access the following items */
    pthread_mutex_t param_lock;
    void *params;
};

struct tx_params
{
    unsigned int repeat_delay;  /* us delay between repetitions */
    unsigned int repeat;        /* # of repetitions */
};

struct rx_params
{
    size_t n_samples;           /* Number of samples to receive */
};

/* Multipliers in units of 1024 */
extern const struct numeric_suffix rxtx_kmg_suffixes[];
extern const size_t rxtx_kmg_suffixes_len;

/* Forward declare thread entry points implemented by rx/tx code */
void * rx_task(void *cli_state);
void * tx_task(void *cli_state);

/**
 * Set tasks's current state
 *
 * @param   rxtx    RX/TX data handle
 * @param   state   Desired state
 *
 * @note This should not be called except from within the task implementation.
 *       rxtx_task_request() should be used to request the task
 *       to transition to another state
 */
void rxtx_set_state(struct rxtx_data *rxtx, enum rxtx_state state);

/**
 * Fetch task's current state
 *
 * @param   rxtx    RX/TX data handle
 *
 * @return current state
 */
enum rxtx_state rxtx_get_state(struct rxtx_data *rxtx);

/**
 * Set the path of the file currently being read from or written to
 *
 * @param   rxtx    RX/TX data handle
 * @param   path    Path to set
 *
 * @return 0 on success, CMD_RET_MEM on memory allocation error
 */
int rxtx_set_file_path(struct rxtx_data *rxtx, const char *path);

/**
 * Set the format of the file currently being read from or written to
 *
 * @param   rxtx    RX/TX data handle
 * @param   format  Format to set
 */
void rxtx_set_file_format(struct rxtx_data *rxtx, enum rxtx_fmt format);

/**
 * Convert a string to an rxtx_fmt value
 *
 * @param   str     Format in string form
 *
 * @return rxtx_fmt value. RXTX_FMT_INVALID is returned for invalid strings.
 */
enum rxtx_fmt rxtx_str2fmt(const char *str);

/**
 * Print the task state with the provided prefix and suffix
 *
 * @param   rxtx    RX/TX data handle
 * @param   prefix  Prefix to print before task state
 * @param   suffix  Suffix to print after task state
 */
void rxtx_print_state(struct rxtx_data *rxtx,
                      const char *prefix, const char *suffix);

/**
 * Print the filename used by a task with the provided prefix and suffix
 *
 * @param   rxtx    RX/TX data handle
 * @param   prefix  Prefix to print before task state
 * @param   suffix  Suffix to print after task state
 */
void rxtx_print_file(struct rxtx_data *rxtx,
                     const char *prefix, const char *suffix);

/**
 * Print the file format used by a task with the provided prefix and suffix
 *
 * @param   rxtx    RX/TX data handle
 * @param   prefix  Prefix to print before task state
 * @param   suffix  Suffix to print after task state
 */
void rxtx_print_file_format(struct rxtx_data *rxtx,
                            const char *prefix, const char *suffix);

/**
 * Print the error status associated with a task, with
 * the provided prefix and suffix
 *
 * @param   rxtx    RX/TX data handle
 * @param   prefix  Prefix to print before task state
 * @param   suffix  Suffix to print after task state
 */
void rxtx_print_error(struct rxtx_data *rxtx,
                      const char *prefix, const char *suffix);


/**
 * Print the stream configuration associated with the task
 *
 * @param   rxtx    RX/TX data handle
 * @param   prefix  Prefix to print before task state
 * @param   suffix  Suffix to print after task state
 */
void rxtx_print_stream_info(struct rxtx_data *rxtx,
                            const char *prefix, const char *suffix);

/**
 * Submit a request to the associated task
 *
 * @param   rxtx    RX/TX data handle
 * @param   req     Requested action (RXTX_TASK_REQ_* value)
 */
void rxtx_submit_request(struct rxtx_data *rxtx, unsigned char req);

/**
 * Get currently pending requests and clear requests specified by
 * the provided mask
 *
 * @param   rxtx    RX/TX data handle
 * @param   mask    Requests to clear
 *
 * @return bitmask of pending requests
 */
unsigned char rxtx_get_requests(struct rxtx_data *rxtx, unsigned char mask);

/**
 * Handle rx/tx config parameters
 *
 * @param[in]       s           CLI state (Needed to print error messages)
 * @param[in]       rxtx        RX/TX data handle
 * @param[in]       argv0       argv[0] string (used to prefix error messages)
 * @param[inout]    param       Parameter to handle. Modified to be
 *                              Nul-terminated after the parameter
 * @param[out]      val         Set to point to value portion of parameter
 *
 * @return 0 if the param was not handled, 1 if it was,
 *         CMD_RET_* if an error occurs. This function will print an error
 *         for CMD_RET_INVALID. param and val are undefined for an error.
 */
int rxtx_handle_config_param(struct cli_state *s, struct rxtx_data *rxtx,
                             const char *argv0, char *param, char **val);

/**
 * Handle an rx/tx wait command.
 *
 * Waits either until the rx/tx operation completes, or a specified amount
 * of time elapses
 *
 * @param   s       CLI state
 * @param   rxtx    RX/TX data handle
 * @param   argc    Number of pararamters provided to rx/tx command
 * @param   argc    Pararamters provided to rx/tx command
 *
 * @return 0 on success, CMD_RET_* for any errors
 */
int rxtx_handle_wait(struct cli_state *s, struct rxtx_data *rxtx,
                     int argc, char **argv);

/**
 * Perform pre-start checks for rx/tx check command
 *
 * @param[in]       s           CLI state
 * @param[in]       rxtx        RX/TX data handle
 * @param[in]       argv0       argv[0] string (used to prefix error messages)
 *
 * @return 0 on success, CMD_RET_* for any errors. An error will be printed
 *         for CMD_RET_INVPARAM
 */
int rxtx_cmd_start_check(struct cli_state *s, struct rxtx_data *rxtx,
                         const char *argv0);

/**
 * RX/TX stop command, common items
 * @param   s       CLI state
 * @param   rxtx    RX/TX data handle
 */
int rxtx_cmd_stop(struct cli_state *s, struct rxtx_data *rxtx);

/**
 * Handle the rx/tx task's IDLE state
 *
 * @param   rxtx        RX/TX data handle
 * @param   requests    Task's copy of pending requests
 */
void rxtx_task_exec_idle(struct rxtx_data *rxtx, unsigned char *requests);

/**
 * Handle the rx/tx task's RUNNING state
 *
 * @param   rxtx        RX/TX data handle
 * @param   s           CLI state handle
 */
void rxtx_task_exec_running(struct rxtx_data *rxtx, struct cli_state *s);

/**
 * Handle the rx/tx task's STOP state
 *
 * @param   rxtx        RX/TX data handle
 * @param   requests    Task's copy of pending requests
 */
void rxtx_task_exec_stop(struct rxtx_data *rxtx, unsigned char *requests);

/**
 * Wait for a task to transition into the specified state
 *
 * @param   rxtx            RX/TX data handle
 * @param   state           Desired state
 * @param   timeout_ms      Timeout in ms. 0 will wait infinitely.
 *
 * @return 0 on the desired state change, -1 on a timeout or error
 */
int rxtx_wait_for_state(struct rxtx_data *rxtx, enum rxtx_state state,
                        unsigned int timeout_ms);

#endif
