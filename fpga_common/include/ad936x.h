/**
 * @file ad936x.h
 *
 * @brief Interface to the library for the AD936X RFIC family
 *
 * Copyright (c) 2018 Nuand LLC.
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
#ifndef AD936X_H_
#define AD936X_H_

#include <inttypes.h>
#include <stdbool.h>

/**
 * The purpose of this header file is to allow the use of libad9361 without
 * including all of the unnecessary defines, etc, used during compilation.
 *
 * This file is largely copied from the files named in each section.  Only
 * necessary declarations are present.
 *
 * In general, defines are prefixed with AD936X_ to avoid conflicts.
 *
 * Comments have been removed for brevity.  Please see the original header
 * files from the third-party ADI library for further details.
 */

/******************************************************************************
 * From common.h
 ******************************************************************************/

struct clk_onecell_data {
    struct clk **clks;
    uint32_t clk_num;
};

/******************************************************************************
 * From ad9361.h
 ******************************************************************************/

#define AD936X_REG_TX1_OUT_1_PHASE_CORR 0x08E
#define AD936X_REG_TX1_OUT_1_GAIN_CORR 0x08F
#define AD936X_REG_TX2_OUT_1_PHASE_CORR 0x090
#define AD936X_REG_TX2_OUT_1_GAIN_CORR 0x091
#define AD936X_REG_TX1_OUT_1_OFFSET_I 0x092
#define AD936X_REG_TX1_OUT_1_OFFSET_Q 0x093
#define AD936X_REG_TX2_OUT_1_OFFSET_I 0x094
#define AD936X_REG_TX2_OUT_1_OFFSET_Q 0x095
#define AD936X_REG_TX1_OUT_2_PHASE_CORR 0x096
#define AD936X_REG_TX1_OUT_2_GAIN_CORR 0x097
#define AD936X_REG_TX2_OUT_2_PHASE_CORR 0x098
#define AD936X_REG_TX2_OUT_2_GAIN_CORR 0x099
#define AD936X_REG_TX1_OUT_2_OFFSET_I 0x09A
#define AD936X_REG_TX1_OUT_2_OFFSET_Q 0x09B
#define AD936X_REG_TX2_OUT_2_OFFSET_I 0x09C
#define AD936X_REG_TX2_OUT_2_OFFSET_Q 0x09D
#define AD936X_REG_TX_FORCE_BITS 0x09F

#define AD936X_REG_RX1_INPUT_A_PHASE_CORR 0x170
#define AD936X_REG_RX1_INPUT_A_GAIN_CORR 0x171
#define AD936X_REG_RX2_INPUT_A_PHASE_CORR 0x172
#define AD936X_REG_RX2_INPUT_A_GAIN_CORR 0x173
#define AD936X_REG_RX1_INPUT_A_Q_OFFSET 0x174
#define AD936X_REG_RX1_INPUT_A_OFFSETS 0x175
#define AD936X_REG_INPUT_A_OFFSETS_1 0x176
#define AD936X_REG_RX2_INPUT_A_OFFSETS 0x177
#define AD936X_REG_RX2_INPUT_A_I_OFFSET 0x178
#define AD936X_REG_RX1_INPUT_BC_PHASE_CORR 0x179
#define AD936X_REG_RX1_INPUT_BC_GAIN_CORR 0x17A
#define AD936X_REG_RX2_INPUT_BC_PHASE_CORR 0x17B
#define AD936X_REG_RX2_INPUT_BC_GAIN_CORR 0x17C
#define AD936X_REG_RX1_INPUT_BC_Q_OFFSET 0x17D
#define AD936X_REG_RX1_INPUT_BC_OFFSETS 0x17E
#define AD936X_REG_INPUT_BC_OFFSETS_1 0x17F
#define AD936X_REG_RX2_INPUT_BC_OFFSETS 0x180
#define AD936X_REG_RX2_INPUT_BC_I_OFFSET 0x181
#define AD936X_REG_FORCE_BITS 0x182

#define AD936X_READ (0 << 15)
#define AD936X_WRITE (1 << 15)
#define AD936X_CNT(x) ((((x)-1) & 0x7) << 12)
#define AD936X_ADDR(x) ((x)&0x3FF)

enum dev_id { ID_AD9361, ID_AD9364, ID_AD9363A };

enum ad9361_clocks {
    BB_REFCLK,
    RX_REFCLK,
    TX_REFCLK,
    BBPLL_CLK,
    ADC_CLK,
    R2_CLK,
    R1_CLK,
    CLKRF_CLK,
    RX_SAMPL_CLK,
    DAC_CLK,
    T2_CLK,
    T1_CLK,
    CLKTF_CLK,
    TX_SAMPL_CLK,
    RX_RFPLL_INT,
    TX_RFPLL_INT,
    RX_RFPLL_DUMMY,
    TX_RFPLL_DUMMY,
    RX_RFPLL,
    TX_RFPLL,
    NUM_AD9361_CLKS,
    EXT_REF_CLK,
};

