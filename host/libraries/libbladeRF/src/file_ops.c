/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2013 Nuand LLC
 * Copyright (C) 2013 Daniel Gröber <dxld ÄT darkboxed DOT org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <libbladeRF.h>
#include "host_config.h"
#include "file_ops.h"
#include "minmax.h"
#include "log.h"
#include "rel_assert.h"

/* Paths to search for bladeRF files */
struct search_path_entries {
    bool prepend_home;
    const char *path;
};

int file_write(FILE *f, uint8_t *buf, size_t len)
{
    size_t rv;

    rv = fwrite(buf, 1, len, f);
    if(rv < len) {
        log_debug("File write failed: %s\n", strerror(errno));
        return BLADERF_ERR_IO;
    }

    return 0;
}

int file_read(FILE *f, char *buf, size_t len)
{
    size_t rv;

    rv = fread(buf, 1, len, f);
    if(rv < len) {
        if(feof(f))
            log_debug("Unexpected end of file: %s\n", strerror(errno));
        else
            log_debug("Error reading file: %s\n", strerror(errno));

        return BLADERF_ERR_IO;
    }

    return 0;
}

ssize_t file_size(FILE *f)
{
    ssize_t rv = BLADERF_ERR_IO;
    long int fpos = ftell(f);
    ssize_t len;

    if(fpos < 0) {
        log_verbose("ftell failed: %s\n", strerror(errno));
        goto out;
    }

    if(fseek(f, 0, SEEK_END)) {
        log_verbose("fseek failed: %s\n", strerror(errno));
        goto out;
    }

    len = ftell(f);
    if(len < 0) {
        log_verbose("ftell failed: %s\n", strerror(errno));
        goto out;
    }

    if(fseek(f, fpos, SEEK_SET)) {
        log_debug("fseek failed: %s\n", strerror(errno));
        goto out;
    }

    rv = len;

out:
    return rv;
}

int file_read_buffer(const char *filename, uint8_t **buf_ret, size_t *size_ret)
{
    int status = BLADERF_ERR_UNEXPECTED;
    FILE *f;
    uint8_t *buf = NULL;
    ssize_t len;

    f = fopen(filename, "rb");
    if (!f) {
        int errno_val = errno;
        return errno_val == ENOENT ? BLADERF_ERR_NO_FILE : BLADERF_ERR_IO;
    }

    len = file_size(f);
    if(len < 0) {
        status = BLADERF_ERR_IO;
        goto out;
    }

    buf = (uint8_t*) malloc(len);
    if (!buf) {
        status = BLADERF_ERR_MEM;
        goto out;
    }

    status = file_read(f, (char*)buf, len);
    if (status < 0) {
        goto out;
    }

    *buf_ret = buf;
    *size_ret = len;
    fclose(f);
    return 0;

out:
    free(buf);
    if (f) {
        fclose(f);
    }

    return status;
}


#if BLADERF_OS_LINUX || BLADERF_OS_OSX
#define ACCESS_FILE_EXISTS F_OK

static const struct search_path_entries search_paths[] = {
    { true,  "/.config/Nuand/bladeRF/" },
    { true,  "/.Nuand/bladeRF/" },
    { false, "/etc/Nuand/bladeRF/" },
    { false, "/usr/share/Nuand/bladeRF/" },
};

static inline size_t get_home_dir(char *buf, size_t max_len)
{
    const uid_t uid = getuid();
    const struct passwd *p = getpwuid(uid);
    strncat(buf, p->pw_dir, max_len);
    return strlen(buf);
}

#elif BLADERF_OS_WINDOWS
#define ACCESS_FILE_EXISTS 0
#include <shlobj.h>

static const struct search_path_entries search_paths[] = {
    { true,  "/Nuand/bladeRF/" },
};

static inline size_t get_home_dir(char *buf, size_t max_len)
{
    /* Explicitly link to a runtime DLL to get SHGetKnownFolderPath.
     * This deals with the case where we might not be able to staticly
     * link it at build time, e.g. mingw32.
     *
     * http://msdn.microsoft.com/en-us/library/784bt7z7.aspx
     */
    typedef HRESULT (CALLBACK* LPFNSHGKFP_T)(REFKNOWNFOLDERID, DWORD, HANDLE, PWSTR*);
    HINSTANCE hDLL;                         // Handle to DLL
    LPFNSHGKFP_T lpfnSHGetKnownFolderPath;  // Function pointer

    const KNOWNFOLDERID folder_id = FOLDERID_RoamingAppData;
    PWSTR path;
    HRESULT status;

    assert(max_len < INT_MAX);

    hDLL = LoadLibrary("Shell32");
    if (hDLL == NULL) {
        // DLL couldn't be loaded, bail out.
        return 0;
    }

    lpfnSHGetKnownFolderPath = (LPFNSHGKFP_T)GetProcAddress(hDLL, "SHGetKnownFolderPath");

    if (!lpfnSHGetKnownFolderPath) {
        // Can't find the procedure we want.  Free and bail.
        FreeLibrary(hDLL);
        return 0;
    }

    status = lpfnSHGetKnownFolderPath(&folder_id, 0, NULL, &path);
    if (status == S_OK) {
        WideCharToMultiByte(CP_ACP, 0, path, -1, buf, (int)max_len, NULL, NULL);
        CoTaskMemFree(path);
    }

    FreeLibrary(hDLL);

    return strlen(buf);
}

#else
#error "Unknown OS or missing BLADERF_OS_* definition"
#endif

/* We're not using functions that use the *nix PATH_MAX (which is known to be problematic),
 * or the WIN32 MAX_PATH.  Therefore,  we'll just use this arbitrary, but "sufficiently"
 * large max buffer size for paths */
#define PATH_MAX_LEN    4096

char *file_find(const char *filename)
{
    size_t i, max_len;
    char *full_path = (char*) calloc(1, PATH_MAX_LEN + 1);

    for (i = 0; i < ARRAY_SIZE(search_paths); i++) {
        memset(full_path, 0, PATH_MAX_LEN);
        max_len = PATH_MAX_LEN - 1;

        if (search_paths[i].prepend_home) {
            max_len -= get_home_dir(full_path, max_len);
        }

        strncat(full_path, search_paths[i].path, max_len);
        max_len = PATH_MAX_LEN - strlen(full_path);
        strncat(full_path, filename, max_len);

        if (access(full_path, ACCESS_FILE_EXISTS) != -1) {
            return full_path;
        }
    }

    free(full_path);
    return NULL;
}

int file_find_and_read(const char *filename, uint8_t **buf, size_t *size)
{
    int status;
    char *full_path = file_find(filename);
    *buf = NULL;
    *size = 0;

    if (full_path != NULL) {
        status = file_read_buffer(full_path, buf, size);
        free(full_path);
        return status;
    } else {
        return BLADERF_ERR_NO_FILE;
    }
}
