#include <linux/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>


struct bladeRF_version {
        unsigned short major;
        unsigned short minor;
};

#define BLADERF_IOCTL_BASE              'N'
#define BLADE_QUERY_VERSION             _IOR(BLADERF_IOCTL_BASE, 0, struct bladeRF_version)
#define BLADE_QUERY_FPGA_STATUS         _IOR(BLADERF_IOCTL_BASE, 1, unsigned int)
#define BLADE_BEGIN_PROG                _IOR(BLADERF_IOCTL_BASE, 2, unsigned int)
#define BLADE_END_PROG                  _IOR(BLADERF_IOCTL_BASE, 3, unsigned int)
#define BLADE_RF_RX                     _IOR(BLADERF_IOCTL_BASE, 4, unsigned int)




int time_past(struct timeval ref, struct timeval now) {
    if (now.tv_sec > ref.tv_sec)
        return 1;

    if (now.tv_sec == ref.tv_sec && now.tv_usec > ref.tv_usec)
        return 1;

    return 0;
}

#define STACK_BUFFER_SZ 1024 

int prog_fpga(int fd, char *file) {
    int ret;
    int fpga_fd, nread, toread;
    int programmed;
    size_t bytes;
    struct stat stat;
    char buf[STACK_BUFFER_SZ];
    struct timeval end_time, curr_time;
    int endTime;


    fpga_fd = open(file, 0);
    if (fpga_fd == -1)
        return 1;
    ioctl(fd, BLADE_BEGIN_PROG, &ret);
    fstat(fpga_fd, &stat);

#define min(x,y) ((x<y)?x:y)
    for (bytes = stat.st_size; bytes;) {
        nread = read(fpga_fd, buf, min(STACK_BUFFER_SZ, bytes));
        write(fd, buf, nread);
        bytes -= nread;
struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = 4000000;
	nanosleep(&ts, NULL);
    }
    close(fpga_fd);

    endTime = time(NULL) + 1;
    gettimeofday(&end_time, NULL);
    end_time.tv_sec++;

    programmed = 0;
    do {
        ioctl(fd, BLADE_QUERY_FPGA_STATUS, &ret);
        if (ret) {
            programmed = 1;
            break;
        }
        gettimeofday(&curr_time, NULL);
    } while(!time_past(end_time, curr_time));

    ioctl(fd, BLADE_END_PROG, &ret);

    return !programmed;
}

int main(int argc, char *argv[]) {
    struct bladeRF_version ver;
    int fd;
    int ret = 0;

    fd = open("/dev/bladerf1", O_WRONLY|O_CREAT|O_TRUNC, 0666);
    if (fd == -1) {
        printf("Can't open bladeRF\n");
        return 1;
    }

    if (!ioctl(fd, BLADE_QUERY_FPGA_STATUS, &ret))
        printf("FPGA programmed: %d\n", ret);

    if (!ioctl(fd, BLADE_QUERY_VERSION, &ver))
        printf("bladeRF firmware version: v%d.%d\n", ver.major, ver.minor);

    if (argc > 1) {
        ret = prog_fpga(fd, argv[1]);
        printf("FPGA programming: %s\n", ret ? "error" : "success");
    }

    return 0;
}
