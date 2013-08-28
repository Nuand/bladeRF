#ifndef INTERACTIVE_H__
#define INTERACTIVE_H__

/*
 * TODO Remove the use of the INTERACTIVE compile-time macro. Interactive
 *      code should always be enabled, but we'll switch between a
 *      libtecla-basedr and a simple fgets-based implementation
 */

#include <libbladeRF.h>
#include "common.h"

#ifdef INTERACTIVE
/**
 * Interactive mode 'main loop'
 *
 * If this function is passed a valid device handle, it will take responsibility
 * for deallocating it before returning.
 *
 * @param   s       CLI state
 * @param   batch   Batch mode script execution -- exit after script completes
 *
 * @return  0 on success, non-zero on failure
 */
int interactive(struct cli_state *s, bool batch);

/**
 * Expand a file path
 *
 * @post Heap-allocated memory is used to return the expanded path. The caller
 *       is responsible for calling free().
 *
 * @return Expanded path on success, NULL on failure.
 */
char * interactive_expand_path(char *path);



#else
#define interactive(x, y) (0)

/* Without interactive mode the shell should expand paths for us, so we
 * can just fake this...
 *
 * This is a temporary hack until the above "TODO" item is addressed
 */
#include <string.h>
#define interactive_expand_path(path) strdup(path)

#endif  /* INTERACTIVE */
#endif  /* INTERACTICE_H__ */
