struct bladerf_linux {
    int fd;         /* File descriptor to associated driver device node */
    char *path;     /* Path of the opened fd */

    int last_errno; /* Added for debugging. TODO Remove or integrate into
                     * error reporting? (I vote the latter -- Jon). */
};