enum rx_gain_table_name {
    TBL_200_1300_MHZ,
    TBL_1300_4000_MHZ,
    TBL_4000_6000_MHZ,
    RXGAIN_TBLS_END,
};

enum rx_gain_table_type {
    RXGAIN_FULL_TBL,
    RXGAIN_SPLIT_TBL,
};

struct rx_gain_info {
    enum rx_gain_table_type tbl_type;
    int32_t starting_gain_db;
    int32_t max_gain_db;
    int32_t gain_step_db;
    int32_t max_idx;
    int32_t idx_step_offset;
};

enum ad9361_pdata_rx_freq {
    BBPLL_FREQ,
    ADC_FREQ,
    R2_FREQ,
    R1_FREQ,
    CLKRF_FREQ,
    RX_SAMPL_FREQ,
    NUM_RX_CLOCKS,
};

enum ad9361_pdata_tx_freq {
    IGNORE_FREQ,
    DAC_FREQ,
    T2_FREQ,
    T1_FREQ,
    CLKTF_FREQ,
    TX_SAMPL_FREQ,
    NUM_TX_CLOCKS,
};

struct ad9361_fastlock_entry {
    //#define FASTLOOK_INIT 1
    uint8_t flags;
    uint8_t alc_orig;
    uint8_t alc_written;
};

struct ad9361_fastlock {
    uint8_t save_profile;
    uint8_t current_profile[2];
    struct ad9361_fastlock_entry entry[2][8];
};

enum ad9361_bist_mode {
    BIST_DISABLE,
    BIST_INJ_TX,
    BIST_INJ_RX,
};

enum ad9361_clkout {
    CLKOUT_DISABLE,
    BUFFERED_XTALN_DCXO,
    ADC_CLK_DIV_2,
    ADC_CLK_DIV_3,
    ADC_CLK_DIV_4,
    ADC_CLK_DIV_8,
    ADC_CLK_DIV_16,
};

enum rf_gain_ctrl_mode {
    RF_GAIN_MGC,
    RF_GAIN_FASTATTACK_AGC,
    RF_GAIN_SLOWATTACK_AGC,
    RF_GAIN_HYBRID_AGC
};

enum f_agc_target_gain_index_type {
    MAX_GAIN,
    SET_GAIN,
    OPTIMIZED_GAIN,
    NO_GAIN_CHANGE,
};

enum rssi_restart_mode {
    AGC_IN_FAST_ATTACK_MODE_LOCKS_THE_GAIN,
    EN_AGC_PIN_IS_PULLED_HIGH,
    ENTERS_RX_MODE,
    GAIN_CHANGE_OCCURS,
    SPI_WRITE_TO_REGISTER,
    GAIN_CHANGE_OCCURS_OR_EN_AGC_PIN_PULLED_HIGH,
};

struct rssi_control {
    enum rssi_restart_mode restart_mode;
    bool rssi_unit_is_rx_samples;
    uint32_t rssi_delay;
    uint32_t rssi_wait;
    uint32_t rssi_duration;
};

struct port_control {
    uint8_t pp_conf[3];
    uint8_t rx_clk_data_delay;
    uint8_t tx_clk_data_delay;
    uint8_t digital_io_ctrl;
    uint8_t lvds_bias_ctrl;
    uint8_t lvds_invert[2];
    uint8_t clk_out_drive;
    uint8_t dataclk_drive;
    uint8_t data_port_drive;
    uint8_t clk_out_slew;
    uint8_t dataclk_slew;
    uint8_t data_port_slew;
};

struct ctrl_outs_control {
    uint8_t index;
    uint8_t en_mask;
};

struct elna_control {
    uint16_t gain_mdB;
    uint16_t bypass_loss_mdB;
    uint32_t settling_delay_ns;
    bool elna_1_control_en;
    bool elna_2_control_en;
    bool elna_in_gaintable_all_index_en;
};

struct auxadc_control {
    int8_t offset;
    uint32_t temp_time_inteval_ms;
    uint32_t temp_sensor_decimation;
    bool periodic_temp_measuremnt;
    uint32_t auxadc_clock_rate;
    uint32_t auxadc_decimation;
};

struct auxdac_control {
    uint16_t dac1_default_value;
    uint16_t dac2_default_value;
    bool auxdac_manual_mode_en;
    bool dac1_in_rx_en;
    bool dac1_in_tx_en;
    bool dac1_in_alert_en;
    bool dac2_in_rx_en;
    bool dac2_in_tx_en;
    bool dac2_in_alert_en;
    uint8_t dac1_rx_delay_us;
    uint8_t dac1_tx_delay_us;
    uint8_t dac2_rx_delay_us;
    uint8_t dac2_tx_delay_us;
};

