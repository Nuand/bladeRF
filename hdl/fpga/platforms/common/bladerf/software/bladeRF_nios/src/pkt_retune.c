/* This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (c) 2015 Nuand LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <stdint.h>
#include <stdbool.h>
#include "pkt_handler.h"
#include "pkt_retune.h"
#include "nios_pkt_retune.h"    /* Packet format definition */
#include "devices.h"
#include "band_select.h"
#include "debug.h"

#ifdef BLADERF_NIOS_DEBUG
    volatile uint32_t pkt_retune_error_count = 0;
#   define INCREMENT_ERROR_COUNT() do { pkt_retune_error_count++; } while (0)
#else
#   define INCREMENT_ERROR_COUNT() do {} while (0)
#endif

/* The enqueue/dequeue routines require that this be a power of two */
#define RETUNE_QUEUE_MAX    16
#define QUEUE_FULL          0xff
#define QUEUE_EMPTY         0xfe

/* State of items in the retune queue */
enum entry_state {
    ENTRY_STATE_INVALID = 0,  /* Marks entry invalid and not in use */
    ENTRY_STATE_NEW,          /* We have a new retune request to satisfy */
    ENTRY_STATE_SCHEDULED,    /* We've scheduled the timer interrupt for
                               * this entry and are awaiting the ISR */
    ENTRY_STATE_READY,        /* The timer interrupt has fired - we should
                               * handle this retune */
};

struct queue_entry {
    volatile enum entry_state state;
    struct lms_freq freq;
    uint64_t timestamp;
};

static struct queue {
    uint8_t count;      /* Total number of items in the queue */
    uint8_t ins_idx;    /* Insertion index */
    uint8_t rem_idx;    /* Removal index */

    struct queue_entry entries[RETUNE_QUEUE_MAX];
} rx_queue, tx_queue;

/* Returns queue size after enqueue operation, or QUEUE_FULL if we could
 * not enqueue the requested item */
static inline uint8_t enqueue_retune(struct queue *q,
                                     const struct lms_freq *f,
                                     uint64_t timestamp)
{
    uint8_t ret;

    if (q->count >= RETUNE_QUEUE_MAX) {
        return QUEUE_FULL;
    }

    memcpy(&q->entries[q->ins_idx].freq, f, sizeof(f[0]));

    q->entries[q->ins_idx].state = ENTRY_STATE_NEW;
    q->entries[q->ins_idx].timestamp = timestamp;

    q->ins_idx = (q->ins_idx + 1) & (RETUNE_QUEUE_MAX - 1);

    q->count++;
    ret = q->count;

    return ret;
}

/* Retune number of items left in the queue after the dequeue operation,
 * or QUEUE_EMPTY if there was nothing to dequeue */
static inline uint8_t dequeue_retune(struct queue *q, struct queue_entry *e)
{
    uint8_t ret;

    if (q->count == 0) {
        return QUEUE_EMPTY;
    }

    if (e != NULL) {
        memcpy(&e, &q->entries[q->rem_idx], sizeof(e[0]));
    }

    q->rem_idx = (q->rem_idx + 1) & (RETUNE_QUEUE_MAX - 1);

    q->count--;
    ret = q->count;

    return ret;
}

/* Get the state of the next item in the retune queue */
static inline struct queue_entry * peek_next_retune(struct queue *q)
{
    if (q->count == 0) {
        return NULL;
    } else {
        return &q->entries[q->rem_idx];
    }
}

/* The retune interrupt may fire while this call is occuring, so we should
 * perform these operations in an order that minimizes the race window, and
 * does not cause the race to be problematic. It's fine if the last retune
 * occurs before we can cancel it. */
static void reset_queue(struct queue *q)
{
    unsigned int i;

    q->count = 0;

    for (i = 0; i < RETUNE_QUEUE_MAX; i++) {
        q->entries[i].state = ENTRY_STATE_INVALID;
    }

    q->rem_idx = q->ins_idx = 0;
}

