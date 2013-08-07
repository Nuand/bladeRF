#ifndef COMMON_H__
#define COMMON_H__
#include <pthread.h>
#include <stdbool.h>
#include <libbladeRF.h>
/**
 * Differentiates error code types
 */
enum error_type {
    ETYPE_CLI,      /**< CMD_RET cli error code */
    ETYPE_BLADERF,  /**< libbladeRF error code */
    ETYPE_ERRNO,    /**< errno value */
};

/**
 * Information about the last error encountered
 */
struct cli_error {
    int value;              /**< Last command error */
    enum error_type type;   /**< Type/source of last error */
    pthread_mutex_t lock;
};

/**
 * Application state
 */
struct cli_state {
    struct bladerf *dev;            /**< Device currently in use */

    int last_lib_error;             /**< Last libbladeRF error */

    /* TODO replace this with a script stack to allow for nested scripts */
    FILE *script;                   /**< Loaded script */
    int lineno;                     /**< Line # count, when running scripts */

    struct rxtx_data *rxtx_data;    /**< Data for transmit/receive commands */
};

/**
 * Allocate and initialize state object
 *
 * @return state structure on success, NULL on failure
 */
struct cli_state *cli_state_create();

/**
 * Deallocate and deinitlize state object
 */
void cli_state_destroy(struct cli_state *s);

/**
 * Query whether a device is currently opend
 *
 * @return true if device is opened, false otherwise
 */
bool cli_device_is_opened(struct cli_state *s);

/**
 * Query whether the device is busy being used by other tasks
 *
 * @return true if device is in use, false otherwise
 */
bool cli_device_in_use(struct cli_state *s);

/**
 * String to integer conversion with range and error checking
 *
 *  @param  str     String to convert
 *  @param  min     Inclusive minimum allowed value
 *  @param  max     Inclusive maximum allowed value
 *  @param  ok      If non-NULL, this will be set to true to denote that
 *                  the conversion succeeded. If this value is not true,
 *                  then the return value should not be used.
 *
 * @return 0 on success, undefined on failure
 */
int str2int(const char *str, int min, int max, bool *ok);

/**
 * String to unsigned integer conversion with range and error checking
 *
 *  @param  str     String to convert
 *  @param  min     Inclusive minimum allowed value
 *  @param  max     Inclusive maximum allowed value
 *  @param  ok      If non-NULL, this will be set to true to denote that
 *                  the conversion succeeded. If this value is not true,
 *                  then the return value should not be used.
 *
 * @return 0 on success, undefined on failure
 */
unsigned int str2uint(const char *str,
                        unsigned int min, unsigned int max, bool *ok);

/**
 * Print an error message, with a line number, if running from a script.
 *
 *
 * @param   s       CLI state.
 * @param   pfx     Error prefix. "Error: " is used if this is null
 * @param   format  Printf-style format string, followed by args
 *
 */
void cli_err(struct cli_state *s, const char *pfx, const char *format, ...);

/**
 * Set the "last encountered error" info
 *
 * Always use this routine for thread safety - do not access these fields
 * directly.
 *
 * @param   e       Error structure
 * @param   type    Type of error (i.e. who's error code is it?)
 * @param   error   Error code
 */
void set_last_error(struct cli_error *e, enum error_type type, int error);

/**
 * Fetch the "last encountered error" info
 *
 * Always use this routine for thread safety - do not access these fields
 * directly.
 *
 * @param[in]       e       Error structure
 * @param[out]      type    Type of error (i.e. who's error code is it?)
 * @param[out]      error   Error code
 */
void get_last_error(struct cli_error *e, enum error_type *type, int *error);

/**
 * Allocate and return a file name (including path) for a given FILE *
 *
 * The caller is responsible for freeing this string
 *
 * @return path string on success, NULL on failure
 */
char *to_path(FILE *f);

static inline unsigned int uint_min(unsigned int x, unsigned int y)
{
    return x < y ? x : y;
}

static inline unsigned int uint_max(unsigned int x, unsigned int y)
{
    return x > y ? x : y;
}

/* expand_file - tecla expansion, optional
 
   Optionally expand tecla paths, if we're in interactive mode.  Unfortunately
   we can't house this in the interactive module since we handle commands
   here.

   Caller is expected to free() the returned char *

   A return value of NULL indicates some form of internal error

   On non-interactive compiles this returns the same path
*/
char *expand_file(const char *inpath);

#endif
