/**
 * @brief   Link layer code for modem
 *
 * This file interfaces with phy.c through its transmit_data_frames() and
 * receive_frames() functions.
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

#include "link.h"
#include "phy.h"
#include "crc32.h"
#include "utils.h"

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

/********************************************
 *                                          *
 *  PRIVATE FUNCTIONS AND DATA STRUCTURES   *
 *                                          *
 ********************************************/

struct data_frame {
    //Total frame length = 1009 bytes (8072 bits)
    uint8_t type;               //0x00 = data frame, 0xFF = ack frame
    uint16_t seq_num;           //Sequence number
    uint16_t payload_length;    //Length of used payload data in bytes
    uint8_t payload[PAYLOAD_LENGTH];    //payload data
    uint32_t crc32;             //32-bit CRC
};

struct ack_frame {
    //Total frame length = 7 bytes (56 bits)
    uint8_t type;               //0x00 = data frame, 0xFF = ack frame
    uint16_t ack_num;           //Acknowledgement number
    uint32_t crc32;             //32-bit CRC
};

struct tx {
    struct data_frame data_frame_buf;   //Input data frame buffer
    struct ack_frame ack_frame_buf;     //Input ack frame buffer
    bool stop;                          //Signal to stop tx thread
    bool data_buf_filled;               //Is the data frame buffer filled
    pthread_t thread;                   //Transmitter thread
    pthread_cond_t data_buf_filled_cond;    //pthread condition corresponding to
                                            //data_buf_filled
    pthread_mutex_t data_buf_status_lock;   //Mutex for data_buf_filled_cond
    bool link_on;   //Is the transmitter on
    bool done;      //Has the transmitter finished a transmission
    bool success;   //Was the transmitter transmission successful
};

struct rx {
    struct data_frame data_frame_buf;   //Output data frame buffer
    struct ack_frame ack_frame_buf;     //Output ack frame buffer
    //Leftover bytes received but not returned to the user after a call to
    //link_receive_data()
    uint8_t extra_bytes[PAYLOAD_LENGTH];
    unsigned int num_extra_bytes;       //Number of bytes in 'extra_bytes' buffer
    pthread_t thread;                   //Receiver thread
    bool stop;                          //Signal to stop rx thread
    bool data_buf_filled;               //Is the data frame buffer filled
    pthread_cond_t data_buf_filled_cond;    //pthread condition for data_buf_filled
    pthread_mutex_t data_buf_status_lock;   //mutex for data_buf_filled_cond
    bool ack_buf_filled;                    //Is the ack frame buffer filled
    pthread_cond_t ack_buf_filled_cond;     //pthread condition for ack_buf_filled
    pthread_mutex_t ack_buf_status_lock;    //mutex for ack_buf_filled_cond
    bool link_on;                           //Is the receiver on
};

struct link_handle {
    struct phy_handle *phy;
    struct tx *tx;
    struct rx *rx;
    bool phy_tx_on;     //Is the phy transmitter on
    bool phy_rx_on;     //Is the phy receiver on
};

//internal functions
//tx:
static int start_transmitter(struct link_handle *link);
static int stop_transmitter(struct link_handle *link);
void *transmit_data_frames(void *arg);
static int send_payload(struct link_handle *link, uint8_t *payload,
                        uint16_t used_payload_length);
//rx:
static int start_receiver(struct link_handle *link);
static int stop_receiver(struct link_handle *link);
void *receive_frames(void *arg);
static int receive_ack(struct link_handle *link, uint16_t ack_num,
                        unsigned int timeout_ms);
static int receive_payload(struct link_handle *link, uint8_t *payload,
                            unsigned int timeout_ms);
//utility:
static void convert_data_frame_struct_to_buf(struct data_frame *frame, uint8_t *buf);
static void convert_ack_frame_struct_to_buf(struct ack_frame *frame, uint8_t *buf);
static void convert_buf_to_data_frame_struct(uint8_t *buf, struct data_frame *frame);
static void convert_buf_to_ack_frame_struct(uint8_t *buf, struct ack_frame *frame);

/****************************************
 *                                      *
 *  INIT/DEINIT AND UTILITY FUNCTIONS   *
 *                                      *
 ****************************************/

struct link_handle *link_init(struct bladerf *dev, struct radio_params *params)
{
    int status;
    struct link_handle *link;

    DEBUG_MSG("[LINK] Initializing\n");
    //-------------Allocate memory for link handle struct--------------
    //Calloc so all pointers are initialized to NULL, and all bools are
    //initialized to false
    link = calloc(1, sizeof(struct link_handle));
    if (link == NULL){
        perror("malloc");
        return NULL;
    }

    //---------------Open/Initialize phy handle--------------------------
    link->phy = phy_init(dev, params);
    if (link->phy == NULL){
        fprintf(stderr, "[LINK] Couldn't initialize phy handle\n");
        goto error;
    }
    //Start phy receiver
    status = phy_start_receiver(link->phy);
    if (status != 0){
        fprintf(stderr, "[LINK] Couldn't start phy recevier\n");
        goto error;
    }
    link->phy_rx_on = true;
    //Start phy transmitter
    status = phy_start_transmitter(link->phy);
    if (status != 0){
        fprintf(stderr, "[LINK] Couldn't start phy transmitter\n");
        goto error;
    }
    link->phy_tx_on = true;

