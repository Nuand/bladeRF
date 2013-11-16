/**
 * @file file_ops.h
 *
 * @brief Wrappers around misc. file operations.
 *
 * These are collected here so that they may easily be dummied out for NIOS
 * headless builds in the future.
 *
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
#ifndef FILE_OPS_H__
#define FILE_OPS_H__

#include <stdint.h>

/**
 * Read file contents into a buffer allocated internally and returned to the
 * caller through `buf'.
 *
 * The caller is responsible for freeing the allocated buffer
 *
 * @param[in]   filename    File open
 * @param[out]  buf         Upon success, this will point to a heap-allocated
 *                          buffer containing the file contents
 * @parma[out]  size        Upon success, this will be updated to reflect the
 *                          size of the buffered file
 *
 * @return 0 on success, negative BLADERF_ERR_* value on failure
 */
int file_read_buffer(const char *filename, uint8_t **buf, size_t *size);

/**
 * Write to an open file stream.
 *
 * @param[in]   f           Open file stream.
 * @param[in]   buf         Data to write to the stream.
 * @parma[in]   len         Number of bytes to write to the stream.
 *
 * @return 0 on success, negative BLADERF_ERR_* value on failure
 */
int file_write(FILE *f, uint8_t *buf, size_t len);

/**
 * Read data from an open file stream.
 *
 * @param[in]   f           Open file stream.
 * @param[out]  buf         Buffer to fill with data read.
 * @parma[in]   len         Number of bytes to read. If EOF is encountered
 *                          before this many bytes have been read will return
 *                          an error.
 *
 * @return 0 on success, negative BLADERF_ERR_* value on failure
 */
int file_read(FILE *f, char *buf, size_t len);

/**
 * Determine the size of an open file stream.
 *
 * @param[in]   f           Open file stream.
 *
 * @return poisitive size of file on success, negative BLADERF_ERR_* value on
 * failure
 */
ssize_t file_size(FILE *f);

#endif
