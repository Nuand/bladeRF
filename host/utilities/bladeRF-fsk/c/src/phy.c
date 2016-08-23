/**
 * @brief   Physical layer code for modem
 *
 * This file handles transmission/reception of data frames over the air. It uses fsk.c to
 * perform baseband IQ modulation and demodulation, and libbladeRF to transmit/receive
 * samples using the bladeRF device. A different modulator could be used by swapping
 * fsk.c with a file that implements a different modulator. On the receive side the file
 * uses fir_filter.c to low-pass filter the received signal, pnorm.c to power normalize
 * the input signal, and correlator.c to correlate the received signal with a preamble.
 * waveform.
 *
 * The structure of a physical layer transmission is as follows:
 * / ramp up | training sequence | preamble | link layer frame | ramp down \
 *
 * The ramps at the beginning/end are SAMP_PER_SYMB samples long
 *
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2016 Nuand LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include "phy.h"
#include "fir_filter.h"
#include "rx_ch_filter.h"
#include "pnorm.h"
#include "prng.h"
#include "correlator.h"
#include "fsk.h"            //modulator/demodulator
#include "radio_config.h"    //bladeRF configuration

#ifdef DEBUG_MODE
    #define DEBUG_MSG(...) fprintf(stderr, __VA_ARGS__)
    #ifndef ENABLE_NOTES
        #define ENABLE_NOTES
    #endif
#else
    #define DEBUG_MSG(...)
#endif

#ifdef ENABLE_NOTES
    #define NOTE(...) fprintf(stderr, __VA_ARGS__)
#else
    #define NOTE(...)
#endif

//Internal structs
struct rx {
    int16_t *in_samples;        //Raw input samples from device
    struct fir_filter *ch_filt;             //Channel filter
    struct pnorm_state_t *pnorm;            //Power normalizer
    struct correlator *corr;                //Correlator
    struct complex_sample *filt_samples;    //Filtered input samples
    struct complex_sample *pnorm_samples;    //power normalized samples
    uint8_t *data_buf;            //received data output buffer (no training seq/preamble)
    bool buf_filled;            //is the rx data buffer filled
    bool stop;                    //control variable to stop the receiver
    pthread_t thread;            //pthread for the receiver
    pthread_cond_t buf_filled_cond;        //condition variable for buf_filled
    pthread_mutex_t buf_status_lock;    //mutex variable for accessing buf_filled
};
struct tx {
    uint8_t *data_buf;            //input data to transmit (including training seq/preamble)
    unsigned int data_length;    //length of data to transmit (not including preamble)
    bool buf_filled;
    bool stop;
    pthread_t thread;
    pthread_cond_t buf_filled_cond;
    pthread_mutex_t buf_status_lock;
    unsigned int max_num_samples;        //Maximum number of tx samples to transmit
    struct complex_sample *samples;        //output samples to transmit
};

struct phy_handle {
    struct bladerf *dev;        //bladeRF device handle
    struct fsk_handle *fsk;        //fsk handle
    struct tx *tx;                //tx data structure
    struct rx *rx;                //rx data structure
    uint8_t *scrambling_sequence;
};

//Internal functions
void *phy_receive_frames(void *arg);
void *phy_transmit_frames(void *arg);
static void scramble_frame(uint8_t *frame, int frame_length, uint8_t *scrambling_sequence);
static void unscramble_frame(uint8_t *frame, int frame_length, uint8_t *scrambling_sequence);
static void create_ramps(unsigned int ramp_length, struct complex_sample ramp_down_init,
                    struct complex_sample *ramp_up, struct complex_sample *ramp_down);

/****************************************
 *                                      *
 *        INIT/DEINIT  FUNCTIONS        *
 *                                      *
 ****************************************/

struct phy_handle *phy_init(struct bladerf *dev, struct radio_params *params)
{
    int status;
    struct phy_handle *phy;
    uint64_t prng_seed;
    uint8_t preamble[PREAMBLE_LENGTH] = PREAMBLE;

    //------------Allocate memory for phy handle struct--------------
    //Calloc so all pointers are initialized to NULL
    phy = calloc(1, sizeof(struct phy_handle));
    if (phy == NULL){
        perror("malloc");
        return NULL;
    }

    DEBUG_MSG("[PHY] Initializing...\n");

    if (dev == NULL){
        fprintf(stderr, "[PHY] %s: BladeRF device uninitialized", __FUNCTION__);
    }
    phy->dev = dev;

    //--------Initialize and configure bladeRF device-------------
    status = radio_init_and_configure(phy->dev, params);
    if (status != 0){
        fprintf(stderr, "[PHY] %s: Couldn't configure bladeRF\n", __FUNCTION__);
        goto error;
    }
    DEBUG_MSG("[PHY] BladeRF initialized and configured successfully\n");

