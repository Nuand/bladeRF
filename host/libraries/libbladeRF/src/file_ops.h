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
 * Read file contents into an allocated buffer.
 *
 * The caller is responsible for freeing the allocated buffer
 *
 * @param[in]   filename    File open
 * @param[out]  buf         Upon success, this will point to a heap-allocated
 *                          buffer containing the file contents
 * @parma[out]  size        Upon success, this will be updated to reflect the
 *                          size of the buffered file
 *
 * @return 0 on success, BLADERF_ERR_* value on failure
 */
int read_file(const char *filename, uint8_t **buf, size_t *size);

#endif
