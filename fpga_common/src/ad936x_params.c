#ifdef BLADERF_NIOS_BUILD
#include "devices.h"
#endif  // BLADERF_NIOS_BUILD

/* Avoid building this in low-memory situations */
#if !defined(BLADERF_NIOS_BUILD) || defined(BLADERF_NIOS_LIBAD936X)

#include "ad9361_api.h"
#include "platform.h"
#include "no_os_gpio.h"
#include "no_os_spi.h"

#ifndef AXI_ADC_NOT_PRESENT
#include "axi_adc_core.h"
#include "axi_dac_core.h"
#endif

/* bladeRF2 ADC initialization structure */
struct axi_adc_init bladerf2_rx_adc_init = {
    .name = "bladeRF AXI ADC",
    .base = BLADERF_RX_ADC_BASEADDR,
    .num_channels = 4, /* 2 RX channels, I and Q each */
#if !defined(BLADERF_NIOS_BUILD) && !defined(BLADERF_NIOS_LIBAD936X)
    .userdata = NULL
#endif
};

/* bladeRF2 DAC initialization structure */
struct axi_dac_init bladerf2_tx_dac_init = {
    .name = "bladeRF AXI DAC",
    .base = BLADERF_TX_DAC_BASEADDR,
    .num_channels = 4, /* 2 TX channels, I and Q each */
#if !defined(BLADERF_NIOS_BUILD) && !defined(BLADERF_NIOS_LIBAD936X)
    .userdata = NULL
#endif
};

/**
 * Reference:
 * https://wiki.analog.com/resources/tools-software/linux-drivers/iio-transceiver/ad9361-customization
 *
 * N/A      = not applicable due to other setting; changes may unmask these
 * DEFAULT  = changed during device initialization
 */