    //-------------------Open fsk handle------------------------
    phy->fsk = fsk_init();
    if(phy->fsk == NULL){
        fprintf(stderr, "[PHY] %s: Couldn't open fsk handle\n", __FUNCTION__);
        goto error;
    }
    DEBUG_MSG("[PHY] FSK Initialized\n");

    //------------------Initialize TX struct--------------------
    phy->tx = calloc(1, sizeof(struct tx));
    if (phy->tx == NULL){
        perror("[PHY] malloc");
        goto error;
    }
    //Allocate memory for tx data buffer
    phy->tx->data_buf = malloc(TRAINING_SEQ_LENGTH + PREAMBLE_LENGTH +
                                MAX_LINK_FRAME_SIZE);
    if (phy->tx->data_buf == NULL){
        perror("[PHY] malloc");
        goto error;
    }
    //Allocate memory for tx samples buffer
    //2*RAMP_LENGTH for the ramp up/ramp down
    phy->tx->max_num_samples = 2*RAMP_LENGTH + (TRAINING_SEQ_LENGTH + PREAMBLE_LENGTH +
                    MAX_LINK_FRAME_SIZE) * 8 * SAMP_PER_SYMB;
    phy->tx->samples = malloc(phy->tx->max_num_samples * sizeof(struct complex_sample));
    if (phy->tx->samples == NULL){
        perror("[PHY] malloc");
        goto error;
    }
    //Initialize control variables
    phy->tx->data_length = 0;
    phy->tx->buf_filled = false;
    phy->tx->stop = false;
    //Initialize pthread condition variable for buf_filled
    status = pthread_cond_init(&(phy->tx->buf_filled_cond), NULL);
    if (status != 0){
        fprintf(stderr, "[PHY] %s: Error initializing pthread_cond\n", __FUNCTION__);
        goto error;
    }
    //Initialize pthread mutex variable for buf_filled
    status = pthread_mutex_init(&(phy->tx->buf_status_lock), NULL);
    if (status != 0){
        fprintf(stderr, "[PHY] %s: Error initializing pthread_mutex\n", __FUNCTION__);
        goto error;
    }

    //------------------Initialize RX struct------------------
    phy->rx = calloc(1, sizeof(struct rx));
    if (phy->rx == NULL){
        perror("[PHY] malloc");
        goto error;
    }
    //Allocate memory for rx data buffer
    phy->rx->data_buf = malloc(MAX_LINK_FRAME_SIZE);
    if (phy->rx->data_buf == NULL){
        perror("[PHY] malloc");
        goto error;
    }
    //Allocate memory for rx samples buffer
    phy->rx->in_samples = malloc(NUM_SAMPLES_RX * 2 * sizeof(phy->rx->in_samples[0]));
    if (phy->rx->in_samples == NULL){
        perror("[PHY] malloc");
        goto error;
    }

    // Allocate memory for filtered RX samples
    phy->rx->filt_samples = malloc(NUM_SAMPLES_RX * sizeof(struct complex_sample));
    if (phy->rx->filt_samples == NULL){
        perror("[PHY] malloc");
        goto error;
    }

    //Allocate memory for power normalized samples
    phy->rx->pnorm_samples = malloc(NUM_SAMPLES_RX * sizeof(struct complex_sample));
    if (phy->rx->pnorm_samples == NULL){
        perror("[PHY] malloc");
        goto error;
    }

    // Create RX Channel Filter
    phy->rx->ch_filt = fir_init(rx_ch_filter, rx_ch_filter_len);
    if (phy->rx->ch_filt == NULL) {
        fprintf(stderr, "[PHY] %s: Failed to create channel filter.\n", __FUNCTION__);
        goto error;
    }

    // Create power normalizer
    phy->rx->pnorm = pnorm_init(0.95f, 0.1f, 20.0f);
    if (phy->rx->pnorm == NULL){
        fprintf(stderr, "[PHY] %s: Couldn't initialize power normalizer\n",
                __FUNCTION__);
        goto error;
    }

    //Create RX correlator
    phy->rx->corr = corr_init(preamble, 8*PREAMBLE_LENGTH, SAMP_PER_SYMB);
    if (phy->rx->corr == NULL){
        fprintf(stderr, "[PHY] %s: Couldn't initialize correlator\n", __FUNCTION__);
        goto error;
    }