struct gpo_control {
    bool gpo0_inactive_state_high_en;
    bool gpo1_inactive_state_high_en;
    bool gpo2_inactive_state_high_en;
    bool gpo3_inactive_state_high_en;
    bool gpo0_slave_rx_en;
    bool gpo0_slave_tx_en;
    bool gpo1_slave_rx_en;
    bool gpo1_slave_tx_en;
    bool gpo2_slave_rx_en;
    bool gpo2_slave_tx_en;
    bool gpo3_slave_rx_en;
    bool gpo3_slave_tx_en;
    uint8_t gpo0_rx_delay_us;
    uint8_t gpo0_tx_delay_us;
    uint8_t gpo1_rx_delay_us;
    uint8_t gpo1_tx_delay_us;
    uint8_t gpo2_rx_delay_us;
    uint8_t gpo2_tx_delay_us;
    uint8_t gpo3_rx_delay_us;
    uint8_t gpo3_tx_delay_us;
};

struct tx_monitor_control {
    bool tx_mon_track_en;
    bool one_shot_mode_en;
    uint32_t low_high_gain_threshold_mdB;
    uint8_t low_gain_dB;
    uint8_t high_gain_dB;
    uint16_t tx_mon_delay;
    uint16_t tx_mon_duration;
    uint8_t tx1_mon_front_end_gain;
    uint8_t tx2_mon_front_end_gain;
    uint8_t tx1_mon_lo_cm;
    uint8_t tx2_mon_lo_cm;
};

struct gain_control {
    enum rf_gain_ctrl_mode rx1_mode;
    enum rf_gain_ctrl_mode rx2_mode;
    uint8_t adc_ovr_sample_size;
    uint8_t adc_small_overload_thresh;
    uint8_t adc_large_overload_thresh;
    uint16_t lmt_overload_high_thresh;
    uint16_t lmt_overload_low_thresh;
    uint16_t dec_pow_measuremnt_duration;
    uint8_t low_power_thresh;
    bool dig_gain_en;
    uint8_t max_dig_gain;
    bool mgc_rx1_ctrl_inp_en;
    bool mgc_rx2_ctrl_inp_en;
    uint8_t mgc_inc_gain_step;
    uint8_t mgc_dec_gain_step;
    uint8_t mgc_split_table_ctrl_inp_gain_mode;
    uint8_t agc_attack_delay_extra_margin_us;
    uint8_t agc_outer_thresh_high;
    uint8_t agc_outer_thresh_high_dec_steps;
    uint8_t agc_inner_thresh_high;
    uint8_t agc_inner_thresh_high_dec_steps;
    uint8_t agc_inner_thresh_low;
    uint8_t agc_inner_thresh_low_inc_steps;
    uint8_t agc_outer_thresh_low;
    uint8_t agc_outer_thresh_low_inc_steps;
    uint8_t adc_small_overload_exceed_counter;
    uint8_t adc_large_overload_exceed_counter;
    uint8_t adc_large_overload_inc_steps;
    bool adc_lmt_small_overload_prevent_gain_inc;
    uint8_t lmt_overload_large_exceed_counter;
    uint8_t lmt_overload_small_exceed_counter;
    uint8_t lmt_overload_large_inc_steps;
    uint8_t dig_saturation_exceed_counter;
    uint8_t dig_gain_step_size;
    bool sync_for_gain_counter_en;
    uint32_t gain_update_interval_us;
    bool immed_gain_change_if_large_adc_overload;
    bool immed_gain_change_if_large_lmt_overload;
    uint32_t f_agc_dec_pow_measuremnt_duration;
    uint32_t f_agc_state_wait_time_ns;
    bool f_agc_allow_agc_gain_increase;
    uint8_t f_agc_lp_thresh_increment_time;
    uint8_t f_agc_lp_thresh_increment_steps;
    uint8_t f_agc_lock_level;
    bool f_agc_lock_level_lmt_gain_increase_en;
    uint8_t f_agc_lock_level_gain_increase_upper_limit;
    uint8_t f_agc_lpf_final_settling_steps;
    uint8_t f_agc_lmt_final_settling_steps;
    uint8_t f_agc_final_overrange_count;
    bool f_agc_gain_increase_after_gain_lock_en;
    enum f_agc_target_gain_index_type f_agc_gain_index_type_after_exit_rx_mode;
    bool f_agc_use_last_lock_level_for_set_gain_en;
    uint8_t f_agc_optimized_gain_offset;
    bool f_agc_rst_gla_stronger_sig_thresh_exceeded_en;
    uint8_t f_agc_rst_gla_stronger_sig_thresh_above_ll;
    bool f_agc_rst_gla_engergy_lost_sig_thresh_exceeded_en;
    bool f_agc_rst_gla_engergy_lost_goto_optim_gain_en;
    uint8_t f_agc_rst_gla_engergy_lost_sig_thresh_below_ll;
    uint8_t f_agc_energy_lost_stronger_sig_gain_lock_exit_cnt;
    bool f_agc_rst_gla_large_adc_overload_en;
    bool f_agc_rst_gla_large_lmt_overload_en;
    bool f_agc_rst_gla_en_agc_pulled_high_en;
    enum f_agc_target_gain_index_type f_agc_rst_gla_if_en_agc_pulled_high_mode;
    uint8_t f_agc_power_measurement_duration_in_state5;
};