    //------------------Allocate memory for tx struct and initialize-----
    link->tx = malloc(sizeof(struct tx));
    if (link->tx == NULL){
        perror("malloc");
        goto error;
    }
    //Initialize control/state variables
    link->tx->stop = false;
    link->tx->data_buf_filled = false;
    link->tx->done = false;
    link->tx->success = false;
    link->tx->link_on = false;
    //Initialize pthread condition variable
    status = pthread_cond_init(&(link->tx->data_buf_filled_cond), NULL);
    if (status != 0){
        fprintf(stderr, "[LINK] Error initializing pthread_cond: %s\n",
                    strerror(status));
        goto error;
    }
    //Initialize pthread mutex variable
    status = pthread_mutex_init(&(link->tx->data_buf_status_lock), NULL);
    if (status != 0){
        fprintf(stderr, "[LINK] Error initializing pthread_mutex: %s\n",
                    strerror(status));
        goto error;
    }
    //------------------Allocate memory for rx struct and initialize-----
    link->rx = malloc(sizeof(struct rx));
    if (link->rx == NULL){
        perror("malloc");
        goto error;
    }
    //Initialize pthread condition variable for data
    status = pthread_cond_init(&(link->rx->data_buf_filled_cond), NULL);
    if (status != 0){
        fprintf(stderr, "[LINK] Error initializing pthread_cond: %s\n",
                    strerror(status));
        goto error;
    }
    //Initialize pthread mutex variable for data
    status = pthread_mutex_init(&(link->rx->data_buf_status_lock), NULL);
    if (status != 0){
        fprintf(stderr, "[LINK] Error initializing pthread_mutex: %s\n",
                    strerror(status));
        goto error;
    }
    //Initialize pthread condition variable for ack
    status = pthread_cond_init(&(link->rx->ack_buf_filled_cond), NULL);
    if (status != 0){
        fprintf(stderr, "[LINK] Error initializing pthread_cond: %s\n",
                    strerror(status));
        goto error;
    }
    //Initialize pthread mutex variable for ack
    status = pthread_mutex_init(&(link->rx->ack_buf_status_lock), NULL);
    if (status != 0){
        fprintf(stderr, "[LINK] Error initializing pthread_mutex: %s\n",
                    strerror(status));
        goto error;
    }
    //Initialize control/state variables
    link->rx->stop = false;
    link->rx->data_buf_filled = false;
    link->rx->ack_buf_filled = false;
    link->rx->num_extra_bytes = 0;
    link->rx->link_on = false;

    //---------------------Start the link receiver--------------------------
    status = start_receiver(link);
    if (status != 0){
        fprintf(stderr, "[LINK] Couldn't start receiver\n");
        goto error;
    }
    link->rx->link_on = true;
    //-------------------Start the link transmitter-------------------------
    status = start_transmitter(link);
    if (status != 0){
        fprintf(stderr, "[LINK] Couldn't start receiver\n");
        goto error;
    }
    link->tx->link_on = true;

    DEBUG_MSG("[LINK] Initialization done\n");
    return link;

    error:
        link_close(link);
        return NULL;
}

void link_close(struct link_handle *link)
{
    int status;

    DEBUG_MSG("[LINK] Closing\n");

    //Cleanup all internal resources
    if (link != NULL){
        //Cleanup tx struct
        if (link->tx != NULL){
            //Stop the link transmitter if it is on
            if (link->tx->link_on){
                status = stop_transmitter(link);
                if (status != 0){
                    fprintf(stderr, "[LINK] Error stopping link transmitter\n");
                }
            }
            status = pthread_mutex_destroy(&(link->tx->data_buf_status_lock));
            if (status != 0){
                fprintf(stderr, "[LINK] Error destroying pthread_mutex\n");
            }
            status = pthread_cond_destroy(&(link->tx->data_buf_filled_cond));
            if (status != 0){
                fprintf(stderr, "[LINK] Error destroying pthread_cond\n");
            }
        }
        free(link->tx);
        //Cleanup rx struct
        if (link->rx != NULL){
            //Stop the link receiver if it is on
            if (link->rx->link_on){
                status = stop_receiver(link);
                if (status != 0){
                    fprintf(stderr, "[LINK] Error stopping link receiver\n");
                }
            }
            status = pthread_mutex_destroy(&(link->rx->data_buf_status_lock));
            if (status != 0){
                fprintf(stderr, "[LINK] Error destroying pthread_mutex\n");
            }
            status = pthread_cond_destroy(&(link->rx->data_buf_filled_cond));
            if (status != 0){
                fprintf(stderr, "[LINK] Error destroying pthread_cond\n");
            }
            status = pthread_mutex_destroy(&(link->rx->ack_buf_status_lock));
            if (status != 0){
                fprintf(stderr, "[LINK] Error destroying pthread_mutex\n");
            }
            status = pthread_cond_destroy(&(link->rx->ack_buf_filled_cond));
            if (status != 0){
                fprintf(stderr, "[LINK] Error destroying pthread_cond\n");
            }
        }
        free(link->rx);
        //Close the phy
        if (link->phy != NULL){
            //Stop phy transmitter/receiver if they are on
            if (link->phy_tx_on){
                status = phy_stop_transmitter(link->phy);
                if (status != 0){
                    fprintf(stderr, "[LINK] Error stopping phy transmitter\n");
                }
            }
            if (link->phy_rx_on){
                status = phy_stop_receiver(link->phy);
                if (status != 0){
                    fprintf(stderr, "[LINK] Error stopping phy receiver\n");
                }
            }
        }
        //Close phy handle
        phy_close(link->phy);
    }
    free(link);
    link = NULL;
}

