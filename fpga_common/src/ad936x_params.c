#ifdef BLADERF_NIOS_BUILD
#include "devices.h"
#endif  // BLADERF_NIOS_BUILD

/* Avoid building this in low-memory situations */
#if !defined(BLADERF_NIOS_BUILD) || defined(BLADERF_NIOS_LIBAD936X)

#include "ad9361_api.h"
#include "axi_adc_core.h"
#include "axi_dac_core.h"
#include "platform.h"

/**
 * Reference:
 * https://wiki.analog.com/resources/tools-software/linux-drivers/iio-transceiver/ad9361-customization
 *
 * N/A      = not applicable due to other setting; changes may unmask these
 * DEFAULT  = changed during device initialization
 */

// clang-format off

/* FPGA-side ADI AXI interface core (axi_ad9361). Flat address space over
 * NIOS_PKT_32x32_TARGET_ADI_AXI: base 0, DAC-side registers at 0x4000+.
 * axi_adc_init() deasserts the core's RSTN and enables the channels; without
 * it the core stays in reset after power-up and no RX samples ever flow. */
static struct axi_adc_init bladerf2_rx_adc_init = {
    .name         = "bladerf2-axi-ad9361",
    .base         = 0,
    .num_channels = 4, // 2R2T, I+Q на канал
};

/* TX side of the same core.
 *
 * The old in-tree ADI bundle initialized both directions from the platform:
 * axiadc_init() called adc_init(phy) and then dac_init(phy, DATA_SEL_DMA, 0).
 * The migration to the current driver dropped adc_core.c and dac_core.c from
 * the build; the ADC path was restored, the DAC path was not, and the current
 * driver only ever touches rx_adc - nothing initializes tx_dac.
 *
 * The consequence is silent: the control plane is fully alive, sync_tx()
 * succeeds, USB transfers complete, and the TX LO leaks at the expected
 * level, but no submitted sample ever reaches the air. The core comes out of
 * power-up with its data source set to DDS, so the DAC plays its internal
 * generator instead of the DMA stream.
 *
 * The channel table is what makes that explicit: with .channels left NULL,
 * axi_dac_data_setup() programs DDS tones instead. Four channels for 2R2T,
 * I and Q each. */
static struct axi_dac_channel bladerf2_tx_dac_channels[4] = {
    { .sel = AXI_DAC_DATA_SEL_DMA },
    { .sel = AXI_DAC_DATA_SEL_DMA },
    { .sel = AXI_DAC_DATA_SEL_DMA },
    { .sel = AXI_DAC_DATA_SEL_DMA },
};

static struct axi_dac_init bladerf2_tx_dac_init = {
    .name         = "bladerf2-axi-ad9361-tx",
    /* DAC half of the core lives at +0x4000 in the same flat space - the
     * old in-tree dac_core.c added that offset in dac_read/dac_write, while
     * the current driver expects it in .base. With base 0 the DAC writes
     * land on the ADC's registers instead: axi_dac_init()'s reset write to
     * 0x40 hits AXI_ADC_REG_RSTN and takes the receiver down with it
     * (measured: every RX bin reads -inf, including the TX LO leak that is
     * always present otherwise). */
    .base         = 0x4000,
    .num_channels = 4,
    .channels     = bladerf2_tx_dac_channels,
};

