#include "driver/thirdparty/adi/ad9361_api.h"
#include "driver/thirdparty/adi/platform.h"

/**
 * Reference:
 * https://wiki.analog.com/resources/tools-software/linux-drivers/iio-transceiver/ad9361-customization
 *
 * N/A 		= not applicable due to other setting; changes may unmask these
 * DEFAULT 	= changed during device initialization
 */

AD9361_InitParam ad9361_init_params = {
	/* Device selection */
	ID_AD9361,		// AD9361 RF Agile Transceiver										// dev_sel

	/* Identification number */
	0,				// Chip ID 0														// id_no

	/* Reference Clock */
	38400000UL,		// RefClk = 38.4 MHz												// reference_clk_rate

	/* Base Configuration */
	1,				// use 2Rx2Tx mode													// two_rx_two_tx_mode_enable *** adi,2rx-2tx-mode-enable
	1,				// N/A when two_rx_two_tx_mode_enable = 1							// one_rx_one_tx_mode_use_rx_num *** adi,1rx-1tx-mode-use-rx-num
	1,				// N/A when two_rx_two_tx_mode_enable = 1							// one_rx_one_tx_mode_use_tx_num *** adi,1rx-1tx-mode-use-tx-num
	1,				// use FDD mode														// frequency_division_duplex_mode_enable *** adi,frequency-division-duplex-mode-enable
	1,				// use independent FDD mode											// frequency_division_duplex_independent_mode_enable *** adi,frequency-division-duplex-independent-mode-enable
	0,				// N/A when frequency_division_duplex_mode_enable = 1				// tdd_use_dual_synth_mode_enable *** adi,tdd-use-dual-synth-mode-enable
	0,				// N/A when frequency_division_duplex_mode_enable = 1				// tdd_skip_vco_cal_enable *** adi,tdd-skip-vco-cal-enable
	0,				// TX fastlock delay = 0 ns											// tx_fastlock_delay_ns *** adi,tx-fastlock-delay-ns
	0,				// RX fastlock delay = 0 ns											// rx_fastlock_delay_ns *** adi,rx-fastlock-delay-ns
	0,				// RX fastlock pin control disabled									// rx_fastlock_pincontrol_enable *** adi,rx-fastlock-pincontrol-enable
	0,				// TX fastlock pin control disabled									// tx_fastlock_pincontrol_enable *** adi,tx-fastlock-pincontrol-enable
	0,				// use internal RX LO												// external_rx_lo_enable *** adi,external-rx-lo-enable
	0,				// use internal TX LO												// external_tx_lo_enable *** adi,external-tx-lo-enable
	5,				// apply new tracking word: on gain change, after exiting RX state	// dc_offset_tracking_update_event_mask *** adi,dc-offset-tracking-update-event-mask
	6,				// atten value for DC tracking, RX LO > 4 GHz						// dc_offset_attenuation_high_range *** adi,dc-offset-attenuation-high-range
	5,				// atten value for DC tracking, RX LO < 4 GHz						// dc_offset_attenuation_low_range *** adi,dc-offset-attenuation-low-range
	0x28,			// loop gain for DC tracking, RX LO > 4 GHz							// dc_offset_count_high_range *** adi,dc-offset-count-high-range
	0x32,			// loop gain for DC tracking, RX LO < 4 GHz							// dc_offset_count_low_range *** adi,dc-offset-count-low-range
	0,				// use full gain table												// split_gain_table_mode_enable *** adi,split-gain-table-mode-enable
	MAX_SYNTH_FREF,	// f_ref window 80 MHz												// trx_synthesizer_target_fref_overwrite_hz *** adi,trx-synthesizer-target-fref-overwrite-hz
	0,				// don't use improved RX QEC tracking								// qec_tracking_slow_mode_enable *** adi,qec-tracking-slow-mode-enable

	/* ENSM Control */
	0,				// use level mode on ENABLE and TXNRX pins							// ensm_enable_pin_pulse_mode_enable *** adi,ensm-enable-pin-pulse-mode-enable
	0,				// use SPI writes for ENSM state, not ENABLE/TXNRX pins				// ensm_enable_txnrx_control_enable *** adi,ensm-enable-txnrx-control-enable

	/* LO Control */
	2400000000UL,	// DEFAULT															// rx_synthesizer_frequency_hz *** adi,rx-synthesizer-frequency-hz
	2400000000UL,	// DEFAULT															// tx_synthesizer_frequency_hz *** adi,tx-synthesizer-frequency-hz

	/* Rate & BW Control */
	{ 983040000, 245760000, 122880000, 61440000, 30720000, 30720000 },	// DEFAULT		// uint32_t	rx_path_clock_frequencies[6] *** adi,rx-path-clock-frequencies
	{ 983040000, 122880000, 122880000, 61440000, 30720000, 30720000 },	// DEFAULT		// uint32_t	tx_path_clock_frequencies[6] *** adi,tx-path-clock-frequencies
	18000000,		// DEFAULT															// rf_rx_bandwidth_hz *** adi,rf-rx-bandwidth-hz
	18000000,		// DEFAULT															// rf_tx_bandwidth_hz *** adi,rf-tx-bandwidth-hz

	/* RF Port Control */
	0,				// DEFAULT															// rx_rf_port_input_select *** adi,rx-rf-port-input-select
	0,				// DEFAULT															// tx_rf_port_input_select *** adi,tx-rf-port-input-select

	/* TX Attenuation Control */
	10000,			// DEFAULT															// tx_attenuation_mdB *** adi,tx-attenuation-mdB
	0,				// N/A when frequency_division_duplex_mode_enable = 1				// update_tx_gain_in_alert_enable *** adi,update-tx-gain-in-alert-enable

	/* Reference Clock Control */
	1,				// Expect external clock into XTALN									// xo_disable_use_ext_refclk_enable *** adi,xo-disable-use-ext-refclk-enable
	{3, 5920},		// ~0 ppm DCXO trim (N/A if ext clk)								// dcxo_coarse_and_fine_tune[2] *** adi,dcxo-coarse-and-fine-tune
	CLKOUT_DISABLE,	// disable clkout pin (see enum ad9361_clkout)						// clk_output_mode_select *** adi,clk-output-mode-select

	/* Gain Control */
	RF_GAIN_SLOWATTACK_AGC,	// RX1 BLADERF_GAIN_DEFAULT = slow attack AGC				// gc_rx1_mode *** adi,gc-rx1-mode
	RF_GAIN_SLOWATTACK_AGC,	// RX2 BLADERF_GAIN_DEFAULT = slow attack AGC				// gc_rx2_mode *** adi,gc-rx2-mode
	58,				// magic AGC setting, see AD9361 docs								// gc_adc_large_overload_thresh *** adi,gc-adc-large-overload-thresh
	4,				// magic AGC setting, see AD9361 docs								// gc_adc_ovr_sample_size *** adi,gc-adc-ovr-sample-size
	47,				// magic AGC setting, see AD9361 docs								// gc_adc_small_overload_thresh *** adi,gc-adc-small-overload-thresh
	8192,			// magic AGC setting, see AD9361 docs								// gc_dec_pow_measurement_duration *** adi,gc-dec-pow-measurement-duration
	0,				// magic AGC setting, see AD9361 docs								// gc_dig_gain_enable *** adi,gc-dig-gain-enable
	800,			// magic AGC setting, see AD9361 docs								// gc_lmt_overload_high_thresh *** adi,gc-lmt-overload-high-thresh
	704,			// magic AGC setting, see AD9361 docs								// gc_lmt_overload_low_thresh *** adi,gc-lmt-overload-low-thresh
	24,				// magic AGC setting, see AD9361 docs								// gc_low_power_thresh *** adi,gc-low-power-thresh
	15,				// magic AGC setting, see AD9361 docs								// gc_max_dig_gain *** adi,gc-max-dig-gain

	/* Gain MGC Control */
	2,				// N/A when mgc_rx(1,2)_ctrl_inp_enable = 0							// mgc_dec_gain_step *** adi,mgc-dec-gain-step
	2,				// N/A when mgc_rx(1,2)_ctrl_inp_enable = 0							// mgc_inc_gain_step *** adi,mgc-inc-gain-step
	0,				// don't use CTRL_IN for RX1 MGC stepping							// mgc_rx1_ctrl_inp_enable *** adi,mgc-rx1-ctrl-inp-enable
	0,				// don't use CTRL_IN for RX2 MGC stepping							// mgc_rx2_ctrl_inp_enable *** adi,mgc-rx2-ctrl-inp-enable
	0,				// N/A when mgc_rx(1,2)_ctrl_inp_enable = 0							// mgc_split_table_ctrl_inp_gain_mode *** adi,mgc-split-table-ctrl-inp-gain-mode

	/* Gain AGC Control */
	10,				// magic AGC setting, see AD9361 docs								// agc_adc_large_overload_exceed_counter *** adi,agc-adc-large-overload-exceed-counter
	2,				// magic AGC setting, see AD9361 docs								// agc_adc_large_overload_inc_steps *** adi,agc-adc-large-overload-inc-steps
	0,				// magic AGC setting, see AD9361 docs								// agc_adc_lmt_small_overload_prevent_gain_inc_enable *** adi,agc-adc-lmt-small-overload-prevent-gain-inc-enable
	10,				// magic AGC setting, see AD9361 docs								// agc_adc_small_overload_exceed_counter *** adi,agc-adc-small-overload-exceed-counter
	4,				// magic AGC setting, see AD9361 docs								// agc_dig_gain_step_size *** adi,agc-dig-gain-step-size
	3,				// magic AGC setting, see AD9361 docs								// agc_dig_saturation_exceed_counter *** adi,agc-dig-saturation-exceed-counter
	1000,			// magic AGC setting, see AD9361 docs								// agc_gain_update_interval_us *** adi,agc-gain-update-interval-us
	0,				// magic AGC setting, see AD9361 docs								// agc_immed_gain_change_if_large_adc_overload_enable *** adi,agc-immed-gain-change-if-large-adc-overload-enable
	0,				// magic AGC setting, see AD9361 docs								// agc_immed_gain_change_if_large_lmt_overload_enable *** adi,agc-immed-gain-change-if-large-lmt-overload-enable
	10,				// magic AGC setting, see AD9361 docs								// agc_inner_thresh_high *** adi,agc-inner-thresh-high
	1,				// magic AGC setting, see AD9361 docs								// agc_inner_thresh_high_dec_steps *** adi,agc-inner-thresh-high-dec-steps
	12,				// magic AGC setting, see AD9361 docs								// agc_inner_thresh_low *** adi,agc-inner-thresh-low
	1,				// magic AGC setting, see AD9361 docs								// agc_inner_thresh_low_inc_steps *** adi,agc-inner-thresh-low-inc-steps
	10,				// magic AGC setting, see AD9361 docs								// agc_lmt_overload_large_exceed_counter *** adi,agc-lmt-overload-large-exceed-counter
	2,				// magic AGC setting, see AD9361 docs								// agc_lmt_overload_large_inc_steps *** adi,agc-lmt-overload-large-inc-steps
	10,				// magic AGC setting, see AD9361 docs								// agc_lmt_overload_small_exceed_counter *** adi,agc-lmt-overload-small-exceed-counter
	5,				// magic AGC setting, see AD9361 docs								// agc_outer_thresh_high *** adi,agc-outer-thresh-high
	2,				// magic AGC setting, see AD9361 docs								// agc_outer_thresh_high_dec_steps *** adi,agc-outer-thresh-high-dec-steps
	18,				// magic AGC setting, see AD9361 docs								// agc_outer_thresh_low *** adi,agc-outer-thresh-low
	2,				// magic AGC setting, see AD9361 docs								// agc_outer_thresh_low_inc_steps *** adi,agc-outer-thresh-low-inc-steps
	1,				// magic AGC setting, see AD9361 docs								// agc_attack_delay_extra_margin_us; *** adi,agc-attack-delay-extra-margin-us
	0,				// magic AGC setting, see AD9361 docs								// agc_sync_for_gain_counter_enable *** adi,agc-sync-for-gain-counter-enable

	/* Fast AGC */
	64,				// magic AGC setting, see AD9361 docs								// fagc_dec_pow_measuremnt_duration ***  adi,fagc-dec-pow-measurement-duration
	260,			// magic AGC setting, see AD9361 docs								// fagc_state_wait_time_ns ***  adi,fagc-state-wait-time-ns

	/* Fast AGC - Low Power */
	0,				// magic AGC setting, see AD9361 docs								// fagc_allow_agc_gain_increase ***  adi,fagc-allow-agc-gain-increase-enable
	5,				// magic AGC setting, see AD9361 docs								// fagc_lp_thresh_increment_time ***  adi,fagc-lp-thresh-increment-time
	1,				// magic AGC setting, see AD9361 docs								// fagc_lp_thresh_increment_steps ***  adi,fagc-lp-thresh-increment-steps

	/* Fast AGC - Lock Level */
	10,				// magic AGC setting, see AD9361 docs								// fagc_lock_level ***  adi,fagc-lock-level
	1,				// magic AGC setting, see AD9361 docs								// fagc_lock_level_lmt_gain_increase_en ***  adi,fagc-lock-level-lmt-gain-increase-enable
	5,				// magic AGC setting, see AD9361 docs								// fagc_lock_level_gain_increase_upper_limit ***  adi,fagc-lock-level-gain-increase-upper-limit

	/* Fast AGC - Peak Detectors and Final Settling */
	1,				// magic AGC setting, see AD9361 docs								// fagc_lpf_final_settling_steps ***  adi,fagc-lpf-final-settling-steps
	1,				// magic AGC setting, see AD9361 docs								// fagc_lmt_final_settling_steps ***  adi,fagc-lmt-final-settling-steps
	3,				// magic AGC setting, see AD9361 docs								// fagc_final_overrange_count ***  adi,fagc-final-overrange-count

	/* Fast AGC - Final Power Test */
	0,				// magic AGC setting, see AD9361 docs								// fagc_gain_increase_after_gain_lock_en ***  adi,fagc-gain-increase-after-gain-lock-enable

	/* Fast AGC - Unlocking the Gain */
	0,				// magic AGC setting, see AD9361 docs								// fagc_gain_index_type_after_exit_rx_mode ***  adi,fagc-gain-index-type-after-exit-rx-mode
	1,				// magic AGC setting, see AD9361 docs								// fagc_use_last_lock_level_for_set_gain_en ***  adi,fagc-use-last-lock-level-for-set-gain-enable
	1,				// magic AGC setting, see AD9361 docs								// fagc_rst_gla_stronger_sig_thresh_exceeded_en ***  adi,fagc-rst-gla-stronger-sig-thresh-exceeded-enable
	5,				// magic AGC setting, see AD9361 docs								// fagc_optimized_gain_offset ***  adi,fagc-optimized-gain-offset
	10,				// magic AGC setting, see AD9361 docs								// fagc_rst_gla_stronger_sig_thresh_above_ll ***  adi,fagc-rst-gla-stronger-sig-thresh-above-ll
	1,				// magic AGC setting, see AD9361 docs								// fagc_rst_gla_engergy_lost_sig_thresh_exceeded_en ***  adi,fagc-rst-gla-engergy-lost-sig-thresh-exceeded-enable
	1,				// magic AGC setting, see AD9361 docs								// fagc_rst_gla_engergy_lost_goto_optim_gain_en ***  adi,fagc-rst-gla-engergy-lost-goto-optim-gain-enable
	10,				// magic AGC setting, see AD9361 docs								// fagc_rst_gla_engergy_lost_sig_thresh_below_ll ***  adi,fagc-rst-gla-engergy-lost-sig-thresh-below-ll
	8,				// magic AGC setting, see AD9361 docs								// fagc_energy_lost_stronger_sig_gain_lock_exit_cnt ***  adi,fagc-energy-lost-stronger-sig-gain-lock-exit-cnt
	1,				// magic AGC setting, see AD9361 docs								// fagc_rst_gla_large_adc_overload_en ***  adi,fagc-rst-gla-large-adc-overload-enable
	1,				// magic AGC setting, see AD9361 docs								// fagc_rst_gla_large_lmt_overload_en ***  adi,fagc-rst-gla-large-lmt-overload-enable
	0,				// magic AGC setting, see AD9361 docs								// fagc_rst_gla_en_agc_pulled_high_en ***  adi,fagc-rst-gla-en-agc-pulled-high-enable
	0,				// magic AGC setting, see AD9361 docs								// fagc_rst_gla_if_en_agc_pulled_high_mode ***  adi,fagc-rst-gla-if-en-agc-pulled-high-mode
	64,				// magic AGC setting, see AD9361 docs								// fagc_power_measurement_duration_in_state5 ***  adi,fagc-power-measurement-duration-in-state5

	/* RSSI Control */
	1,				// settling delay on RSSI algo restart = 1 μs						// rssi_delay *** adi,rssi-delay
	1000,			// total RSSI measurement duration = 1000 μs						// rssi_duration *** adi,rssi-duration
	GAIN_CHANGE_OCCURS,	// reset RSSI accumulator on gain change event					// rssi_restart_mode *** adi,rssi-restart-mode
	0,				// RSSI control values are in microseconds							// rssi_unit_is_rx_samples_enable *** adi,rssi-unit-is-rx-samples-enable
	1,				// wait 1 μs between RSSI measurements								// rssi_wait *** adi,rssi-wait

	/* Aux ADC Control */
	/* bladeRF Micro: N/A, pin tied to GND */
	256,			// AuxADC decimate by 256											// aux_adc_decimation *** adi,aux-adc-decimation
	40000000UL,		// AuxADC sample rate 40 MHz										// aux_adc_rate *** adi,aux-adc-rate

	/* AuxDAC Control */
	/* bladeRF Micro: AuxDAC1 is TP7 and AUXDAC_TRIM, AuxDAC2 is TP8 */
	1,				// AuxDAC does not slave the ENSM									// aux_dac_manual_mode_enable ***  adi,aux-dac-manual-mode-enable
	0,				// AuxDAC1 default value = 0 mV										// aux_dac1_default_value_mV ***  adi,aux-dac1-default-value-mV
	0,				// N/A when aux_dac_manual_mode_enable = 1							// aux_dac1_active_in_rx_enable ***  adi,aux-dac1-active-in-rx-enable
	0,				// N/A when aux_dac_manual_mode_enable = 1							// aux_dac1_active_in_tx_enable ***  adi,aux-dac1-active-in-tx-enable
	0,				// N/A when aux_dac_manual_mode_enable = 1							// aux_dac1_active_in_alert_enable ***  adi,aux-dac1-active-in-alert-enable
	0,				// N/A when aux_dac_manual_mode_enable = 1							// aux_dac1_rx_delay_us ***  adi,aux-dac1-rx-delay-us
	0,				// N/A when aux_dac_manual_mode_enable = 1							// aux_dac1_tx_delay_us ***  adi,aux-dac1-tx-delay-us
	0,				// AuxDAC2 default value = 0 mV										// aux_dac2_default_value_mV ***  adi,aux-dac2-default-value-mV
	0,				// N/A when aux_dac_manual_mode_enable = 1							// aux_dac2_active_in_rx_enable ***  adi,aux-dac2-active-in-rx-enable
	0,				// N/A when aux_dac_manual_mode_enable = 1							// aux_dac2_active_in_tx_enable ***  adi,aux-dac2-active-in-tx-enable
	0,				// N/A when aux_dac_manual_mode_enable = 1							// aux_dac2_active_in_alert_enable ***  adi,aux-dac2-active-in-alert-enable
	0,				// N/A when aux_dac_manual_mode_enable = 1							// aux_dac2_rx_delay_us ***  adi,aux-dac2-rx-delay-us
	0,				// N/A when aux_dac_manual_mode_enable = 1							// aux_dac2_tx_delay_us ***  adi,aux-dac2-tx-delay-us

	/* Temperature Sensor Control */
	256,			// Temperature sensor decimate by 256								// temp_sense_decimation *** adi,temp-sense-decimation
	1000,			// Measure temperature every 1000 ms								// temp_sense_measurement_interval_ms *** adi,temp-sense-measurement-interval-ms
	206,			// Offset = +206 degrees C											// temp_sense_offset_signed *** adi,temp-sense-offset-signed
	1,				// Periodic temperature measurements enabled 						// temp_sense_periodic_measurement_enable *** adi,temp-sense-periodic-measurement-enable

	/* Control Out Setup */
	/* See https://wiki.analog.com/resources/tools-software/linux-drivers/iio-transceiver/ad9361-customization#control_output_setup */
	0xFF,			// Enable all CTRL_OUT bits											// ctrl_outs_enable_mask *** adi,ctrl-outs-enable-mask
	0,				// CTRL_OUT index is 0												// ctrl_outs_index *** adi,ctrl-outs-index

	/* External LNA Control */
	/* bladeRF Micro: GPO_0 is TP3, GPO_1 is TP4 */
	0,				// N/A when elna_rx(1,2)_gpo(0,1)_control_enable = 0				// elna_settling_delay_ns *** adi,elna-settling-delay-ns
	0,				// MUST be 0 when elna_rx(1,2)_gpo(0,1)_control_enable = 0			// elna_gain_mdB *** adi,elna-gain-mdB
	0,				// MUST be 0 when elna_rx(1,2)_gpo(0,1)_control_enable = 0			// elna_bypass_loss_mdB *** adi,elna-bypass-loss-mdB
	0,				// Ext LNA Ctrl bit in Rx1 gain table does NOT set GPO0 state		// elna_rx1_gpo0_control_enable *** adi,elna-rx1-gpo0-control-enable
	0,				// Ext LNA Ctrl bit in Rx2 gain table does NOT set GPO1 state		// elna_rx2_gpo1_control_enable *** adi,elna-rx2-gpo1-control-enable
	0,				// N/A when elna_rx(1,2)_gpo(0,1)_control_enable = 0				// elna_gaintable_all_index_enable *** adi,elna-gaintable-all-index-enable

	/* Digital Interface Control */
#ifdef ENABLE_AD9361_DIGITAL_INTERFACE_TIMING_VERIFICATION
	/* Calibrate the digital interface delay (hardware validation) */
	0,				// Don't skip digital interface tuning								// digital_interface_tune_skip_mode *** adi,digital-interface-tune-skip-mode
#else
	/* Use hardcoded digital interface delay values (production) */
	2,				// Skip RX and TX tuning; use hardcoded values below				// digital_interface_tune_skip_mode *** adi,digital-interface-tune-skip-mode
#endif // ENABLE_AD9361_DIGITAL_INTERFACE_TIMING_VERIFICATION
	0,				// ?? UNDOCUMENTED ??												// digital_interface_tune_fir_disable *** adi,digital-interface-tune-fir-disable
	1,				// Swap I and Q (spectral inversion)								// pp_tx_swap_enable *** adi,pp-tx-swap-enable
	1,				// Swap I and Q (spectral inversion)								// pp_rx_swap_enable *** adi,pp-rx-swap-enable
	0,				// Don't swap TX1 and TX2											// tx_channel_swap_enable *** adi,tx-channel-swap-enable
	0,				// Don't swap RX1 and RX2											// rx_channel_swap_enable *** adi,rx-channel-swap-enable
	1,				// Toggle RX_FRAME with 50% duty cycle								// rx_frame_pulse_mode_enable *** adi,rx-frame-pulse-mode-enable
	0,				// Data port timing reflects # of enabled signal paths				// two_t_two_r_timing_enable *** adi,2t2r-timing-enable
	0,				// Don't invert data bus											// invert_data_bus_enable *** adi,invert-data-bus-enable
	0,				// Don't invert data clock											// invert_data_clk_enable *** adi,invert-data-clk-enable
	0,				// MUST be 0 when lvds_mode_enable = 1								// fdd_alt_word_order_enable *** adi,fdd-alt-word-order-enable
	0,				// Don't invert RX_FRAME											// invert_rx_frame_enable *** adi,invert-rx-frame-enable
	0,				// Don't make RX sample rate 2x the TX sample rate					// fdd_rx_rate_2tx_enable *** adi,fdd-rx-rate-2tx-enable
	0,				// MUST be 0 when lvds_mode_enable = 1								// swap_ports_enable *** adi,swap-ports-enable
	0,				// MUST be 0 when lvds_mode_enable = 1								// single_data_rate_enable *** adi,single-data-rate-enable
	1,				// Use LVDS mode on data port										// lvds_mode_enable *** adi,lvds-mode-enable
	0,				// MUST be 0 when lvds_mode_enable = 1								// half_duplex_mode_enable *** adi,half-duplex-mode-enable
	0,				// MUST be 0 when lvds_mode_enable = 1								// single_port_mode_enable *** adi,single-port-mode-enable
	0,				// MUST be 0 when lvds_mode_enable = 1								// full_port_enable *** adi,full-port-enable
	0,				// MUST be 0 when lvds_mode_enable = 1								// full_duplex_swap_bits_enable *** adi,full-duplex-swap-bits-enable
	0,				// RX_DATA delay rel to RX_FRAME = 0 DATA_CLK/2 cycles				// delay_rx_data *** adi,delay-rx-data
	// Approx 0.3 ns/LSB on next 4 values
	5,				// DATA_CLK delay = 1.5 ns											// rx_data_clock_delay *** adi,rx-data-clock-delay
	0,				// RX_DATA/RX_FRAME delay = 0 ns									// rx_data_delay *** adi,rx-data-delay
	0,				// FB_CLK delay = 0 ns												// tx_fb_clock_delay *** adi,tx-fb-clock-delay
	5,				// TX_DATA/TX_FRAME delay = 1.5 ns									// tx_data_delay *** adi,tx-data-delay
	300,			// LVDS driver bias 300 mV											// lvds_bias_mV *** adi,lvds-bias-mV
	1,				// Enable LVDS on-chip termination									// lvds_rx_onchip_termination_enable *** adi,lvds-rx-onchip-termination-enable
	0,				// RX1 and RX2 are not phase-aligned								// rx1rx2_phase_inversion_en *** adi,rx1-rx2-phase-inversion-enable
	0xFF,			// Default signal inversion mappings								// lvds_invert1_control *** adi,lvds-invert1-control
	0x0F,			// Default signal inversion mappings								// lvds_invert2_control *** adi,lvds-invert2-control
	1,				// CLK_OUT drive increased by ~20%									// clk_out_drive
	1,				// DATA_CLK drive increased by ~20%									// dataclk_drive
	1,				// Data port drive increased by ~20%								// data_port_drive
	0,				// CLK_OUT minimum slew (fastest rise/fall)							// clk_out_slew
	0,				// DATA_CLK minimum slew (fastest rise/fall)						// dataclk_slew
	0,				// Data port minimum slew (fastest rise/fall)						// data_port_slew

	/* GPO Control */
	0,				// GPO0 is LOW in Sleep/Wait/Alert states							// gpo0_inactive_state_high_enable *** adi,gpo0-inactive-state-high-enable
	0,				// GPO1 is LOW in Sleep/Wait/Alert states							// gpo1_inactive_state_high_enable *** adi,gpo1-inactive-state-high-enable
	0,				// GPO2 is LOW in Sleep/Wait/Alert states							// gpo2_inactive_state_high_enable *** adi,gpo2-inactive-state-high-enable
	0,				// GPO3 is LOW in Sleep/Wait/Alert states							// gpo3_inactive_state_high_enable *** adi,gpo3-inactive-state-high-enable
	0,				// GPO0 does not change state when entering RX state				// gpo0_slave_rx_enable *** adi,gpo0-slave-rx-enable
	0,				// GPO0 does not change state when entering TX state				// gpo0_slave_tx_enable *** adi,gpo0-slave-tx-enable
	0,				// GPO1 does not change state when entering RX state				// gpo1_slave_rx_enable *** adi,gpo1-slave-rx-enable
	0,				// GPO1 does not change state when entering TX state				// gpo1_slave_tx_enable *** adi,gpo1-slave-tx-enable
	0,				// GPO2 does not change state when entering RX state				// gpo2_slave_rx_enable *** adi,gpo2-slave-rx-enable
	0,				// GPO2 does not change state when entering TX state				// gpo2_slave_tx_enable *** adi,gpo2-slave-tx-enable
	0,				// GPO3 does not change state when entering RX state				// gpo3_slave_rx_enable *** adi,gpo3-slave-rx-enable
	0,				// GPO3 does not change state when entering TX state				// gpo3_slave_tx_enable *** adi,gpo3-slave-tx-enable
	0,				// N/A when gpo0_slave_rx_enable = 0								// gpo0_rx_delay_us *** adi,gpo0-rx-delay-us
	0,				// N/A when gpo0_slave_tx_enable = 0								// gpo0_tx_delay_us *** adi,gpo0-tx-delay-us
	0,				// N/A when gpo1_slave_rx_enable = 0								// gpo1_rx_delay_us *** adi,gpo1-rx-delay-us
	0,				// N/A when gpo1_slave_tx_enable = 0								// gpo1_tx_delay_us *** adi,gpo1-tx-delay-us
	0,				// N/A when gpo2_slave_rx_enable = 0								// gpo2_rx_delay_us *** adi,gpo2-rx-delay-us
	0,				// N/A when gpo2_slave_tx_enable = 0								// gpo2_tx_delay_us *** adi,gpo2-tx-delay-us
	0,				// N/A when gpo3_slave_rx_enable = 0								// gpo3_rx_delay_us *** adi,gpo3-rx-delay-us
	0,				// N/A when gpo3_slave_tx_enable = 0								// gpo3_tx_delay_us *** adi,gpo3-tx-delay-us

	/* Tx Monitor Control */
	/* bladeRF Micro: N/A, TX_MON1 and TX_MON2 tied to GND */
	37000,			// N/A																// low_high_gain_threshold_mdB *** adi,txmon-low-high-thresh
	0,				// N/A																// low_gain_dB *** adi,txmon-low-gain
	24,				// N/A																// high_gain_dB *** adi,txmon-high-gain
	0,				// N/A																// tx_mon_track_en *** adi,txmon-dc-tracking-enable
	0,				// N/A																// one_shot_mode_en *** adi,txmon-one-shot-mode-enable
	511,			// N/A																// tx_mon_delay *** adi,txmon-delay
	8192,			// N/A																// tx_mon_duration *** adi,txmon-duration
	2,				// N/A																// tx1_mon_front_end_gain *** adi,txmon-1-front-end-gain
	2,				// N/A																// tx2_mon_front_end_gain *** adi,txmon-2-front-end-gain
	48,				// N/A																// tx1_mon_lo_cm *** adi,txmon-1-lo-cm
	48,				// N/A																// tx2_mon_lo_cm *** adi,txmon-2-lo-cm

	/* GPIO definitions */
	RFFE_CONTROL_RESET_N,	// Reset using RFFE bit 0									// gpio_resetb *** reset-gpios

	/* MCS Sync */
	-1,				// Future use (MCS Sync)											// gpio_sync *** sync-gpios
	-1,				// Future use (MCS Sync)											// gpio_cal_sw1 *** cal-sw1-gpios
	-1,				// Future use (MCS Sync)											// gpio_cal_sw2 *** cal-sw2-gpios

	/* External LO clocks */
	NULL,			// Future use (RX_EXT_LO, TX_EXT_LO control)						// (*ad9361_rfpll_ext_recalc_rate)()
	NULL,			// Future use (RX_EXT_LO, TX_EXT_LO control)						// (*ad9361_rfpll_ext_round_rate)()
	NULL			// Future use (RX_EXT_LO, TX_EXT_LO control)						// (*ad9361_rfpll_ext_set_rate)()
};