// clang-format off
AD9361_InitParam bladerf2_rfic_init_params = {
    .dev_sel = ID_AD9361,
    .reference_clk_rate = 38400000UL,
    .two_rx_two_tx_mode_enable = 1,
    .one_rx_one_tx_mode_use_rx_num = 1,
    .one_rx_one_tx_mode_use_tx_num = 1,
    .frequency_division_duplex_mode_enable = 1,
    .frequency_division_duplex_independent_mode_enable = 0,
    .tdd_use_dual_synth_mode_enable = 0,
    .tdd_skip_vco_cal_enable = 0,
    .tx_fastlock_delay_ns = 0,
    .rx_fastlock_delay_ns = 0,
    .rx_fastlock_pincontrol_enable = 0,
    .tx_fastlock_pincontrol_enable = 0,
    .external_rx_lo_enable = 0,
    .external_tx_lo_enable = 0,
    .dc_offset_tracking_update_event_mask = 5,
    .dc_offset_attenuation_high_range = 6,
    .dc_offset_attenuation_low_range = 5,
    .dc_offset_count_high_range = 0x28,
    .dc_offset_count_low_range = 0x32,
    .split_gain_table_mode_enable = 0,
    .trx_synthesizer_target_fref_overwrite_hz = 80008000UL,
    .qec_tracking_slow_mode_enable = 0,

    // ENSM Control
    .ensm_enable_pin_pulse_mode_enable = 0,
    .ensm_enable_txnrx_control_enable = 0,

    // LO Control
    .rx_synthesizer_frequency_hz = 2400000000ULL,
    .tx_synthesizer_frequency_hz = 2400000000ULL,
    .tx_lo_powerdown_managed_enable = 1,

    // Rate & BW Control
    .rx_path_clock_frequencies = {983040000, 245760000, 122880000, 61440000, 30720000, 30720000},
    .tx_path_clock_frequencies = {983040000, 122880000, 122880000, 61440000, 30720000, 30720000},
    .rf_rx_bandwidth_hz = 18000000,
    .rf_tx_bandwidth_hz = 18000000,

    // RF Port Control
    .rx_rf_port_input_select = 0,
    .tx_rf_port_input_select = 0,

    // TX Attenuation Control
    .tx_attenuation_mdB = 10000,
    .update_tx_gain_in_alert_enable = 0,

    // Reference Clock Control
    .xo_disable_use_ext_refclk_enable = 1,
    .dcxo_coarse_and_fine_tune = {3, 5920},
    .clk_output_mode_select = 0,

    // Gain Control
    .gc_rx1_mode = 2,
    .gc_rx2_mode = 2,
    .gc_adc_large_overload_thresh = 58,
    .gc_adc_ovr_sample_size = 4,
    .gc_adc_small_overload_thresh = 47,
    .gc_dec_pow_measurement_duration = 8192,
    .gc_dig_gain_enable = 0,
    .gc_lmt_overload_high_thresh = 800,
    .gc_lmt_overload_low_thresh = 704,
    .gc_low_power_thresh = 24,
    .gc_max_dig_gain = 15,
    .gc_use_rx_fir_out_for_dec_pwr_meas_enable = 0,

    // Gain MGC Control
    .mgc_dec_gain_step = 2,
    .mgc_inc_gain_step = 2,
    .mgc_rx1_ctrl_inp_enable = 0,
    .mgc_rx2_ctrl_inp_enable = 0,
    .mgc_split_table_ctrl_inp_gain_mode = 0,

    // Gain AGC Control
    .agc_adc_large_overload_exceed_counter = 10,
    .agc_adc_large_overload_inc_steps = 2,
    .agc_adc_lmt_small_overload_prevent_gain_inc_enable = 0,
    .agc_adc_small_overload_exceed_counter = 10,
    .agc_dig_gain_step_size = 4,
    .agc_dig_saturation_exceed_counter = 3,
    .agc_gain_update_interval_us = 1000,
    .agc_immed_gain_change_if_large_adc_overload_enable = 0,
    .agc_immed_gain_change_if_large_lmt_overload_enable = 0,
    .agc_inner_thresh_high = 10,
    .agc_inner_thresh_high_dec_steps = 1,
    .agc_inner_thresh_low = 12,
    .agc_inner_thresh_low_inc_steps = 1,
    .agc_lmt_overload_large_exceed_counter = 10,
    .agc_lmt_overload_large_inc_steps = 2,
    .agc_lmt_overload_small_exceed_counter = 10,
    .agc_outer_thresh_high = 5,
    .agc_outer_thresh_high_dec_steps = 2,
    .agc_outer_thresh_low = 18,
    .agc_outer_thresh_low_inc_steps = 2,
    .agc_attack_delay_extra_margin_us = 1,
    .agc_sync_for_gain_counter_enable = 0,

    // Fast AGC
    .fagc_dec_pow_measuremnt_duration = 64,
    .fagc_state_wait_time_ns = 260,

    // Fast AGC - Low Power
    .fagc_allow_agc_gain_increase = 0,
    .fagc_lp_thresh_increment_time = 5,
    .fagc_lp_thresh_increment_steps = 1,

    // Fast AGC - Lock Level
    .fagc_lock_level_lmt_gain_increase_en = 1,
    .fagc_lock_level_gain_increase_upper_limit = 5,

    // Fast AGC - Peak Detectors and Final Settling
    .fagc_lpf_final_settling_steps = 1,
    .fagc_lmt_final_settling_steps = 1,
    .fagc_final_overrange_count = 3,

    // Fast AGC - Final Power Test
    .fagc_gain_increase_after_gain_lock_en = 0,

    // Fast AGC - Unlocking the Gain
    .fagc_gain_index_type_after_exit_rx_mode = 0,
    .fagc_use_last_lock_level_for_set_gain_en = 1,
    .fagc_rst_gla_stronger_sig_thresh_exceeded_en = 1,
    .fagc_optimized_gain_offset = 5,
    .fagc_rst_gla_stronger_sig_thresh_above_ll = 10,
    .fagc_rst_gla_engergy_lost_sig_thresh_exceeded_en = 1,
    .fagc_rst_gla_engergy_lost_goto_optim_gain_en = 1,
    .fagc_rst_gla_engergy_lost_sig_thresh_below_ll = 10,
    .fagc_energy_lost_stronger_sig_gain_lock_exit_cnt = 8,
    .fagc_rst_gla_large_adc_overload_en = 1,
    .fagc_rst_gla_large_lmt_overload_en = 1,
    .fagc_rst_gla_en_agc_pulled_high_en = 0,
    .fagc_rst_gla_if_en_agc_pulled_high_mode = 0,
    .fagc_power_measurement_duration_in_state5 = 64,

    // RSSI Control
    .rssi_delay = 1,
    .rssi_duration = 1000,
    .rssi_restart_mode = 3,
    .rssi_unit_is_rx_samples_enable = 0,

    // Aux ADC Control
    .aux_adc_decimation = 256,
    .aux_adc_rate = 40000000,

    // AuxDAC Control
    .aux_dac_manual_mode_enable = 1,
    .aux_dac1_default_value_mV = 0,
    .aux_dac1_active_in_rx_enable = 0,
    .aux_dac1_active_in_tx_enable = 0,
    .aux_dac1_active_in_alert_enable = 0,
    .aux_dac2_default_value_mV = 0,
    .aux_dac2_active_in_rx_enable = 0,
    .aux_dac2_active_in_tx_enable = 0,
    .aux_dac2_active_in_alert_enable = 0,

    // Temperature Sensor Control
    .temp_sense_decimation = 256,
    .temp_sense_measurement_interval_ms = 1000,
    .temp_sense_offset_signed = 0xCE,
    .temp_sense_periodic_measurement_enable = 1,

    // Control Out Setup
    .ctrl_outs_enable_mask = 0xFF,
    .ctrl_outs_index = 0x00,

    // External LNA Control
    .elna_settling_delay_ns = 0,
    .elna_gain_mdB = 0,
    .elna_bypass_loss_mdB = 0,
    .elna_rx1_gpo0_control_enable = 0,
    .elna_rx2_gpo1_control_enable = 0,
    .elna_gaintable_all_index_enable = 0,

    // Digital Interface Control
#ifdef ENABLE_AD9361_DIGITAL_INTERFACE_TIMING_VERIFICATION
    .digital_interface_tune_skip_mode = 0,
#else
    .digital_interface_tune_skip_mode = 2,
#endif
    .digital_interface_tune_fir_disable = 0,  /* Enable FIR during tuning */
    .pp_tx_swap_enable = 1,
    .pp_rx_swap_enable = 1,
    .tx_channel_swap_enable = 0,  /* Don't swap channels */
    .rx_channel_swap_enable = 0,  /* Don't swap channels */
    .rx_frame_pulse_mode_enable = 1,  /* Enable RX frame pulse mode */
    .two_t_two_r_timing_enable = 0,
    .invert_data_bus_enable = 0,
    .invert_data_clk_enable = 0,
    .fdd_alt_word_order_enable = 0,
    .invert_rx_frame_enable = 0,
    .fdd_rx_rate_2tx_enable = 0,
    .swap_ports_enable = 0,
    .single_data_rate_enable = 0,
    .lvds_mode_enable = 1,
    .half_duplex_mode_enable = 0,
    .single_port_mode_enable = 0,
    .full_port_enable = 0,
    .full_duplex_swap_bits_enable = 0,
    .delay_rx_data = 0,
    .rx_data_clock_delay = 5,
    .rx_data_delay = 0,
    .tx_fb_clock_delay = 0,
    .tx_data_delay = 5,
    .lvds_bias_mV = 300,            // LVDS driver bias 300 mV
    .lvds_rx_onchip_termination_enable = 1,              // Enable LVDS on-chip termination
    .rx1rx2_phase_inversion_en = 1,              // RX1 and RX2 are not phase-aligned
    .lvds_invert1_control = 0xFF,           // Default signal inversion mappings
    .lvds_invert2_control = 0x0F,           // Default signal inversion mappings

    // GPO Control
    .gpo0_inactive_state_high_enable = 0,
    .gpo1_inactive_state_high_enable = 0,
    .gpo2_inactive_state_high_enable = 0,
    .gpo3_inactive_state_high_enable = 0,
    .gpo0_slave_rx_enable = 0,
    .gpo0_slave_tx_enable = 0,
    .gpo1_slave_rx_enable = 0,
    .gpo1_slave_tx_enable = 0,
    .gpo2_slave_rx_enable = 0,
    .gpo2_slave_tx_enable = 0,
    .gpo3_slave_rx_enable = 0,
    .gpo3_slave_tx_enable = 0,
    .gpo0_rx_delay_us = 0,
    .gpo0_tx_delay_us = 0,
    .gpo1_rx_delay_us = 0,
    .gpo1_tx_delay_us = 0,
    .gpo2_rx_delay_us = 0,
    .gpo2_tx_delay_us = 0,
    .gpo3_rx_delay_us = 0,
    .gpo3_tx_delay_us = 0,

    // TX Monitor Control
    .low_high_gain_threshold_mdB = 37000,
    .low_gain_dB = 0,
    .high_gain_dB = 24,
    .tx_mon_track_en = 0,
    .one_shot_mode_en = 0,
    .tx_mon_delay = 511,
    .tx_mon_duration = 8192,
    .tx1_mon_front_end_gain = 2,
    .tx2_mon_front_end_gain = 2,
    .tx1_mon_lo_cm = 48,
    .tx2_mon_lo_cm = 48,

    // GPIO configuration
    .gpio_resetb = {.number = RFFE_CONTROL_RESET_N, .port = 0, .pull = NO_OS_PULL_NONE, .platform_ops = NULL, .extra = NULL},
    .gpio_sync = {.number = -1, .port = 0, .pull = NO_OS_PULL_NONE, .platform_ops = NULL, .extra = NULL},
    .gpio_cal_sw1 = {.number = -1, .port = 0, .pull = NO_OS_PULL_NONE, .platform_ops = NULL, .extra = NULL},
    .gpio_cal_sw2 = {.number = -1, .port = 0, .pull = NO_OS_PULL_NONE, .platform_ops = NULL, .extra = NULL},

    // SPI configuration
    .spi_param = {.device_id = 0, .max_speed_hz = 1000000, .mode = NO_OS_SPI_MODE_0, .chip_select = 0, .platform_ops = NULL, .extra = NULL},

    // External LO clocks
    .ad9361_rfpll_ext_recalc_rate = NULL,
    .ad9361_rfpll_ext_round_rate = NULL,
    .ad9361_rfpll_ext_set_rate = NULL,
#ifndef AXI_ADC_NOT_PRESENT
    .rx_adc_init = &bladerf2_rx_adc_init,
    .tx_dac_init = &bladerf2_tx_dac_init
#else
    .rx_adc_init = NULL,
    .tx_dac_init = NULL
#endif
};
// clang-format on