AD9361_InitParam bladerf2_rfic_init_params = {
    /* Device selection */
    .dev_sel = ID_AD9361,	// AD9361 RF Agile Transceiver                                      // dev_sel

    /* Identification number */

    /* Reference Clock */
    .reference_clk_rate = 38400000UL,	// RefClk = 38.4 MHz                                                // reference_clk_rate

    /* Base Configuration */
    .two_rx_two_tx_mode_enable = 1,	// use 2Rx2Tx mode                                                  // two_rx_two_tx_mode_enable *** adi,2rx-2tx-mode-enable
    .one_rx_one_tx_mode_use_rx_num = 1,	// N/A when two_rx_two_tx_mode_enable = 1                           // one_rx_one_tx_mode_use_rx_num *** adi,1rx-1tx-mode-use-rx-num
    .one_rx_one_tx_mode_use_tx_num = 1,	// N/A when two_rx_two_tx_mode_enable = 1                           // one_rx_one_tx_mode_use_tx_num *** adi,1rx-1tx-mode-use-tx-num
    .frequency_division_duplex_mode_enable = 1,	// use FDD mode                                                     // frequency_division_duplex_mode_enable *** adi,frequency-division-duplex-mode-enable
    .frequency_division_duplex_independent_mode_enable = 1,	// use independent FDD mode                                         // frequency_division_duplex_independent_mode_enable *** adi,frequency-division-duplex-independent-mode-enable
    .tdd_use_dual_synth_mode_enable = 0,	// N/A when frequency_division_duplex_mode_enable = 1               // tdd_use_dual_synth_mode_enable *** adi,tdd-use-dual-synth-mode-enable
    .tdd_skip_vco_cal_enable = 0,	// N/A when frequency_division_duplex_mode_enable = 1               // tdd_skip_vco_cal_enable *** adi,tdd-skip-vco-cal-enable
    .tx_fastlock_delay_ns = 0,	// TX fastlock delay = 0 ns                                         // tx_fastlock_delay_ns *** adi,tx-fastlock-delay-ns
    .rx_fastlock_delay_ns = 0,	// RX fastlock delay = 0 ns                                         // rx_fastlock_delay_ns *** adi,rx-fastlock-delay-ns
    .rx_fastlock_pincontrol_enable = 0,	// RX fastlock pin control disabled                                 // rx_fastlock_pincontrol_enable *** adi,rx-fastlock-pincontrol-enable
    .tx_fastlock_pincontrol_enable = 0,	// TX fastlock pin control disabled                                 // tx_fastlock_pincontrol_enable *** adi,tx-fastlock-pincontrol-enable
    .external_rx_lo_enable = 0,	// use internal RX LO                                               // external_rx_lo_enable *** adi,external-rx-lo-enable
    .external_tx_lo_enable = 0,	// use internal TX LO                                               // external_tx_lo_enable *** adi,external-tx-lo-enable
    .dc_offset_tracking_update_event_mask = 5,	// apply new tracking word: on gain change, after exiting RX state  // dc_offset_tracking_update_event_mask *** adi,dc-offset-tracking-update-event-mask
    .dc_offset_attenuation_high_range = 6,	// atten value for DC tracking, RX LO > 4 GHz                       // dc_offset_attenuation_high_range *** adi,dc-offset-attenuation-high-range
    .dc_offset_attenuation_low_range = 5,	// atten value for DC tracking, RX LO < 4 GHz                       // dc_offset_attenuation_low_range *** adi,dc-offset-attenuation-low-range
    .dc_offset_count_high_range = 0x28,	// loop gain for DC tracking, RX LO > 4 GHz                         // dc_offset_count_high_range *** adi,dc-offset-count-high-range
    .dc_offset_count_low_range = 0x32,	// loop gain for DC tracking, RX LO < 4 GHz                         // dc_offset_count_low_range *** adi,dc-offset-count-low-range
    .split_gain_table_mode_enable = 0,	// use full gain table                                              // split_gain_table_mode_enable *** adi,split-gain-table-mode-enable
    .trx_synthesizer_target_fref_overwrite_hz = MAX_SYNTH_FREF,	// f_ref window 80 MHz                                              // trx_synthesizer_target_fref_overwrite_hz *** adi,trx-synthesizer-target-fref-overwrite-hz
    .qec_tracking_slow_mode_enable = 0,	// don't use improved RX QEC tracking                               // qec_tracking_slow_mode_enable *** adi,qec-tracking-slow-mode-enable

    /* ENSM Control */
    .ensm_enable_pin_pulse_mode_enable = 0,	// use level mode on ENABLE and TXNRX pins                          // ensm_enable_pin_pulse_mode_enable *** adi,ensm-enable-pin-pulse-mode-enable
    .ensm_enable_txnrx_control_enable = 0,	// use SPI writes for ENSM state, not ENABLE/TXNRX pins             // ensm_enable_txnrx_control_enable *** adi,ensm-enable-txnrx-control-enable

    /* LO Control */
    .rx_synthesizer_frequency_hz = 2400000000UL,	// DEFAULT                                                          // rx_synthesizer_frequency_hz *** adi,rx-synthesizer-frequency-hz
    .tx_synthesizer_frequency_hz = 2400000000UL,	// DEFAULT                                                          // tx_synthesizer_frequency_hz *** adi,tx-synthesizer-frequency-hz

    /* Rate & BW Control */
    .rx_path_clock_frequencies = { 983040000, 245760000, 122880000, 61440000, 30720000, 30720000 },	// DEFAULT      // uint32_t rx_path_clock_frequencies[6] *** adi,rx-path-clock-frequencies
    .tx_path_clock_frequencies = { 983040000, 122880000, 122880000, 61440000, 30720000, 30720000 },	// DEFAULT      // uint32_t tx_path_clock_frequencies[6] *** adi,tx-path-clock-frequencies
    .rf_rx_bandwidth_hz = 18000000,	// DEFAULT                                                          // rf_rx_bandwidth_hz *** adi,rf-rx-bandwidth-hz
    .rf_tx_bandwidth_hz = 18000000,	// DEFAULT                                                          // rf_tx_bandwidth_hz *** adi,rf-tx-bandwidth-hz

    /* RF Port Control */
    .rx_rf_port_input_select = 0,	// DEFAULT                                                          // rx_rf_port_input_select *** adi,rx-rf-port-input-select
    .tx_rf_port_input_select = 0,	// DEFAULT                                                          // tx_rf_port_input_select *** adi,tx-rf-port-input-select

    /* TX Attenuation Control */
    .tx_attenuation_mdB = 10000,	// DEFAULT                                                          // tx_attenuation_mdB *** adi,tx-attenuation-mdB
    .update_tx_gain_in_alert_enable = 0,	// N/A when frequency_division_duplex_mode_enable = 1               // update_tx_gain_in_alert_enable *** adi,update-tx-gain-in-alert-enable

    /* Reference Clock Control */
    .xo_disable_use_ext_refclk_enable = 1,	// Expect external clock into XTALN                                 // xo_disable_use_ext_refclk_enable *** adi,xo-disable-use-ext-refclk-enable
    .dcxo_coarse_and_fine_tune = {3, 5920},	// ~0 ppm DCXO trim (N/A if ext clk)                                // dcxo_coarse_and_fine_tune[2] *** adi,dcxo-coarse-and-fine-tune
    .clk_output_mode_select = CLKOUT_DISABLE,	// disable clkout pin (see enum ad9361_clkout)                      // clk_output_mode_select *** adi,clk-output-mode-select

    /* Gain Control */
    .gc_rx1_mode = RF_GAIN_SLOWATTACK_AGC,	// RX1 BLADERF_GAIN_DEFAULT = slow attack AGC               // gc_rx1_mode *** adi,gc-rx1-mode
    .gc_rx2_mode = RF_GAIN_SLOWATTACK_AGC,	// RX2 BLADERF_GAIN_DEFAULT = slow attack AGC               // gc_rx2_mode *** adi,gc-rx2-mode
    .gc_adc_large_overload_thresh = 58,	// magic AGC setting, see AD9361 docs                               // gc_adc_large_overload_thresh *** adi,gc-adc-large-overload-thresh
    .gc_adc_ovr_sample_size = 4,	// magic AGC setting, see AD9361 docs                               // gc_adc_ovr_sample_size *** adi,gc-adc-ovr-sample-size
    .gc_adc_small_overload_thresh = 47,	// magic AGC setting, see AD9361 docs                               // gc_adc_small_overload_thresh *** adi,gc-adc-small-overload-thresh
    .gc_dec_pow_measurement_duration = 8192,	// magic AGC setting, see AD9361 docs                               // gc_dec_pow_measurement_duration *** adi,gc-dec-pow-measurement-duration
    .gc_dig_gain_enable = 0,	// magic AGC setting, see AD9361 docs                               // gc_dig_gain_enable *** adi,gc-dig-gain-enable
    .gc_lmt_overload_high_thresh = 800,	// magic AGC setting, see AD9361 docs                               // gc_lmt_overload_high_thresh *** adi,gc-lmt-overload-high-thresh
    .gc_lmt_overload_low_thresh = 704,	// magic AGC setting, see AD9361 docs                               // gc_lmt_overload_low_thresh *** adi,gc-lmt-overload-low-thresh
    .gc_low_power_thresh = 24,	// magic AGC setting, see AD9361 docs                               // gc_low_power_thresh *** adi,gc-low-power-thresh
    .gc_max_dig_gain = 15,	// magic AGC setting, see AD9361 docs                               // gc_max_dig_gain *** adi,gc-max-dig-gain

    /* Gain MGC Control */
    .mgc_dec_gain_step = 2,	// N/A when mgc_rx(1,2)_ctrl_inp_enable = 0                         // mgc_dec_gain_step *** adi,mgc-dec-gain-step
    .mgc_inc_gain_step = 2,	// N/A when mgc_rx(1,2)_ctrl_inp_enable = 0                         // mgc_inc_gain_step *** adi,mgc-inc-gain-step
    .mgc_rx1_ctrl_inp_enable = 0,	// don't use CTRL_IN for RX1 MGC stepping                           // mgc_rx1_ctrl_inp_enable *** adi,mgc-rx1-ctrl-inp-enable
    .mgc_rx2_ctrl_inp_enable = 0,	// don't use CTRL_IN for RX2 MGC stepping                           // mgc_rx2_ctrl_inp_enable *** adi,mgc-rx2-ctrl-inp-enable
    .mgc_split_table_ctrl_inp_gain_mode = 0,	// N/A when mgc_rx(1,2)_ctrl_inp_enable = 0                         // mgc_split_table_ctrl_inp_gain_mode *** adi,mgc-split-table-ctrl-inp-gain-mode

    /* Gain AGC Control */
    .agc_adc_large_overload_exceed_counter = 10,	// magic AGC setting, see AD9361 docs                               // agc_adc_large_overload_exceed_counter *** adi,agc-adc-large-overload-exceed-counter
    .agc_adc_large_overload_inc_steps = 2,	// magic AGC setting, see AD9361 docs                               // agc_adc_large_overload_inc_steps *** adi,agc-adc-large-overload-inc-steps
    .agc_adc_lmt_small_overload_prevent_gain_inc_enable = 0,	// magic AGC setting, see AD9361 docs                               // agc_adc_lmt_small_overload_prevent_gain_inc_enable *** adi,agc-adc-lmt-small-overload-prevent-gain-inc-enable
    .agc_adc_small_overload_exceed_counter = 10,	// magic AGC setting, see AD9361 docs                               // agc_adc_small_overload_exceed_counter *** adi,agc-adc-small-overload-exceed-counter
    .agc_dig_gain_step_size = 4,	// magic AGC setting, see AD9361 docs                               // agc_dig_gain_step_size *** adi,agc-dig-gain-step-size
    .agc_dig_saturation_exceed_counter = 3,	// magic AGC setting, see AD9361 docs                               // agc_dig_saturation_exceed_counter *** adi,agc-dig-saturation-exceed-counter
    .agc_gain_update_interval_us = 1000,	// magic AGC setting, see AD9361 docs                               // agc_gain_update_interval_us *** adi,agc-gain-update-interval-us
    .agc_immed_gain_change_if_large_adc_overload_enable = 0,	// magic AGC setting, see AD9361 docs                               // agc_immed_gain_change_if_large_adc_overload_enable *** adi,agc-immed-gain-change-if-large-adc-overload-enable
    .agc_immed_gain_change_if_large_lmt_overload_enable = 0,	// magic AGC setting, see AD9361 docs                               // agc_immed_gain_change_if_large_lmt_overload_enable *** adi,agc-immed-gain-change-if-large-lmt-overload-enable
    .agc_inner_thresh_high = 10,	// magic AGC setting, see AD9361 docs                               // agc_inner_thresh_high *** adi,agc-inner-thresh-high
    .agc_inner_thresh_high_dec_steps = 1,	// magic AGC setting, see AD9361 docs                               // agc_inner_thresh_high_dec_steps *** adi,agc-inner-thresh-high-dec-steps
    .agc_inner_thresh_low = 12,	// magic AGC setting, see AD9361 docs                               // agc_inner_thresh_low *** adi,agc-inner-thresh-low
    .agc_inner_thresh_low_inc_steps = 1,	// magic AGC setting, see AD9361 docs                               // agc_inner_thresh_low_inc_steps *** adi,agc-inner-thresh-low-inc-steps
    .agc_lmt_overload_large_exceed_counter = 10,	// magic AGC setting, see AD9361 docs                               // agc_lmt_overload_large_exceed_counter *** adi,agc-lmt-overload-large-exceed-counter
    .agc_lmt_overload_large_inc_steps = 2,	// magic AGC setting, see AD9361 docs                               // agc_lmt_overload_large_inc_steps *** adi,agc-lmt-overload-large-inc-steps
    .agc_lmt_overload_small_exceed_counter = 10,	// magic AGC setting, see AD9361 docs                               // agc_lmt_overload_small_exceed_counter *** adi,agc-lmt-overload-small-exceed-counter
    .agc_outer_thresh_high = 5,	// magic AGC setting, see AD9361 docs                               // agc_outer_thresh_high *** adi,agc-outer-thresh-high
    .agc_outer_thresh_high_dec_steps = 2,	// magic AGC setting, see AD9361 docs                               // agc_outer_thresh_high_dec_steps *** adi,agc-outer-thresh-high-dec-steps
    .agc_outer_thresh_low = 18,	// magic AGC setting, see AD9361 docs                               // agc_outer_thresh_low *** adi,agc-outer-thresh-low
    .agc_outer_thresh_low_inc_steps = 2,	// magic AGC setting, see AD9361 docs                               // agc_outer_thresh_low_inc_steps *** adi,agc-outer-thresh-low-inc-steps
    .agc_attack_delay_extra_margin_us = 1,	// magic AGC setting, see AD9361 docs                               // agc_attack_delay_extra_margin_us; *** adi,agc-attack-delay-extra-margin-us
    .agc_sync_for_gain_counter_enable = 0,	// magic AGC setting, see AD9361 docs                               // agc_sync_for_gain_counter_enable *** adi,agc-sync-for-gain-counter-enable

    /* Fast AGC */
    .fagc_dec_pow_measuremnt_duration = 64,	// magic AGC setting, see AD9361 docs                               // fagc_dec_pow_measuremnt_duration ***  adi,fagc-dec-pow-measurement-duration
    .fagc_state_wait_time_ns = 260,	// magic AGC setting, see AD9361 docs                               // fagc_state_wait_time_ns ***  adi,fagc-state-wait-time-ns

    /* Fast AGC - Low Power */
    .fagc_allow_agc_gain_increase = 0,	// magic AGC setting, see AD9361 docs                               // fagc_allow_agc_gain_increase ***  adi,fagc-allow-agc-gain-increase-enable
    .fagc_lp_thresh_increment_time = 5,	// magic AGC setting, see AD9361 docs                               // fagc_lp_thresh_increment_time ***  adi,fagc-lp-thresh-increment-time
    .fagc_lp_thresh_increment_steps = 1,	// magic AGC setting, see AD9361 docs                               // fagc_lp_thresh_increment_steps ***  adi,fagc-lp-thresh-increment-steps

    /* Fast AGC - Lock Level */
    .fagc_lock_level_lmt_gain_increase_en = 1,	// magic AGC setting, see AD9361 docs                               // fagc_lock_level_lmt_gain_increase_en ***  adi,fagc-lock-level-lmt-gain-increase-enable
    .fagc_lock_level_gain_increase_upper_limit = 5,	// magic AGC setting, see AD9361 docs                               // fagc_lock_level_gain_increase_upper_limit ***  adi,fagc-lock-level-gain-increase-upper-limit

    /* Fast AGC - Peak Detectors and Final Settling */
    .fagc_lpf_final_settling_steps = 1,	// magic AGC setting, see AD9361 docs                               // fagc_lpf_final_settling_steps ***  adi,fagc-lpf-final-settling-steps
    .fagc_lmt_final_settling_steps = 1,	// magic AGC setting, see AD9361 docs                               // fagc_lmt_final_settling_steps ***  adi,fagc-lmt-final-settling-steps
    .fagc_final_overrange_count = 3,	// magic AGC setting, see AD9361 docs                               // fagc_final_overrange_count ***  adi,fagc-final-overrange-count

    /* Fast AGC - Final Power Test */
    .fagc_gain_increase_after_gain_lock_en = 0,	// magic AGC setting, see AD9361 docs                               // fagc_gain_increase_after_gain_lock_en ***  adi,fagc-gain-increase-after-gain-lock-enable

    /* Fast AGC - Unlocking the Gain */
    .fagc_gain_index_type_after_exit_rx_mode = 0,	// magic AGC setting, see AD9361 docs                               // fagc_gain_index_type_after_exit_rx_mode ***  adi,fagc-gain-index-type-after-exit-rx-mode
    .fagc_use_last_lock_level_for_set_gain_en = 1,	// magic AGC setting, see AD9361 docs                               // fagc_use_last_lock_level_for_set_gain_en ***  adi,fagc-use-last-lock-level-for-set-gain-enable
    .fagc_rst_gla_stronger_sig_thresh_exceeded_en = 1,	// magic AGC setting, see AD9361 docs                               // fagc_rst_gla_stronger_sig_thresh_exceeded_en ***  adi,fagc-rst-gla-stronger-sig-thresh-exceeded-enable
    .fagc_optimized_gain_offset = 5,	// magic AGC setting, see AD9361 docs                               // fagc_optimized_gain_offset ***  adi,fagc-optimized-gain-offset
    .fagc_rst_gla_stronger_sig_thresh_above_ll = 10,	// magic AGC setting, see AD9361 docs                               // fagc_rst_gla_stronger_sig_thresh_above_ll ***  adi,fagc-rst-gla-stronger-sig-thresh-above-ll
    .fagc_rst_gla_engergy_lost_sig_thresh_exceeded_en = 1,	// magic AGC setting, see AD9361 docs                               // fagc_rst_gla_engergy_lost_sig_thresh_exceeded_en ***  adi,fagc-rst-gla-engergy-lost-sig-thresh-exceeded-enable
    .fagc_rst_gla_engergy_lost_goto_optim_gain_en = 1,	// magic AGC setting, see AD9361 docs                               // fagc_rst_gla_engergy_lost_goto_optim_gain_en ***  adi,fagc-rst-gla-engergy-lost-goto-optim-gain-enable
    .fagc_rst_gla_engergy_lost_sig_thresh_below_ll = 10,	// magic AGC setting, see AD9361 docs                               // fagc_rst_gla_engergy_lost_sig_thresh_below_ll ***  adi,fagc-rst-gla-engergy-lost-sig-thresh-below-ll
    .fagc_energy_lost_stronger_sig_gain_lock_exit_cnt = 8,	// magic AGC setting, see AD9361 docs                               // fagc_energy_lost_stronger_sig_gain_lock_exit_cnt ***  adi,fagc-energy-lost-stronger-sig-gain-lock-exit-cnt
    .fagc_rst_gla_large_adc_overload_en = 1,	// magic AGC setting, see AD9361 docs                               // fagc_rst_gla_large_adc_overload_en ***  adi,fagc-rst-gla-large-adc-overload-enable
    .fagc_rst_gla_large_lmt_overload_en = 1,	// magic AGC setting, see AD9361 docs                               // fagc_rst_gla_large_lmt_overload_en ***  adi,fagc-rst-gla-large-lmt-overload-enable
    .fagc_rst_gla_en_agc_pulled_high_en = 0,	// magic AGC setting, see AD9361 docs                               // fagc_rst_gla_en_agc_pulled_high_en ***  adi,fagc-rst-gla-en-agc-pulled-high-enable
    .fagc_rst_gla_if_en_agc_pulled_high_mode = 0,	// magic AGC setting, see AD9361 docs                               // fagc_rst_gla_if_en_agc_pulled_high_mode ***  adi,fagc-rst-gla-if-en-agc-pulled-high-mode
    .fagc_power_measurement_duration_in_state5 = 64,	// magic AGC setting, see AD9361 docs                               // fagc_power_measurement_duration_in_state5 ***  adi,fagc-power-measurement-duration-in-state5

    /* RSSI Control */
    .rssi_delay = 1,	// settling delay on RSSI algo restart = 1 μs                       // rssi_delay *** adi,rssi-delay
    .rssi_duration = 1000,	// total RSSI measurement duration = 1000 μs                        // rssi_duration *** adi,rssi-duration
    .rssi_restart_mode = GAIN_CHANGE_OCCURS,	// reset RSSI accumulator on gain change event                  // rssi_restart_mode *** adi,rssi-restart-mode
    .rssi_unit_is_rx_samples_enable = 0,	// RSSI control values are in microseconds                          // rssi_unit_is_rx_samples_enable *** adi,rssi-unit-is-rx-samples-enable
    .rssi_wait = 1,	// wait 1 μs between RSSI measurements                              // rssi_wait *** adi,rssi-wait

    /* Aux ADC Control */
    /* bladeRF Micro: N/A, pin tied to GND */
    .aux_adc_decimation = 256,	// AuxADC decimate by 256                                           // aux_adc_decimation *** adi,aux-adc-decimation
    .aux_adc_rate = 40000000UL,	// AuxADC sample rate 40 MHz                                        // aux_adc_rate *** adi,aux-adc-rate

    /* AuxDAC Control */
    /* bladeRF Micro: AuxDAC1 is TP7 and AUXDAC_TRIM, AuxDAC2 is TP8 */
    .aux_dac_manual_mode_enable = 1,	// AuxDAC does not slave the ENSM                                   // aux_dac_manual_mode_enable ***  adi,aux-dac-manual-mode-enable
    .aux_dac1_default_value_mV = 0,	// AuxDAC1 default value = 0 mV                                     // aux_dac1_default_value_mV ***  adi,aux-dac1-default-value-mV
    .aux_dac1_active_in_rx_enable = 0,	// N/A when aux_dac_manual_mode_enable = 1                          // aux_dac1_active_in_rx_enable ***  adi,aux-dac1-active-in-rx-enable
    .aux_dac1_active_in_tx_enable = 0,	// N/A when aux_dac_manual_mode_enable = 1                          // aux_dac1_active_in_tx_enable ***  adi,aux-dac1-active-in-tx-enable
    .aux_dac1_active_in_alert_enable = 0,	// N/A when aux_dac_manual_mode_enable = 1                          // aux_dac1_active_in_alert_enable ***  adi,aux-dac1-active-in-alert-enable
    .aux_dac1_rx_delay_us = 0,	// N/A when aux_dac_manual_mode_enable = 1                          // aux_dac1_rx_delay_us ***  adi,aux-dac1-rx-delay-us
    .aux_dac1_tx_delay_us = 0,	// N/A when aux_dac_manual_mode_enable = 1                          // aux_dac1_tx_delay_us ***  adi,aux-dac1-tx-delay-us
    .aux_dac2_default_value_mV = 0,	// AuxDAC2 default value = 0 mV                                     // aux_dac2_default_value_mV ***  adi,aux-dac2-default-value-mV
    .aux_dac2_active_in_rx_enable = 0,	// N/A when aux_dac_manual_mode_enable = 1                          // aux_dac2_active_in_rx_enable ***  adi,aux-dac2-active-in-rx-enable
    .aux_dac2_active_in_tx_enable = 0,	// N/A when aux_dac_manual_mode_enable = 1                          // aux_dac2_active_in_tx_enable ***  adi,aux-dac2-active-in-tx-enable
    .aux_dac2_active_in_alert_enable = 0,	// N/A when aux_dac_manual_mode_enable = 1                          // aux_dac2_active_in_alert_enable ***  adi,aux-dac2-active-in-alert-enable
    .aux_dac2_rx_delay_us = 0,	// N/A when aux_dac_manual_mode_enable = 1                          // aux_dac2_rx_delay_us ***  adi,aux-dac2-rx-delay-us
    .aux_dac2_tx_delay_us = 0,	// N/A when aux_dac_manual_mode_enable = 1                          // aux_dac2_tx_delay_us ***  adi,aux-dac2-tx-delay-us

    /* Temperature Sensor Control */
    .temp_sense_decimation = 256,	// Temperature sensor decimate by 256                               // temp_sense_decimation *** adi,temp-sense-decimation
    .temp_sense_measurement_interval_ms = 1000,	// Measure temperature every 1000 ms                                // temp_sense_measurement_interval_ms *** adi,temp-sense-measurement-interval-ms
    .temp_sense_offset_signed = 206,	// Offset = +206 degrees C                                          // temp_sense_offset_signed *** adi,temp-sense-offset-signed
    .temp_sense_periodic_measurement_enable = 1,	// Periodic temperature measurements enabled                        // temp_sense_periodic_measurement_enable *** adi,temp-sense-periodic-measurement-enable

    /* Control Out Setup */
    /* See https://wiki.analog.com/resources/tools-software/linux-drivers/iio-transceiver/ad9361-customization#control_output_setup */
    .ctrl_outs_enable_mask = 0xFF,	// Enable all CTRL_OUT bits                                         // ctrl_outs_enable_mask *** adi,ctrl-outs-enable-mask
    .ctrl_outs_index = 0,	// CTRL_OUT index is 0                                              // ctrl_outs_index *** adi,ctrl-outs-index

    /* External LNA Control */
    /* bladeRF Micro: GPO_0 is TP3, GPO_1 is TP4 */
    .elna_settling_delay_ns = 0,	// N/A when elna_rx(1,2)_gpo(0,1)_control_enable = 0                // elna_settling_delay_ns *** adi,elna-settling-delay-ns
    .elna_gain_mdB = 0,	// MUST be 0 when elna_rx(1,2)_gpo(0,1)_control_enable = 0          // elna_gain_mdB *** adi,elna-gain-mdB
    .elna_bypass_loss_mdB = 0,	// MUST be 0 when elna_rx(1,2)_gpo(0,1)_control_enable = 0          // elna_bypass_loss_mdB *** adi,elna-bypass-loss-mdB
    .elna_rx1_gpo0_control_enable = 0,	// Ext LNA Ctrl bit in Rx1 gain table does NOT set GPO0 state       // elna_rx1_gpo0_control_enable *** adi,elna-rx1-gpo0-control-enable
    .elna_rx2_gpo1_control_enable = 0,	// Ext LNA Ctrl bit in Rx2 gain table does NOT set GPO1 state       // elna_rx2_gpo1_control_enable *** adi,elna-rx2-gpo1-control-enable
    .elna_gaintable_all_index_enable = 0,	// N/A when elna_rx(1,2)_gpo(0,1)_control_enable = 0                // elna_gaintable_all_index_enable *** adi,elna-gaintable-all-index-enable

    /* Digital Interface Control */
#ifdef ENABLE_AD9361_DIGITAL_INTERFACE_TIMING_VERIFICATION
    /* Calibrate the digital interface delay (hardware validation) */
    .digital_interface_tune_skip_mode = 0,	// Don't skip digital interface tuning                              // digital_interface_tune_skip_mode *** adi,digital-interface-tune-skip-mode
#else
    /* Use hardcoded digital interface delay values (production) */
    .digital_interface_tune_skip_mode = 2,	// Skip RX and TX tuning; use hardcoded values below                // digital_interface_tune_skip_mode *** adi,digital-interface-tune-skip-mode
#endif // ENABLE_AD9361_DIGITAL_INTERFACE_TIMING_VERIFICATION
    .digital_interface_tune_fir_disable = 0,	// ?? UNDOCUMENTED ??                                               // digital_interface_tune_fir_disable *** adi,digital-interface-tune-fir-disable
    .pp_tx_swap_enable = 1,	// Swap I and Q (spectral inversion)                                // pp_tx_swap_enable *** adi,pp-tx-swap-enable
    .pp_rx_swap_enable = 1,	// Swap I and Q (spectral inversion)                                // pp_rx_swap_enable *** adi,pp-rx-swap-enable
    .tx_channel_swap_enable = 0,	// Don't swap TX1 and TX2                                           // tx_channel_swap_enable *** adi,tx-channel-swap-enable
    .rx_channel_swap_enable = 0,	// Don't swap RX1 and RX2                                           // rx_channel_swap_enable *** adi,rx-channel-swap-enable
    .rx_frame_pulse_mode_enable = 1,	// Toggle RX_FRAME with 50% duty cycle                              // rx_frame_pulse_mode_enable *** adi,rx-frame-pulse-mode-enable
    .two_t_two_r_timing_enable = 0,	// Data port timing reflects # of enabled signal paths              // two_t_two_r_timing_enable *** adi,2t2r-timing-enable
    .invert_data_bus_enable = 0,	// Don't invert data bus                                            // invert_data_bus_enable *** adi,invert-data-bus-enable
    .invert_data_clk_enable = 0,	// Don't invert data clock                                          // invert_data_clk_enable *** adi,invert-data-clk-enable
    .fdd_alt_word_order_enable = 0,	// MUST be 0 when lvds_mode_enable = 1                              // fdd_alt_word_order_enable *** adi,fdd-alt-word-order-enable
    .invert_rx_frame_enable = 0,	// Don't invert RX_FRAME                                            // invert_rx_frame_enable *** adi,invert-rx-frame-enable
    .fdd_rx_rate_2tx_enable = 0,	// Don't make RX sample rate 2x the TX sample rate                  // fdd_rx_rate_2tx_enable *** adi,fdd-rx-rate-2tx-enable
    .swap_ports_enable = 0,	// MUST be 0 when lvds_mode_enable = 1                              // swap_ports_enable *** adi,swap-ports-enable
    .single_data_rate_enable = 0,	// MUST be 0 when lvds_mode_enable = 1                              // single_data_rate_enable *** adi,single-data-rate-enable
    .lvds_mode_enable = 1,	// Use LVDS mode on data port                                       // lvds_mode_enable *** adi,lvds-mode-enable
    .half_duplex_mode_enable = 0,	// MUST be 0 when lvds_mode_enable = 1                              // half_duplex_mode_enable *** adi,half-duplex-mode-enable
    .single_port_mode_enable = 0,	// MUST be 0 when lvds_mode_enable = 1                              // single_port_mode_enable *** adi,single-port-mode-enable
    .full_port_enable = 0,	// MUST be 0 when lvds_mode_enable = 1                              // full_port_enable *** adi,full-port-enable
    .full_duplex_swap_bits_enable = 0,	// MUST be 0 when lvds_mode_enable = 1                              // full_duplex_swap_bits_enable *** adi,full-duplex-swap-bits-enable
    .delay_rx_data = 0,	// RX_DATA delay rel to RX_FRAME = 0 DATA_CLK/2 cycles              // delay_rx_data *** adi,delay-rx-data
    // Approx 0.3 ns/LSB on next 4 values
    .rx_data_clock_delay = 5,	// DATA_CLK delay = 1.5 ns                                          // rx_data_clock_delay *** adi,rx-data-clock-delay
    .rx_data_delay = 0,	// RX_DATA/RX_FRAME delay = 0 ns                                    // rx_data_delay *** adi,rx-data-delay
    .tx_fb_clock_delay = 0,	// FB_CLK delay = 0 ns                                              // tx_fb_clock_delay *** adi,tx-fb-clock-delay
    .tx_data_delay = 5,	// TX_DATA/TX_FRAME delay = 1.5 ns                                  // tx_data_delay *** adi,tx-data-delay
    .lvds_bias_mV = 300,	// LVDS driver bias 300 mV                                          // lvds_bias_mV *** adi,lvds-bias-mV
    .lvds_rx_onchip_termination_enable = 1,	// Enable LVDS on-chip termination                                  // lvds_rx_onchip_termination_enable *** adi,lvds-rx-onchip-termination-enable
    .rx1rx2_phase_inversion_en = 1,	// RX1 and RX2 are not phase-aligned                                // rx1rx2_phase_inversion_en *** adi,rx1-rx2-phase-inversion-enable
    .lvds_invert1_control = 0xFF,	// Default signal inversion mappings                                // lvds_invert1_control *** adi,lvds-invert1-control
    .lvds_invert2_control = 0x0F,	// Default signal inversion mappings                                // lvds_invert2_control *** adi,lvds-invert2-control
    .clk_out_drive = 1,	// CLK_OUT drive increased by ~20%                                  // clk_out_drive
    .dataclk_drive = 1,	// DATA_CLK drive increased by ~20%                                 // dataclk_drive
    .data_port_drive = 1,	// Data port drive increased by ~20%                                // data_port_drive
    .clk_out_slew = 0,	// CLK_OUT minimum slew (fastest rise/fall)                         // clk_out_slew
    .dataclk_slew = 0,	// DATA_CLK minimum slew (fastest rise/fall)                        // dataclk_slew
    .data_port_slew = 0,	// Data port minimum slew (fastest rise/fall)                       // data_port_slew

    /* GPO Control */
    .gpo0_inactive_state_high_enable = 0,	// GPO0 is LOW in Sleep/Wait/Alert states                           // gpo0_inactive_state_high_enable *** adi,gpo0-inactive-state-high-enable
    .gpo1_inactive_state_high_enable = 0,	// GPO1 is LOW in Sleep/Wait/Alert states                           // gpo1_inactive_state_high_enable *** adi,gpo1-inactive-state-high-enable
    .gpo2_inactive_state_high_enable = 0,	// GPO2 is LOW in Sleep/Wait/Alert states                           // gpo2_inactive_state_high_enable *** adi,gpo2-inactive-state-high-enable
    .gpo3_inactive_state_high_enable = 0,	// GPO3 is LOW in Sleep/Wait/Alert states                           // gpo3_inactive_state_high_enable *** adi,gpo3-inactive-state-high-enable
    .gpo0_slave_rx_enable = 0,	// GPO0 does not change state when entering RX state                // gpo0_slave_rx_enable *** adi,gpo0-slave-rx-enable
    .gpo0_slave_tx_enable = 0,	// GPO0 does not change state when entering TX state                // gpo0_slave_tx_enable *** adi,gpo0-slave-tx-enable
    .gpo1_slave_rx_enable = 0,	// GPO1 does not change state when entering RX state                // gpo1_slave_rx_enable *** adi,gpo1-slave-rx-enable
    .gpo1_slave_tx_enable = 0,	// GPO1 does not change state when entering TX state                // gpo1_slave_tx_enable *** adi,gpo1-slave-tx-enable
    .gpo2_slave_rx_enable = 0,	// GPO2 does not change state when entering RX state                // gpo2_slave_rx_enable *** adi,gpo2-slave-rx-enable
    .gpo2_slave_tx_enable = 0,	// GPO2 does not change state when entering TX state                // gpo2_slave_tx_enable *** adi,gpo2-slave-tx-enable
    .gpo3_slave_rx_enable = 0,	// GPO3 does not change state when entering RX state                // gpo3_slave_rx_enable *** adi,gpo3-slave-rx-enable
    .gpo3_slave_tx_enable = 0,	// GPO3 does not change state when entering TX state                // gpo3_slave_tx_enable *** adi,gpo3-slave-tx-enable
    .gpo0_rx_delay_us = 0,	// N/A when gpo0_slave_rx_enable = 0                                // gpo0_rx_delay_us *** adi,gpo0-rx-delay-us
    .gpo0_tx_delay_us = 0,	// N/A when gpo0_slave_tx_enable = 0                                // gpo0_tx_delay_us *** adi,gpo0-tx-delay-us
    .gpo1_rx_delay_us = 0,	// N/A when gpo1_slave_rx_enable = 0                                // gpo1_rx_delay_us *** adi,gpo1-rx-delay-us
    .gpo1_tx_delay_us = 0,	// N/A when gpo1_slave_tx_enable = 0                                // gpo1_tx_delay_us *** adi,gpo1-tx-delay-us
    .gpo2_rx_delay_us = 0,	// N/A when gpo2_slave_rx_enable = 0                                // gpo2_rx_delay_us *** adi,gpo2-rx-delay-us
    .gpo2_tx_delay_us = 0,	// N/A when gpo2_slave_tx_enable = 0                                // gpo2_tx_delay_us *** adi,gpo2-tx-delay-us
    .gpo3_rx_delay_us = 0,	// N/A when gpo3_slave_rx_enable = 0                                // gpo3_rx_delay_us *** adi,gpo3-rx-delay-us
    .gpo3_tx_delay_us = 0,	// N/A when gpo3_slave_tx_enable = 0                                // gpo3_tx_delay_us *** adi,gpo3-tx-delay-us

    /* Tx Monitor Control */
    /* bladeRF Micro: N/A, TX_MON1 and TX_MON2 tied to GND */
    .low_high_gain_threshold_mdB = 37000,	// N/A                                                              // low_high_gain_threshold_mdB *** adi,txmon-low-high-thresh
    .low_gain_dB = 0,	// N/A                                                              // low_gain_dB *** adi,txmon-low-gain
    .high_gain_dB = 24,	// N/A                                                              // high_gain_dB *** adi,txmon-high-gain
    .tx_mon_track_en = 0,	// N/A                                                              // tx_mon_track_en *** adi,txmon-dc-tracking-enable
    .one_shot_mode_en = 0,	// N/A                                                              // one_shot_mode_en *** adi,txmon-one-shot-mode-enable
    .tx_mon_delay = 511,	// N/A                                                              // tx_mon_delay *** adi,txmon-delay
    .tx_mon_duration = 8192,	// N/A                                                              // tx_mon_duration *** adi,txmon-duration
    .tx1_mon_front_end_gain = 2,	// N/A                                                              // tx1_mon_front_end_gain *** adi,txmon-1-front-end-gain
    .tx2_mon_front_end_gain = 2,	// N/A                                                              // tx2_mon_front_end_gain *** adi,txmon-2-front-end-gain
    .tx1_mon_lo_cm = 48,	// N/A                                                              // tx1_mon_lo_cm *** adi,txmon-1-lo-cm
    .tx2_mon_lo_cm = 48,	// N/A                                                              // tx2_mon_lo_cm *** adi,txmon-2-lo-cm

    /* GPIO definitions */
    /* Bit of the FPGA RFFE control register, driven through the accessor
     * table in platform_bladerf2. The device handle goes into .extra at
     * init time, where it is known. */
    .gpio_resetb = { .number = RFFE_CONTROL_RESET_N,
                     .platform_ops = &bladerf2_gpio_ops },	// Reset using RFFE bit 0                                   // gpio_resetb *** reset-gpios

    /* MCS Sync */
    .gpio_sync = { .number = -1 },	// Future use (MCS Sync)                                            // gpio_sync *** sync-gpios
    .gpio_cal_sw1 = { .number = -1 },	// Future use (MCS Sync)                                            // gpio_cal_sw1 *** cal-sw1-gpios
    .gpio_cal_sw2 = { .number = -1 },	// Future use (MCS Sync)                                            // gpio_cal_sw2 *** cal-sw2-gpios

    /* External LO clocks */
    /* SPI reaches the device through the accessor table in
     * platform_bladerf2; the handle goes into .extra at init time. */
    .spi_param = { .platform_ops = &bladerf2_spi_ops },

    /* FPGA AXI interface core init (деасерт RSTN у axi_adc_init) */
    .rx_adc_init = &bladerf2_rx_adc_init,
    .tx_dac_init = &bladerf2_tx_dac_init,

    .ad9361_rfpll_ext_recalc_rate = NULL,	// Future use (RX_EXT_LO, TX_EXT_LO control)                        // (*ad9361_rfpll_ext_recalc_rate)()
    .ad9361_rfpll_ext_round_rate = NULL,	// Future use (RX_EXT_LO, TX_EXT_LO control)                        // (*ad9361_rfpll_ext_round_rate)()
    .ad9361_rfpll_ext_set_rate = NULL,	// Future use (RX_EXT_LO, TX_EXT_LO control)                        // (*ad9361_rfpll_ext_set_rate)()
};
// clang-format on