struct ad9361_phy_platform_data {
    bool rx2tx2;
    bool fdd;
    bool fdd_independent_mode;
    bool split_gt;
    bool use_extclk;
    bool ensm_pin_pulse_mode;
    bool ensm_pin_ctrl;
    bool debug_mode;
    bool tdd_use_dual_synth;
    bool tdd_skip_vco_cal;
    bool use_ext_rx_lo;
    bool use_ext_tx_lo;
    bool rx1rx2_phase_inversion_en;
    bool qec_tracking_slow_mode_en;
    uint8_t dc_offset_update_events;
    uint8_t dc_offset_attenuation_high;
    uint8_t dc_offset_attenuation_low;
    uint8_t rf_dc_offset_count_high;
    uint8_t rf_dc_offset_count_low;
    uint8_t dig_interface_tune_skipmode;
    uint8_t dig_interface_tune_fir_disable;
    uint32_t dcxo_coarse;
    uint32_t dcxo_fine;
    uint32_t rf_rx_input_sel;
    uint32_t rf_tx_output_sel;
    uint32_t rx1tx1_mode_use_rx_num;
    uint32_t rx1tx1_mode_use_tx_num;
    uint32_t rx_path_clks[NUM_RX_CLOCKS];
    uint32_t tx_path_clks[NUM_TX_CLOCKS];
    uint32_t trx_synth_max_fref;
    uint64_t rx_synth_freq;
    uint64_t tx_synth_freq;
    uint32_t rf_rx_bandwidth_Hz;
    uint32_t rf_tx_bandwidth_Hz;
    int32_t tx_atten;
    bool update_tx_gain_via_alert;
    uint32_t rx_fastlock_delay_ns;
    uint32_t tx_fastlock_delay_ns;
    bool trx_fastlock_pinctrl_en[2];
    enum ad9361_clkout ad9361_clkout_mode;
    struct gain_control gain_ctrl;
    struct rssi_control rssi_ctrl;
    struct port_control port_ctrl;
    struct ctrl_outs_control ctrl_outs_ctrl;
    struct elna_control elna_ctrl;
    struct auxadc_control auxadc_ctrl;
    struct auxdac_control auxdac_ctrl;
    struct gpo_control gpo_ctrl;
    struct tx_monitor_control txmon_ctrl;
    int32_t gpio_resetb;
    int32_t gpio_sync;
    int32_t gpio_cal_sw1;
    int32_t gpio_cal_sw2;
};

struct ad9361_rf_phy {
    enum dev_id dev_sel;
    uint8_t id_no;
    struct spi_device *spi;
    struct gpio_device *gpio;
    struct clk *clk_refin;
    struct clk *clks[NUM_AD9361_CLKS];
    struct refclk_scale *ref_clk_scale[NUM_AD9361_CLKS];
    struct clk_onecell_data clk_data;
    uint32_t (*ad9361_rfpll_ext_recalc_rate)(struct refclk_scale *clk_priv);
    int32_t (*ad9361_rfpll_ext_round_rate)(struct refclk_scale *clk_priv,
                                           uint32_t rate);
    int32_t (*ad9361_rfpll_ext_set_rate)(struct refclk_scale *clk_priv,
                                         uint32_t rate);
    struct ad9361_phy_platform_data *pdata;
    uint8_t prev_ensm_state;
    uint8_t curr_ensm_state;
    uint8_t cached_rx_rfpll_div;
    uint8_t cached_tx_rfpll_div;
    struct rx_gain_info rx_gain[RXGAIN_TBLS_END];
    enum rx_gain_table_name current_table;
    bool ensm_pin_ctl_en;
    bool auto_cal_en;
    uint64_t last_tx_quad_cal_freq;
    uint32_t last_tx_quad_cal_phase;
    uint64_t current_tx_lo_freq;
    uint64_t current_rx_lo_freq;
    bool current_tx_use_tdd_table;
    bool current_rx_use_tdd_table;
    uint32_t flags;
    uint32_t cal_threshold_freq;
    uint32_t current_rx_bw_Hz;
    uint32_t current_tx_bw_Hz;
    uint32_t rxbbf_div;
    uint32_t rate_governor;
    bool bypass_rx_fir;
    bool bypass_tx_fir;
    bool rx_eq_2tx;
    bool filt_valid;
    uint32_t filt_rx_path_clks[NUM_RX_CLOCKS];
    uint32_t filt_tx_path_clks[NUM_TX_CLOCKS];
    uint32_t filt_rx_bw_Hz;
    uint32_t filt_tx_bw_Hz;
    uint8_t tx_fir_int;
    uint8_t tx_fir_ntaps;
    uint8_t rx_fir_dec;
    uint8_t rx_fir_ntaps;
    uint8_t agc_mode[2];
    bool rfdc_track_en;
    bool bbdc_track_en;
    bool quad_track_en;
    bool txmon_tdd_en;
    uint16_t auxdac1_value;
    uint16_t auxdac2_value;
    uint32_t tx1_atten_cached;
    uint32_t tx2_atten_cached;
    struct ad9361_fastlock fastlock;
    struct axiadc_converter *adc_conv;
    struct axiadc_state *adc_state;
    int32_t bist_loopback_mode;
    enum ad9361_bist_mode bist_prbs_mode;
    enum ad9361_bist_mode bist_tone_mode;
    uint32_t bist_tone_freq_Hz;
    uint32_t bist_tone_level_dB;
    uint32_t bist_tone_mask;
    bool bbpll_initialized;
};