// clang-format off
AD9361_InitParam bladerf2_rfic_init_params_fastagc_burst = {
    .dev_sel = ID_AD9361,
    .reference_clk_rate = 38400000UL,
    .two_rx_two_tx_mode_enable = 1,
    .one_rx_one_tx_mode_use_rx_num = 1,
    .one_rx_one_tx_mode_use_tx_num = 1,
    .frequency_division_duplex_mode_enable = 1,
    .frequency_division_duplex_independent_mode_enable = 0,
    .tdd_use_dual_synth_mode_enable = 0,
    .tdd_skip_vco_cal_enable = 0,
    .tx_fastlock_delay_ns = 0,
    .rx_fastlock_delay_ns = 0,
    .rx_fastlock_pincontrol_enable = 0,
    .tx_fastlock_pincontrol_enable = 0,
    .external_rx_lo_enable = 0,
    .external_tx_lo_enable = 0,
    .dc_offset_tracking_update_event_mask = 5,
    .dc_offset_attenuation_high_range = 6,
    .dc_offset_attenuation_low_range = 5,
    .dc_offset_count_high_range = 0x28,
    .dc_offset_count_low_range = 0x32,
    .split_gain_table_mode_enable = 0,
    .trx_synthesizer_target_fref_overwrite_hz = 80008000UL,
    .qec_tracking_slow_mode_enable = 0,
    .ensm_enable_pin_pulse_mode_enable = 0,
    .ensm_enable_txnrx_control_enable = 0,
    .rx_synthesizer_frequency_hz = 2400000000ULL,
    .tx_synthesizer_frequency_hz = 2400000000ULL,
    .tx_lo_powerdown_managed_enable = 1,
    .rx_path_clock_frequencies = {983040000, 245760000, 122880000, 61440000, 30720000, 30720000},
    .tx_path_clock_frequencies = {983040000, 122880000, 122880000, 61440000, 30720000, 30720000},
    .rf_rx_bandwidth_hz = 18000000,
    .rf_tx_bandwidth_hz = 18000000,
    .rx_rf_port_input_select = 0,
    .tx_rf_port_input_select = 0,
    .tx_attenuation_mdB = 10000,
    .update_tx_gain_in_alert_enable = 0,
    .xo_disable_use_ext_refclk_enable = 1,
    .dcxo_coarse_and_fine_tune = {3, 5920},
    .clk_output_mode_select = 0,
    .gc_rx1_mode = 0,  // Fast AGC mode
    .gc_rx2_mode = 0,  // Fast AGC mode
    .gc_adc_large_overload_thresh = 58,
    .gc_adc_ovr_sample_size = 4,
    .gc_adc_small_overload_thresh = 47,
    .gc_dec_pow_measurement_duration = 2,
    .gc_dig_gain_enable = 0,
    .gc_lmt_overload_high_thresh = 480,
    .gc_lmt_overload_low_thresh = 400,
    .gc_low_power_thresh = 40,
    .gc_max_dig_gain = 15,
    .gc_use_rx_fir_out_for_dec_pwr_meas_enable = 0,
    .mgc_dec_gain_step = 2,
    .mgc_inc_gain_step = 2,
    .mgc_rx1_ctrl_inp_enable = 0,
    .mgc_rx2_ctrl_inp_enable = 0,
    .mgc_split_table_ctrl_inp_gain_mode = 0,
    .agc_adc_large_overload_exceed_counter = 10,
    .agc_adc_large_overload_inc_steps = 2,
    .agc_adc_lmt_small_overload_prevent_gain_inc_enable = 0,
    .agc_adc_small_overload_exceed_counter = 10,
    .agc_dig_gain_step_size = 4,
    .agc_dig_saturation_exceed_counter = 3,
    .agc_gain_update_interval_us = 1,
    .agc_immed_gain_change_if_large_adc_overload_enable = 0,
    .agc_immed_gain_change_if_large_lmt_overload_enable = 0,
    .agc_inner_thresh_high = 10,
    .agc_inner_thresh_high_dec_steps = 1,
    .agc_inner_thresh_low = 12,
    .agc_inner_thresh_low_inc_steps = 1,
    .agc_lmt_overload_large_exceed_counter = 10,
    .agc_lmt_overload_large_inc_steps = 2,
    .agc_lmt_overload_small_exceed_counter = 10,
    .agc_outer_thresh_high = 5,
    .agc_outer_thresh_high_dec_steps = 2,
    .agc_outer_thresh_low = 18,
    .agc_outer_thresh_low_inc_steps = 2,
    .agc_attack_delay_extra_margin_us = 1,
    .agc_sync_for_gain_counter_enable = 0,
    .fagc_dec_pow_measuremnt_duration = 16,
    .fagc_state_wait_time_ns = 260,
    .fagc_allow_agc_gain_increase = 0,
    .fagc_lp_thresh_increment_time = 5,
    .fagc_lp_thresh_increment_steps = 1,
    .fagc_lock_level_lmt_gain_increase_en = 1,
    .fagc_lock_level_gain_increase_upper_limit = 63,
    .fagc_lpf_final_settling_steps = 1,
    .fagc_lmt_final_settling_steps = 1,
    .fagc_final_overrange_count = 3,
    .fagc_gain_increase_after_gain_lock_en = 0,
    .fagc_gain_index_type_after_exit_rx_mode = 0,
    .fagc_use_last_lock_level_for_set_gain_en = 1,
    .fagc_rst_gla_stronger_sig_thresh_exceeded_en = 1,
    .fagc_optimized_gain_offset = 5,
    .fagc_rst_gla_stronger_sig_thresh_above_ll = 10,
    .fagc_rst_gla_engergy_lost_sig_thresh_exceeded_en = 1,
    .fagc_rst_gla_engergy_lost_goto_optim_gain_en = 0,
    .fagc_rst_gla_engergy_lost_sig_thresh_below_ll = 10,
    .fagc_energy_lost_stronger_sig_gain_lock_exit_cnt = 3,
    .fagc_rst_gla_large_adc_overload_en = 1,
    .fagc_rst_gla_large_lmt_overload_en = 1,
    .fagc_rst_gla_en_agc_pulled_high_en = 0,
    .fagc_rst_gla_if_en_agc_pulled_high_mode = 0,
    .fagc_power_measurement_duration_in_state5 = 64,
    .rssi_delay = 1,
    .rssi_duration = 1000,
    .rssi_restart_mode = 3,
    .rssi_unit_is_rx_samples_enable = 0,
    .aux_adc_decimation = 256,
    .aux_adc_rate = 40000000,
    .aux_dac_manual_mode_enable = 1,
    .aux_dac1_default_value_mV = 0,
    .aux_dac1_active_in_rx_enable = 0,
    .aux_dac1_active_in_tx_enable = 0,
    .aux_dac1_active_in_alert_enable = 0,
    .aux_dac2_default_value_mV = 0,
    .aux_dac2_active_in_rx_enable = 0,
    .aux_dac2_active_in_tx_enable = 0,
    .aux_dac2_active_in_alert_enable = 0,
    .temp_sense_decimation = 256,
    .temp_sense_measurement_interval_ms = 1000,
    .temp_sense_offset_signed = 0xCE,
    .temp_sense_periodic_measurement_enable = 1,
    .ctrl_outs_enable_mask = 0xFF,
    .ctrl_outs_index = 7,
    .elna_settling_delay_ns = 0,
    .elna_gain_mdB = 0,
    .elna_bypass_loss_mdB = 0,
    .elna_rx1_gpo0_control_enable = 0,
    .elna_rx2_gpo1_control_enable = 0,
    .elna_gaintable_all_index_enable = 0,
#ifdef ENABLE_AD9361_DIGITAL_INTERFACE_TIMING_VERIFICATION
    .digital_interface_tune_skip_mode = 0,
#else
    .digital_interface_tune_skip_mode = 2,
#endif
    .digital_interface_tune_fir_disable = 0,  /* Enable FIR during tuning */
    .pp_tx_swap_enable = 1,
    .pp_rx_swap_enable = 1,
    .tx_channel_swap_enable = 0,  /* Don't swap channels */
    .rx_channel_swap_enable = 0,  /* Don't swap channels */
    .rx_frame_pulse_mode_enable = 1,  /* Enable RX frame pulse mode */
    .two_t_two_r_timing_enable = 0,
    .invert_data_bus_enable = 0,
    .invert_data_clk_enable = 0,
    .fdd_alt_word_order_enable = 0,
    .invert_rx_frame_enable = 0,
    .fdd_rx_rate_2tx_enable = 0,
    .swap_ports_enable = 0,
    .single_data_rate_enable = 0,
    .lvds_mode_enable = 1,
    .half_duplex_mode_enable = 0,
    .single_port_mode_enable = 0,
    .full_port_enable = 0,
    .full_duplex_swap_bits_enable = 0,
    .delay_rx_data = 0,
    .rx_data_clock_delay = 5,
    .rx_data_delay = 0,
    .tx_fb_clock_delay = 0,
    .tx_data_delay = 5,
    .lvds_bias_mV = 300,            // LVDS driver bias 300 mV
    .lvds_rx_onchip_termination_enable = 1,              // Enable LVDS on-chip termination
    .rx1rx2_phase_inversion_en = 1,              // RX1 and RX2 are not phase-aligned
    .lvds_invert1_control = 0xFF,           // Default signal inversion mappings
    .lvds_invert2_control = 0x0F,           // Default signal inversion mappings
    .gpo0_inactive_state_high_enable = 0,
    .gpo1_inactive_state_high_enable = 0,
    .gpo2_inactive_state_high_enable = 0,
    .gpo3_inactive_state_high_enable = 0,
    .gpo0_slave_rx_enable = 0,
    .gpo0_slave_tx_enable = 0,
    .gpo1_slave_rx_enable = 0,
    .gpo1_slave_tx_enable = 0,
    .gpo2_slave_rx_enable = 0,
    .gpo2_slave_tx_enable = 0,
    .gpo3_slave_rx_enable = 0,
    .gpo3_slave_tx_enable = 0,
    .gpo0_rx_delay_us = 0,
    .gpo0_tx_delay_us = 0,
    .gpo1_rx_delay_us = 0,
    .gpo1_tx_delay_us = 0,
    .gpo2_rx_delay_us = 0,
    .gpo2_tx_delay_us = 0,
    .gpo3_rx_delay_us = 0,
    .gpo3_tx_delay_us = 0,
    .low_high_gain_threshold_mdB = 37000,
    .low_gain_dB = 0,
    .high_gain_dB = 24,
    .tx_mon_track_en = 0,
    .one_shot_mode_en = 0,
    .tx_mon_delay = 511,
    .tx_mon_duration = 8192,
    .tx1_mon_front_end_gain = 2,
    .tx2_mon_front_end_gain = 2,
    .tx1_mon_lo_cm = 48,
    .tx2_mon_lo_cm = 48,
    .gpio_resetb = {.number = RFFE_CONTROL_RESET_N, .port = 0, .pull = NO_OS_PULL_NONE, .platform_ops = NULL, .extra = NULL},
    .gpio_sync = {.number = -1, .port = 0, .pull = NO_OS_PULL_NONE, .platform_ops = NULL, .extra = NULL},
    .gpio_cal_sw1 = {.number = -1, .port = 0, .pull = NO_OS_PULL_NONE, .platform_ops = NULL, .extra = NULL},
    .gpio_cal_sw2 = {.number = -1, .port = 0, .pull = NO_OS_PULL_NONE, .platform_ops = NULL, .extra = NULL},
    .spi_param = {.device_id = 0, .max_speed_hz = 1000000, .mode = NO_OS_SPI_MODE_0, .chip_select = 0, .platform_ops = NULL, .extra = NULL},
    .ad9361_rfpll_ext_recalc_rate = NULL,
    .ad9361_rfpll_ext_round_rate = NULL,
    .ad9361_rfpll_ext_set_rate = NULL,
#ifndef AXI_ADC_NOT_PRESENT
    .rx_adc_init = &bladerf2_rx_adc_init,
    .tx_dac_init = &bladerf2_tx_dac_init
#else
    .rx_adc_init = NULL,
    .tx_dac_init = NULL
#endif
};
// clang-format on