    //Initialize control variables
    phy->rx->buf_filled = false;
    phy->rx->stop = false;
    //Initialize pthread condition variable for buf_filled
    status = pthread_cond_init(&(phy->rx->buf_filled_cond), NULL);
    if (status != 0){
        fprintf(stderr, "[PHY] %s: Error initializing pthread_cond\n", __FUNCTION__);
        goto error;
    }
    //Initialize pthread mutex variable for buf_filled
    status = pthread_mutex_init(&(phy->rx->buf_status_lock), NULL);
    if (status != 0){
        fprintf(stderr, "[PHY] %s: Error initializing pthread_mutex\n", __FUNCTION__);
        goto error;
    }

    //-----------------Load scrambling sequence--------------------
    prng_seed = PRNG_SEED;
    phy->scrambling_sequence = prng_fill(&prng_seed, MAX_LINK_FRAME_SIZE);
    if (phy->scrambling_sequence == NULL){
        fprintf(stderr, "[PHY] %s: Couldn't load scrambling sequence\n", __FUNCTION__);
        goto error;
    }

    #ifdef BYPASS_RX_CHANNEL_FILTER
        DEBUG_MSG("[PHY] Info: Bypassing rx channel filter\n");
    #endif
    #ifdef BYPASS_RX_PNORM
        DEBUG_MSG("[PHY] Info: Bypassing rx power normalization\n");
    #endif
    #ifdef BYPASS_PHY_SCRAMBLING
        DEBUG_MSG("[PHY] Info: Bypassing scrambling\n");
    #endif

    DEBUG_MSG("[PHY] Initialization done\n");

    return phy;

    error:
        phy_close(phy);
        return NULL;
}

void phy_close(struct phy_handle *phy)
{
    int status;

    DEBUG_MSG("[PHY] Closing\n");
    if (phy != NULL){
        //close fsk handle
        fsk_close(phy->fsk);
        //Stop bladeRF (handle closed elsewhere)
        radio_stop(phy->dev);
        //free scrambling sequence buffer
        free(phy->scrambling_sequence);
        //free TX struct and its buffers
        if (phy->tx != NULL){
            free(phy->tx->data_buf);
            free(phy->tx->samples);
            status = pthread_mutex_destroy(&(phy->tx->buf_status_lock));
            if (status != 0){
                fprintf(stderr, "[PHY] %s: Error destroying pthread_mutex\n",
                        __FUNCTION__);
            }
            status = pthread_cond_destroy(&(phy->tx->buf_filled_cond));
            if (status != 0){
                fprintf(stderr, "[PHY] %s: Error destroying pthread_cond\n",
                        __FUNCTION__);
            }
        }
        free(phy->tx);
        //free RX struct and its buffers
        if (phy->rx != NULL){
            free(phy->rx->data_buf);
            fir_deinit(phy->rx->ch_filt);
            corr_deinit(phy->rx->corr);
            pnorm_deinit(phy->rx->pnorm);
            free(phy->rx->in_samples);
            free(phy->rx->filt_samples);
            free(phy->rx->pnorm_samples);
            status = pthread_mutex_destroy(&(phy->rx->buf_status_lock));
            if (status != 0){
                fprintf(stderr, "[PHY] %s: Error destroying pthread_mutex\n",
                        __FUNCTION__);
            }
            status = pthread_cond_destroy(&(phy->rx->buf_filled_cond));
            if (status != 0){
                fprintf(stderr, "[PHY] %s: Error destroying pthread_cond\n",
                        __FUNCTION__);
            }
        }
        free(phy->rx);
    }
    //free phy struct
    free(phy);
    phy = NULL;
}

/****************************************
 *                                      *
 *          TRANSMITTER FUNCTIONS       *
 *                                      *
 ****************************************/

int phy_start_transmitter(struct phy_handle *phy)
{
    int status;

    //turn off stop signal
    phy->tx->stop = false;
    //Kick off frame transmitter thread
    status = pthread_create(&(phy->tx->thread), NULL, phy_transmit_frames, phy);
    if (status != 0){
        fprintf(stderr, "[PHY] %s: Error creating tx thread: %s\n", __FUNCTION__,
                strerror(status));
        return -1;
    }
    return 0;
}

