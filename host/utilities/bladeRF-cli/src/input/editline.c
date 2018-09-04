/*
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2018 Nuand LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <stdlib.h>
#include <stdio.h>
#include <histedit.h>
#include <editline/readline.h>
#include <string.h>
#include <errno.h>
#include "input_impl.h"

EditLine *el = NULL;
History *hist = NULL;
char *last_line = NULL;

static char* bladerf_prompt(EditLine *el) {
    static char prompt[] = CLI_DEFAULT_PROMPT;
    return prompt;
}

int input_init()
{
    HistEvent ev;
    input_deinit();
    el = el_init("bladeRF-cli", stdin, stdout, stderr);
    hist = history_init();
    if (el && hist) {
        if (history(hist, &ev, H_SETSIZE, CLI_MAX_HIST_LEN) < 0 ||
            el_set(el, EL_PROMPT, bladerf_prompt) ||
            el_set(el, EL_HIST, history, hist)) {
            input_deinit();
            return CLI_RET_UNKNOWN;
        }
        return 0;
    } else {
        input_deinit();
        return CLI_RET_UNKNOWN;
    }
}

void input_deinit()
{
    if (last_line) {
        free(last_line);
        last_line = NULL;
    }
    if (el) {
        el_end(el);
    }
    if (hist) {
        history_end(hist);
    }
}

char * input_get_line(const char *prompt)
{
    int count;
    const char *ret = NULL;
    HistEvent ev;

    if (last_line) {
        free(last_line);
    }

    ret = el_gets(el, &count);

    if (ret && count > 0 && count < CLI_MAX_LINE_LEN) {
        if (history(hist, &ev, H_ENTER, ret) < 0) {
            fprintf(stderr, "%d - %s\n", ev.num, ev.str);
        }
        last_line = strdup(ret);
    } else {
        last_line = NULL;
    }

    return last_line;
}

int input_set_input(FILE *file)
{
    if (el_set(el, EL_SETFP, 0, file)) {
        return CLI_RET_UNKNOWN;
    }
    return 0;
}

char * input_expand_path(const char *path)
{
    return strdup(path);
}

void input_clear_terminal()
{
    fprintf(stderr, "editline backend does not currently support clear\n");
}

/* Nothing to do here, libedit handles the signal if we're in a call */
void input_ctrlc(void)
{
}
