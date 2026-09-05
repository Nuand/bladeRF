/*
 * TX submission must never be parked on a callback that can no longer run.
 *
 * Replays the ordering a usbmon trace showed during a stalled feed:
 *
 *   every transfer busy -> submit returns WOULD_BLOCK
 *   -> submission duty handed to the worker callback, cons_i records the buffer
 *   -> those transfers come back CANCELLED, which libusb delivers through its
 *      own callback path, so the sync callback never runs
 *   -> cons_i still points at a buffer marked IN_FLIGHT that will not come back
 *   -> the data waiting to go out is in the FULL buffers behind it
 *   -> next sync_tx finds no EMPTY buffer and waits out the stream timeout
 *
 * On the wire that appeared as eight submissions in 86 us, none completing,
 * all eight returning -ENOENT exactly 1000 ms later, while the RX endpoint
 * carried 941 successful completions in the same window. The device was
 * taking data; the host had stopped offering any.
 *
 * The ring here is the real one's state machine, driven by hand, so the
 * ordering is fixed instead of waiting for a race on hardware - the device
 * reproducer gave "stalled at repeat 1", "stalled at repeat 8" and "clean
 * 24/24" from the same binary within an hour.
 *
 *   cc -o /tmp/t test_tx_submit_liveness.c && /tmp/t
 */
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>

#define BUFFER_MGMT_INVALID_INDEX (UINT_MAX)
#define NUM_BUFFERS 8
#define NUM_TRANSFERS 4

typedef enum {
    SYNC_BUFFER_EMPTY = 0,
    SYNC_BUFFER_PARTIAL,
    SYNC_BUFFER_FULL,
    SYNC_BUFFER_IN_FLIGHT,
} sync_buffer_status;

typedef enum {
    SYNC_TX_SUBMITTER_INVALID = 0,
    SYNC_TX_SUBMITTER_FN,
    SYNC_TX_SUBMITTER_CALLBACK,
} sync_tx_submitter;

struct buffer_mgmt {
    unsigned int num_buffers;
    unsigned int prod_i;
    unsigned int cons_i;
    sync_tx_submitter submitter;
    sync_buffer_status status[NUM_BUFFERS];
};

/* --- the code under test, kept in step with sync.c --- */

static unsigned int oldest_full_buffer(struct buffer_mgmt *b)
{
    unsigned int i, idx;
    const unsigned int from = (b->cons_i == BUFFER_MGMT_INVALID_INDEX)
                                  ? b->prod_i : b->cons_i;

    for (i = 0; i < b->num_buffers; i++) {
        idx = (from + i) % b->num_buffers;
        if (b->status[idx] == SYNC_BUFFER_FULL) {
            return idx;
        }
    }

    return BUFFER_MGMT_INVALID_INDEX;
}

static bool tx_submission_parked(struct buffer_mgmt *b)
{
    if (b->submitter == SYNC_TX_SUBMITTER_CALLBACK) {
        return b->cons_i != BUFFER_MGMT_INVALID_INDEX;
    }
    return b->submitter == SYNC_TX_SUBMITTER_FN &&
           oldest_full_buffer(b) != BUFFER_MGMT_INVALID_INDEX;
}

/* reclaim_tx_submission() with the submit call replaced by a stub, since the
 * point is the ownership bookkeeping around it. free_transfers models how
 * many transfers libusb would accept right now. */
static int reclaim(struct buffer_mgmt *b, int free_transfers)
{
    const unsigned int idx = oldest_full_buffer(b);

    if (idx == BUFFER_MGMT_INVALID_INDEX) {
        b->submitter = SYNC_TX_SUBMITTER_FN;
        b->cons_i    = BUFFER_MGMT_INVALID_INDEX;
        return 0;
    }

    if (free_transfers <= 0) {
        return 0;                       /* WOULD_BLOCK, buffer stays FULL */
    }

    b->status[idx] = SYNC_BUFFER_IN_FLIGHT;
    b->cons_i      = (idx + 1) % b->num_buffers;
    if (b->status[b->cons_i] != SYNC_BUFFER_FULL) {
        b->submitter = SYNC_TX_SUBMITTER_FN;
        b->cons_i    = BUFFER_MGMT_INVALID_INDEX;
    }
    return 0;
}

/* The loop sync_tx runs while the ring stays parked: keep submitting while
 * the backend accepts buffers, stop as soon as nothing moves. */
static int drain(struct buffer_mgmt *b, int free_transfers)
{
    int status = 0;

    while (status == 0 && tx_submission_parked(b)) {
        const unsigned int before = b->cons_i;

        status = reclaim(b, free_transfers);
        if (b->cons_i == before) {
            break;
        }
        if (free_transfers > 0) {
            free_transfers--;
        }
    }
    return status;
}

