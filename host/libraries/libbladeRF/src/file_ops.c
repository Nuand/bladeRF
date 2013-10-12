/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2013 Nuand LLC
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

int read_file(const char *filename, uint8_t **buf_ret, size_t *size_ret)
{
    int status = BLADERF_ERR_UNEXPECTED;
    struct stat sb;
    FILE *f;
    int fd;
    uint8_t *buf;
    ssize_t n_read;

    f = fopen(filename, "rb");
    if (!f) {
        log_error("fopen: %s\n", strerror(errno));
        return BLADERF_ERR_IO;
    }

    fd = fileno(f);
    if (fd < 0) {
        log_error("fileno: %s\n", strerror(errno));
        status = BLADERF_ERR_IO;
        goto os_read_file__err_fileno;
    }

    if (fstat(fd, &sb) < 0) {
        log_error("fstat: %s\n", strerror(errno));
        status = BLADERF_ERR_IO;
        goto os_read_file__err_stat;
    }

    buf = malloc(sb.st_size);
    if (!buf) {
        status = BLADERF_ERR_MEM;
        goto os_read_file__err_malloc;
    }

    n_read = fread(buf, 1, sb.st_size, f);
    if (n_read != sb.st_size) {
        if (n_read < 0) {
            log_error("fread: %s\n", strerror(errno));
        } else {
            log_warning("short read: " PRIu64 "/" PRIu64  "\n",
                        (uint64_t)n_read, (uint64_t)sb.st_size);
        }

        status = BLADERF_ERR_IO;
        goto os_read_file__err_fread;
    }

    *buf_ret = buf;
    *size_ret = sb.st_size;
    fclose(f);
    return 0;

os_read_file__err_fread:
    free(buf);
os_read_file__err_malloc:
os_read_file__err_stat:
os_read_file__err_fileno:
    fclose(f);
    return status;
}
