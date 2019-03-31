/**
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2013-2018 Nuand LLC
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
#ifndef PRINTSET_H__
#define PRINTSET_H__

#include "cmd.h"

enum ps_board_option {
    BOARD_ANY,      /**< Supported on all hardware */
    BOARD_BLADERF1, /**< Supported on bladeRF 1 */
    BOARD_BLADERF2, /**< Supported on bladeRF 2 */
};

/**
 * @brief      calls func() for all matching channels
 *
 * declare func as:
 *  `int the_func(struct cli_state *state, bladerf_channel ch, void *arg)`
 * returning CLI_RET_OK if ok, or another value if error
 *
 * @param      state      CLI state structure
 * @param[in]  rx         consider rx channels if true
 * @param[in]  tx         consider tx channels if true
 * @param[in]  only_chan  consider only this channel if true
 * @param[in]  func       The function to call
 * @param      arg        The argument to pass to the function
 *
 * @return     CLI_RET_*
 */
int ps_foreach_chan(struct cli_state *state,
                    bool const rx,
                    bool const tx,
                    bladerf_channel const only_chan,
                    int (*func)(struct cli_state *, bladerf_channel, void *),
                    void *const arg);

/**
 * @brief      test if the open device matches a given board model
 *
 * @param      dev    device structure
 * @param[in]  board  model to test
 *
 * @return     true if `dev` is a `board`, false otherwise
 */
bool ps_is_board(struct bladerf *dev, enum ps_board_option board);

/* printset_hardware.c */
int print_hardware(struct cli_state *state, int argc, char **argv);
int print_clock_ref(struct cli_state *state, int argc, char **argv);
int set_clock_ref(struct cli_state *state, int argc, char **argv);
int print_refin_freq(struct cli_state *state, int argc, char **argv);
int set_refin_freq(struct cli_state *state, int argc, char **argv);
int print_clock_out(struct cli_state *state, int argc, char **argv);
int set_clock_out(struct cli_state *state, int argc, char **argv);
int print_clock_sel(struct cli_state *state, int argc, char **argv);
int set_clock_sel(struct cli_state *state, int argc, char **argv);
int print_biastee(struct cli_state *state, int argc, char **argv);
int set_biastee(struct cli_state *state, int argc, char **argv);

/* printset_xb.c */
int print_xb_gpio(struct cli_state *state, int argc, char **argv);
int set_xb_gpio(struct cli_state *state, int argc, char **argv);
int print_xb_gpio_dir(struct cli_state *state, int argc, char **argv);
int set_xb_gpio_dir(struct cli_state *state, int argc, char **argv);
int print_xb_spi(struct cli_state *state, int argc, char **argv);
int set_xb_spi(struct cli_state *state, int argc, char **argv);
int print_gpio(struct cli_state *state, int argc, char **argv);
int set_gpio(struct cli_state *state, int argc, char **argv);

/* printset_impl.c */
int print_agc(struct cli_state *state, int argc, char **argv);
int set_agc(struct cli_state *state, int argc, char **argv);
int print_bandwidth(struct cli_state *state, int argc, char **argv);
int set_bandwidth(struct cli_state *state, int argc, char **argv);
int print_filter(struct cli_state *state, int argc, char **argv);
int set_filter(struct cli_state *state, int argc, char **argv);
int print_frequency(struct cli_state *state, int argc, char **argv);
int set_frequency(struct cli_state *state, int argc, char **argv);
int print_gain(struct cli_state *state, int argc, char **argv);
int set_gain(struct cli_state *state, int argc, char **argv);
int print_lnagain(struct cli_state *state, int argc, char **argv);
int set_lnagain(struct cli_state *state, int argc, char **argv);
int print_loopback(struct cli_state *state, int argc, char **argv);
int set_loopback(struct cli_state *state, int argc, char **argv);
int print_rssi(struct cli_state *state, int argc, char **argv);
int print_rx_mux(struct cli_state *state, int argc, char **argv);
int set_rx_mux(struct cli_state *state, int argc, char **argv);
int print_tuning_mode(struct cli_state *state, int argc, char **argv);
int set_tuning_mode(struct cli_state *state, int argc, char **argv);
int print_rxvga1(struct cli_state *state, int argc, char **argv);
int set_rxvga1(struct cli_state *state, int argc, char **argv);
int print_rxvga2(struct cli_state *state, int argc, char **argv);
int set_rxvga2(struct cli_state *state, int argc, char **argv);
int print_samplerate(struct cli_state *state, int argc, char **argv);
int set_samplerate(struct cli_state *state, int argc, char **argv);
int set_sampling(struct cli_state *state, int argc, char **argv);
int print_sampling(struct cli_state *state, int argc, char **argv);
int print_smb_mode(struct cli_state *state, int argc, char **argv);
int set_smb_mode(struct cli_state *state, int argc, char **argv);
int print_trimdac(struct cli_state *state, int argc, char **argv);
int set_trimdac(struct cli_state *state, int argc, char **argv);
int print_txvga1(struct cli_state *state, int argc, char **argv);
int set_txvga1(struct cli_state *state, int argc, char **argv);
int print_txvga2(struct cli_state *state, int argc, char **argv);
int set_txvga2(struct cli_state *state, int argc, char **argv);
int set_vctcxo_tamer(struct cli_state *state, int argc, char **argv);

#endif