int phy_stop_transmitter(struct phy_handle *phy)
{
    int status;

    DEBUG_MSG("[PHY] TX: Stopping transmitter...\n");
    //signal stop
    phy->tx->stop = true;
    //Signal the buffer filled condition so that the thread will stop
    //waiting for a filled buffer
    status = pthread_mutex_lock(&(phy->tx->buf_status_lock));
    if (status != 0){
        fprintf(stderr, "[PHY] %s: Error locking pthread_mutex\n", __FUNCTION__);
    }
    status = pthread_cond_signal(&(phy->tx->buf_filled_cond));
    if (status != 0){
        fprintf(stderr, "[PHY] %s: Error signaling pthread_cond\n", __FUNCTION__);
    }
    status = pthread_mutex_unlock(&(phy->tx->buf_status_lock));
    if (status != 0){
        fprintf(stderr, "[PHY] %s: Error unlocking pthread_mutex\n", __FUNCTION__);
    }
    //Wait for tx thread to finish
    status = pthread_join(phy->tx->thread, NULL);
    if (status != 0){
        fprintf(stderr, "[PHY] %s: Error joining tx thread: %s\n", __FUNCTION__,
                strerror(status));
        return -1;
    }
    DEBUG_MSG("[PHY] TX: Transmitter stopped\n");
    return 0;
}

int phy_fill_tx_buf(struct phy_handle *phy, uint8_t *data_buf, unsigned int length)
{
    int status;

    //Check for null
    if (data_buf == NULL){
        fprintf(stderr, "[PHY] %s: The supplied data buf is null\n", __FUNCTION__);
        return -1;
    }
    //Check for length outside of bounds
    if (length > MAX_LINK_FRAME_SIZE){
        fprintf(stderr, "[PHY] %s: Data length of %u is greater than the maximum "
                        "allowed length (%u)\n", __FUNCTION__, length,
                        MAX_LINK_FRAME_SIZE);
        return -1;
    }
    //Wait for the buffer to be empty (and therefore ready for the next frame)
    //Not doing a pthread_cond_wait() because this shouldn't take long
    while(phy->tx->buf_filled){
        usleep(50);
    }

    //Copy the tx data into the phy's tx data buf, just after where the preamble goes
    memcpy(&(phy->tx->data_buf[TRAINING_SEQ_LENGTH+PREAMBLE_LENGTH]), data_buf, length);
    //Set the data length
    phy->tx->data_length = length;
    //Mark the buffer filled
    phy->tx->buf_filled = true;
    //Signal the buffer filled condition
    status = pthread_mutex_lock(&(phy->tx->buf_status_lock));
    if (status != 0){
        fprintf(stderr, "[PHY] %s: Error locking pthread_mutex\n", __FUNCTION__);
        return -1;
    }
    status = pthread_cond_signal(&(phy->tx->buf_filled_cond));
    if (status != 0){
        fprintf(stderr, "[PHY] %s: Error signaling pthread_cond\n", __FUNCTION__);
        return -1;
    }
    status = pthread_mutex_unlock(&(phy->tx->buf_status_lock));
    if (status != 0){
        fprintf(stderr, "[PHY] %s: Error unlocking pthread_mutex\n", __FUNCTION__);
        return -1;
    }
    return 0;
}

/**
 * Thread function which transmits data frames
 *
 * @param[in]   arg     pointer to phy handle struct
 */
