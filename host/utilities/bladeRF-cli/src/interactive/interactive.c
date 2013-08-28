/*
 * libtecla-based interactive mode
 */

#include <errno.h>
#include <string.h>
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

static void exit_script(GetLine *gl, struct cli_state *s, bool *in_script)
{
    if (*in_script) {
        *in_script = false;

        /* TODO pop from a script stack here, restore line count */
        if (gl_change_terminal(gl, stdin, stdout, getenv("term")) < 0) {
            /* At least attempt to report something... */
            cli_err(s, "Error", "Failed to reset terminal when exiting script");
        }

        if (s->script) {
            fclose(s->script);
            s->script = NULL;
        }
    }
}

int interactive(struct cli_state *s, bool batch)
{
    char *line;
    int status;
    GetLine *gl;
    const char *error;
    bool in_script;

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
    in_script = s->script != NULL;
    s->lineno = 1;

    if (in_script) {
        if (gl_change_terminal(gl, s->script, stdout, NULL) < 0) {
            fprintf(stderr, "Failed to run script. Aborting!\n");
            status = CMD_RET_UNKNOWN;
        }
    }

    while (!cmd_fatal(status) && status != CMD_RET_QUIT) {

        /* TODO: Change the prompt based on which device is open */
        line = gl_get_line(gl, CLI_DEFAULT_PROMPT, NULL, 0);
        if (!line) {
            if (in_script) {
                exit_script(gl, s, &in_script);

                /* Exit if we were run with -b -s <script> */
                if (batch)
                    status = CMD_RET_QUIT;

            } else {
                /* Leaving interactivce mode */
                break;
            }
        } else {
            status = cmd_handle(s, line);

            if (status < 0) {
                error = cmd_strerror(status, s->last_lib_error);
                if (error) {
                    cli_err(s, "Error", "%s", error);
                }

                /* Stop executing script if we're in one */
                exit_script(gl, s, &in_script);

            } else if (status > 0){
                switch (status) {
                    case CMD_RET_CLEAR_TERM:
                        gl_erase_terminal(gl);
                        break;
                    case CMD_RET_RUN_SCRIPT:
                        break;
                    default:
                        printf("Unknown return code: %d\n", status);
                }
            }
        }

        if (in_script) {
            s->lineno++;
        }
    }

    del_GetLine(gl);

    return 0;
}

char * interactive_expand_path(char * path)
{
    ExpandFile *ef = NULL;
    FileExpansion *fe = NULL;
    char *ret = NULL;

    ef = new_ExpandFile();
    if (!ef)
        return NULL;

    fe = ef_expand_file(ef, path, strlen(path));

    if (!fe)
        return NULL;

    if (fe->nfile <= 0)
        return NULL;

    ret = strdup(fe->files[0]);

    del_ExpandFile(ef);

    return ret;
}
