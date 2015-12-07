/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014 Nuand LLC
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

#include <stdint.h>
#include <errno.h>

#include "bladerf_priv.h"
#include "fx3_fw_log.h"
#include "log.h"

#define LOGGER_ID_STRING
#include "logger_id.h"
#include "logger_entry.h"

int fx3_fw_log_dump(struct bladerf *dev, const char *filename)
{
    int status;
    FILE *f;
    logger_entry e;

    if (filename != NULL) {
        f = fopen(filename, "w");
        if (f == NULL) {
            switch (errno) {
                case ENOENT:
                    return BLADERF_ERR_NO_FILE;

                case EACCES:
                    return BLADERF_ERR_PERMISSION;

                default:
                    return BLADERF_ERR_IO;
            }
        }
    } else {
        f = stdout;
    }

    do {
        status = dev->fn->read_fw_log(dev, &e);
        if (status != 0) {
            log_debug("Failed to read FW log: %s\n", bladerf_strerror(status));
            goto out;
        }

        if (e == LOG_ERR) {
            fprintf(f, "<Unexpected error>,,\n");
        } else if (e != LOG_EOF) {
            uint8_t file_id;
            uint16_t line;
            uint16_t data;
            const char *src_file;

            logger_entry_unpack(e, &file_id, &line, &data);
            src_file = logger_id_string(file_id);

            fprintf(f, "%s, %u, 0x%04x\n", src_file, line, data);
        }
    } while (e != LOG_EOF && e != LOG_ERR);

out:
    if (f != NULL && f != stdout) {
        fclose(f);
    }

    return status;
}