static inline void retune_isr(struct queue *q)
{
    struct queue_entry *e = peek_next_retune(q);
    if (e != NULL) {
        if (e->state == ENTRY_STATE_SCHEDULED) {
            e->state = ENTRY_STATE_READY;
        } else {
            INCREMENT_ERROR_COUNT();
        }
    }
}


#ifndef BLADERF_NIOS_PC_SIMULATION
static void retune_rx(void *context)
{
	/* Handle the ISR */
    retune_isr(&rx_queue);

    /* Clear the interrupt */
    timer_tamer_clear_interrupt(BLADERF_MODULE_RX);
}

static void retune_tx(void *context)
{
	/* Handle the ISR */
    retune_isr(&tx_queue);

    /* Clear the interrupt */
    timer_tamer_clear_interrupt(BLADERF_MODULE_TX);
}
#endif


void pkt_retune_init()
{
    reset_queue(&rx_queue);
    reset_queue(&tx_queue);

#ifndef BLADERF_NIOS_PC_SIMULATION

    /* Register RX Time Tamer ISR */
    alt_ic_isr_register(
        RX_TAMER_IRQ_INTERRUPT_CONTROLLER_ID,
        RX_TAMER_IRQ,
        retune_rx,
        NULL,
        NULL
    ) ;

    /* Register TX Time Tamer ISR */
    alt_ic_isr_register(
        TX_TAMER_IRQ_INTERRUPT_CONTROLLER_ID,
        TX_TAMER_IRQ,
        retune_tx,
        NULL,
        NULL
    ) ;
#endif
}

#ifndef BLADERF_XB_TX_ENABLE
#define BLADERF_XB_TX_ENABLE 0x1000
#endif
#ifndef BLADERF_XB_RX_ENABLE
#define BLADERF_XB_RX_ENABLE 0x2000
#endif

static void xb_config_write(uint8_t xb_gpio) {
   uint32_t val;
   val = expansion_port_read();
   if (!(xb_gpio & LMS_FREQ_XB_200_ENABLE)) {
      return;
   }
   if (xb_gpio & LMS_FREQ_XB_200_MODULE_RX) {
      val &= ~(0x30000000 | 0x30);
      val |= (((xb_gpio & LMS_FREQ_XB_200_FILTER_SW)
                        >> LMS_FREQ_XB_200_FILTER_SW_SHIFT) & 3 ) << 28;
      val |= (((xb_gpio & LMS_FREQ_XB_200_PATH)
                        >> LMS_FREQ_XB_200_PATH_SHIFT) & 3 ) << 4;
      val |= BLADERF_XB_RX_ENABLE;
   } else {
      val &= ~(0x0C000000 | 0x0C);
      val |= (((xb_gpio & LMS_FREQ_XB_200_FILTER_SW)
                        >> LMS_FREQ_XB_200_FILTER_SW_SHIFT) & 3 ) << 26;
      val |= (((xb_gpio & LMS_FREQ_XB_200_PATH)
                           >> LMS_FREQ_XB_200_PATH_SHIFT) & 3 ) << 2;
      val |= BLADERF_XB_TX_ENABLE;
   }
   expansion_port_write(val);

   uint8_t reg;
   uint8_t lreg;
   LMS_READ(NULL, 0x5A, &reg);

#define LMS_RX_SWAP 0x40
#define LMS_TX_SWAP 0x08

   lreg = reg;
   if (((xb_gpio & LMS_FREQ_XB_200_PATH) >> LMS_FREQ_XB_200_PATH_SHIFT) & 1) {
      lreg |= (xb_gpio & LMS_FREQ_XB_200_MODULE_RX) ? LMS_RX_SWAP : LMS_TX_SWAP;
   } else {
      lreg &= ~((xb_gpio & LMS_FREQ_XB_200_MODULE_RX) ? LMS_RX_SWAP : LMS_TX_SWAP);
   }

   LMS_WRITE(NULL, 0x5A, lreg);


}