/**
 * AD9361 FIR Filters
 *
 * The AD9361 RFIC provides programmable FIR filters on both the RX and TX
 * paths.
 *
 * On TX, the signal path is:
 *
 * DIGITAL:
 *   [ Programmable TX FIR ] -> [ HB1 ] -> [ HB2 ] -> [ HB3/INT3 ] -> [ DAC ]
 * ANALOG:
 *   [ DAC ] -> [ BB LPF ] -> [ Secondary LPF ] -> ...
 *
 * The Programmable TX FIR is a programmable polyphase FIR filter, which can
 * interpolate by 1, 2, 4, or be bypassed. Taps are stored in 16-bit
 * twos-complement. If interpolating by 1, there is a limit of 64 taps;
 * otherwise, the limit is 128 taps.
 *
 * HB1 and HB2 are fixed-coefficient half-band interpolating filters, and can
 * interpolate by 2 or be bypassed. HB3/INT3 is a fixed-coefficient
 * interpolating filter, and can interpolate by 2 or 3, or be bypassed.
 *
 * BB LPF is a third-order Butterworth LPF, and the Secondary LPF is a
 * single-pole low-pass filter. Both have programmable corner frequencies.
 *
 * On RX, the signal path is:
 *
 * ANALOG:
 *   ... -> [ TIA LPF ] -> [ BB LPF ] -> [ ADC ]
 * DIGITAL:
 *   [ ADC ] -> [ HB3/DEC3 ] -> [ HB2 ] -> [ HB1 ] -> [ Programmable RX FIR ]
 *
 * The TIA LPF is a transimpedance amplifier which applies a single-pole
 * low-pass filter, and the BB LPF is a third-order Butterworth low-pass filter.
 * Both have programmable corner frequencies.
 *
 * HB3/DEC3 is a fixed-coefficient decimating filter, and can decimate by a
 * factor of 2 or 3, or be bypassed. HB2 is a fixed-coefficient half-band
 * decimating filter, and can decimate by a factor of 2 or be bypassed. HB1 is a
 * fixed-coefficient half-band decimating filter, and can also decimate by a
 * factor of 2 or be bypassed.
 *
 * The Programmable RX FIR filter is a programmable polyphase filter, which can
 * decimate by a factor of 1, 2, or 4, or be bypassed. Similar to the TX FIR,
 * taps are stored in 16-bit twos-complement. The maximum number of taps is
 * limited to the ratio of the sample clock to the filter's output rate,
 * multiplied by 16, up to a maximum of 128 taps. There is a fixed +6 dB gain,
 * so the below RX filters are configured for a -6 dB gain to effect a net gain
 * of 0 dB.
 *
 * In practice, the decimation/interpolation settings must match for both the RX
 * and TX FIR filters. If they differ, TX quadrature calibration (and likely
 * other calibrations) will fail.
 *
 *
 * This file specifies four filters:
 *  bladerf2_rfic_rx_fir_config       = decimate by 1 RX FIR
 *  bladerf2_rfic_tx_fir_config       = interpolate by 1 TX FIR
 *  bladerf2_rfic_rx_fir_config_dec2  = decimate by 2 RX FIR
 *  bladerf2_rfic_tx_fir_config_int2  = interpolate by 2 TX FIR
 *
 * The first two (the 1x filters) are the default, and should provide reasonable
 * performance under most circumstances. The other two filters are primarily
 * intended for situations requiring a flatter TX spectrum, particularly when
 * the ratio of sample rate to signal bandwidth is low.
 */

