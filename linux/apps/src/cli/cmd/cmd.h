#ifndef CMD_H__
#define CMD_H__

#include <stdbool.h>

#include "common.h"

/* Fatal errors */
#define CMD_RET_MEM         (-1)    /**< A memory allocation failure occurred */

/** Command OK */
#define CMD_RET_OK          0

/* Non-fatal errors */
#define CMD_RET_QUIT        1       /**< Got request to quit */
#define CMD_RET_NOCMD       2       /**< Non-existant command */
#define CMD_RET_MAX_ARGC    3       /**< Maximum number of arguments reached */
#define CMD_RET_INVPARAM    4       /**< Invalid parameters passed */
#define CMD_RET_LIBBLADERF  5       /**< See cli_state for libladerf error */
#define CMD_RET_NODEV       6       /**< No device is currently opened */

// I'll define this later. It will contain the "current" struct bladerf *device
// and some other state info?
struct cli_state;

/**
 * Parse and execute the supplied command
 *
 * @return 0 on success, CMD_RET_ on failure/exceptional condition
 */
int cmd_handle(struct cli_state *s, const char *line);

static inline bool cmd_fatal(int status) { return status < 0; }

const char * cmd_strerror(int error, int lib_error);

#endif