struct rf_rx_gain {
    uint32_t ant;
    int32_t gain_db;
    uint32_t fgt_lmt_index;
    uint32_t lmt_gain;
    uint32_t lpf_gain;
    uint32_t digital_gain;
    uint32_t lna_index;
    uint32_t tia_index;
    uint32_t mixer_index;
};

struct rf_rssi {
    uint32_t ant;
    uint32_t symbol;
    uint32_t preamble;
    int32_t multiplier;
    uint8_t duration;
};

int32_t ad9361_get_rx_gain(struct ad9361_rf_phy *phy,
                           uint32_t rx_id,
                           struct rf_rx_gain *rx_gain);
int32_t ad9361_spi_read(struct spi_device *spi, uint32_t reg);
int32_t ad9361_spi_write(struct spi_device *spi, uint32_t reg, uint32_t val);
void ad9361_get_bist_loopback(struct ad9361_rf_phy *phy, int32_t *mode);
int32_t ad9361_bist_loopback(struct ad9361_rf_phy *phy, int32_t mode);

/******************************************************************************
 * From ad9361_api.h
 ******************************************************************************/

typedef struct {
    enum dev_id dev_sel;
    uint8_t id_no;
    uint32_t reference_clk_rate;
    uint8_t two_rx_two_tx_mode_enable;
    uint8_t one_rx_one_tx_mode_use_rx_num;
    uint8_t one_rx_one_tx_mode_use_tx_num;
    uint8_t frequency_division_duplex_mode_enable;
    uint8_t frequency_division_duplex_independent_mode_enable;
    uint8_t tdd_use_dual_synth_mode_enable;
    uint8_t tdd_skip_vco_cal_enable;
    uint32_t tx_fastlock_delay_ns;
    uint32_t rx_fastlock_delay_ns;
    uint8_t rx_fastlock_pincontrol_enable;
    uint8_t tx_fastlock_pincontrol_enable;
    uint8_t external_rx_lo_enable;
    uint8_t external_tx_lo_enable;
    uint8_t dc_offset_tracking_update_event_mask;
    uint8_t dc_offset_attenuation_high_range;
    uint8_t dc_offset_attenuation_low_range;
    uint8_t dc_offset_count_high_range;
    uint8_t dc_offset_count_low_range;
    uint8_t split_gain_table_mode_enable;
    uint32_t trx_synthesizer_target_fref_overwrite_hz;
    uint8_t qec_tracking_slow_mode_enable;
    uint8_t ensm_enable_pin_pulse_mode_enable;
    uint8_t ensm_enable_txnrx_control_enable;
    uint64_t rx_synthesizer_frequency_hz;
    uint64_t tx_synthesizer_frequency_hz;
    uint32_t rx_path_clock_frequencies[6];
    uint32_t tx_path_clock_frequencies[6];
    uint32_t rf_rx_bandwidth_hz;
    uint32_t rf_tx_bandwidth_hz;
    uint32_t rx_rf_port_input_select;
    uint32_t tx_rf_port_input_select;
    int32_t tx_attenuation_mdB;
    uint8_t update_tx_gain_in_alert_enable;
    uint8_t xo_disable_use_ext_refclk_enable;
    uint32_t dcxo_coarse_and_fine_tune[2];
    uint32_t clk_output_mode_select;
    uint8_t gc_rx1_mode;
    uint8_t gc_rx2_mode;
    uint8_t gc_adc_large_overload_thresh;
    uint8_t gc_adc_ovr_sample_size;
    uint8_t gc_adc_small_overload_thresh;
    uint16_t gc_dec_pow_measurement_duration;
    uint8_t gc_dig_gain_enable;
    uint16_t gc_lmt_overload_high_thresh;
    uint16_t gc_lmt_overload_low_thresh;
    uint8_t gc_low_power_thresh;
    uint8_t gc_max_dig_gain;
    uint8_t mgc_dec_gain_step;
    uint8_t mgc_inc_gain_step;
    uint8_t mgc_rx1_ctrl_inp_enable;
    uint8_t mgc_rx2_ctrl_inp_enable;
    uint8_t mgc_split_table_ctrl_inp_gain_mode;
    uint8_t agc_adc_large_overload_exceed_counter;
    uint8_t agc_adc_large_overload_inc_steps;
    uint8_t agc_adc_lmt_small_overload_prevent_gain_inc_enable;
    uint8_t agc_adc_small_overload_exceed_counter;
    uint8_t agc_dig_gain_step_size;
    uint8_t agc_dig_saturation_exceed_counter;
    uint32_t agc_gain_update_interval_us;
    uint8_t agc_immed_gain_change_if_large_adc_overload_enable;
    uint8_t agc_immed_gain_change_if_large_lmt_overload_enable;
    uint8_t agc_inner_thresh_high;
    uint8_t agc_inner_thresh_high_dec_steps;
    uint8_t agc_inner_thresh_low;
    uint8_t agc_inner_thresh_low_inc_steps;
    uint8_t agc_lmt_overload_large_exceed_counter;
    uint8_t agc_lmt_overload_large_inc_steps;
    uint8_t agc_lmt_overload_small_exceed_counter;
    uint8_t agc_outer_thresh_high;
    uint8_t agc_outer_thresh_high_dec_steps;
    uint8_t agc_outer_thresh_low;
    uint8_t agc_outer_thresh_low_inc_steps;
    uint32_t agc_attack_delay_extra_margin_us;
    uint8_t agc_sync_for_gain_counter_enable;
    uint32_t fagc_dec_pow_measuremnt_duration;
    uint32_t fagc_state_wait_time_ns;
    uint8_t fagc_allow_agc_gain_increase;
    uint32_t fagc_lp_thresh_increment_time;
    uint32_t fagc_lp_thresh_increment_steps;
    uint8_t fagc_lock_level_lmt_gain_increase_en;
    uint32_t fagc_lock_level_gain_increase_upper_limit;
    uint32_t fagc_lpf_final_settling_steps;
    uint32_t fagc_lmt_final_settling_steps;
    uint32_t fagc_final_overrange_count;
    uint8_t fagc_gain_increase_after_gain_lock_en;
    uint32_t fagc_gain_index_type_after_exit_rx_mode;
    uint8_t fagc_use_last_lock_level_for_set_gain_en;
    uint8_t fagc_rst_gla_stronger_sig_thresh_exceeded_en;
    uint32_t fagc_optimized_gain_offset;
    uint32_t fagc_rst_gla_stronger_sig_thresh_above_ll;
    uint8_t fagc_rst_gla_engergy_lost_sig_thresh_exceeded_en;
    uint8_t fagc_rst_gla_engergy_lost_goto_optim_gain_en;
    uint32_t fagc_rst_gla_engergy_lost_sig_thresh_below_ll;
    uint32_t fagc_energy_lost_stronger_sig_gain_lock_exit_cnt;
    uint8_t fagc_rst_gla_large_adc_overload_en;
    uint8_t fagc_rst_gla_large_lmt_overload_en;
    uint8_t fagc_rst_gla_en_agc_pulled_high_en;
    uint32_t fagc_rst_gla_if_en_agc_pulled_high_mode;
    uint32_t fagc_power_measurement_duration_in_state5;
    uint32_t rssi_delay;
    uint32_t rssi_duration;
    uint8_t rssi_restart_mode;
    uint8_t rssi_unit_is_rx_samples_enable;
    uint32_t rssi_wait;
    uint32_t aux_adc_decimation;
    uint32_t aux_adc_rate;
    uint8_t aux_dac_manual_mode_enable;
    uint32_t aux_dac1_default_value_mV;
    uint8_t aux_dac1_active_in_rx_enable;
    uint8_t aux_dac1_active_in_tx_enable;
    uint8_t aux_dac1_active_in_alert_enable;
    uint32_t aux_dac1_rx_delay_us;
    uint32_t aux_dac1_tx_delay_us;
    uint32_t aux_dac2_default_value_mV;
    uint8_t aux_dac2_active_in_rx_enable;
    uint8_t aux_dac2_active_in_tx_enable;
    uint8_t aux_dac2_active_in_alert_enable;
    uint32_t aux_dac2_rx_delay_us;
    uint32_t aux_dac2_tx_delay_us;
    uint32_t temp_sense_decimation;
    uint16_t temp_sense_measurement_interval_ms;
    int8_t temp_sense_offset_signed;
    uint8_t temp_sense_periodic_measurement_enable;
    uint8_t ctrl_outs_enable_mask;
    uint8_t ctrl_outs_index;
    uint32_t elna_settling_delay_ns;
    uint32_t elna_gain_mdB;
    uint32_t elna_bypass_loss_mdB;
    uint8_t elna_rx1_gpo0_control_enable;
    uint8_t elna_rx2_gpo1_control_enable;
    uint8_t elna_gaintable_all_index_enable;
    uint8_t digital_interface_tune_skip_mode;
    uint8_t digital_interface_tune_fir_disable;
    uint8_t pp_tx_swap_enable;
    uint8_t pp_rx_swap_enable;
    uint8_t tx_channel_swap_enable;
    uint8_t rx_channel_swap_enable;
    uint8_t rx_frame_pulse_mode_enable;
    uint8_t two_t_two_r_timing_enable;
    uint8_t invert_data_bus_enable;
    uint8_t invert_data_clk_enable;
    uint8_t fdd_alt_word_order_enable;
    uint8_t invert_rx_frame_enable;
    uint8_t fdd_rx_rate_2tx_enable;
    uint8_t swap_ports_enable;
    uint8_t single_data_rate_enable;
    uint8_t lvds_mode_enable;
    uint8_t half_duplex_mode_enable;
    uint8_t single_port_mode_enable;
    uint8_t full_port_enable;
    uint8_t full_duplex_swap_bits_enable;
    uint32_t delay_rx_data;
    uint32_t rx_data_clock_delay;
    uint32_t rx_data_delay;
    uint32_t tx_fb_clock_delay;
    uint32_t tx_data_delay;
    uint32_t lvds_bias_mV;
    uint8_t lvds_rx_onchip_termination_enable;
    uint8_t rx1rx2_phase_inversion_en;
    uint8_t lvds_invert1_control;
    uint8_t lvds_invert2_control;
    uint8_t clk_out_drive;
    uint8_t dataclk_drive;
    uint8_t data_port_drive;
    uint8_t clk_out_slew;
    uint8_t dataclk_slew;
    uint8_t data_port_slew;
    uint8_t gpo0_inactive_state_high_enable;
    uint8_t gpo1_inactive_state_high_enable;
    uint8_t gpo2_inactive_state_high_enable;
    uint8_t gpo3_inactive_state_high_enable;
    uint8_t gpo0_slave_rx_enable;
    uint8_t gpo0_slave_tx_enable;
    uint8_t gpo1_slave_rx_enable;
    uint8_t gpo1_slave_tx_enable;
    uint8_t gpo2_slave_rx_enable;
    uint8_t gpo2_slave_tx_enable;
    uint8_t gpo3_slave_rx_enable;
    uint8_t gpo3_slave_tx_enable;
    uint8_t gpo0_rx_delay_us;
    uint8_t gpo0_tx_delay_us;
    uint8_t gpo1_rx_delay_us;
    uint8_t gpo1_tx_delay_us;
    uint8_t gpo2_rx_delay_us;
    uint8_t gpo2_tx_delay_us;
    uint8_t gpo3_rx_delay_us;
    uint8_t gpo3_tx_delay_us;
    uint32_t low_high_gain_threshold_mdB;
    uint32_t low_gain_dB;
    uint32_t high_gain_dB;
    uint8_t tx_mon_track_en;
    uint8_t one_shot_mode_en;
    uint32_t tx_mon_delay;
    uint32_t tx_mon_duration;
    uint32_t tx1_mon_front_end_gain;
    uint32_t tx2_mon_front_end_gain;
    uint32_t tx1_mon_lo_cm;
    uint32_t tx2_mon_lo_cm;
    int32_t gpio_resetb;
    int32_t gpio_sync;
    int32_t gpio_cal_sw1;
    int32_t gpio_cal_sw2;
    uint32_t (*ad9361_rfpll_ext_recalc_rate)(struct refclk_scale *clk_priv);
    int32_t (*ad9361_rfpll_ext_round_rate)(struct refclk_scale *clk_priv,
                                           uint32_t rate);
    int32_t (*ad9361_rfpll_ext_set_rate)(struct refclk_scale *clk_priv,
                                         uint32_t rate);
} AD9361_InitParam;

