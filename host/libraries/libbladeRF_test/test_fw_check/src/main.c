#include <stdio.h>
#include <libbladeRF.h>

#include "log.h"

#include "driver/fx3_fw.h"
#include "helpers/file.h"

int main(int argc, char *argv[])
{
    int status;
    uint8_t *buf;
    size_t buf_len;
    struct fx3_firmware *fw;

    log_set_verbosity(BLADERF_LOG_LEVEL_VERBOSE);

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <firmware file>\n", argv[0]);
        return 1;
    }

    status = file_read_buffer(argv[1], &buf, &buf_len);
    if (status != 0) {
        return status;
    }

    status = fx3_fw_parse(&fw, buf, buf_len);
    free(buf);
    if (status == 0) {
        fx3_fw_free(fw);
    }

    return status;
}