AD9361_RXFIRConfig bladerf2_rfic_rx_fir_config = {
    3,   // rx (RX1 = 1, RX2 = 2, both = 3)
    -6,  // rx_gain (-12, -6, 0, or 6 dB)
    1,   // rx_dec (decimate by 1, 2, or 4)

    /**
     * RX FIR Filter
     * Built using https://github.com/analogdevicesinc/libad9361-iio
     * Branch: filter_generation
     * Commit: f749cef974f687f696226455dc7684277886cf3b
     *
     * This filter is intended to improve the flatness of the RX spectrum. It is
     * a 64-tap, decimate-by-1 filter.
     *
     * Design parameters:
     *
     * fdp.Rdata = 30720000;
     * fdp.RxTx = "Rx";
     * fdp.Type = "Lowpass";
     * fdp.DAC_div = 1;
     * fdp.HB3 = 2;
     * fdp.HB2 = 2;
     * fdp.HB1 = 2;
     * fdp.FIR = 1;
     * fdp.PLL_mult = 4;
     * fdp.converter_rate = 245760000;
     * fdp.PLL_rate = 983040000;
     * fdp.Fpass = fdp.Rdata*0.42;
     * fdp.Fstop = fdp.Rdata*0.50;
     * fdp.Fcenter = 0;
     * fdp.Apass = 0.125;
     * fdp.Astop = 85;
     * fdp.phEQ = -1;
     * fdp.wnom = 17920000;
     * fdp.caldiv = 7;
     * fdp.RFbw = 22132002;
     * fdp.FIRdBmin = 0;
     * fdp.int_FIR = 1;
     */
    // clang-format off
    {
          0,      0,      0,      1,     -1,      3,     -6,     11,
        -19,     33,    -53,     84,   -129,    193,   -282,    404,
       -565,    777,  -1052,   1401,  -1841,   2390,  -3071,   3911,
      -4947,   6230,  -7833,   9888, -12416,  15624, -21140,  32767,
      32767, -21140,  15624, -12416,   9888,  -7833,   6230,  -4947,
       3911,  -3071,   2390,  -1841,   1401,  -1052,    777,   -565,
        404,   -282,    193,   -129,     84,    -53,     33,    -19,
         11,     -6,      3,     -1,      1,      0,      0,      0,
          0,      0,      0,      0,      0,      0,      0,      0, // unused
          0,      0,      0,      0,      0,      0,      0,      0, // unused
          0,      0,      0,      0,      0,      0,      0,      0, // unused
          0,      0,      0,      0,      0,      0,      0,      0, // unused
          0,      0,      0,      0,      0,      0,      0,      0, // unused
          0,      0,      0,      0,      0,      0,      0,      0, // unused
          0,      0,      0,      0,      0,      0,      0,      0, // unused
          0,      0,      0,      0,      0,      0,      0,      0, // unused
    }, // rx_coef[128]
    // clang-format on
    64,                    // rx_coef_size
    { 0, 0, 0, 0, 0, 0 },  // rx_path_clks[6]
    0                      // rx_bandwidth
};