typedef struct {
    uint32_t rx;     /* 1, 2, 3(both) */
    int32_t rx_gain; /* -12, -6, 0, 6 */
    uint32_t rx_dec; /* 1, 2, 4 */
    int16_t rx_coef[128];
    uint8_t rx_coef_size;
    uint32_t rx_path_clks[6];
    uint32_t rx_bandwidth;
} AD9361_RXFIRConfig;

typedef struct {
    uint32_t tx;     /* 1, 2, 3(both) */
    int32_t tx_gain; /* -6, 0 */
    uint32_t tx_int; /* 1, 2, 4 */
    int16_t tx_coef[128];
    uint8_t tx_coef_size;
    uint32_t tx_path_clks[6];
    uint32_t tx_bandwidth;
} AD9361_TXFIRConfig;

#define AD936X_A_BALANCED 0
#define AD936X_B_BALANCED 1
#define AD936X_C_BALANCED 2
#define AD936X_A_N 3
#define AD936X_A_P 4
#define AD936X_B_N 5
#define AD936X_B_P 6
#define AD936X_C_N 7
#define AD936X_C_P 8
#define AD936X_TX_MON1 9
#define AD936X_TX_MON2 10
#define AD936X_TX_MON1_2 11

#define AD936X_TXA 0
#define AD936X_TXB 1

