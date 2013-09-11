#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
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
            log_warning("short read: %zd/%zd\n", n_read, sb.st_size);
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
