/*
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2013 Nuand LLC
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
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include "script.h"
#include "host_config.h"
#include "common.h"

#if BLADERF_OS_WINDOWS
#include <io.h>
typedef BY_HANDLE_FILE_INFORMATION file_info;
#else
typedef struct stat file_info;
#endif

struct script {
    struct script *prev;
    unsigned int line_no;
    FILE *file;
    const char *name;
    file_info info;
};

#if BLADERF_OS_WINDOWS
static int populate_file_info(struct script *s, FILE *f)
{
    HANDLE win_handle;

    int fd = fileno(f);
    if (fd < 0) {
        return -1;
    }

    win_handle = (HANDLE)_get_osfhandle(fd);
    if (!win_handle) {
        return errno;
    }

    if (GetFileInformationByHandle(win_handle, &s->info)) {
        return 0;
    } else {
        /* For lack of a better error...*/
        return -ENOTTY;
    }
}

static inline bool file_info_matches(const struct script *a, const struct script *b)
{
    return (a->info.nFileIndexHigh == b->info.nFileIndexHigh) &&
           (a->info.nFileIndexLow == b->info.nFileIndexLow);
}
#else
static int populate_file_info(struct script *s, FILE *f)
{
    int fd = fileno(f);
    if (fd < 0) {
        return -errno;
    }

    if (fstat(fd, &s->info) < 0) {
        return -errno;
    }

    return 0;
}

static inline bool file_info_matches(const struct script *a, const struct script *b)
{
    return a->info.st_ino == b->info.st_ino;
}
#endif

int cli_open_script(struct script **s, const char *filename)
{
    FILE *f;
    int error;
    struct script *to_check, *new_script;

    f = expand_and_open(filename, "r");
    if (!f) {
        error = -errno;
        goto cli_open_script__file_err;
    }

    new_script = (struct script *)malloc(sizeof(*new_script));
    if (!new_script) {
        error = -errno;
        goto cli_open_script__malloc_err;
    }

    error = populate_file_info(new_script, f);
    if (error != 0) {
        goto cli_open_script__file_info_error;
    }

    for (to_check = *s; to_check != NULL; to_check = (*s)->prev) {
        if (file_info_matches(to_check, new_script)) {
            error = 1;
            goto cli_open_script__recursive_err;
        }
    }

    new_script->name = strdup(filename);
    if (!new_script->name) {
        error = -errno;
        goto cli_open_script__name_err;
    }

    new_script->prev = *s;
    new_script->line_no = 0;
    new_script->file = f;
    *s = new_script;

    return 0;

cli_open_script__file_info_error:
cli_open_script__recursive_err:
cli_open_script__name_err:
    free(new_script);
cli_open_script__malloc_err:
    fclose(f);
cli_open_script__file_err:
    return error;
}

int cli_close_script(struct script **s)
{
    int ret = 0;
    struct script *prev;

    if (*s == NULL) {
        return 0;
    }

    prev = (*s)->prev;
    free((void*)(*s)->name);

    if (fclose((*s)->file) < 0) {
        ret = -errno;
    }
    free(*s);
    *s = prev;
    return ret;
}

int cli_close_all_scripts(struct script **s)
{
    int ret = 0;

    while (!ret && *s) {
        ret = cli_close_script(s);
    }

    return ret;
}

unsigned int cli_num_scripts(struct script *curr_script) {
    struct script *s;
    unsigned int ret = 0;

    for (s = curr_script; s != NULL; s = curr_script->prev) {
        ret++;
    }

    return ret;
}

bool cli_script_loaded(struct script *s)
{
    bool is_loaded;

    is_loaded = s != NULL;

    /* Sanity check...if s is non-null we *should* have a file opened */
    assert(!is_loaded || s->file != NULL);

    return is_loaded;
}

FILE * cli_script_file(struct script *s)
{
    FILE *ret = NULL;

    if (s && s->file) {
        ret = s->file;
    }

    return ret;
}

const char * cli_script_file_name(struct script *s)
{
    if (s) {
        return s->name;
    } else {
        return "(unknown file)";
    }
}

void cli_script_bump_line_count(struct script *s)
{
    if (s) {
        s->line_no++;
    }
}

unsigned int cli_script_line(struct script *s)
{
    unsigned int ret;

    if (s) {
        ret = s->line_no;
    } else {
        ret = 0;
    }

    return ret;
}