/* --- the state the wire trace showed --- */

static void build_stalled_ring(struct buffer_mgmt *b)
{
    unsigned int i;

    b->num_buffers = NUM_BUFFERS;
    b->submitter   = SYNC_TX_SUBMITTER_CALLBACK;

    /* Four transfers went out and were cancelled; their buffers keep the
     * IN_FLIGHT mark because only the sync callback clears it, and the
     * cancellation path bypassed it. */
    for (i = 0; i < NUM_TRANSFERS; i++) {
        b->status[i] = SYNC_BUFFER_IN_FLIGHT;
    }
    /* sync_tx kept filling behind them. */
    for (i = NUM_TRANSFERS; i < NUM_BUFFERS; i++) {
        b->status[i] = SYNC_BUFFER_FULL;
    }
    /* cons_i is stuck on the first cancelled transfer's buffer. */
    b->cons_i  = 0;
    b->prod_i  = 0;
}

/* Forbidden state: something is waiting to be sent, the duty is with the
 * callback, and no completion can arrive to run it. */
static bool stuck(struct buffer_mgmt *b, int live_transfers)
{
    unsigned int i, full = 0;

    for (i = 0; i < b->num_buffers; i++) {
        if (b->status[i] == SYNC_BUFFER_FULL) {
            full++;
        }
    }
    /* Who holds the duty does not matter: what makes it stuck is data waiting
     * with nothing in flight to bring the callback that would move it. Both
     * states were seen on hardware - CALLBACK with 8 in flight and 8 full, and
     * FN with 0 in flight and 26 full. */
    return full > 0 && live_transfers == 0;
}

int main(void)
{
    struct buffer_mgmt b;

    /* The wire ordering: transfers cancelled, none live, data waiting. */
    build_stalled_ring(&b);
    assert(stuck(&b, 0));
    assert(tx_submission_parked(&b));

    /* Transfers were cancelled, so libusb has slots free again. Draining -
     * the loop the caller runs while the ring stays parked - must leave it in
     * a live state. One submission is not enough: each buffer still FULL
     * behind the first would need its own callback. */
    assert(drain(&b, NUM_TRANSFERS) == 0);
    assert(!stuck(&b, 0));

    /* Every transfer still busy: the attempt must change nothing rather than
     * lie about progress, and the caller waits as before. */
    build_stalled_ring(&b);
    assert(reclaim(&b, 0) == 0);
    assert(stuck(&b, 0));
    assert(tx_submission_parked(&b));

    /* Nothing waiting to go out: the duty comes back with no submission, so a
     * later sync_tx can submit for itself. */
    build_stalled_ring(&b);
    for (unsigned int i = NUM_TRANSFERS; i < NUM_BUFFERS; i++) {
        b.status[i] = SYNC_BUFFER_EMPTY;
    }
    assert(reclaim(&b, 0) == 0);
    assert(b.submitter == SYNC_TX_SUBMITTER_FN);
    assert(b.cons_i == BUFFER_MGMT_INVALID_INDEX);
    assert(!tx_submission_parked(&b));

    /* Duty ours and nothing waiting: not parked. */
    for (unsigned int i = 0; i < NUM_BUFFERS; i++) {
        b.status[i] = SYNC_BUFFER_EMPTY;
    }
    b.submitter = SYNC_TX_SUBMITTER_FN;
    b.cons_i    = BUFFER_MGMT_INVALID_INDEX;
    b.prod_i    = 0;
    assert(!tx_submission_parked(&b));

    /* Duty ours but buffers piled up while it was parked with the callback.
     * Measured on hardware as submitter FN, 0 in flight, 26 of 64 FULL, and
     * sync_tx timing out: it only ever submits the buffer prod_i points at,
     * so the backlog never moves and no transfer is out to bring a callback.
     */
    for (unsigned int i = 0; i < NUM_BUFFERS; i++) {
        b.status[i] = SYNC_BUFFER_EMPTY;
    }
    b.status[5] = SYNC_BUFFER_FULL;
    b.status[6] = SYNC_BUFFER_FULL;
    b.submitter = SYNC_TX_SUBMITTER_FN;
    b.cons_i    = BUFFER_MGMT_INVALID_INDEX;
    b.prod_i    = 0;
    assert(stuck(&b, 0));
    assert(tx_submission_parked(&b));
    assert(oldest_full_buffer(&b) == 5);
    assert(drain(&b, NUM_TRANSFERS) == 0);
    assert(!stuck(&b, 0));

    printf("tx submit liveness: ok\n");
    return 0;
}
