#include <errno.h>
#include <libtecla.h>
#include "interactive.h"
#include "cmd.h"

#ifndef CLI_MAX_LINE_LEN
#   define CLI_MAX_LINE_LEN 320
#endif

#ifndef CLI_MAX_HIST_LEN
#   define CLI_MAX_HIST_LEN 500
#endif

#ifndef CLI_PROMPT
#   define CLI_DEFAULT_PROMPT   "bladeRF> "
#endif

int interactive(struct cli_state *s)
{
    char *line;
    int status;
    GetLine *gl;
    const char *error;

    gl = new_GetLine(CLI_MAX_LINE_LEN, CLI_MAX_HIST_LEN);

    if (!gl) {
        perror("new_GetLine");
        return 2;
    }

    /* Try to set up a clean exit on these signals. If it fails, we'll
     * trudge along with a warning */
    status = gl_trap_signal(gl, SIGINT, GLS_DONT_FORWARD, GLS_ABORT, EINTR);
    status |= gl_trap_signal(gl, SIGTERM, GLS_DONT_FORWARD, GLS_ABORT, EINTR);
    status |= gl_trap_signal(gl, SIGQUIT, GLS_DONT_FORWARD, GLS_ABORT, EINTR);

    if (status) {
        fprintf(stderr, "Warning: Failed to set up signal handlers.\n");
    }

    status = 0;
    while (!cmd_fatal(status) && status != CMD_RET_QUIT) {
        if( s->curr_device ) {
           /* TODO: Change the prompt based on which device is open */
        }
        line = gl_get_line(gl, CLI_DEFAULT_PROMPT, NULL, 0);
        if (!line) {
            break;
        } else {
            status = cmd_handle( s, line );

            if (status) {
                error = cmd_strerror(status, s->last_lib_error);
                if (error) {
                    printf("%s\n", error);
                }
            }
        }
    }

    if (s->curr_device) {
        bladerf_close(s->curr_device);
        s->curr_device = NULL;
    }

    del_GetLine(gl);

    return 0;
}

