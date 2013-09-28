/*
 * Simple fgets-based interactive mode support
 */
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include "interactive_impl.h"
#include "host_config.h"

static FILE *input = NULL;
static char *line_buf = NULL;
static const int line_buf_size = CLI_MAX_LINE_LEN + 1;
static bool caught_signal = false;

/* Makes the assumption that the caller hasn't done all sorts of closing
 * and dup'ing... */
static inline bool is_stdin(FILE *file)
{
    return file && fileno(file) == 0;
}

#if BLADERF_OS_WINDOWS
static void sig_handler(int signal) {
    if (signal == SIGTERM || signal == SIGINT) {
        caught_signal = true;
    }
}

void init_signal_handling()
{
    if (signal(SIGINT, sig_handler) || signal(SIGTERM, sig_handler)) {
        fprintf(stderr, SIGHANLDER_FAILED);
    }
}

#else
static void sig_handler(int signal, siginfo_t *signal_info, void *unused)
{
    if (signal == SIGTERM || signal == SIGINT) {
        caught_signal = true;
    }
}

void init_signal_handling()
{
    struct sigaction sigact;

    sigemptyset(&sigact.sa_mask);
    sigact.sa_sigaction = sig_handler;
    sigact.sa_flags = SA_SIGINFO;

    if (sigaction(SIGINT, &sigact, NULL)   ||
        sigaction(SIGTERM, &sigact, NULL)  ||
        sigaction(SIGQUIT, &sigact, NULL)) {

        fprintf(stderr, SIGHANLDER_FAILED);
    }
}
#endif

int interactive_init()
{

    if (input || line_buf) {
        interactive_deinit();
    }

    input = stdin;

    line_buf = calloc(line_buf_size, 1);
    if (!line_buf) {
        interactive_deinit();
        return CMD_RET_MEM;
    }

    init_signal_handling();
    caught_signal = false;

    return 0;
}

void interactive_deinit()
{
    if (input) {
        input = NULL;
    }

    if (line_buf) {
        free(line_buf);
        line_buf = NULL;
    }
}

int interactive_set_input(FILE *new_input)
{
    input = new_input;
    return 0;
}

char * interactive_get_line(const char *prompt)
{
    char *eol;
    char *ret = NULL;

    if (input) {

        if (is_stdin(input)) {
            fputs(prompt, stdout);
        }

        memset(line_buf, 0, line_buf_size);
        ret = fgets(line_buf, line_buf_size - 1, input);

        if (ret) {

            /* Scrape out newline */
            if ((eol = strchr(line_buf, '\r')) ||
                (eol = strchr(line_buf, '\n'))) {

                *eol = '\0';
            }

            /* Somewhat unnecessary, since fgets would have returned NULL if it
             * was interrupted. Just here as a sanity check and nice place to
             * plop a breakpoint */
            if (caught_signal) {
                ret = NULL;
            }
        }
    }

    return ret;
}


void interactive_clear_terminal()
{
    /* If our input is a script, we should be nice and not clear the user's
     * terminal - only do this if our input's from stdin */
    if (input && is_stdin(input)) {
        /* FIXME Ugly, disgusting hack alert! Sound the alarms! */
#       if BLADERF_OS_WINDOWS
            system("cls");
#       else
            fputs("\033[2J\033[1;3f", stdout);
#endif

    }
}

/* TODO: Linux/OSX: Expand ~/ to home directory, $<variables>
 *       Windows: %<varibles%
 */
char * interactive_expand_path(char *path)
{
    return strdup(path);
}