void *phy_transmit_frames(void *arg)
{
    int status;
    //Cast arg
    struct phy_handle *phy = (struct phy_handle *) arg;
    uint8_t preamble[PREAMBLE_LENGTH] = PREAMBLE;
    uint8_t training_seq[TRAINING_SEQ_LENGTH] = TRAINING_SEQ;
    int ramp_down_index;
    int num_mod_samples, num_samples;
    bool failed = false;
    struct bladerf_metadata metadata;
    int16_t *out_samples_raw = NULL;

    out_samples_raw = malloc(phy->tx->max_num_samples * 2 * sizeof(int16_t));
    if (out_samples_raw == NULL){
        perror("[PHY] malloc");
        return NULL;
    }

    //Set field(s) in bladerf metadata struct
    memset(&metadata, 0, sizeof(metadata));
    metadata.flags =     BLADERF_META_FLAG_TX_BURST_START |
                        BLADERF_META_FLAG_TX_NOW |
                        BLADERF_META_FLAG_TX_BURST_END;

    while (!phy->tx->stop){
        //--------Wait for buffer to be filled---------
        //Lock mutex
        status = pthread_mutex_lock(&(phy->tx->buf_status_lock));
        if (status != 0){
            fprintf(stderr, "[PHY] %s: Mutex lock failed: %s\n", __FUNCTION__,
                    strerror(status));
            return NULL;
        }
        //Wait for condition signal - meaning buffer is full
        while (!phy->tx->buf_filled && !phy->tx->stop){
            DEBUG_MSG("[PHY] TX: Waiting for buffer to be filled\n");
            status = pthread_cond_wait(&(phy->tx->buf_filled_cond), &(phy->tx->buf_status_lock));
            if (status != 0){
                fprintf(stderr, "[PHY] %s: Condition wait failed: %s\n", __FUNCTION__,
                        strerror(status));
                failed = true;
                break;
            }
        }
        //Unlock mutex
        status = pthread_mutex_unlock(&(phy->tx->buf_status_lock));
        if (status != 0){
            fprintf(stderr, "[PHY] %s: Mutex unlock failed: %s\n", __FUNCTION__,
                    strerror(status));
            failed = true;
        }
        //Stop thread if stop variable is true, or something with pthreads went wrong
        if (phy->tx->stop || failed){
            phy->tx->buf_filled = false;
            return NULL;
        }
        //------------Transmit the frame-------------
        DEBUG_MSG("[PHY] TX: Buffer filled. Transmitting.\n");
        //Calculate the number of samples to transmit.
        num_samples = 2*RAMP_LENGTH + (TRAINING_SEQ_LENGTH + PREAMBLE_LENGTH +
                    phy->tx->data_length) * 8 * SAMP_PER_SYMB;
        //Add training sequence to tx data buffer
        memcpy(phy->tx->data_buf, &training_seq, TRAINING_SEQ_LENGTH);
        //Add preamble to tx data buffer
        memcpy(&(phy->tx->data_buf[TRAINING_SEQ_LENGTH]), &preamble, PREAMBLE_LENGTH);
        #ifndef BYPASS_PHY_SCRAMBLING
            //Scramble the frame data (not including the training sequence or preamble)
            scramble_frame(&(phy->tx->data_buf[TRAINING_SEQ_LENGTH + PREAMBLE_LENGTH]),
                            phy->tx->data_length, phy->scrambling_sequence);
        #endif
        //zero the tx samples buffer
        memset(phy->tx->samples, 0, sizeof(int16_t) * 2 * num_samples);
        //modulate samples - leave space for ramp up/ramp down in the samples buffer
        num_mod_samples = fsk_mod(phy->fsk, phy->tx->data_buf,
                            TRAINING_SEQ_LENGTH + PREAMBLE_LENGTH + phy->tx->data_length,
                            &(phy->tx->samples[RAMP_LENGTH]));
        //Mark the buffer empty
        phy->tx->buf_filled = false;

        //Add the ramp up/ ramp down of samples
        ramp_down_index = RAMP_LENGTH+num_mod_samples;
        create_ramps(RAMP_LENGTH, phy->tx->samples[ramp_down_index-1], phy->tx->samples,
                        &(phy->tx->samples[ramp_down_index]));
        //Convert samples
        conv_struct_to_samples(phy->tx->samples, num_samples, out_samples_raw);

        //transmit all samples. TX_NOW
        status = bladerf_sync_tx(phy->dev, out_samples_raw, num_samples,
                                &metadata, 5000);
        if (status != 0){
            fprintf(stderr, "[PHY] %s: Couldn't transmit samples with bladeRF: %s\n",
                    __FUNCTION__, bladerf_strerror(status));
            return NULL;
        }
    }

    free(out_samples_raw);
    return NULL;
}

/**
 * Creates a ramp up and ramp down of samples in the following format: Ramp up will use
 * 'ramp_length' samples to ramp up the I samples from 0 to 2048, and set the Q samples to 0.
 * Ramp down will use 'ramp_length' samples to ramp down  the I samples from
 * ramp_down_init.i to 0, and ramp down the Q samples from ramp_down_init.q to 0.
 *
 * Ex: If ramp_length is 4, ramp_down_i_init is -2048, and ramp_down_q_init is 0,
 * ramp_up buffer will be:
 * [.25+0j  .5+0j  .75+0j  1+0j] scaled by 2048
 * and ramp_down will be
 * [-.75+0j  -.5+0j  -.25+0j  0+0j] scaled by 2048
 */
static void create_ramps(unsigned int ramp_length, struct complex_sample ramp_down_init,
                    struct complex_sample *ramp_up, struct complex_sample *ramp_down)
{
    unsigned int samp;
    double ramp_up_step = 2048.0/ramp_length;
    double ramp_down_step_i;
    double ramp_down_step_q;

    //Determine ramp down steps
    if (ramp_down_init.i == 0){
        ramp_down_step_i = 0;
    }else{
        ramp_down_step_i = (double)ramp_down_init.i/(double)ramp_length;
    }
    if (ramp_down_init.q == 0){
        ramp_down_step_q = 0;
    }else{
        ramp_down_step_q = (double)ramp_down_init.q/(double)ramp_length;
    }
    //Create the ramps
    for (samp = 0; samp < ramp_length-1; samp++){
        //ramp up
        ramp_up[samp].i = (int16_t) round(ramp_up_step*(samp+1));    //I
        ramp_up[samp].q = 0;                            //Q

        //ramp down
		ramp_down[samp].i = (int16_t) round(ramp_down_init.i - ramp_down_step_i*(samp + 1));
		ramp_down[samp].q = (int16_t) round(ramp_down_init.q - ramp_down_step_q*(samp + 1));
    }