/**
 * Convert data frame struct to a buffer of uint8_t
 * @param[in]   frame   pointer to data_frame structure to convert
 * @param[out]  buf     pointer to buffer to place the frame in
 */
static void convert_data_frame_struct_to_buf(struct data_frame *frame, uint8_t *buf)
{
    int i = 0;

    //Frame type
    memcpy(&buf[i], &(frame->type), sizeof(frame->type));
    i += sizeof(frame->type);
    //Seq num
    memcpy(&buf[i], &(frame->seq_num), sizeof(frame->seq_num));
    i += sizeof(frame->seq_num);
    //payload length
    memcpy(&buf[i], &(frame->payload_length), sizeof(frame->payload_length));
    i += sizeof(frame->payload_length);
    //payload
    memcpy(&buf[i], frame->payload, PAYLOAD_LENGTH);
    i += PAYLOAD_LENGTH;
    //crc
    memcpy(&buf[i], &(frame->crc32), sizeof(frame->crc32));
    i += sizeof(frame->crc32);

    if (i != DATA_FRAME_LENGTH){
        fprintf(stderr, "[LINK] %s: ERROR: Link layer is using a different data frame "
                        "length (%d) than the phy layer (%d). Update the DATA_FRAME_LENGTH"
                        " macro in phy.h and recompile or there will be unexpected results\n",
                        __FUNCTION__, i, DATA_FRAME_LENGTH);
    }
}

/**
 * Convert ack frame struct to a buffer of uint8_t
 * @param[in]   frame   pointer to ack_frame structure to convert
 * @param[out]  buf     pointer to buffer to place the frame in
 */
static void convert_ack_frame_struct_to_buf(struct ack_frame *frame, uint8_t *buf)
{
    int i = 0;

    //Frame type
    memcpy(&buf[i], &(frame->type), sizeof(frame->type));
    i += sizeof(frame->type);
    //ack num
    memcpy(&buf[i], &(frame->ack_num), sizeof(frame->ack_num));
    i += sizeof(frame->ack_num);
    //crc
    memcpy(&buf[i], &(frame->crc32), sizeof(frame->crc32));
    i += sizeof(frame->crc32);

    if (i != ACK_FRAME_LENGTH){
        fprintf(stderr, "[LINK] %s: ERROR: Link layer is using a different ack frame "
                        "length (%d) than the phy layer (%d). Update the ACK_FRAME_LENGTH"
                        " macro in phy.h and recompile or there will be unexpected results\n",
                        __FUNCTION__, i, ACK_FRAME_LENGTH);
    }
}

/**
 * Convert uint8_t buffer to data frame struct
 *
 * @param[in]   buf     pointer to buffer holding a data frame
 * @param[out]  frame   pointer to data_frame struct to place frame in
 */
static void convert_buf_to_data_frame_struct(uint8_t *buf, struct data_frame *frame)
{
    int i = 0;

    //Frame type
    memcpy(&(frame->type), &buf[i], sizeof(frame->type));
    i += sizeof(frame->type);
    //Seq num
    memcpy(&(frame->seq_num), &buf[i], sizeof(frame->seq_num));
    i += sizeof(frame->seq_num);
    //payload length
    memcpy(&(frame->payload_length), &buf[i], sizeof(frame->payload_length));
    i += sizeof(frame->payload_length);
    //payload
    memcpy(frame->payload, &buf[i], PAYLOAD_LENGTH);
    i += PAYLOAD_LENGTH;
    //crc
    memcpy(&(frame->crc32), &buf[i], sizeof(frame->crc32));
    i += sizeof(frame->crc32);

    if (i != DATA_FRAME_LENGTH){
        fprintf(stderr, "[LINK] %s: ERROR: Link layer is using a different data frame "
                        "length (%d) than the phy layer (%d). Update the DATA_FRAME_LENGTH"
                        " macro in phy.h and recompile or there will be unexpected results\n",
                        __FUNCTION__, i, DATA_FRAME_LENGTH);
    }
}

/**
 * Convert uint8_t buffer to ack frame struct
 *
 * @param[in]   buf     pointer to buffer holding a ack frame
 * @param[out]  frame   pointer to ack_frame struct to place frame in
 */
static void convert_buf_to_ack_frame_struct(uint8_t *buf, struct ack_frame *frame)
{
    int i = 0;

    //Frame type
    memcpy(&(frame->type), &buf[i], sizeof(frame->type));
    i += sizeof(frame->type);
    //ack num
    memcpy(&(frame->ack_num), &buf[i], sizeof(frame->ack_num));
    i += sizeof(frame->ack_num);
    //crc
    memcpy(&(frame->crc32), &buf[i], sizeof(frame->crc32));
    i += sizeof(frame->crc32);

    if (i != ACK_FRAME_LENGTH){
        fprintf(stderr, "[LINK] %s: ERROR: Link layer is using a different ack frame "
                        "length (%d) than the phy layer (%d). Update the ACK_FRAME_LENGTH"
                        " macro in phy.h and recompile or there will be unexpected results\n",
                        __FUNCTION__, i, ACK_FRAME_LENGTH);
    }
}

/****************************************
 *                                      *
 *          TRANSMITTER FUNCTIONS       *
 *                                      *
 ****************************************/