static inline void perform_work(struct queue *q, bladerf_module module)
{
    struct queue_entry *e = peek_next_retune(q);

    if (e == NULL) {
        return;
    }

    switch (e->state) {
        case ENTRY_STATE_NEW:
            e->state = ENTRY_STATE_SCHEDULED;
            tamer_schedule(module, e->timestamp);
            break;

        case ENTRY_STATE_SCHEDULED:
            /* Nothing to do.
             * We're just waiting for this entry to become */
            break;

        case ENTRY_STATE_READY:

            /* Perform our retune */
            if (lms_set_precalculated_frequency(NULL, module, &e->freq)) {
                INCREMENT_ERROR_COUNT();
            } else {
                bool low_band = (e->freq.flags & LMS_FREQ_FLAGS_LOW_BAND) != 0;
                if (band_select(NULL, module, low_band)) {
                    INCREMENT_ERROR_COUNT();
                }

                xb_config_write(e->freq.xb_gpio);
            }

            /* Drop the item from the queue */
            dequeue_retune(q, NULL);
            break;

        default:
            INCREMENT_ERROR_COUNT();
            break;
    }
}

void pkt_retune_work(void)
{
    perform_work(&rx_queue, BLADERF_MODULE_RX);
    perform_work(&tx_queue, BLADERF_MODULE_TX);
}

void pkt_retune(struct pkt_buf *b)
{
    int status = -1;
    bladerf_module module;
    uint8_t flags;
    struct lms_freq f;
    uint64_t timestamp;
    uint64_t start_time;
    uint64_t end_time;
    uint64_t duration = 0;
    bool low_band;
    uint8_t xb_gpio;
    bool quick_tune;

    flags = NIOS_PKT_RETUNERESP_FLAG_SUCCESS;

    nios_pkt_retune_unpack(b->req, &module, &timestamp,
                           &f.nint, &f.nfrac, &f.freqsel, &f.vcocap,
                           &low_band, &xb_gpio, &quick_tune);

    f.vcocap_result = 0xff;

    if (low_band) {
        f.flags = LMS_FREQ_FLAGS_LOW_BAND;
    } else {
        f.flags = 0;
    }

    if (quick_tune) {
        f.flags |= LMS_FREQ_FLAGS_FORCE_VCOCAP;
    }

    start_time = time_tamer_read(module);

    if (timestamp == NIOS_PKT_RETUNE_NOW) {
        /* Fire off this retune operation now */
        switch (module) {
            case BLADERF_MODULE_RX:
            case BLADERF_MODULE_TX:
                status = lms_set_precalculated_frequency(NULL, module, &f);
                if (status != 0) {
                    goto out;
                }

                flags |= NIOS_PKT_RETUNERESP_FLAG_TSVTUNE_VALID;

                status = band_select(NULL, module, low_band);
                if (status != 0) {
                    goto out;
                }

                xb_config_write(xb_gpio);

                status = 0;
                break;

            default:
                INCREMENT_ERROR_COUNT();
                status = -1;
        }

    } else if (timestamp == NIOS_PKT_RETUNE_CLEAR_QUEUE) {
        switch (module) {
            case BLADERF_MODULE_RX:
                reset_queue(&rx_queue);
                status = 0;
                break;

            case BLADERF_MODULE_TX:
                reset_queue(&tx_queue);
                status = 0;
                break;

            default:
                INCREMENT_ERROR_COUNT();
                status = -1;
        }
    } else {
        uint8_t queue_size;

        switch (module) {
            case BLADERF_MODULE_RX:
                queue_size = enqueue_retune(&rx_queue, &f, timestamp);
                break;

            case BLADERF_MODULE_TX:
                queue_size = enqueue_retune(&tx_queue, &f, timestamp);
                break;

            default:
                INCREMENT_ERROR_COUNT();
                queue_size = QUEUE_FULL;

        }

        if (queue_size == QUEUE_FULL) {
            status = -1;
        } else {
            status = 0;
        }
    }

    end_time = time_tamer_read(module);
    duration = end_time - start_time;

out:
    if (status != 0) {
        flags &= ~(NIOS_PKT_RETUNERESP_FLAG_SUCCESS);
    }

    nios_pkt_retune_resp_pack(b->resp, duration, f.vcocap_result, flags);
}