AD9361_TXFIRConfig bladerf2_rfic_tx_fir_config = {
    3,  // tx (TX1 = 1, TX2 = 2, both = 3)
    0,  // tx_gain (-6 or 0 dB)
    1,  // tx_int (interpolate by 1, 2, or 4)

    /**
     * TX FIR Filter
     *
     * This filter literally does nothing, but it is here as a placeholder.
     */
    // clang-format off
    {
      32767,      0,      0,      0,      0,      0,      0,      0,
          0,      0,      0,      0,      0,      0,      0,      0,
          0,      0,      0,      0,      0,      0,      0,      0,
          0,      0,      0,      0,      0,      0,      0,      0,
          0,      0,      0,      0,      0,      0,      0,      0,
          0,      0,      0,      0,      0,      0,      0,      0,
          0,      0,      0,      0,      0,      0,      0,      0,
          0,      0,      0,      0,      0,      0,      0,      0,
          0,      0,      0,      0,      0,      0,      0,      0, // unused
          0,      0,      0,      0,      0,      0,      0,      0, // unused
          0,      0,      0,      0,      0,      0,      0,      0, // unused
          0,      0,      0,      0,      0,      0,      0,      0, // unused
          0,      0,      0,      0,      0,      0,      0,      0, // unused
          0,      0,      0,      0,      0,      0,      0,      0, // unused
          0,      0,      0,      0,      0,      0,      0,      0, // unused
          0,      0,      0,      0,      0,      0,      0,      0, // unused
    }, // tx_coef[128]
    // clang-format on
    64,                    // tx_coef_size
    { 0, 0, 0, 0, 0, 0 },  // tx_path_clks[6]
    0                      // tx_bandwidth
};

