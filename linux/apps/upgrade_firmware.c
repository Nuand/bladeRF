#include <linux/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../common/bladeRF.h"

int main(int argc, char *argv[]) {
    struct bladeRF_version ver;
    int fd;
    int ret = 0;
    int val;

    fd = open("/dev/bladerf1", O_RDWR|O_CREAT|O_TRUNC, 0666);
    if (fd == -1) {
        printf("Can't open bladeRF\n");
        return 1;
    }

    if (argc > 1) {
        int fw_fd;
        int nread, nidx;
        struct stat fw_stat;
        struct bladeRF_firmware fw_param;

        fw_fd = open(argv[1], O_RDWR);
        if (fw_fd == -1) {
            printf("Can't open `%s'\n", argv[1]);
            return 1;
        }
        
        if (fstat(fw_fd, &fw_stat) == -1) {
            printf("Can't fstat firmware file\n");
            return 1;
        }

        fw_param.len = fw_stat.st_size;
        fw_param.ptr = malloc(fw_param.len);
        if (!fw_param.ptr) {
            printf("Can't allocate buffer\n");
            return 1;
        }

        nread = fw_stat.st_size;
        nidx = 0;
        do {
            ret = read(fw_fd, &fw_param.ptr[nidx], nread);
            if (ret == -1) {
                printf("Couldn't read file\n");
                return 1;
            }
            nread -= ret;
        } while(nread);

        ret = ioctl(fd, BLADE_UPGRADE_FW, &fw_param);
        printf("Upgrading fw, ptr=%p, len=%d\n", fw_param.ptr, fw_param.len); 
        printf("BLADE_UPGRADE_FW = %d\n", ret);
        free(fw_param.ptr);
        close(fw_fd);
    }

    return ret;
}