/**
 * Start the transmitter thread
 *
 * @param[in]   link    pointer to link handle
 *
 * @return  0 on success, -1 on failure
 */
static int start_transmitter(struct link_handle *link)
{
    int status;

    link->tx->done = false;
    link->tx->success = false;
    //be sure stop signal is off
    link->tx->stop = false;
    //Kick off transmitter thread
    status = pthread_create(&(link->tx->thread), NULL, transmit_data_frames, link);
    if (status != 0){
        fprintf(stderr, "[LINK] Error creating tx thread: %s\n",
                    strerror(status));
        return -1;
    }
    return 0;
}

/**
 * Stop the transmitter thread
 *
 * @param[in]   link    pointer to link handle
 *
 * @return  0 on success, -1 on failure
 */
static int stop_transmitter(struct link_handle *link)
{
    int status;

    DEBUG_MSG("[LINK] TX: Stopping transmitter...\n");
    //signal stop
    link->tx->stop = true;
    //Signal the buffer filled condition so the thread will stop waiting
    //for a filled buffer
    status = pthread_mutex_lock(&(link->tx->data_buf_status_lock));
    if (status != 0){
        fprintf(stderr, "[LINK] Error locking pthread_mutex\n");
    }
    status = pthread_cond_signal(&(link->tx->data_buf_filled_cond));
    if (status != 0){
        fprintf(stderr, "[LINK] Error signaling pthread_cond\n");
    }
    status = pthread_mutex_unlock(&(link->tx->data_buf_status_lock));
    if (status != 0){
        fprintf(stderr, "[LINK] Error unlocking pthread_mutex\n");
    }
    //Wait for tx thread to finish
    status = pthread_join(link->tx->thread, NULL);
    if (status != 0){
        fprintf(stderr, "[LINK] Error joining tx thread: %s\n",
                    strerror(status));
        return -1;
    }
    DEBUG_MSG("[LINK] TX: Transmitter stopped\n");
    return 0;
}

int link_send_data(struct link_handle *link, uint8_t *data, unsigned int data_length)
{
    unsigned int num_full_payloads;
    unsigned int i;
    unsigned int last_payload_length;
    int status;

    num_full_payloads = data_length/PAYLOAD_LENGTH;

    //Loop through each full payload
    for(i = 0; i < num_full_payloads; i++){
        //Send the frame
        status = send_payload(link, &data[i*PAYLOAD_LENGTH], PAYLOAD_LENGTH);
        if (status != 0){
            if (status == -2){
                DEBUG_MSG("[LINK] TX: Send data failed: "
                            "No response for payload #%d\n", i+1);
            }else{
                fprintf(stderr, "[LINK] TX: Send data failed: "
                            "Unexpected error sending payload #%d\n", i+1);
            }
            return status;
        }
    }

    //Send the last payload for the remaining bytes
    last_payload_length = data_length % PAYLOAD_LENGTH;
    if (last_payload_length != 0){
        status = send_payload(link, &data[i*PAYLOAD_LENGTH],
                                (uint16_t) last_payload_length);
        if (status != 0){
            if (status == -2){
                DEBUG_MSG("[LINK] TX: Send data failed: "
                            "No response for payload #%d\n", i+1);
            }else{
                fprintf(stderr, "[LINK] TX: Send data failed: "
                            "Unexpected error sending payload #%d\n", i+1);
            }
            return status;
        }
    }
    return 0;
}

/**
 * Sends a payload. Blocks until either the transmission was successful (with an
 * acknowledgement) or there was no response (exceeded max number of retransmissions)
 *
 * @param[in]   link                    pointer to link handle
 * @param[in]   payload                 buffer of bytes to send
 * @param[in]   used_payload_length     number of bytes to send in 'payload'. If less
 *                                      than PAYLOAD_LENGTH, zeros will be padded.
 * @return      0 on success, -1 on error, -2 on timeout/no response
 */
static int send_payload(struct link_handle *link, uint8_t *payload,
                    uint16_t used_payload_length)
{
    int status;

    if (used_payload_length > PAYLOAD_LENGTH){
        fprintf(stderr, "[LINK] %s: Invalid payload length of %hu\n", __FUNCTION__,
                used_payload_length);
        return -1;
    }

    //Wait for tx buf to be empty
    while(link->tx->data_buf_filled){
        usleep(50);
    }
    //Copy payload data into frame buffer
    memcpy(link->tx->data_frame_buf.payload, payload, used_payload_length);
    //Pad zeros to unused portion of the payload
    memset(&(link->tx->data_frame_buf.payload[used_payload_length]), 0,
            PAYLOAD_LENGTH - used_payload_length);
    //Set payload length
    link->tx->data_frame_buf.payload_length = used_payload_length;
    //Mark tx not done
    link->tx->done = false;
    //Mark buffer filled
    link->tx->data_buf_filled = true;
    //Signal the buffer filled condition
    status = pthread_mutex_lock(&(link->tx->data_buf_status_lock));
    if (status != 0){
        fprintf(stderr, "[LINK] Error locking pthread_mutex: %s\n",
                    strerror(status));
        return -1;
    }
    status = pthread_cond_signal(&(link->tx->data_buf_filled_cond));
    if (status != 0){
        fprintf(stderr, "[LINK] Error signaling pthread_cond: %s\n",
                    strerror(status));
        return -1;
    }
    status = pthread_mutex_unlock(&(link->tx->data_buf_status_lock));
    if (status != 0){
        fprintf(stderr, "[LINK] Error unlocking pthread_mutex: %s\n",
                    strerror(status));
        return -1;
    }
    //Wait for the transmission to complete
    //TODO: Change this to a pthread condition signal
    while (!link->tx->done){
        usleep(100);
    }
    //Check the status of the transmission
    if (!link->tx->success){
        return -2;
    }

    return 0;
}

