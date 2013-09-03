#ifndef INTERACTIVE_H__
#define INTERACTIVE_H__

#include <libbladeRF.h>
#include "common.h"

/* TODO - Alleviate dependecny upon cmd. Change CMD_RET_*  to CLI_RET_* */
#include "cmd.h"

/**
 * Interactive mode or script execution 'main loop'
 *
 * @param   s           CLI state
 * @param   script_only Exit after script completes
 *
 * @return  0 on success, CMD_RET_* on failure
 */
int interactive(struct cli_state *s, bool script_only);

/**
 * Expand a file path using the interactive mode support backend
 *
 * @post Heap-allocated memory is used to return the expanded path. The caller
 *       is responsible for calling free().
 *
 * @return Expanded path on success, NULL on failure.
 */
char * interactive_expand_path(char *path);

#endif  /* INTERACTICE_H__ */
