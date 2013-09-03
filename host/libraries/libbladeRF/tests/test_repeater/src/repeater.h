#ifndef REPEATER_H__
#define REPEATER_H__

#define DEFAULT_NUM_BUFFERS         32
#define DEFAULT_NUM_TRANSFERS       16
#define DEFAULT_SAMPLES_PER_BUFFER  8192
#define DEFAULT_SAMPLE_RATE         10000000
#define DEFAULT_FREQUENCY           1000000000
#define DEFAULT_BANDWIDTH           7000000

/**
 * Application configuration
 */
struct repeater_config
{
    char *device_str;           /**< Device to use */

    int tx_freq;                /**< TX frequency */
    int rx_freq;                /**< RX frequency */
    int sample_rate;            /**< Sampling rate */
    int bandwidth;              /**< Bandwidth */

    int num_buffers;            /**< Number of buffers to allocate and use */
    int num_transfers;          /**< Number of transfers to allocate and use */
    int samples_per_buffer;     /**< Number of SC16Q12 samples per buffer */


    bladerf_log_level verbosity;    /** Library verbosity */
};

/**
 * Initialize the provided application config to defaults
 *
 * @param   c   Configuration to initialize
 */
void repeater_config_init(struct repeater_config *c);

/**
 * Deinitialize configuration and free any heap-allocated items
 *
 * @param   c   Configuration to deinitialize
 */
void repeater_config_deinit(struct repeater_config *c);

/**
 * Kick off repeater test
 *
 * @param   c   Repeater configuration
 *
 * @return  0 on succes, non-zero on failure
 */
int repeater_run(struct repeater_config *c);

#endif