AD9361_RXFIRConfig bladerf2_rfic_rx_fir_config_dec2 = {
    3,   // rx (RX1 = 1, RX2 = 2, both = 3)
    -6,  // rx_gain (-12, -6, 0, or 6 dB)
    2,   // rx_dec (decimate by 1, 2, or 4)

    /**
     * RX FIR Filter
     * Built using https://github.com/analogdevicesinc/libad9361-iio
     * Branch: filter_generation
     * Commit: f749cef974f687f696226455dc7684277886cf3b
     *
     * This filter is intended to improve the flatness of the RX spectrum.
     *
     * It is a 128-tap, decimate-by-2 filter. Note that you MUST use a
     * interpolate-by-2 filter on TX if you are using this filter.
     *
     * Design parameters:
     *
     * fdp.Rdata = 15360000;
     * fdp.RxTx = "Rx";
     * fdp.Type = "Lowpass";
     * fdp.DAC_div = 1;
     * fdp.HB3 = 2;
     * fdp.HB2 = 2;
     * fdp.HB1 = 2;
     * fdp.FIR = 2;
     * fdp.PLL_mult = 4;
     * fdp.converter_rate = 245760000;
     * fdp.PLL_rate = 983040000;
     * fdp.Fpass = fdp.Rdata*0.45;
     * fdp.Fstop = fdp.Rdata*0.50;
     * fdp.Fcenter = 0;
     * fdp.Apass = 0.1250;
     * fdp.Astop = 85;
     * fdp.phEQ = 217;
     * fdp.wnom = 8800000;
     * fdp.caldiv = 19;
     * fdp.RFbw = 8472407;
     * fdp.FIRdBmin = 0;
     * fdp.int_FIR = 1;
     */
    // clang-format off
    {
         22,    125,    207,    190,     15,    -98,    -45,     91,
         60,    -76,    -90,     69,    115,    -47,   -147,     22,
        173,     18,   -198,    -66,    211,    127,   -214,   -194,
        200,    269,   -168,   -345,    113,    419,    -36,   -484,
        -66,    536,    193,   -566,   -343,    568,    513,   -535,
       -699,    458,    897,   -329,  -1099,    140,   1296,    120,
      -1479,   -464,   1636,    912,  -1750,  -1496,   1797,   2275,
      -1734,  -3378,   1464,   5120,   -659,  -8461,  -2238,  18338,
      32689,  24727,   4100,  -7107,  -2663,   4128,   2513,  -2578,
      -2378,   1567,   2184,   -861,  -1953,    351,   1703,     22,
      -1446,   -290,   1190,    474,   -942,   -590,    710,    650,
       -498,   -664,    311,    642,   -150,   -593,     18,    524,
         86,   -444,   -162,    357,    211,   -271,   -238,    189,
        245,   -116,   -237,     53,    216,     -2,   -187,    -37,
        154,     64,   -119,    -82,     87,     89,    -56,    -96,
         30,     99,      3,   -120,   -107,      0,     56,     45,
    }, // rx_coef[128]
    // clang-format on
    128,                   // rx_coef_size
    { 0, 0, 0, 0, 0, 0 },  // rx_path_clks[6]
    0                      // rx_bandwidth
};

AD9361_TXFIRConfig bladerf2_rfic_tx_fir_config_int2 = {
    3,  // tx (TX1 = 1, TX2 = 2, both = 3)
    0,  // tx_gain (-6 or 0 dB)
    2,  // tx_int (interpolate by 1, 2, or 4)

    /**
     * TX FIR Filter
     * Built using https://github.com/analogdevicesinc/libad9361-iio
     * Branch: filter_generation
     * Commit: f749cef974f687f696226455dc7684277886cf3b
     *
     * This filter is intended to improve the flatness of the TX spectrum.
     *
     * It is a 128-tap, interpolate-by-2 filter. Note that you MUST use a
     * decimate-by-2 filter on RX if you are using this filter.
     *
     * Design parameters:
     *
     * fdp.Rdata = 15360000;
     * fdp.RxTx = "Tx";
     * fdp.Type = "Lowpass";
     * fdp.DAC_div = 1;
     * fdp.HB3 = 2;
     * fdp.HB2 = 2;
     * fdp.HB1 = 2;
     * fdp.FIR = 2;
     * fdp.PLL_mult = 4;
     * fdp.converter_rate = 245760000;
     * fdp.PLL_rate = 983040000;
     * fdp.Fpass = fdp.Rdata*0.45;
     * fdp.Fstop = fdp.Rdata*0.50;
     * fdp.Fcenter = 0;
     * fdp.Apass = 0.1250;
     * fdp.Astop = 85;
     * fdp.phEQ = 217;
     * fdp.wnom = 8800000;
     * fdp.caldiv = 19;
     * fdp.RFbw = 8472407;
     * fdp.FIRdBmin = 0;
     * fdp.int_FIR = 1;
     */
    // clang-format off
    {
         20,    104,    183,    161,      0,   -129,    -82,     61,
         69,    -65,   -108,     31,    117,    -15,   -145,    -23,
        155,     61,   -167,   -113,    163,    167,   -149,   -227,
        116,    286,    -67,   -342,     -3,    388,     91,   -421,
       -197,    433,    321,   -420,   -457,    376,    602,   -294,
       -749,    171,    891,      1,  -1019,   -225,   1123,    507,
      -1190,   -855,   1205,   1279,  -1148,  -1800,    984,   2456,
       -656,  -3329,     31,   4619,   1275,  -6897,  -4889,  12679,
      29822,  27710,   9244,  -5193,  -4330,   2732,   3367,  -1405,
      -2793,    571,   2318,    -25,  -1901,   -338,   1527,    574,
      -1189,   -716,    885,    787,   -617,   -802,    383,    774,
       -187,   -714,     25,    632,    101,   -535,   -193,    432,
        254,   -330,   -289,    232,    299,   -143,   -291,     66,
        267,     -3,   -234,    -46,    192,     79,   -153,   -103,
        107,    109,    -76,   -117,     32,    103,    -19,   -115,
        -35,     83,     34,   -120,   -204,   -134,    -42,     12,
    }, // tx_coef[128]
    // clang-format on
    128,                   // tx_coef_size
    { 0, 0, 0, 0, 0, 0 },  // tx_path_clks[6]
    0                      // tx_bandwidth
};

