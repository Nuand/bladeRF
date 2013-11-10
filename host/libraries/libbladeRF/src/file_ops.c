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
#include <errno.h>
#include <inttypes.h>
#include <libbladeRF.h>
#include "host_config.h"
#include "file_ops.h"
#include "log.h"

int file_write(FILE *f, char *buf, size_t len)
{
    size_t rv;

    rv = fwrite(buf, 1, len, f);
    if(rv < len)
        return BLADERF_ERR_IO;

    return 0;
}

int file_read(FILE *f, char *buf, size_t len)
{
    size_t rv;

    rv = fread(buf, 1, len, f);
    if(rv < len) {
        if(feof(f))
            log_error("Unexpected end of file");
        else
            log_error("Error reading file");

        return BLADERF_ERR_IO;
    }

    return 0;
}

ssize_t file_size(FILE *f)
{
    int rv = BLADERF_ERR_IO;

    long int fpos = ftell(f);
    if(fpos < 0)
        goto out;

    if(fseek(f, 0, SEEK_END))
        goto out;

    ssize_t len = ftell(f);
    if(len < 0)
        goto out;

    if(fseek(f, fpos, SEEK_SET))
        goto out;

    rv = len;

out:
    return rv;
}

int file_read_buffer(const char *filename, uint8_t **buf_ret, size_t *size_ret)
{
    int status = BLADERF_ERR_UNEXPECTED;
    FILE *f;
    uint8_t *buf;
    ssize_t len;

    f = fopen(filename, "rb");
    if (!f) {
        log_error("fopen: %s\n", strerror(errno));
        return BLADERF_ERR_IO;
    }

    len = file_size(f);
    if(len < 0) {
        status = BLADERF_ERR_IO;
        goto os_file_read__err_size;
    }

    buf = malloc(len);
    if (!buf) {
        status = BLADERF_ERR_MEM;
        goto os_file_read__err_malloc;
    }

    status = file_read(f, (char*)buf, len);
    if (status < 0)
        goto os_file_read__err_read;

    *buf_ret = buf;
    *size_ret = len;
    fclose(f);
    return 0;

os_file_read__err_read:
    free(buf);
os_file_read__err_size:
os_file_read__err_malloc:
    fclose(f);
    return status;
}