/**
 * Thread function that transmits data frames and waits for acks.
 * Does not directly receive acks - the receive_frames() function does this.
 * Does not transmit acks - the receive_frames function does this.
 *
 * @param[in]   arg     pointer to link handle
 */
void *transmit_data_frames(void *arg)
{
    int status;
    uint16_t seq_num;
    uint32_t crc_32;
    unsigned int tries;
    uint8_t data_send_buf[DATA_FRAME_LENGTH];
    bool failed;

    //cast arg
    struct link_handle *link = (struct link_handle *) arg;
    //Set initial sequence number to random value
    srand((unsigned int)time(NULL));
    seq_num = rand() % 65536;
    DEBUG_MSG("[LINK] TX: Initial seq num = %hu\n", seq_num);
    tries = 1;

    while (!link->tx->stop){
        if (tries > LINK_MAX_TRIES){
            DEBUG_MSG("[LINK] TX: Exceeded max tries (%u) without an ACK."
                            " Skipping frame\n", tries-1);
            link->tx->success = false;
            link->tx->done = true;
            tries = 1;
        }
        if (tries == 1){
            //---------Wait for data buffer to be filled-----------
            //Lock mutex
            status = pthread_mutex_lock(&(link->tx->data_buf_status_lock));
            if (status != 0){
                fprintf(stderr, "[LINK] Mutex lock failed: %s\n",
                        strerror(status));
                goto out;
            }
            //Wait for condition signal - meaning buffer is full
            failed = false;
            while (!link->tx->data_buf_filled && !link->tx->stop){
                DEBUG_MSG("[LINK] TX: Waiting for buffer to be filled\n");
                status = pthread_cond_wait(&(link->tx->data_buf_filled_cond),
                                            &(link->tx->data_buf_status_lock));
                if (status != 0){
                    fprintf(stderr, "[LINK] transmit_frames(): "
                            "Condition wait failed: %s\n", strerror(status));
                    failed = true;
                    break;
                }
            }
            //Unlock mutex
            status = pthread_mutex_unlock(&(link->tx->data_buf_status_lock));
            if (status != 0){
                fprintf(stderr, "[LINK] transmit_frames(): Mutex unlock failed: %s\n",
                        strerror(status));
                failed = true;
            }
            //Stop thread if stop variable is true, or something with pthreads went wrong
            if (link->tx->stop || failed){
                link->tx->data_buf_filled = false;
                goto out;
            }
            DEBUG_MSG("[LINK] TX: Frame buffer filled. Sending...\n");
            //Set frame type
            link->tx->data_frame_buf.type = DATA_FRAME_CODE;
            //Set sequence number
            link->tx->data_frame_buf.seq_num = seq_num;
            //Copy frame into send buf
            convert_data_frame_struct_to_buf(&(link->tx->data_frame_buf), data_send_buf);
            //Calculate the CRC
            crc_32 = crc32(data_send_buf, DATA_FRAME_LENGTH - sizeof(crc_32));
            //Copy this CRC to the send buf
            memcpy(&data_send_buf[DATA_FRAME_LENGTH - sizeof(crc_32)],
                    &crc_32, sizeof(crc_32));
            //Mark tx data buffer empty
            link->tx->data_buf_filled = false;
        }
        //Transmit the frame
        status = phy_fill_tx_buf(link->phy, data_send_buf, DATA_FRAME_LENGTH);
        if (status != 0){
            fprintf(stderr, "[LINK] Couldn't fill phy tx buffer\n");
            goto out;
        }
        DEBUG_MSG("[LINK] TX: Frame sent to PHY. Waiting for ACK...\n");
        //Wait for an ack
        status = receive_ack(link, seq_num, ACK_TIMEOUT_MS);
        if (status == -2){
            DEBUG_MSG("[LINK] TX: Didn't get an ACK (timed out). Resending\n");
            tries++;
            continue;
        }else if (status == -3){
            DEBUG_MSG("[LINK] TX: Received wrong ACK number. Resending\n");
            tries++;
            continue;
        }else if (status < 0){
            fprintf(stderr, "[LINK] TX: Error receiving ACK. "
                            "Stopping transmitter\n");
            goto out;
        }
        //Success!
        DEBUG_MSG("[LINK] TX: Got an ACK\n");
        link->tx->success = true;
        link->tx->done = true;
        tries = 1;
        seq_num++;
    }

    out:
        link->tx->done = true;
        return NULL;
}

/****************************************
 *                                      *
 *          RECEIVER FUNCTIONS          *
 *                                      *
 ****************************************/

/**
 * Start the receiver thread
 *
 * @param[in]   link    pointer to link handle
 *
 * @return  0 on success, -1 on failure
 */
static int start_receiver(struct link_handle *link)
{
    int status;

    //be sure stop signal is off
    link->rx->stop = false;
    //Kick off receiver thread
    status = pthread_create(&(link->rx->thread), NULL, receive_frames, link);
    if (status != 0){
        fprintf(stderr, "[LINK] Error creating rx thread: %s\n",
                    strerror(status));
        return -1;
    }
    return 0;
}