    //Set last ramp up/ ramp down sample (will always be the same)
    ramp_up[ramp_length-1].i = 2047;    //I
    ramp_up[ramp_length-1].q = 0;        //Q

    ramp_down[ramp_length-1].i = 0;        //I
    ramp_down[ramp_length-1].q = 0;        //Q
}

/**
 * Scrambles the given data with the given scrambling sequence. The size of the
 * scrambling sequence array must be at least as large as the frame.
 */
static void scramble_frame(uint8_t *frame, int frame_length,
                            uint8_t *scrambling_sequence)
{
    int i;
    for (i = 0; i < frame_length; i++){
        //XOR byte with byte from scrambling sequence
        frame[i] ^= scrambling_sequence[i];
    }
}

/****************************************
 *                                      *
 *          RECEIVER FUNCTIONS          *
 *                                      *
 ****************************************/

int phy_start_receiver(struct phy_handle *phy)
{
    int status;

    //turn off stop signal
    phy->rx->stop = false;
    //Kick off frame receiver thread
    status = pthread_create(&(phy->rx->thread), NULL, phy_receive_frames, phy);
    if (status != 0){
        fprintf(stderr, "[PHY] %s: Error creating rx thread: %s\n", __FUNCTION__,
                strerror(status));
        return -1;
    }
    return 0;
}

int phy_stop_receiver(struct phy_handle *phy)
{
    int status;

    DEBUG_MSG("[PHY] RX: Stopping receiver...\n");
    //signal stop
    phy->rx->stop = true;
    //Wait for rx thread to finish
    status = pthread_join(phy->rx->thread, NULL);
    if (status != 0){
        fprintf(stderr, "[PHY] %s: Error joining rx thread: %s\n", __FUNCTION__,
                strerror(status));
        return -1;
    }
    DEBUG_MSG("[PHY] RX: Receiver stopped\n");
    return 0;
}

uint8_t *phy_request_rx_buf(struct phy_handle *phy, unsigned int timeout_ms)
{
    int status;
    struct timespec timeout_abs;
    bool failed = false;

    //Create absolute time format timeout
    status = create_timeout_abs(timeout_ms, &timeout_abs);
    if (status != 0){
        fprintf(stderr, "[PHY] %s: Error creating timeout\n", __FUNCTION__);
        return NULL;
    }

    //Lock mutex
    status = pthread_mutex_lock(&(phy->rx->buf_status_lock));
    if (status != 0){
        fprintf(stderr, "[PHY] %s: Error locking mutex: %s\n", __FUNCTION__,
                    strerror(status));
        return NULL;
    }
    //Wait for condition signal - meaning buffer is full
    while (!phy->rx->buf_filled){
        status = pthread_cond_timedwait(&(phy->rx->buf_filled_cond), &(phy->rx->buf_status_lock),
                                        &timeout_abs);
        if (status != 0){
            if (status == ETIMEDOUT){
                //DEBUG_MSG("[PHY] phy_request_rx_buf(): Condition wait timed out\n");
            }else{
                fprintf(stderr, "[PHY] %s: Condition wait failed: %s\n", __FUNCTION__,
                            strerror(status));
            }
            failed = true;
            break;
        }
    }
    //Unlock mutex
    status = pthread_mutex_unlock(&(phy->rx->buf_status_lock));
    if (status != 0){
        fprintf(stderr, "[PHY] %s: Mutex unlock failed: %s\n", __FUNCTION__,
                strerror(status));
        failed = true;
    }

    if (failed){
        return NULL;
    }
    return phy->rx->data_buf;
}

void phy_release_rx_buf(struct phy_handle *phy)
{
    phy->rx->buf_filled = false;
}

/**
 * Thread function which listens for and receives frames. The steps are:
 * 1) Receive samples with libbladeRF
 * 2) Low pass filter the samples
 * 3) Power normalize the samples
 * 4) Correlate the samples with the preamble waveform
 * 5) If a match is found, demodulate the samples into data bytes
 * 6) Unscramble the data
 * 7) Copy the frame to a buffer which can be acquired with phy_request_rx_buf(), or
 *    drop the frame if the buffer is still in use
 *
 * @param    arg        pointer to phy_handle struct
 */