int32_t ad9361_init(struct ad9361_rf_phy **ad9361_phy,
                    AD9361_InitParam *init_param,
                    void *userdata);
int32_t ad9361_deinit(struct ad9361_rf_phy *phy);
int32_t ad9361_set_rx_rf_gain(struct ad9361_rf_phy *phy,
                              uint8_t ch,
                              int32_t gain_db);
int32_t ad9361_set_rx_rf_bandwidth(struct ad9361_rf_phy *phy,
                                   uint32_t bandwidth_hz);
int32_t ad9361_get_rx_rf_bandwidth(struct ad9361_rf_phy *phy,
                                   uint32_t *bandwidth_hz);
int32_t ad9361_set_rx_sampling_freq(struct ad9361_rf_phy *phy,
                                    uint32_t sampling_freq_hz);
int32_t ad9361_get_rx_sampling_freq(struct ad9361_rf_phy *phy,
                                    uint32_t *sampling_freq_hz);
int32_t ad9361_set_rx_lo_freq(struct ad9361_rf_phy *phy, uint64_t lo_freq_hz);
int32_t ad9361_get_rx_lo_freq(struct ad9361_rf_phy *phy, uint64_t *lo_freq_hz);
int32_t ad9361_get_rx_rssi(struct ad9361_rf_phy *phy,
                           uint8_t ch,
                           struct rf_rssi *rssi);