/**
 * Stop the receiver thread
 *
 * @param[in]   link    pointer to link handle
 *
 * @return  0 on success, -1 on failure
 */
static int stop_receiver(struct link_handle *link)
{
    int status;

    DEBUG_MSG("[LINK] RX: Stopping receiver...\n");
    //signal stop
    link->rx->stop = true;
    //Wait for rx thread to finish
    status = pthread_join(link->rx->thread, NULL);
    if (status != 0){
        fprintf(stderr, "[LINK] Error joining rx thread: %s\n",
                    strerror(status));
        return -1;
    }
    DEBUG_MSG("[LINK] RX: Receiver stopped\n");
    return 0;
}

int link_receive_data(struct link_handle *link, int size, int max_timeouts,
                        uint8_t *data_buf)
{
    int bytes_received;
    int bytes_remaining;
    uint8_t temp_buf[PAYLOAD_LENGTH];
    int i;
    int timeouts;

    //Check for invalid input
    if (size < 0 || max_timeouts < 0){
        fprintf(stderr, "[LINK] RX: link_receive_data(): parameter is negative\n");
        return -1;
    }

    timeouts = 0;
    //Read any extra bytes left over from the last call to this function
    if (size < (int)(link->rx->num_extra_bytes)){
        //Special case: More extra bytes than the request size.
        memcpy(data_buf, link->rx->extra_bytes, size);
        //Move over the remaining extra_bytes
        memmove(link->rx->extra_bytes, &(link->rx->extra_bytes[size]),
                link->rx->num_extra_bytes - size);
        i = size;
        return i;
    }else{
        memcpy(data_buf, link->rx->extra_bytes, link->rx->num_extra_bytes);
        i = link->rx->num_extra_bytes;
    }

    while(i < size && timeouts < max_timeouts){
        bytes_remaining = size - i;
        //Make sure to not write more than 'size' bytes. For the last payload(s),
        //Use a temp buffer first
        if (bytes_remaining < PAYLOAD_LENGTH){
            bytes_received = receive_payload(link, temp_buf, 500);
            //Check for timeout
            if (bytes_received == -2){
                timeouts++;
                continue;
            }else if (bytes_received < 0){
                fprintf(stderr, "[LINK] RX: Error receiving payload\n");
                return -1;
            }
            //Copy bytes from temp buf to data_buf
            if (bytes_received < bytes_remaining){
                memcpy(&data_buf[i], temp_buf, bytes_received);
                i += bytes_received;
            }else{    //bytes_received >= bytes_remaining
                memcpy(&data_buf[i], temp_buf, bytes_remaining);
                i += bytes_remaining;
                //Copy extra bytes to extra_bytes buffer
                link->rx->num_extra_bytes = bytes_received - bytes_remaining;
                memcpy(link->rx->extra_bytes, &temp_buf[bytes_remaining],
                        link->rx->num_extra_bytes);
            }
        //For all other payloads
        }else{
            bytes_received = receive_payload(link, &data_buf[i], 500);
            //Check for timeout
            if (bytes_received == -2){
                timeouts++;
                continue;
            }
            if (bytes_received < 0){
                fprintf(stderr, "[LINK] RX: Error receiving payload\n");
                return -1;
            }
            i += bytes_received;
        }
    }

    return i;
}

/**
 * Receives a payload and copies it into the given buffer
 * @param[in]   link            pointer to link handle
 * @param[in]   timeout_ms      Amount of time to wait for a received payload
 * @param[out]  payload         pointer to buffer to place payload in
 *
 * @return      number of bytes received (0 - PAYLOAD_LENGTH), -1 on error,
 *              -2 on timeout
 */
static int receive_payload(struct link_handle *link, uint8_t *payload, unsigned int timeout_ms)
{
    int payload_length = 10;    //must be initialized above 0
    struct timespec timeout_abs;
    int status;

    //Create absolute time format timeout
    status = create_timeout_abs(timeout_ms, &timeout_abs);
    if (status != 0){
        fprintf(stderr, "[LINK] RX: receive_payload(): Error creating timeout\n");
        return -1;
    }

    //Prepare to wait with pthread_cond_timedwait()
    status = pthread_mutex_lock(&(link->rx->data_buf_status_lock));
    if (status != 0){
        fprintf(stderr, "[LINK] RX: receive_payload(): Error locking mutex: %s\n",
                    strerror(status));
        return -1;
    }
    //Wait for condition signal - meaning rx data buffer is full
    while (!link->rx->data_buf_filled){
        status = pthread_cond_timedwait(&(link->rx->data_buf_filled_cond),
                                    &(link->rx->data_buf_status_lock), &timeout_abs);
        if (status != 0){
            if (status == ETIMEDOUT){
                payload_length = -2;
            }else{
                fprintf(stderr, "[LINK] RX: receive_payload(): "
                        "Condition wait failed: %s\n", strerror(status));
                payload_length = -1;
            }
            break;
        }
    }
    //Waiting is done. Unlock mutex.
    status = pthread_mutex_unlock(&(link->rx->data_buf_status_lock));
    if (status != 0){
        fprintf(stderr, "[LINK] RX: receive_payload(): Mutex unlock failed: %s\n",
                strerror(status));
        payload_length = -1;
    }
    //Did we error or timeout? Return
    if (payload_length < 0){
        return payload_length;
    }

    //Get the length of the used portion of the payload
    payload_length = link->rx->data_frame_buf.payload_length;
    //Copy the used portion of the payload
    memcpy(payload, link->rx->data_frame_buf.payload, payload_length);
    //Mark the rx data buffer empty
    link->rx->data_buf_filled = false;

    return payload_length;
}