AD9361_RXFIRConfig ad9361_init_rx_fir_config = {
	3,				// rx (RX1 = 1, RX2 = 2, both = 3)
	0,				// rx_gain (-12, -6, 0, or 6 dB)
	1,				// rx_dec (decimate by 1, 2, or 4)

	// LPF corner at 0.9 fs, generated w/ Octave:
	// >> b = fir1(63, 0.9);
	// >> bb = round(b*32768);
	// >> freqz(bb);
	{
	      22,     -27,      31,     -34,      34,     -28,      15,       7,
	     -38,      77,    -121,     164,    -198,     214,    -204,     159,
	     -75,     -48,     207,    -389,     578,    -751,     880,    -934,
	     882,    -686,     306,     316,   -1287,    2884,   -6146,   20575,
	   20575,   -6146,    2884,   -1287,     316,     306,    -686,     882,
	    -934,     880,    -751,     578,    -389,     207,     -48,     -75,
	     159,    -204,     214,    -198,     164,    -121,      77,     -38,
	       7,      15,     -28,      34,     -34,      31,     -27,      22,
	       0,       0,       0,       0,       0,       0,       0,       0,
	       0,       0,       0,       0,       0,       0,       0,       0,
	       0,       0,       0,       0,       0,       0,       0,       0,
	       0,       0,       0,       0,       0,       0,       0,       0,
	       0,       0,       0,       0,       0,       0,       0,       0,
	       0,       0,       0,       0,       0,       0,       0,       0,
	       0,       0,       0,       0,       0,       0,       0,       0,
	       0,       0,       0,       0,       0,       0,       0,       0,
	},				// rx_coef[128]
	64,				// rx_coef_size
	{0, 0, 0, 0, 0, 0},	//rx_path_clks[6]
	0				// rx_bandwidth
};