void *phy_receive_frames(void *arg)
{
    struct phy_handle *phy = (struct phy_handle *) arg;
    int status;
    unsigned int num_bytes_rx;
    unsigned int data_index;        //Current index of rx_buffer to receive new bytes on
                                    //doubles as the current received frame length
    uint64_t samples_index=0;            //Current index of samples buffer to correlate/demod
                                    //samples from
    bool preamble_detected;
    bool new_frame = false;
    int frame_length = 0;            //link layer frame length
    uint8_t *rx_buffer = NULL;    //local rx data buffer
    uint8_t frame_type;
    struct bladerf_metadata metadata;            //bladerf metadata for sync_rx()
    unsigned int num_bytes_to_demod = 0;
    uint64_t timestamp = UINT64_MAX;

    enum states {RECEIVE, PREAMBLE_CORRELATE, DEMOD,
                    CHECK_FRAME_TYPE, DECODE, COPY};
    enum states state;                //current state variable

    /* corr_process() takes a size_t count.
     * Ensure a cast from uint64_t to size_t is valid. */
    assert(NUM_SAMPLES_RX < SIZE_MAX);

    //Allocate memory for buffer
    rx_buffer = malloc(MAX_LINK_FRAME_SIZE);
    if (rx_buffer == NULL){
        perror("[PHY] malloc");
        goto out;
    }

    //Set bladeRF metadata
    memset(&metadata, 0, sizeof(metadata));
    metadata.flags = BLADERF_META_FLAG_RX_NOW;

    preamble_detected = false;
    data_index = 0;
    state = RECEIVE;
    //Loop until stop signal detected
    while(!phy->rx->stop){
        switch(state){
            case RECEIVE:
                //--Receive samples, filter, and power normalize
                //DEBUG_MSG("[PHY] RX: State = RECEIVE\n");
                samples_index = 0;
                status = bladerf_sync_rx(phy->dev, phy->rx->in_samples, NUM_SAMPLES_RX,
                                            &metadata, 5000);
                if (status != 0){
                    fprintf(stderr, "[PHY] %s: Couldn't receive samples from bladeRF\n",
                            __FUNCTION__);
                    goto out;
                }
                //Check metadata
                if (metadata.status & BLADERF_META_STATUS_OVERRUN){
                    NOTE("[PHY] %s: Got an overrun. Expected count = %u;"
                                " actual count = %u. Skipping these samples.\n",
                                __FUNCTION__, NUM_SAMPLES_RX, metadata.actual_count);
                    break;
                }
                if (timestamp != UINT64_MAX && metadata.timestamp != timestamp+NUM_SAMPLES_RX){
                    NOTE("[PHY] %s: Unexpected timestamp. Expected %lu, got %lu.\n",
                            __FUNCTION__, timestamp+NUM_SAMPLES_RX, metadata.timestamp);
                }
                timestamp = metadata.timestamp;

                #ifndef BYPASS_RX_CHANNEL_FILTER
                    // Apply channel filter
                    fir_process(phy->rx->ch_filt, phy->rx->in_samples,
                                phy->rx->filt_samples, NUM_SAMPLES_RX);
                #else
                    conv_samples_to_struct(phy->rx->in_samples, NUM_SAMPLES_RX,
                                            phy->rx->filt_samples);
                #endif
                //Power normalize
                #ifndef BYPASS_RX_PNORM
                    pnorm(phy->rx->pnorm, NUM_SAMPLES_RX, phy->rx->filt_samples,
                            phy->rx->pnorm_samples, NULL, NULL);
                #else
                    memcpy(phy->rx->pnorm_samples, phy->rx->filt_samples,
                            NUM_SAMPLES_RX * sizeof(struct complex_sample));
                #endif
                if (preamble_detected){
                    state = DEMOD;
                }else{
                    state = PREAMBLE_CORRELATE;
                }
                break;
            case PREAMBLE_CORRELATE:
                //--Cross correlate received samples with preamble to find start
                //--of the data frame
                //DEBUG_MSG("[PHY] RX: State = PREAMBLE_CORRELATE\n");
                samples_index = corr_process(phy->rx->corr,
                                            &(phy->rx->pnorm_samples[samples_index]),
                                            (size_t) (NUM_SAMPLES_RX-samples_index), 0);
                if (samples_index != CORRELATOR_NO_RESULT){
                    DEBUG_MSG("[PHY] RX: Preamble matched @ index %lu\n", samples_index);
                    preamble_detected = true;
                    new_frame = true;
                    //First we only demod the first byte to determine frame type
                    num_bytes_to_demod = 1;
                    data_index = 0;
                    state = DEMOD;
                }else{
                    //No preamble match. Receive more samples
                    state = RECEIVE;
                }
                break;
            case DEMOD:
                //--Demod samples
                DEBUG_MSG("[PHY] RX: State = DEMOD\n");
                num_bytes_rx = fsk_demod(phy->fsk, &(phy->rx->pnorm_samples[samples_index]),
                                        NUM_SAMPLES_RX-(int)samples_index, new_frame,
                                        num_bytes_to_demod, &rx_buffer[data_index]);
                if (num_bytes_rx < num_bytes_to_demod){
                    //Receive more samples
                    num_bytes_to_demod -= num_bytes_rx;
                    state = RECEIVE;
                }else{
                    if (new_frame){
                        state = CHECK_FRAME_TYPE;
                        //Account for extra sample which defines initial phase
                        samples_index++;
                    }else{
                        //We demoded all samples in the frame
                        state = DECODE;
                    }
                }
                data_index += num_bytes_rx;
                samples_index += num_bytes_rx*8*SAMP_PER_SYMB;
                new_frame = false;
                break;
            case CHECK_FRAME_TYPE:
                //--Check the frame type byte
                DEBUG_MSG("[PHY] RX: State = CHECK_FRAME_TYPE\n");
                #ifndef BYPASS_PHY_SCRAMBLING
                    frame_type = rx_buffer[0] ^ phy->scrambling_sequence[0];
                #else
                    frame_type = rx_buffer[0];
                #endif
                //Set frame length according to what type of frame it is
                if (frame_type == ACK_FRAME_CODE){
                    DEBUG_MSG("[PHY] RX: Getting an ACK frame...\n");
                    frame_length = ACK_FRAME_LENGTH;
                }else if(frame_type == DATA_FRAME_CODE){
                    DEBUG_MSG("[PHY] RX: Getting a data frame...\n");
                    frame_length = DATA_FRAME_LENGTH;
                }else{
                    NOTE("[PHY] %s: rx'ed unknown frame type 0x%.2X\n",
                            __FUNCTION__, frame_type);
                    data_index = 0;
                    preamble_detected = false;
                    state = RECEIVE;
                    break;
                }
                //Demod the rest of the bytes
                num_bytes_to_demod = frame_length-1;
                state = DEMOD;
                break;
            case DECODE:
                //--Remove any phy encoding on the received frame
                DEBUG_MSG("[PHY] RX: State = DECODE\n");
                #ifndef BYPASS_PHY_SCRAMBLING
                    //Unscramble the frame
                    unscramble_frame(rx_buffer, frame_length, phy->scrambling_sequence);
                #endif
                state = COPY;
                break;
            case COPY:
                //--Copy frame into buffer which can be accessed by the link layer
                DEBUG_MSG("[PHY] RX: State = COPY\n");
                //Is the link layer still working with the previous frame?
                if (phy->rx->buf_filled){
                    //Instead of disrupting the link layer, drop this frame
                    NOTE("[PHY] RX: Frame dropped!\n");
                }else{
                    //Copy frame into rx_data_buf
                    memcpy(phy->rx->data_buf, rx_buffer,
                            frame_length);
                    phy->rx->buf_filled = true;
                    //Signal that the buffer is filled
                    status = pthread_mutex_lock(&(phy->rx->buf_status_lock));
                    if (status != 0){
                        fprintf(stderr, "[PHY] %s: Error locking pthread_mutex\n",
                                __FUNCTION__);
                        goto out;
                    }
                    status = pthread_cond_signal(&(phy->rx->buf_filled_cond));
                    if (status != 0){
                        fprintf(stderr, "[PHY] %s: Error signaling pthread_cond\n",
                                __FUNCTION__);
                        goto out;
                    }
                    status = pthread_mutex_unlock(&(phy->rx->buf_status_lock));
                    if (status != 0){
                        fprintf(stderr, "[PHY] %s: Error unlocking pthread_mutex\n",
                                __FUNCTION__);
                        goto out;
                    }
                    DEBUG_MSG("[PHY] RX: Frame ready\n");
                }
                preamble_detected = false;

                if (samples_index == NUM_SAMPLES_RX){
                    state = RECEIVE;
                }else{
                    state = PREAMBLE_CORRELATE;
                }
                break;
            default:
                fprintf(stderr, "[PHY] %s: Invalid state\n", __FUNCTION__);
                goto out;
        }
    }
    out:
        free(rx_buffer);
        return NULL;
}

/**
 * Unscrambles the given data with the given scrambling sequence. The size of the
 * scrambling sequence array must be at least as large as the frame.
 */
static void unscramble_frame(uint8_t *frame, int frame_length,
                                uint8_t *scrambling_sequence)
{
    int i;
    for (i = 0; i < frame_length; i++){
        //XOR byte with byte from scrambling sequence
        frame[i] ^= scrambling_sequence[i];
    }
}
