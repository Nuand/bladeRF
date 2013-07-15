#include <libbladeRF.h>
#include <time.h>
#include <inttypes.h>

/* TODO Should there be a "big-lock" wrapped around accesses to a device */
struct bladerf {
        int fd;   /* File descriptor to associated driver device node */
        struct bladerf_stats stats;
};

/* Subtracting two timespecs */
int timespec_subtract(struct timespec *result, struct timespec *x, struct timespec *y)
{
    /* Perform the carry for the later subtraction by updating y. */
    if (x->tv_nsec < y->tv_nsec) {
        int psec = (y->tv_nsec - x->tv_nsec) / 1000000000 + 1;
        y->tv_nsec -= 1000000000 * psec;
        y->tv_sec += psec;
    }

    if (x->tv_nsec - y->tv_nsec > 1000000000) {
        int psec = (x->tv_nsec - y->tv_nsec) / 1000000000;
        y->tv_nsec += 1000000000 * psec;
        y->tv_sec -= psec;
    }

    /* Compute the time remaining to wait.
       tv_usec is certainly positive. */
    result->tv_sec = x->tv_sec - y->tv_sec;
    result->tv_nsec = x->tv_nsec - y->tv_nsec;

    /* Return 1 if result is negative. */
    return x->tv_sec < y->tv_sec;
}

int16_t buf[2048] ;

/* Printing stats */
void print_stats( struct timespec *end, struct timespec *begin, uint32_t expected, uint64_t numsamps )
{
    struct timespec diff;
    struct timespec t;
    int rv ;
    uint32_t sps ;
    double ppm ;
    rv = timespec_subtract( &diff, end, begin );
    sps = (uint32_t)(((uint64_t)numsamps*1000000000)/(((uint64_t)diff.tv_sec)*1000000000+diff.tv_nsec));
    ppm = ((double)sps-(double)expected)/((double)expected/1000000.0) ;
    printf( "\r" );
    printf( "Samples: %10"PRIu64"    dTime: %ld.%09ld    Samples/second: %10"PRIu32"    %15.4fppm",
        numsamps,
        diff.tv_sec,
        diff.tv_nsec,
        sps,
        ppm
    ) ;
    fflush( stdout );
    return;
}

int main(int argc, char *argv[]) {
    int numsam, numread;
    unsigned short *rb;
    //unsigned char *buf;
    int bufidx, rv;
    struct timespec begin, end ;
    uint32_t update, expected ;

    if(argc != 3 ) {
        printf( "Usage: %s <expected-rate> <update-rate>\n", argv[0] );
        printf( "\n" );
        printf( "    expected-rate          The expected sample rate that the radio is running\n" );
        printf( "    update-rate            The number of samples inbetween statistics printing\n" );
    }

    struct bladerf *dev = bladerf_open("/dev/bladerf0");
    if (!dev) {
        printf( "Couldn't open device\n" ) ;
        return 0;
    }

    expected = atoi(argv[1]);
    update = atoi(argv[2]);

    int nread, j ;
    uint64_t x, lastx ;

    // Get the time
    rv = clock_gettime( CLOCK_REALTIME, &begin );
    lastx = 0;
    if( rv ) {
        printf( "TIME PROBLEM!!\n" );
    }
    for (x = 0; /*x < numsam*/; x+=1024) {
        nread = read(dev->fd, &buf, 1024*4);
        /* Every ~1000000 samples, print out the update */
        if( x-lastx >= update ) {
            // Get the time
            rv = clock_gettime( CLOCK_REALTIME, &end );
            print_stats( &end, &begin, expected, x-lastx );
            rv = clock_gettime( CLOCK_REALTIME, &begin );
            lastx = x;
        }
    }

    return 0 ;
}
