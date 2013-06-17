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
    /* TODO trap SIGINT and SIGTERM - use these to exit cleanly as if
     * we have received a "quit"/"exit" command */

    if (!gl) {
        perror("new_GetLine");
        return 2;
    }

    status = 0;
    while (!cmd_fatal(status) && status != CMD_RET_QUIT) {
        if( s->curr_device ) {
           /* TODO: Change the prompt based on which device is open */
        }
        line = gl_get_line(gl, CLI_DEFAULT_PROMPT, NULL, 0);
        if (!line)
            break;
        else {
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

