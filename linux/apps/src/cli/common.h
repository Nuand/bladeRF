#ifndef COMMON_H__
#define COMMON_H__
#include <stdbool.h>
#include <libbladeRF.h>

/**
 * Application state
 */
struct cli_state {
    struct bladerf *curr_device;    /**< Device currently in use */
    int last_lib_error;             /**< Last libbladeRF error */
    int last_error;                 /**< Last command error */

    FILE *script;                   /**< Loaded script */
    int lineno;                     /**< Line # count, when running scripts */
};

/** Static initializer */
#define CLI_STATE_INITIALIZER { \
    .curr_device = NULL, \
    .last_lib_error = 0, \
    .last_error = 0, \
    .lineno = 0,\
}

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
#endif