/**
 * Attempts to receive an ACK with the given ACK number
 * Waits until it either receives an ack or times out.
 *
 * @param[in]   link        pointer to link handle
 * @param[in]   ack_num     acknowledgement number to look for
 * @param[in]   timeout_ms  amount of time to wait for an ACK, in milliseconds
 *
 * @return      0 if successfully received ack, -1 if pthread error or wrong ack number,
 *              -2 if timed out, -3 if wrong ack number
 */
static int receive_ack(struct link_handle *link, uint16_t ack_num, unsigned int timeout_ms)
{
    struct timespec timeout_abs;
    int status, ret = 0;

    //Create absolute time format timeout
    status = create_timeout_abs(timeout_ms, &timeout_abs);
    if (status != 0){
        fprintf(stderr, "[LINK] RX: receive_ack(): Error creating timeout\n");
        return -1;
    }

    //Prepare to wait with pthread_cond_timedwait()
    status = pthread_mutex_lock(&(link->rx->ack_buf_status_lock));
    if (status != 0){
        fprintf(stderr, "[LINK] RX: receive_ack(): Error locking mutex: %s\n",
                    strerror(status));
        return -1;
    }
    //Wait for condition signal - meaning buffer is full
    while (!link->rx->ack_buf_filled){
        status = pthread_cond_timedwait(&(link->rx->ack_buf_filled_cond),
                                    &(link->rx->ack_buf_status_lock), &timeout_abs);
        if (status != 0){
            if (status == ETIMEDOUT){
                ret = -2;
            }else{
                fprintf(stderr, "[LINK] RX: receive_ack(): Condition wait failed: %s\n",
                    strerror(status));
                ret = -1;
            }
            break;
        }
    }
    //Waiting is done. Unlock mutex.
    status = pthread_mutex_unlock(&(link->rx->ack_buf_status_lock));
    if (status != 0){
        fprintf(stderr, "[LINK] RX: receive_ack(): Mutex unlock failed: %s\n",
                strerror(status));
        ret = -1;
    }
    //Check the sequence number if nothing failed
    if (ret == 0 && link->rx->ack_frame_buf.ack_num != ack_num){
        fprintf(stderr, "[LINK] RX: receive_ack(): Incorrect ack number %hu "
                    "(expected %hu)\n", link->rx->ack_frame_buf.ack_num, ack_num);
        ret = -3;
    }
    //mark the rx ack buffer empty
    link->rx->ack_buf_filled = false;

    return ret;
}

/**
 * Thread function which receives data and ACK frames from the PHY, and transmits ACKs.
 * Checks CRC on all received frames before copying them to a buffer which other
 * functions can use. If the CRC is incorrect, it disregards the frame and does not
 * copy it. If a data frame is received, it checks sequence number for duplicate.
 * If not a duplicate, it copies the frame to rx data_frame_buf and marks the buffer
 * full. An acknowledgement is always sent, regardless of whether or not the received
 * data frame is a duplicate. This is the only function that sends acks.
 *
 * @param[in]   arg     pointer to link handle
 */
