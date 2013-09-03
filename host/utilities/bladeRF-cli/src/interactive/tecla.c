/*
 * libtecla-based support for interactive mode and script support
 */
#include <stdlib.h>
#include <stdio.h>
#include <libtecla.h>
#include <string.h>
#include <errno.h>
#include "interactive_impl.h"

static GetLine *gl = NULL;

int interactive_init()
{
    int status = CMD_RET_UNKNOWN;

    if (gl) {
        interactive_deinit();
    }

    gl = new_GetLine(CLI_MAX_LINE_LEN, CLI_MAX_HIST_LEN);

    if (gl) {

        /* Try to set up a clean exit on these signals. If it fails, we'll
        * trudge along with a warning */
        if (gl_trap_signal(gl, SIGINT, GLS_DONT_FORWARD, GLS_ABORT, EINTR)  ||
            gl_trap_signal(gl, SIGTERM, GLS_DONT_FORWARD, GLS_ABORT, EINTR) ||
            gl_trap_signal(gl, SIGQUIT, GLS_DONT_FORWARD, GLS_ABORT, EINTR)) {

            fprintf(stderr, SIGHANLDER_FAILED);
        }

        status = 0;
    }

    return status;
}

void interactive_deinit()
{
    if (gl) {
        del_GetLine(gl);
        gl = NULL;
    }
}

char * interactive_get_line(const char *prompt)
{
    return gl_get_line(gl, prompt, NULL, 0);
}

int interactive_set_input(FILE *file)
{
    int status;

    status = gl_change_terminal(gl, file, stdout, getenv("term"));
    if (status < 0) {
        return CMD_RET_FILEOP;
    } else {
        return 0;
    }
}

char * interactive_expand_path(char *path)
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

void interactive_clear_terminal()
{
    gl_erase_terminal(gl);
}
