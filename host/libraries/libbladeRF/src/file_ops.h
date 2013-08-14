/**
 * Wrappers around misc. file operations. These are collected here so that
 * they may easily be dummied out for NIOS headless builds in the future.
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