void *receive_frames(void *arg)
{
    uint8_t *rx_buf = NULL;
    unsigned int frame_length;
    uint32_t crc_32, crc_32_rx;
    bool is_data_frame = false;
    int status;
    uint8_t ack_send_buf[ACK_FRAME_LENGTH];
    uint16_t seq_num = 0;
    bool first_frame = true;
    bool duplicate;

    //cast arg
    struct link_handle *link = (struct link_handle *) arg;
    //declare states
    enum states {WAIT, CHECK_CRC, COPY, SEND_ACK};
    //current state variable
    enum states state = WAIT;

    while(!link->rx->stop){
        switch(state){
            case WAIT:
                //--Wait for PHY to receive a frame
                //DEBUG_MSG("[LINK] RX: State = WAIT\n");
                rx_buf = phy_request_rx_buf(link->phy, 500);
                if (rx_buf == NULL){
                    //timed out (or error). Continue waiting.
                    state = WAIT;
                }else{
                    state = CHECK_CRC;
                }
                break;
            case CHECK_CRC:
                //--We received a frame from the PHY. Check the CRC
                DEBUG_MSG("[LINK] RX: State = CHECK_CRC\n");
                //First check if it's an ack or data frame to determine length
                if (rx_buf[0] == DATA_FRAME_CODE){
                    DEBUG_MSG("[LINK] RX: Received a data frame\n");
                    is_data_frame = true;
                    frame_length = DATA_FRAME_LENGTH;
                }else{
                    DEBUG_MSG("[LINK] RX: Received a ack frame\n");
                    is_data_frame = false;
                    frame_length = ACK_FRAME_LENGTH;
                }
                //Copy the received CRC
                memcpy(&crc_32_rx, &rx_buf[frame_length - sizeof(crc_32)],
                        sizeof(crc_32));
                //Compute expected CRC
                crc_32 = crc32(rx_buf, frame_length - sizeof(crc_32));
                //Compare received CRC vs expected CRC
                if (crc_32_rx != crc_32){
                    //Drop the frame since there was an error
                    //First release the buffer for the PHY
                    phy_release_rx_buf(link->phy);
                    NOTE("[LINK] RX: Frame received with errors. Dropping.\n");
                    state = WAIT;
                }else{
                    DEBUG_MSG("[LINK] RX: Frame received with no errors\n");
                    state = COPY;
                }
                break;
            case COPY:
                //--CRC passed. Now copy the frame to link layer buffer
                //--if it is not a duplicate frame
                DEBUG_MSG("[LINK] RX: State = COPY\n");
                if (is_data_frame){
                    //Is the previous data frame still being worked with?
                    if (link->rx->data_buf_filled){
                        //Instead of causing a disruption, drop the current frame
                        fprintf(stderr, "[LINK] RX: Data frame dropped!\n");
                        phy_release_rx_buf(link->phy);
                        break;
                    }
                    //Copy/convert to data frame struct
                    convert_buf_to_data_frame_struct(rx_buf,
                                                &(link->rx->data_frame_buf));
                    //Release buffer from the phy
                    phy_release_rx_buf(link->phy);
                    //Is this frame a duplicate of the last frame?
                    if (link->rx->data_frame_buf.seq_num == seq_num && !first_frame){
                        DEBUG_MSG("[LINK] RX: Received a duplicate frame.\n");
                        duplicate = true;
                    }else{
                        duplicate = false;
                    }
                    seq_num = link->rx->data_frame_buf.seq_num;
                    first_frame = false;
                    //Mark buffer filled if not a duplicate data frame
                    if (!duplicate){
                        //Signal that the link data buffer is filled
                        link->rx->data_buf_filled = true;
                        status = pthread_mutex_lock(&(link->rx->data_buf_status_lock));
                        if (status != 0){
                            fprintf(stderr, "[LINK] RX: receive_frames(): "
                                            "Error locking pthread_mutex\n");
                            return NULL;
                        }
                        status = pthread_cond_signal(&(link->rx->data_buf_filled_cond));
                        if (status != 0){
                            fprintf(stderr, "[LINK] RX: receive_frames(): "
                                            "Error signaling pthread_cond\n");
                            return NULL;
                        }
                        status = pthread_mutex_unlock(&(link->rx->data_buf_status_lock));
                        if (status != 0){
                            fprintf(stderr, "[LINK] RX: receive_frames(): "
                                            "Error unlocking pthread_mutex\n");
                            return NULL;
                        }
                    }
                    //Transition to send acknowledgement
                    state = SEND_ACK;
                }else{
                    //Is the previous ack frame still being worked with?
                    if (link->rx->ack_buf_filled){
                        //Instead of causing a disruption, drop the current frame
                        fprintf(stderr, "[LINK] RX: ACK frame dropped!\n");
                        phy_release_rx_buf(link->phy);
                        break;
                    }
                    //Copy/convert to ack frame struct and mark ack buffer filled
                    convert_buf_to_ack_frame_struct(rx_buf, &(link->rx->ack_frame_buf));
                    //Release buffer from the phy
                    phy_release_rx_buf(link->phy);
                    //Signal that the link ack buffer is filled
                    link->rx->ack_buf_filled = true;
                    status = pthread_mutex_lock(&(link->rx->ack_buf_status_lock));
                    if (status != 0){
                        fprintf(stderr, "[LINK] RX: receive_frames(): "
                                        "Error locking pthread_mutex\n");
                        return NULL;
                    }
                    status = pthread_cond_signal(&(link->rx->ack_buf_filled_cond));
                    if (status != 0){
                        fprintf(stderr, "[LINK] RX: receive_frames(): "
                                        "Error signaling pthread_cond\n");
                        return NULL;
                    }
                    status = pthread_mutex_unlock(&(link->rx->ack_buf_status_lock));
                    if (status != 0){
                        fprintf(stderr, "[LINK] RX: receive_frames(): "
                                        "Error unlocking pthread_mutex\n");
                        return NULL;
                    }
                    //Done with the frame. Go back to WAIT state
                    state = WAIT;
                }
                break;
            case SEND_ACK:
                //--We received a data frame, now it's time to send the ack
                DEBUG_MSG("[LINK] RX: State = SEND_ACK (ack# = %hu)\n", seq_num);

                link->tx->ack_frame_buf.type = 0xFF;
                link->tx->ack_frame_buf.ack_num = seq_num;

                //Copy frame into send buf
                convert_ack_frame_struct_to_buf(&(link->tx->ack_frame_buf),
                                                ack_send_buf);
                //Calculate the CRC
                crc_32 = crc32(ack_send_buf, ACK_FRAME_LENGTH - sizeof(crc_32));
                //Copy this CRC to the send buf
                memcpy(&ack_send_buf[ACK_FRAME_LENGTH - sizeof(crc_32)], &crc_32,
                        sizeof(crc_32));
                //Transmit with phy
                status = phy_fill_tx_buf(link->phy, ack_send_buf, ACK_FRAME_LENGTH);
                if (status != 0){
                    fprintf(stderr, "[LINK] Couldn't fill phy tx buffer\n");
                    goto out;
                }
                //Done; go back to the WAIT state
                state = WAIT;
                break;
            default:
                fprintf(stderr, "[LINK] receive_frames(): invalid state\n");
                return NULL;
        }
    }
    out:
        return NULL;
}
