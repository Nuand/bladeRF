/* This file is not part of the API and may be changed on a whim.
 * If you're interfacing with libbladerf, DO NOT use this file! */

#ifndef BLADERF_PRIV_H_
#define BLADERF_PRIV_H_

/* TODO Should there be a "big-lock" wrapped around accesses to a device */
struct bladerf {
    int fd;   /* File descriptor to associated driver device node */
    char *path; /* Path of the opened fd */
    struct bladerf_stats stats;
};

#endif