int32_t ad9361_set_rx_gain_control_mode(struct ad9361_rf_phy *phy,
                                        uint8_t ch,
                                        uint8_t gc_mode);
int32_t ad9361_get_rx_gain_control_mode(struct ad9361_rf_phy *phy,
                                        uint8_t ch,
                                        uint8_t *gc_mode);
int32_t ad9361_set_rx_fir_config(struct ad9361_rf_phy *phy,
                                 AD9361_RXFIRConfig fir_cfg);
int32_t ad9361_set_rx_fir_en_dis(struct ad9361_rf_phy *phy, uint8_t en_dis);
int32_t ad9361_set_rx_rf_port_input(struct ad9361_rf_phy *phy, uint32_t mode);
int32_t ad9361_get_rx_rf_port_input(struct ad9361_rf_phy *phy, uint32_t *mode);
int32_t ad9361_set_tx_attenuation(struct ad9361_rf_phy *phy,
                                  uint8_t ch,
                                  uint32_t attenuation_mdb);
int32_t ad9361_get_tx_attenuation(struct ad9361_rf_phy *phy,
                                  uint8_t ch,
                                  uint32_t *attenuation_mdb);
int32_t ad9361_set_tx_rf_bandwidth(struct ad9361_rf_phy *phy,
                                   uint32_t bandwidth_hz);
int32_t ad9361_get_tx_rf_bandwidth(struct ad9361_rf_phy *phy,
                                   uint32_t *bandwidth_hz);
int32_t ad9361_set_tx_sampling_freq(struct ad9361_rf_phy *phy,
                                    uint32_t sampling_freq_hz);
int32_t ad9361_get_tx_sampling_freq(struct ad9361_rf_phy *phy,
                                    uint32_t *sampling_freq_hz);
int32_t ad9361_set_tx_lo_freq(struct ad9361_rf_phy *phy, uint64_t lo_freq_hz);
int32_t ad9361_get_tx_lo_freq(struct ad9361_rf_phy *phy, uint64_t *lo_freq_hz);
int32_t ad9361_set_tx_fir_config(struct ad9361_rf_phy *phy,
                                 AD9361_TXFIRConfig fir_cfg);
int32_t ad9361_set_tx_fir_en_dis(struct ad9361_rf_phy *phy, uint8_t en_dis);
int32_t ad9361_get_tx_rssi(struct ad9361_rf_phy *phy,
                           uint8_t ch,
                           uint32_t *rssi_db_x_1000);
int32_t ad9361_set_tx_rf_port_output(struct ad9361_rf_phy *phy, uint32_t mode);
int32_t ad9361_get_tx_rf_port_output(struct ad9361_rf_phy *phy, uint32_t *mode);
int32_t ad9361_get_temp(struct ad9361_rf_phy *phy);
int32_t ad9361_rx_fastlock_store(struct ad9361_rf_phy *phy, uint32_t profile);
int32_t ad9361_rx_fastlock_save(struct ad9361_rf_phy *phy,
                                uint32_t profile,
                                uint8_t *values);
int32_t ad9361_tx_fastlock_store(struct ad9361_rf_phy *phy, uint32_t profile);
int32_t ad9361_tx_fastlock_save(struct ad9361_rf_phy *phy,
                                uint32_t profile,
                                uint8_t *values);
int32_t ad9361_set_no_ch_mode(struct ad9361_rf_phy *phy, uint8_t no_ch_mode);

#endif  // AD936X_H_