AD9361_TXFIRConfig ad9361_init_tx_fir_config = {
	3,				// tx (TX1 = 1, TX2 = 2, both = 3)
	-6,				// tx_gain (-6 or 0 dB)
	1,				// tx_int (interpolate by 1, 2, or 4)

	// BPF PASSBAND 3/20 fs to 1/4 fs
	{
	      -4,      -6,     -37,      35,     186,      86,    -284,    -315,
	     107,     219,      -4,     271,     558,    -307,   -1182,    -356,
	     658,     157,     207,    1648,     790,   -2525,   -2553,     748,
	     865,    -476,    3737,    6560,   -3583,  -14731,   -5278,   14819,
	   14819,   -5278,  -14731,   -3583,    6560,    3737,    -476,     865,
	     748,   -2553,   -2525,     790,    1648,     207,     157,     658,
	    -356,   -1182,    -307,     558,     271,      -4,     219,     107,
	    -315,    -284,      86,     186,      35,     -37,      -6,      -4,
	       0,       0,       0,       0,       0,       0,       0,       0,
	       0,       0,       0,       0,       0,       0,       0,       0,
	       0,       0,       0,       0,       0,       0,       0,       0,
	       0,       0,       0,       0,       0,       0,       0,       0,
	       0,       0,       0,       0,       0,       0,       0,       0,
	       0,       0,       0,       0,       0,       0,       0,       0,
	       0,       0,       0,       0,       0,       0,       0,       0,
	       0,       0,       0,       0,       0,       0,       0,       0,
	},				// tx_coef[128]
	64,				// tx_coef_size
	{0, 0, 0, 0, 0, 0}, // tx_path_clks[6]
	0				// tx_bandwidth
};

const float ina219_r_shunt = 0.001;
