/*
 * Which buffer still needs sending, when transfers were cancelled.
 *
 * Cancelled transfers come back through libusb's own callback path, which
 * never runs the sync callback. So cons_i keeps pointing at a buffer that is
 * already IN_FLIGHT and will never come back, while the data waiting to go
 * out sits in the FULL buffers behind it. Looking only at cons_i finds
 * nothing to do and leaves submission parked on a callback that can no longer
 * run - the state a wire trace showed as 8 in flight, 8 full, 0 empty and no
 * submissions for a full stream timeout.
 *
 * The search is a handful of lines and easy to get subtly wrong (wrap-around,
 * the empty case), and a wedged TX feed is expensive to reproduce, so it gets
 * a test that needs no device.
 *
 *   cc -o /tmp/t test_oldest_full_buffer.c && /tmp/t
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_MGMT_INVALID_INDEX (UINT_MAX)
#include <limits.h>

typedef enum {
    SYNC_BUFFER_EMPTY = 0,
    SYNC_BUFFER_PARTIAL,
    SYNC_BUFFER_FULL,
    SYNC_BUFFER_IN_FLIGHT,
} sync_buffer_status;

struct buffer_mgmt {
    unsigned int num_buffers;
    unsigned int cons_i;
    sync_buffer_status *status;
};

/* Kept byte-for-byte in step with sync.c. */
static unsigned int oldest_full_buffer(struct buffer_mgmt *b)
{
    unsigned int i, idx;

    if (b->cons_i == BUFFER_MGMT_INVALID_INDEX) {
        return BUFFER_MGMT_INVALID_INDEX;
    }

    for (i = 0; i < b->num_buffers; i++) {
        idx = (b->cons_i + i) % b->num_buffers;
        if (b->status[idx] == SYNC_BUFFER_FULL) {
            return idx;
        }
    }

    return BUFFER_MGMT_INVALID_INDEX;
}

int main(void)
{
    sync_buffer_status st[4];
    struct buffer_mgmt b = { .num_buffers = 4, .cons_i = 0, .status = st };
    unsigned int i;

    /* cons_i points at what needs sending: take it. */
    for (i = 0; i < 4; i++) {
        st[i] = SYNC_BUFFER_EMPTY;
    }
    st[1] = SYNC_BUFFER_FULL;
    b.cons_i = 1;
    assert(oldest_full_buffer(&b) == 1);

    /* The defect this exists for: cons_i was left on a cancelled transfer's
     * buffer, and the data is behind it. Looking only at cons_i finds
     * nothing. */
    for (i = 0; i < 4; i++) {
        st[i] = SYNC_BUFFER_EMPTY;
    }
    st[1] = SYNC_BUFFER_IN_FLIGHT;
    st[2] = SYNC_BUFFER_FULL;
    b.cons_i = 1;
    assert(oldest_full_buffer(&b) == 2);

    /* Wrap-around: the oldest FULL is before cons_i in index order but after
     * it in ring order. */
    for (i = 0; i < 4; i++) {
        st[i] = SYNC_BUFFER_IN_FLIGHT;
    }
    st[0] = SYNC_BUFFER_FULL;
    b.cons_i = 2;
    assert(oldest_full_buffer(&b) == 0);

    /* Ring order, not index order: with FULL on both sides of cons_i the one
     * at or after cons_i wins. */
    for (i = 0; i < 4; i++) {
        st[i] = SYNC_BUFFER_EMPTY;
    }
    st[0] = SYNC_BUFFER_FULL;
    st[3] = SYNC_BUFFER_FULL;
    b.cons_i = 3;
    assert(oldest_full_buffer(&b) == 3);

    /* Every tracked buffer came back through the cancellation path: nothing
     * is owed, so the caller takes submission duty back. */
    for (i = 0; i < 4; i++) {
        st[i] = SYNC_BUFFER_IN_FLIGHT;
    }
    b.cons_i = 0;
    assert(oldest_full_buffer(&b) == BUFFER_MGMT_INVALID_INDEX);

    /* PARTIAL is not ready to ship - sync_tx is still filling it. */
    for (i = 0; i < 4; i++) {
        st[i] = SYNC_BUFFER_EMPTY;
    }
    st[2] = SYNC_BUFFER_PARTIAL;
    b.cons_i = 0;
    assert(oldest_full_buffer(&b) == BUFFER_MGMT_INVALID_INDEX);

    /* No index to start from at all. */
    b.cons_i = BUFFER_MGMT_INVALID_INDEX;
    assert(oldest_full_buffer(&b) == BUFFER_MGMT_INVALID_INDEX);

    printf("oldest_full_buffer: ok\n");
    return 0;
}
