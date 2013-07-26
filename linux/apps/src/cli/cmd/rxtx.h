#ifndef CMD_RXTX_H_
#define CMD_RXTX_H_

#include <stdbool.h>
#include "common.h"

/**
 * Allocate and initialize data for RX and TX tasks.
 *
 * @param   s   CLI state
 *
 * @return  allocated data on succss, NULL on failure
 */
struct rxtx_data * rxtx_data_alloc(struct cli_state *s);

/**
 * Kick off RX/TX tasks
 *
 * @param   s   CLI state
 *
 * @pre     Must be preceeded by a call to rxtx_data_alloc()
 *
 * @return  0 on success, nonzero on failure
 */
int rxtx_start_tasks(struct cli_state *s);

/**
 * Request RX/TX tasks to shut down and exit
 *
 * @param   s   CLI state
 */
void rxtx_stop_tasks(struct rxtx_data *d);

/**
 * Free data allocated with rxtx_data_alloc()
 *
 * @pre     Must be preceeded by a call to rxtx_stop_tasks()
 *
 * @param   d   RX/TX data
 */
void rxtx_data_free(struct rxtx_data *d);

/**
 * @return true if the TX task is currently running,
 *         false if it is any other state
 */
bool rxtx_tx_task_running(struct cli_state *s);

/**
 * @return true if the RX task is currently running,
 *         false if it is any other state
 */
bool rxtx_rx_task_running(struct cli_state *s);


#endif