AD9361_RXFIRConfig bladerf2_rfic_rx_fir_config_dec4 = {
    3,   // rx (RX1 = 1, RX2 = 2, both = 3)
    -6,  // rx_gain (-12, -6, 0, or 6 dB)
    4,   // rx_dec (decimate by 1, 2, or 4)

    /**
     * RX FIR Filter
     * Built using https://github.com/analogdevicesinc/libad9361-iio
     * Branch: filter_generation
     * Commit: f749cef974f687f696226455dc7684277886cf3b
     *
     * This filter is intended to allow sample rates down to 520834 sps.
     *
     * It is a 128-tap, decimate-by-4 filter. Note that you MUST use a
     * interpolate-by-4 filter on TX if you are using this filter.
     *
     * Design parameters:
     *
     * fdp.Rdata = 520834;
     * fdp.RxTx = "Rx";
     * fdp.Type = "Lowpass";
     * fdp.DAC_div = 1;
     * fdp.HB3 = 3;
     * fdp.HB2 = 2;
     * fdp.HB1 = 2;
     * fdp.FIR = 4;
     * fdp.PLL_mult = 32;
     * fdp.converter_rate = 25000000;
     * fdp.PLL_rate = 800000000;
     * fdp.Fpass = fdp.Rdata*0.375;
     * fdp.Fstop = fdp.Rdata*0.50;
     * fdp.Fcenter = 0;
     * fdp.Apass = 0.125;
     * fdp.Astop = 85;
     * fdp.phEQ = 217;
     * fdp.wnom = 347220;
     * fdp.caldiv = 309;
     * fdp.RFbw = 433256;
     * fdp.FIRdBmin = 0;
     * fdp.int_FIR = 1;
     */
    // clang-format off
    {
        -30,    -24,    -46,    -54,    -28,     -6,     50,     82,
        108,     84,     28,    -60,   -136,   -172,   -132,    -24,
        122,    244,    280,    192,     -2,   -238,   -410,   -428,
       -250,     80,    436,    656,    614,    276,   -252,   -760,
      -1006,   -830,   -234,    580,   1272,   1494,   1060,     48,
      -1178,  -2086,  -2192,  -1284,    420,   2296,   3504,   3324,
       1502,  -1544,  -4746,  -6664,  -5978,  -1984,   5054,  13852,
      22416,  28606,  30790,  28370,  21974,  13264,   4422,  -2522,
      -6282,  -6630,  -4358,   -892,   2236,   3914,   3762,   2144,
        -80,  -1948,  -2774,  -2376,  -1078,    486,   1652,   2008,
       1510,    466,   -640,  -1352,  -1434,   -928,   -110,    652,
       1060,    990,    532,    -82,   -586,   -792,   -654,   -274,
        164,    480,    562,    410,    118,   -178,   -362,   -378,
       -244,    -34,    154,    256,    240,    136,     -4,   -116,
       -168,   -144,    -74,     14,     74,    102,     76,     38,
        -28,    -54,    -96,    -68,    -82,    -26,    -34,     -2,
    }, // rx_coef[128]
    // clang-format on
    128,                   // rx_coef_size
    { 0, 0, 0, 0, 0, 0 },  // rx_path_clks[6]
    0                      // rx_bandwidth
};

AD9361_TXFIRConfig bladerf2_rfic_tx_fir_config_int4 = {
    3,  // tx (TX1 = 1, TX2 = 2, both = 3)
    0,  // tx_gain (-6 or 0 dB)
    4,  // tx_int (interpolate by 1, 2, or 4)

    /**
     * TX FIR Filter
     * Built using https://github.com/analogdevicesinc/libad9361-iio
     * Branch: filter_generation
     * Commit: f749cef974f687f696226455dc7684277886cf3b
     *
     * This filter is intended to allow sample rates down to 520834 sps.
     *
     * It is a 128-tap, interpolate-by-4 filter. Note that you MUST use a
     * decimate-by-4 filter on RX if you are using this filter.
     *
     * Design parameters:
     *
     * fdp.Rdata = 520834;
     * fdp.RxTx = "Tx";
     * fdp.Type = "Lowpass";
     * fdp.DAC_div = 1;
     * fdp.HB3 = 3;
     * fdp.HB2 = 2;
     * fdp.HB1 = 2;
     * fdp.FIR = 4;
     * fdp.PLL_mult = 32;
     * fdp.converter_rate = 25000000;
     * fdp.PLL_rate = 800000000;
     * fdp.Fpass = fdp.Rdata*0.375;
     * fdp.Fstop = fdp.Rdata*0.50;
     * fdp.Fcenter = 0;
     * fdp.Apass = 0.125;
     * fdp.Astop = 85;
     * fdp.phEQ = 217;
     * fdp.wnom = 347220;
     * fdp.caldiv = 309;
     * fdp.RFbw = 1253611;
     * fdp.FIRdBmin = 0;
     * fdp.int_FIR = 1;
     */
    // clang-format off
    {
        -18,      2,    -14,     16,     34,     76,    104,    124,
        108,     62,    -12,    -86,   -136,   -128,    -58,     58,
        174,    242,    214,     84,   -110,   -294,   -382,   -310,
        -84,    226,    494,    586,    426,     40,   -434,   -796,
       -860,   -538,     92,    792,   1258,   1226,    622,   -386,
      -1406,  -1972,  -1730,   -628,   1002,   2522,   3200,   2526,
        466,  -2400,  -4986,  -6012,  -4444,     90,   7064,  15112,
      22370,  27018,  27836,  24590,  18120,  10072,   2400,  -3220,
      -5868,  -5578,  -3214,   -106,   2446,   3584,   3126,   1508,
       -464,  -1970,  -2484,  -1940,   -696,    666,   1590,   1764,
       1212,    244,   -706,  -1258,  -1242,   -732,     10,    656,
        964,    850,    412,   -134,   -560,   -710,   -560,   -208,
        178,    444,    500,    352,     88,   -172,   -330,   -336,
       -212,    -24,    144,    230,    218,    124,      2,   -100,
       -146,   -126,    -62,     18,     82,    110,     98,     60,
         12,    -28,    -52,    -56,    -50,    -32,    -18,     -8,
    }, // tx_coef[128]
    // clang-format on
    128,                   // tx_coef_size
    { 0, 0, 0, 0, 0, 0 },  // tx_path_clks[6]
    0                      // tx_bandwidth
};

#endif  // !defined(BLADERF_NIOS_BUILD) || defined(BLADERF_NIOS_LIBAD936X)