// clang-format off
AD9361_InitParam bladerf2_rfic_init_params_fastagc_burst = {
    /* Device selection */
    .dev_sel = ID_AD9361,	// AD9361 RF Agile Transceiver                                      // dev_sel

    /* Identification number */

    /* Reference Clock */
    .reference_clk_rate = 38400000UL,	// RefClk = 38.4 MHz                                                // reference_clk_rate

    /* Base Configuration */
    .two_rx_two_tx_mode_enable = 1,	// use 2Rx2Tx mode                                                  // two_rx_two_tx_mode_enable *** adi,2rx-2tx-mode-enable
    .one_rx_one_tx_mode_use_rx_num = 1,	// N/A when two_rx_two_tx_mode_enable = 1                           // one_rx_one_tx_mode_use_rx_num *** adi,1rx-1tx-mode-use-rx-num
    .one_rx_one_tx_mode_use_tx_num = 1,	// N/A when two_rx_two_tx_mode_enable = 1                           // one_rx_one_tx_mode_use_tx_num *** adi,1rx-1tx-mode-use-tx-num
    .frequency_division_duplex_mode_enable = 1,	// use FDD mode                                                     // frequency_division_duplex_mode_enable *** adi,frequency-division-duplex-mode-enable
    .frequency_division_duplex_independent_mode_enable = 1,	// use independent FDD mode                                         // frequency_division_duplex_independent_mode_enable *** adi,frequency-division-duplex-independent-mode-enable
    .tdd_use_dual_synth_mode_enable = 0,	// N/A when frequency_division_duplex_mode_enable = 1               // tdd_use_dual_synth_mode_enable *** adi,tdd-use-dual-synth-mode-enable
    .tdd_skip_vco_cal_enable = 0,	// N/A when frequency_division_duplex_mode_enable = 1               // tdd_skip_vco_cal_enable *** adi,tdd-skip-vco-cal-enable
    .tx_fastlock_delay_ns = 0,	// TX fastlock delay = 0 ns                                         // tx_fastlock_delay_ns *** adi,tx-fastlock-delay-ns
    .rx_fastlock_delay_ns = 0,	// RX fastlock delay = 0 ns                                         // rx_fastlock_delay_ns *** adi,rx-fastlock-delay-ns
    .rx_fastlock_pincontrol_enable = 0,	// RX fastlock pin control disabled                                 // rx_fastlock_pincontrol_enable *** adi,rx-fastlock-pincontrol-enable
    .tx_fastlock_pincontrol_enable = 0,	// TX fastlock pin control disabled                                 // tx_fastlock_pincontrol_enable *** adi,tx-fastlock-pincontrol-enable
    .external_rx_lo_enable = 0,	// use internal RX LO                                               // external_rx_lo_enable *** adi,external-rx-lo-enable
    .external_tx_lo_enable = 0,	// use internal TX LO                                               // external_tx_lo_enable *** adi,external-tx-lo-enable
    .dc_offset_tracking_update_event_mask = 5,	// apply new tracking word: on gain change, after exiting RX state  // dc_offset_tracking_update_event_mask *** adi,dc-offset-tracking-update-event-mask
    .dc_offset_attenuation_high_range = 6,	// atten value for DC tracking, RX LO > 4 GHz                       // dc_offset_attenuation_high_range *** adi,dc-offset-attenuation-high-range
    .dc_offset_attenuation_low_range = 5,	// atten value for DC tracking, RX LO < 4 GHz                       // dc_offset_attenuation_low_range *** adi,dc-offset-attenuation-low-range
    .dc_offset_count_high_range = 0x28,	// loop gain for DC tracking, RX LO > 4 GHz                         // dc_offset_count_high_range *** adi,dc-offset-count-high-range
    .dc_offset_count_low_range = 0x32,	// loop gain for DC tracking, RX LO < 4 GHz                         // dc_offset_count_low_range *** adi,dc-offset-count-low-range
    .split_gain_table_mode_enable = 0,	// use full gain table                                              // split_gain_table_mode_enable *** adi,split-gain-table-mode-enable
    .trx_synthesizer_target_fref_overwrite_hz = MAX_SYNTH_FREF,	// f_ref window 80 MHz                                              // trx_synthesizer_target_fref_overwrite_hz *** adi,trx-synthesizer-target-fref-overwrite-hz
    .qec_tracking_slow_mode_enable = 0,	// don't use improved RX QEC tracking                               // qec_tracking_slow_mode_enable *** adi,qec-tracking-slow-mode-enable

    /* ENSM Control */
    .ensm_enable_pin_pulse_mode_enable = 0,	// use level mode on ENABLE and TXNRX pins                          // ensm_enable_pin_pulse_mode_enable *** adi,ensm-enable-pin-pulse-mode-enable
    .ensm_enable_txnrx_control_enable = 0,	// use SPI writes for ENSM state, not ENABLE/TXNRX pins             // ensm_enable_txnrx_control_enable *** adi,ensm-enable-txnrx-control-enable

    /* LO Control */
    .rx_synthesizer_frequency_hz = 2400000000UL,	// DEFAULT                                                          // rx_synthesizer_frequency_hz *** adi,rx-synthesizer-frequency-hz
    .tx_synthesizer_frequency_hz = 2400000000UL,	// DEFAULT                                                          // tx_synthesizer_frequency_hz *** adi,tx-synthesizer-frequency-hz

    /* Rate & BW Control */
    .rx_path_clock_frequencies = { 983040000, 245760000, 122880000, 61440000, 30720000, 30720000 },	// DEFAULT      // uint32_t rx_path_clock_frequencies[6] *** adi,rx-path-clock-frequencies
    .tx_path_clock_frequencies = { 983040000, 122880000, 122880000, 61440000, 30720000, 30720000 },	// DEFAULT      // uint32_t tx_path_clock_frequencies[6] *** adi,tx-path-clock-frequencies
    .rf_rx_bandwidth_hz = 18000000,	// DEFAULT                                                          // rf_rx_bandwidth_hz *** adi,rf-rx-bandwidth-hz
    .rf_tx_bandwidth_hz = 18000000,	// DEFAULT                                                          // rf_tx_bandwidth_hz *** adi,rf-tx-bandwidth-hz

    /* RF Port Control */
    .rx_rf_port_input_select = 0,	// DEFAULT                                                          // rx_rf_port_input_select *** adi,rx-rf-port-input-select
    .tx_rf_port_input_select = 0,	// DEFAULT                                                          // tx_rf_port_input_select *** adi,tx-rf-port-input-select

    /* TX Attenuation Control */
    .tx_attenuation_mdB = 10000,	// DEFAULT                                                          // tx_attenuation_mdB *** adi,tx-attenuation-mdB
    .update_tx_gain_in_alert_enable = 0,	// N/A when frequency_division_duplex_mode_enable = 1               // update_tx_gain_in_alert_enable *** adi,update-tx-gain-in-alert-enable

    /* Reference Clock Control */
    .xo_disable_use_ext_refclk_enable = 1,	// Expect external clock into XTALN                                 // xo_disable_use_ext_refclk_enable *** adi,xo-disable-use-ext-refclk-enable
    .dcxo_coarse_and_fine_tune = {3, 5920},	// ~0 ppm DCXO trim (N/A if ext clk)                                // dcxo_coarse_and_fine_tune[2] *** adi,dcxo-coarse-and-fine-tune
    .clk_output_mode_select = CLKOUT_DISABLE,	// disable clkout pin (see enum ad9361_clkout)                      // clk_output_mode_select *** adi,clk-output-mode-select

    /* Gain Control */
    .gc_rx1_mode = RF_GAIN_FASTATTACK_AGC,	// RX1 BLADERF_GAIN_DEFAULT = slow attack AGC               // gc_rx1_mode *** adi,gc-rx1-mode
    .gc_rx2_mode = RF_GAIN_FASTATTACK_AGC,	// RX2 BLADERF_GAIN_DEFAULT = slow attack AGC               // gc_rx2_mode *** adi,gc-rx2-mode
    .gc_adc_large_overload_thresh = 58,	// magic AGC setting, see AD9361 docs                               // gc_adc_large_overload_thresh *** adi,gc-adc-large-overload-thresh
    .gc_adc_ovr_sample_size = 4,	// magic AGC setting, see AD9361 docs                               // gc_adc_ovr_sample_size *** adi,gc-adc-ovr-sample-size
    .gc_adc_small_overload_thresh = 47,	// magic AGC setting, see AD9361 docs                               // gc_adc_small_overload_thresh *** adi,gc-adc-small-overload-thresh
    .gc_dec_pow_measurement_duration = 2,	// magic AGC setting, see AD9361 docs                               // gc_dec_pow_measurement_duration *** adi,gc-dec-pow-measurement-duration
    .gc_dig_gain_enable = 0,	// magic AGC setting, see AD9361 docs                               // gc_dig_gain_enable *** adi,gc-dig-gain-enable
    .gc_lmt_overload_high_thresh = 480,	// magic AGC setting, see AD9361 docs                               // gc_lmt_overload_high_thresh *** adi,gc-lmt-overload-high-thresh
    .gc_lmt_overload_low_thresh = 400,	//  magic AGC setting, see AD9361 docs                               // gc_lmt_overload_low_thresh *** adi,gc-lmt-overload-low-thresh
    .gc_low_power_thresh = 40,	// magic AGC setting, see AD9361 docs                               // gc_low_power_thresh *** adi,gc-low-power-thresh
    .gc_max_dig_gain = 15,	// magic AGC setting, see AD9361 docs                               // gc_max_dig_gain *** adi,gc-max-dig-gain

    /* Gain MGC Control */
    .mgc_dec_gain_step = 2,	// N/A when mgc_rx(1,2)_ctrl_inp_enable = 0                         // mgc_dec_gain_step *** adi,mgc-dec-gain-step
    .mgc_inc_gain_step = 2,	// N/A when mgc_rx(1,2)_ctrl_inp_enable = 0                         // mgc_inc_gain_step *** adi,mgc-inc-gain-step
    .mgc_rx1_ctrl_inp_enable = 0,	// don't use CTRL_IN for RX1 MGC stepping                           // mgc_rx1_ctrl_inp_enable *** adi,mgc-rx1-ctrl-inp-enable
    .mgc_rx2_ctrl_inp_enable = 0,	// don't use CTRL_IN for RX2 MGC stepping                           // mgc_rx2_ctrl_inp_enable *** adi,mgc-rx2-ctrl-inp-enable
    .mgc_split_table_ctrl_inp_gain_mode = 0,	// N/A when mgc_rx(1,2)_ctrl_inp_enable = 0                         // mgc_split_table_ctrl_inp_gain_mode *** adi,mgc-split-table-ctrl-inp-gain-mode

    /* Gain AGC Control */
    .agc_adc_large_overload_exceed_counter = 10,	// magic AGC setting, see AD9361 docs                               // agc_adc_large_overload_exceed_counter *** adi,agc-adc-large-overload-exceed-counter
    .agc_adc_large_overload_inc_steps = 2,	// magic AGC setting, see AD9361 docs                               // agc_adc_large_overload_inc_steps *** adi,agc-adc-large-overload-inc-steps
    .agc_adc_lmt_small_overload_prevent_gain_inc_enable = 0,	// magic AGC setting, see AD9361 docs                               // agc_adc_lmt_small_overload_prevent_gain_inc_enable *** adi,agc-adc-lmt-small-overload-prevent-gain-inc-enable
    .agc_adc_small_overload_exceed_counter = 10,	// magic AGC setting, see AD9361 docs                               // agc_adc_small_overload_exceed_counter *** adi,agc-adc-small-overload-exceed-counter
    .agc_dig_gain_step_size = 4,	// magic AGC setting, see AD9361 docs                               // agc_dig_gain_step_size *** adi,agc-dig-gain-step-size
    .agc_dig_saturation_exceed_counter = 3,	// magic AGC setting, see AD9361 docs                               // agc_dig_saturation_exceed_counter *** adi,agc-dig-saturation-exceed-counter
    .agc_gain_update_interval_us = 1,	// magic AGC setting, see AD9361 docs                               // agc_gain_update_interval_us *** adi,agc-gain-update-interval-us
    .agc_immed_gain_change_if_large_adc_overload_enable = 0,	// magic AGC setting, see AD9361 docs                               // agc_immed_gain_change_if_large_adc_overload_enable *** adi,agc-immed-gain-change-if-large-adc-overload-enable
    .agc_immed_gain_change_if_large_lmt_overload_enable = 0,	// magic AGC setting, see AD9361 docs                               // agc_immed_gain_change_if_large_lmt_overload_enable *** adi,agc-immed-gain-change-if-large-lmt-overload-enable
    .agc_inner_thresh_high = 10,	// magic AGC setting, see AD9361 docs                               // agc_inner_thresh_high *** adi,agc-inner-thresh-high
    .agc_inner_thresh_high_dec_steps = 1,	// magic AGC setting, see AD9361 docs                               // agc_inner_thresh_high_dec_steps *** adi,agc-inner-thresh-high-dec-steps
    .agc_inner_thresh_low = 12,	// magic AGC setting, see AD9361 docs                               // agc_inner_thresh_low *** adi,agc-inner-thresh-low
    .agc_inner_thresh_low_inc_steps = 1,	// magic AGC setting, see AD9361 docs                               // agc_inner_thresh_low_inc_steps *** adi,agc-inner-thresh-low-inc-steps
    .agc_lmt_overload_large_exceed_counter = 10,	// magic AGC setting, see AD9361 docs                               // agc_lmt_overload_large_exceed_counter *** adi,agc-lmt-overload-large-exceed-counter
    .agc_lmt_overload_large_inc_steps = 2,	// magic AGC setting, see AD9361 docs                               // agc_lmt_overload_large_inc_steps *** adi,agc-lmt-overload-large-inc-steps
    .agc_lmt_overload_small_exceed_counter = 10,	// magic AGC setting, see AD9361 docs                               // agc_lmt_overload_small_exceed_counter *** adi,agc-lmt-overload-small-exceed-counter
    .agc_outer_thresh_high = 5,	// magic AGC setting, see AD9361 docs                               // agc_outer_thresh_high *** adi,agc-outer-thresh-high
    .agc_outer_thresh_high_dec_steps = 2,	// magic AGC setting, see AD9361 docs                               // agc_outer_thresh_high_dec_steps *** adi,agc-outer-thresh-high-dec-steps
    .agc_outer_thresh_low = 18,	// magic AGC setting, see AD9361 docs                               // agc_outer_thresh_low *** adi,agc-outer-thresh-low
    .agc_outer_thresh_low_inc_steps = 2,	// magic AGC setting, see AD9361 docs                               // agc_outer_thresh_low_inc_steps *** adi,agc-outer-thresh-low-inc-steps
    .agc_attack_delay_extra_margin_us = 1,	// magic AGC setting, see AD9361 docs                               // agc_attack_delay_extra_margin_us; *** adi,agc-attack-delay-extra-margin-us
    .agc_sync_for_gain_counter_enable = 0,	// magic AGC setting, see AD9361 docs                               // agc_sync_for_gain_counter_enable *** adi,agc-sync-for-gain-counter-enable

    /* Fast AGC */
    .fagc_dec_pow_measuremnt_duration = 16,	// magic AGC setting, see AD9361 docs                               // fagc_dec_pow_measuremnt_duration ***  adi,fagc-dec-pow-measurement-duration
    .fagc_state_wait_time_ns = 260,	// magic AGC setting, see AD9361 docs                               // fagc_state_wait_time_ns ***  adi,fagc-state-wait-time-ns

    /* Fast AGC - Low Power */
    .fagc_allow_agc_gain_increase = 0,	// magic AGC setting, see AD9361 docs                               // fagc_allow_agc_gain_increase ***  adi,fagc-allow-agc-gain-increase-enable
    .fagc_lp_thresh_increment_time = 5,	// magic AGC setting, see AD9361 docs                               // fagc_lp_thresh_increment_time ***  adi,fagc-lp-thresh-increment-time
    .fagc_lp_thresh_increment_steps = 7,	// magic AGC setting, see AD9361 docs                               // fagc_lp_thresh_increment_steps ***  adi,fagc-lp-thresh-increment-steps

    /* Fast AGC - Lock Level */
    .fagc_lock_level_lmt_gain_increase_en = 1,	// magic AGC setting, see AD9361 docs                               // fagc_lock_level_lmt_gain_increase_en ***  adi,fagc-lock-level-lmt-gain-increase-enable
    .fagc_lock_level_gain_increase_upper_limit = 63,	// magic AGC setting, see AD9361 docs                               // fagc_lock_level_gain_increase_upper_limit ***  adi,fagc-lock-level-gain-increase-upper-limit

    /* Fast AGC - Peak Detectors and Final Settling */
    .fagc_lpf_final_settling_steps = 1,	// magic AGC setting, see AD9361 docs                               // fagc_lpf_final_settling_steps ***  adi,fagc-lpf-final-settling-steps
    .fagc_lmt_final_settling_steps = 1,	// magic AGC setting, see AD9361 docs                               // fagc_lmt_final_settling_steps ***  adi,fagc-lmt-final-settling-steps
    .fagc_final_overrange_count = 3,	// magic AGC setting, see AD9361 docs                               // fagc_final_overrange_count ***  adi,fagc-final-overrange-count

    /* Fast AGC - Final Power Test */
    .fagc_gain_increase_after_gain_lock_en = 0,	// magic AGC setting, see AD9361 docs                               // fagc_gain_increase_after_gain_lock_en ***  adi,fagc-gain-increase-after-gain-lock-enable

    /* Fast AGC - Unlocking the Gain */
    .fagc_gain_index_type_after_exit_rx_mode = 0,	// magic AGC setting, see AD9361 docs                               // fagc_gain_index_type_after_exit_rx_mode ***  adi,fagc-gain-index-type-after-exit-rx-mode
    .fagc_use_last_lock_level_for_set_gain_en = 1,	// magic AGC setting, see AD9361 docs                               // fagc_use_last_lock_level_for_set_gain_en ***  adi,fagc-use-last-lock-level-for-set-gain-enable
    .fagc_rst_gla_stronger_sig_thresh_exceeded_en = 1,	// magic AGC setting, see AD9361 docs                               // fagc_rst_gla_stronger_sig_thresh_exceeded_en ***  adi,fagc-rst-gla-stronger-sig-thresh-exceeded-enable
    .fagc_optimized_gain_offset = 5,	// magic AGC setting, see AD9361 docs                               // fagc_optimized_gain_offset ***  adi,fagc-optimized-gain-offset
    .fagc_rst_gla_stronger_sig_thresh_above_ll = 10,	// magic AGC setting, see AD9361 docs                               // fagc_rst_gla_stronger_sig_thresh_above_ll ***  adi,fagc-rst-gla-stronger-sig-thresh-above-ll
    .fagc_rst_gla_engergy_lost_sig_thresh_exceeded_en = 1,	// magic AGC setting, see AD9361 docs                               // fagc_rst_gla_engergy_lost_sig_thresh_exceeded_en ***  adi,fagc-rst-gla-engergy-lost-sig-thresh-exceeded-enable
    .fagc_rst_gla_engergy_lost_goto_optim_gain_en = 0,	// magic AGC setting, see AD9361 docs                               // fagc_rst_gla_engergy_lost_goto_optim_gain_en ***  adi,fagc-rst-gla-engergy-lost-goto-optim-gain-enable
    .fagc_rst_gla_engergy_lost_sig_thresh_below_ll = 10,	// magic AGC setting, see AD9361 docs                               // fagc_rst_gla_engergy_lost_sig_thresh_below_ll ***  adi,fagc-rst-gla-engergy-lost-sig-thresh-below-ll
    .fagc_energy_lost_stronger_sig_gain_lock_exit_cnt = 3,	// magic AGC setting, see AD9361 docs                               // fagc_energy_lost_stronger_sig_gain_lock_exit_cnt ***  adi,fagc-energy-lost-stronger-sig-gain-lock-exit-cnt
    .fagc_rst_gla_large_adc_overload_en = 1,	// magic AGC setting, see AD9361 docs                               // fagc_rst_gla_large_adc_overload_en ***  adi,fagc-rst-gla-large-adc-overload-enable
    .fagc_rst_gla_large_lmt_overload_en = 1,	// magic AGC setting, see AD9361 docs                               // fagc_rst_gla_large_lmt_overload_en ***  adi,fagc-rst-gla-large-lmt-overload-enable
    .fagc_rst_gla_en_agc_pulled_high_en = 0,	// magic AGC setting, see AD9361 docs                               // fagc_rst_gla_en_agc_pulled_high_en ***  adi,fagc-rst-gla-en-agc-pulled-high-enable
    .fagc_rst_gla_if_en_agc_pulled_high_mode = 0,	// magic AGC setting, see AD9361 docs                               // fagc_rst_gla_if_en_agc_pulled_high_mode ***  adi,fagc-rst-gla-if-en-agc-pulled-high-mode
    .fagc_power_measurement_duration_in_state5 = 64,	// magic AGC setting, see AD9361 docs                               // fagc_power_measurement_duration_in_state5 ***  adi,fagc-power-measurement-duration-in-state5

    /* RSSI Control */
    .rssi_delay = 1,	// settling delay on RSSI algo restart = 1 μs                       // rssi_delay *** adi,rssi-delay
    .rssi_duration = 1000,	// total RSSI measurement duration = 1000 μs                        // rssi_duration *** adi,rssi-duration
    .rssi_restart_mode = GAIN_CHANGE_OCCURS,	// reset RSSI accumulator on gain change event                  // rssi_restart_mode *** adi,rssi-restart-mode
    .rssi_unit_is_rx_samples_enable = 0,	// RSSI control values are in microseconds                          // rssi_unit_is_rx_samples_enable *** adi,rssi-unit-is-rx-samples-enable
    .rssi_wait = 1,	// wait 1 μs between RSSI measurements                              // rssi_wait *** adi,rssi-wait

    /* Aux ADC Control */
    /* bladeRF Micro: N/A, pin tied to GND */
    .aux_adc_decimation = 256,	// AuxADC decimate by 256                                           // aux_adc_decimation *** adi,aux-adc-decimation
    .aux_adc_rate = 40000000UL,	// AuxADC sample rate 40 MHz                                        // aux_adc_rate *** adi,aux-adc-rate

    /* AuxDAC Control */
    /* bladeRF Micro: AuxDAC1 is TP7 and AUXDAC_TRIM, AuxDAC2 is TP8 */
    .aux_dac_manual_mode_enable = 1,	// AuxDAC does not slave the ENSM                                   // aux_dac_manual_mode_enable ***  adi,aux-dac-manual-mode-enable
    .aux_dac1_default_value_mV = 0,	// AuxDAC1 default value = 0 mV                                     // aux_dac1_default_value_mV ***  adi,aux-dac1-default-value-mV
    .aux_dac1_active_in_rx_enable = 0,	// N/A when aux_dac_manual_mode_enable = 1                          // aux_dac1_active_in_rx_enable ***  adi,aux-dac1-active-in-rx-enable
    .aux_dac1_active_in_tx_enable = 0,	// N/A when aux_dac_manual_mode_enable = 1                          // aux_dac1_active_in_tx_enable ***  adi,aux-dac1-active-in-tx-enable
    .aux_dac1_active_in_alert_enable = 0,	// N/A when aux_dac_manual_mode_enable = 1                          // aux_dac1_active_in_alert_enable ***  adi,aux-dac1-active-in-alert-enable
    .aux_dac1_rx_delay_us = 0,	// N/A when aux_dac_manual_mode_enable = 1                          // aux_dac1_rx_delay_us ***  adi,aux-dac1-rx-delay-us
    .aux_dac1_tx_delay_us = 0,	// N/A when aux_dac_manual_mode_enable = 1                          // aux_dac1_tx_delay_us ***  adi,aux-dac1-tx-delay-us
    .aux_dac2_default_value_mV = 0,	// AuxDAC2 default value = 0 mV                                     // aux_dac2_default_value_mV ***  adi,aux-dac2-default-value-mV
    .aux_dac2_active_in_rx_enable = 0,	// N/A when aux_dac_manual_mode_enable = 1                          // aux_dac2_active_in_rx_enable ***  adi,aux-dac2-active-in-rx-enable
    .aux_dac2_active_in_tx_enable = 0,	// N/A when aux_dac_manual_mode_enable = 1                          // aux_dac2_active_in_tx_enable ***  adi,aux-dac2-active-in-tx-enable
    .aux_dac2_active_in_alert_enable = 0,	// N/A when aux_dac_manual_mode_enable = 1                          // aux_dac2_active_in_alert_enable ***  adi,aux-dac2-active-in-alert-enable
    .aux_dac2_rx_delay_us = 0,	// N/A when aux_dac_manual_mode_enable = 1                          // aux_dac2_rx_delay_us ***  adi,aux-dac2-rx-delay-us
    .aux_dac2_tx_delay_us = 0,	// N/A when aux_dac_manual_mode_enable = 1                          // aux_dac2_tx_delay_us ***  adi,aux-dac2-tx-delay-us

    /* Temperature Sensor Control */
    .temp_sense_decimation = 256,	// Temperature sensor decimate by 256                               // temp_sense_decimation *** adi,temp-sense-decimation
    .temp_sense_measurement_interval_ms = 1000,	// Measure temperature every 1000 ms                                // temp_sense_measurement_interval_ms *** adi,temp-sense-measurement-interval-ms
    .temp_sense_offset_signed = 206,	// Offset = +206 degrees C                                          // temp_sense_offset_signed *** adi,temp-sense-offset-signed
    .temp_sense_periodic_measurement_enable = 1,	// Periodic temperature measurements enabled                        // temp_sense_periodic_measurement_enable *** adi,temp-sense-periodic-measurement-enable

    /* Control Out Setup */
    /* See https://wiki.analog.com/resources/tools-software/linux-drivers/iio-transceiver/ad9361-customization#control_output_setup */
    .ctrl_outs_enable_mask = 0xFF,	// Enable all CTRL_OUT bits                                         // ctrl_outs_enable_mask *** adi,ctrl-outs-enable-mask
    .ctrl_outs_index = 7,	// CTRL_OUT index is 0                                              // ctrl_outs_index *** adi,ctrl-outs-index

    /* External LNA Control */
    /* bladeRF Micro: GPO_0 is TP3, GPO_1 is TP4 */
    .elna_settling_delay_ns = 0,	// N/A when elna_rx(1,2)_gpo(0,1)_control_enable = 0                // elna_settling_delay_ns *** adi,elna-settling-delay-ns
    .elna_gain_mdB = 0,	// MUST be 0 when elna_rx(1,2)_gpo(0,1)_control_enable = 0          // elna_gain_mdB *** adi,elna-gain-mdB
    .elna_bypass_loss_mdB = 0,	// MUST be 0 when elna_rx(1,2)_gpo(0,1)_control_enable = 0          // elna_bypass_loss_mdB *** adi,elna-bypass-loss-mdB
    .elna_rx1_gpo0_control_enable = 0,	// Ext LNA Ctrl bit in Rx1 gain table does NOT set GPO0 state       // elna_rx1_gpo0_control_enable *** adi,elna-rx1-gpo0-control-enable
    .elna_rx2_gpo1_control_enable = 0,	// Ext LNA Ctrl bit in Rx2 gain table does NOT set GPO1 state       // elna_rx2_gpo1_control_enable *** adi,elna-rx2-gpo1-control-enable
    .elna_gaintable_all_index_enable = 0,	// N/A when elna_rx(1,2)_gpo(0,1)_control_enable = 0                // elna_gaintable_all_index_enable *** adi,elna-gaintable-all-index-enable

    /* Digital Interface Control */
#ifdef ENABLE_AD9361_DIGITAL_INTERFACE_TIMING_VERIFICATION
    /* Calibrate the digital interface delay (hardware validation) */
    .digital_interface_tune_skip_mode = 0,	// Don't skip digital interface tuning                              // digital_interface_tune_skip_mode *** adi,digital-interface-tune-skip-mode
#else
    /* Use hardcoded digital interface delay values (production) */
    .digital_interface_tune_skip_mode = 2,	// Skip RX and TX tuning; use hardcoded values below                // digital_interface_tune_skip_mode *** adi,digital-interface-tune-skip-mode
#endif // ENABLE_AD9361_DIGITAL_INTERFACE_TIMING_VERIFICATION
    .digital_interface_tune_fir_disable = 0,	// ?? UNDOCUMENTED ??                                               // digital_interface_tune_fir_disable *** adi,digital-interface-tune-fir-disable
    .pp_tx_swap_enable = 1,	// Swap I and Q (spectral inversion)                                // pp_tx_swap_enable *** adi,pp-tx-swap-enable
    .pp_rx_swap_enable = 1,	// Swap I and Q (spectral inversion)                                // pp_rx_swap_enable *** adi,pp-rx-swap-enable
    .tx_channel_swap_enable = 0,	// Don't swap TX1 and TX2                                           // tx_channel_swap_enable *** adi,tx-channel-swap-enable
    .rx_channel_swap_enable = 0,	// Don't swap RX1 and RX2                                           // rx_channel_swap_enable *** adi,rx-channel-swap-enable
    .rx_frame_pulse_mode_enable = 1,	// Toggle RX_FRAME with 50% duty cycle                              // rx_frame_pulse_mode_enable *** adi,rx-frame-pulse-mode-enable
    .two_t_two_r_timing_enable = 0,	// Data port timing reflects # of enabled signal paths              // two_t_two_r_timing_enable *** adi,2t2r-timing-enable
    .invert_data_bus_enable = 0,	// Don't invert data bus                                            // invert_data_bus_enable *** adi,invert-data-bus-enable
    .invert_data_clk_enable = 0,	// Don't invert data clock                                          // invert_data_clk_enable *** adi,invert-data-clk-enable
    .fdd_alt_word_order_enable = 0,	// MUST be 0 when lvds_mode_enable = 1                              // fdd_alt_word_order_enable *** adi,fdd-alt-word-order-enable
    .invert_rx_frame_enable = 0,	// Don't invert RX_FRAME                                            // invert_rx_frame_enable *** adi,invert-rx-frame-enable
    .fdd_rx_rate_2tx_enable = 0,	// Don't make RX sample rate 2x the TX sample rate                  // fdd_rx_rate_2tx_enable *** adi,fdd-rx-rate-2tx-enable
    .swap_ports_enable = 0,	// MUST be 0 when lvds_mode_enable = 1                              // swap_ports_enable *** adi,swap-ports-enable
    .single_data_rate_enable = 0,	// MUST be 0 when lvds_mode_enable = 1                              // single_data_rate_enable *** adi,single-data-rate-enable
    .lvds_mode_enable = 1,	// Use LVDS mode on data port                                       // lvds_mode_enable *** adi,lvds-mode-enable
    .half_duplex_mode_enable = 0,	// MUST be 0 when lvds_mode_enable = 1                              // half_duplex_mode_enable *** adi,half-duplex-mode-enable
    .single_port_mode_enable = 0,	// MUST be 0 when lvds_mode_enable = 1                              // single_port_mode_enable *** adi,single-port-mode-enable
    .full_port_enable = 0,	// MUST be 0 when lvds_mode_enable = 1                              // full_port_enable *** adi,full-port-enable
    .full_duplex_swap_bits_enable = 0,	// MUST be 0 when lvds_mode_enable = 1                              // full_duplex_swap_bits_enable *** adi,full-duplex-swap-bits-enable
    .delay_rx_data = 0,	// RX_DATA delay rel to RX_FRAME = 0 DATA_CLK/2 cycles              // delay_rx_data *** adi,delay-rx-data
    // Approx 0.3 ns/LSB on next 4 values
    .rx_data_clock_delay = 5,	// DATA_CLK delay = 1.5 ns                                          // rx_data_clock_delay *** adi,rx-data-clock-delay
    .rx_data_delay = 0,	// RX_DATA/RX_FRAME delay = 0 ns                                    // rx_data_delay *** adi,rx-data-delay
    .tx_fb_clock_delay = 0,	// FB_CLK delay = 0 ns                                              // tx_fb_clock_delay *** adi,tx-fb-clock-delay
    .tx_data_delay = 5,	// TX_DATA/TX_FRAME delay = 1.5 ns                                  // tx_data_delay *** adi,tx-data-delay
    .lvds_bias_mV = 300,	// LVDS driver bias 300 mV                                          // lvds_bias_mV *** adi,lvds-bias-mV
    .lvds_rx_onchip_termination_enable = 1,	// Enable LVDS on-chip termination                                  // lvds_rx_onchip_termination_enable *** adi,lvds-rx-onchip-termination-enable
    .rx1rx2_phase_inversion_en = 1,	// RX1 and RX2 are not phase-aligned                                // rx1rx2_phase_inversion_en *** adi,rx1-rx2-phase-inversion-enable
    .lvds_invert1_control = 0xFF,	// Default signal inversion mappings                                // lvds_invert1_control *** adi,lvds-invert1-control
    .lvds_invert2_control = 0x0F,	// Default signal inversion mappings                                // lvds_invert2_control *** adi,lvds-invert2-control
    .clk_out_drive = 1,	// CLK_OUT drive increased by ~20%                                  // clk_out_drive
    .dataclk_drive = 1,	// DATA_CLK drive increased by ~20%                                 // dataclk_drive
    .data_port_drive = 1,	// Data port drive increased by ~20%                                // data_port_drive
    .clk_out_slew = 0,	// CLK_OUT minimum slew (fastest rise/fall)                         // clk_out_slew
    .dataclk_slew = 0,	// DATA_CLK minimum slew (fastest rise/fall)                        // dataclk_slew
    .data_port_slew = 0,	// Data port minimum slew (fastest rise/fall)                       // data_port_slew

    /* GPO Control */
    .gpo0_inactive_state_high_enable = 0,	// GPO0 is LOW in Sleep/Wait/Alert states                           // gpo0_inactive_state_high_enable *** adi,gpo0-inactive-state-high-enable
    .gpo1_inactive_state_high_enable = 0,	// GPO1 is LOW in Sleep/Wait/Alert states                           // gpo1_inactive_state_high_enable *** adi,gpo1-inactive-state-high-enable
    .gpo2_inactive_state_high_enable = 0,	// GPO2 is LOW in Sleep/Wait/Alert states                           // gpo2_inactive_state_high_enable *** adi,gpo2-inactive-state-high-enable
    .gpo3_inactive_state_high_enable = 0,	// GPO3 is LOW in Sleep/Wait/Alert states                           // gpo3_inactive_state_high_enable *** adi,gpo3-inactive-state-high-enable
    .gpo0_slave_rx_enable = 0,	// GPO0 does not change state when entering RX state                // gpo0_slave_rx_enable *** adi,gpo0-slave-rx-enable
    .gpo0_slave_tx_enable = 0,	// GPO0 does not change state when entering TX state                // gpo0_slave_tx_enable *** adi,gpo0-slave-tx-enable
    .gpo1_slave_rx_enable = 0,	// GPO1 does not change state when entering RX state                // gpo1_slave_rx_enable *** adi,gpo1-slave-rx-enable
    .gpo1_slave_tx_enable = 0,	// GPO1 does not change state when entering TX state                // gpo1_slave_tx_enable *** adi,gpo1-slave-tx-enable
    .gpo2_slave_rx_enable = 0,	// GPO2 does not change state when entering RX state                // gpo2_slave_rx_enable *** adi,gpo2-slave-rx-enable
    .gpo2_slave_tx_enable = 0,	// GPO2 does not change state when entering TX state                // gpo2_slave_tx_enable *** adi,gpo2-slave-tx-enable
    .gpo3_slave_rx_enable = 0,	// GPO3 does not change state when entering RX state                // gpo3_slave_rx_enable *** adi,gpo3-slave-rx-enable
    .gpo3_slave_tx_enable = 0,	// GPO3 does not change state when entering TX state                // gpo3_slave_tx_enable *** adi,gpo3-slave-tx-enable
    .gpo0_rx_delay_us = 0,	// N/A when gpo0_slave_rx_enable = 0                                // gpo0_rx_delay_us *** adi,gpo0-rx-delay-us
    .gpo0_tx_delay_us = 0,	// N/A when gpo0_slave_tx_enable = 0                                // gpo0_tx_delay_us *** adi,gpo0-tx-delay-us
    .gpo1_rx_delay_us = 0,	// N/A when gpo1_slave_rx_enable = 0                                // gpo1_rx_delay_us *** adi,gpo1-rx-delay-us
    .gpo1_tx_delay_us = 0,	// N/A when gpo1_slave_tx_enable = 0                                // gpo1_tx_delay_us *** adi,gpo1-tx-delay-us
    .gpo2_rx_delay_us = 0,	// N/A when gpo2_slave_rx_enable = 0                                // gpo2_rx_delay_us *** adi,gpo2-rx-delay-us
    .gpo2_tx_delay_us = 0,	// N/A when gpo2_slave_tx_enable = 0                                // gpo2_tx_delay_us *** adi,gpo2-tx-delay-us
    .gpo3_rx_delay_us = 0,	// N/A when gpo3_slave_rx_enable = 0                                // gpo3_rx_delay_us *** adi,gpo3-rx-delay-us
    .gpo3_tx_delay_us = 0,	// N/A when gpo3_slave_tx_enable = 0                                // gpo3_tx_delay_us *** adi,gpo3-tx-delay-us

    /* Tx Monitor Control */
    /* bladeRF Micro: N/A, TX_MON1 and TX_MON2 tied to GND */
    .low_high_gain_threshold_mdB = 37000,	// N/A                                                              // low_high_gain_threshold_mdB *** adi,txmon-low-high-thresh
    .low_gain_dB = 0,	// N/A                                                              // low_gain_dB *** adi,txmon-low-gain
    .high_gain_dB = 24,	// N/A                                                              // high_gain_dB *** adi,txmon-high-gain
    .tx_mon_track_en = 0,	// N/A                                                              // tx_mon_track_en *** adi,txmon-dc-tracking-enable
    .one_shot_mode_en = 0,	// N/A                                                              // one_shot_mode_en *** adi,txmon-one-shot-mode-enable
    .tx_mon_delay = 511,	// N/A                                                              // tx_mon_delay *** adi,txmon-delay
    .tx_mon_duration = 8192,	// N/A                                                              // tx_mon_duration *** adi,txmon-duration
    .tx1_mon_front_end_gain = 2,	// N/A                                                              // tx1_mon_front_end_gain *** adi,txmon-1-front-end-gain
    .tx2_mon_front_end_gain = 2,	// N/A                                                              // tx2_mon_front_end_gain *** adi,txmon-2-front-end-gain
    .tx1_mon_lo_cm = 48,	// N/A                                                              // tx1_mon_lo_cm *** adi,txmon-1-lo-cm
    .tx2_mon_lo_cm = 48,	// N/A                                                              // tx2_mon_lo_cm *** adi,txmon-2-lo-cm

    /* GPIO definitions */
    /* Bit of the FPGA RFFE control register, driven through the accessor
     * table in platform_bladerf2. The device handle goes into .extra at
     * init time, where it is known. */
    .gpio_resetb = { .number = RFFE_CONTROL_RESET_N,
                     .platform_ops = &bladerf2_gpio_ops },	// Reset using RFFE bit 0                                   // gpio_resetb *** reset-gpios

    /* MCS Sync */
    .gpio_sync = { .number = -1 },	// Future use (MCS Sync)                                            // gpio_sync *** sync-gpios
    .gpio_cal_sw1 = { .number = -1 },	// Future use (MCS Sync)                                            // gpio_cal_sw1 *** cal-sw1-gpios
    .gpio_cal_sw2 = { .number = -1 },	// Future use (MCS Sync)                                            // gpio_cal_sw2 *** cal-sw2-gpios

    /* External LO clocks */
    /* SPI reaches the device through the accessor table in
     * platform_bladerf2; the handle goes into .extra at init time. */
    .spi_param = { .platform_ops = &bladerf2_spi_ops },

    /* FPGA AXI interface core init (деасерт RSTN у axi_adc_init) */
    .rx_adc_init = &bladerf2_rx_adc_init,
    .tx_dac_init = &bladerf2_tx_dac_init,

    .ad9361_rfpll_ext_recalc_rate = NULL,	// Future use (RX_EXT_LO, TX_EXT_LO control)                        // (*ad9361_rfpll_ext_recalc_rate)()
    .ad9361_rfpll_ext_round_rate = NULL,	// Future use (RX_EXT_LO, TX_EXT_LO control)                        // (*ad9361_rfpll_ext_round_rate)()
    .ad9361_rfpll_ext_set_rate = NULL,	// Future use (RX_EXT_LO, TX_EXT_LO control)                        // (*ad9361_rfpll_ext_set_rate)()
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
