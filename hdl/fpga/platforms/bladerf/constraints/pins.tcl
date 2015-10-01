# Top level pins for bladeRF
package require ::quartus::project

# IO Standard
set_instance_assignment -name IO_STANDARD "1.8 V" -to c4_clock
set_instance_assignment -name IO_STANDARD "1.8 V" -to dac_csx
set_instance_assignment -name IO_STANDARD "1.8 V" -to dac_sclk
set_instance_assignment -name IO_STANDARD "1.8 V" -to dac_sdi
set_instance_assignment -name IO_STANDARD "1.8 V" -to dac_sdo
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[16]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[15]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[14]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[13]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[12]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[11]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[10]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[9]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[8]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[7]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[6]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[5]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[4]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[3]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[2]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_present
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_spi_clock
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_spi_miso
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_spi_mosi
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_ctl[12]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_ctl[11]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_ctl[10]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_ctl[9]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_ctl[8]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_ctl[7]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_ctl[6]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_ctl[5]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_ctl[4]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_ctl[3]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_ctl[2]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_ctl[1]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_ctl[0]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[31]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[30]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[29]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[28]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[27]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[26]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[25]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[24]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[23]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[22]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[21]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[20]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[19]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[18]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[17]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[16]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[15]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[14]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[13]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[12]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[11]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[10]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[9]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[8]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[7]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[6]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[5]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[4]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[3]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[2]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[1]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_gpif[0]
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_uart_cts
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_uart_rxd
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_uart_txd
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_pll_out
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_reset
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_rx_clock_out
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_rx_data[11]
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_rx_data[10]
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_rx_data[9]
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_rx_data[8]
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_rx_data[7]
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_rx_data[6]
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_rx_data[5]
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_rx_data[4]
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_rx_data[3]
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_rx_data[2]
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_rx_data[1]
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_rx_data[0]
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_rx_enable
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_rx_iq_select
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_sclk
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_sdio
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_sdo
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_sen
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_tx_data[11]
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_tx_data[10]
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_tx_data[9]
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_tx_data[8]
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_tx_data[7]
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_tx_data[6]
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_tx_data[5]
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_tx_data[4]
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_tx_data[3]
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_tx_data[2]
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_tx_data[1]
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_tx_data[0]
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_tx_enable
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_tx_iq_select
set_instance_assignment -name IO_STANDARD "1.8 V" -to led[3]
set_instance_assignment -name IO_STANDARD "1.8 V" -to altera_reserved_tdi
set_instance_assignment -name IO_STANDARD "1.8 V" -to altera_reserved_tck
set_instance_assignment -name IO_STANDARD "1.8 V" -to altera_reserved_tdo
set_instance_assignment -name IO_STANDARD "1.8 V" -to altera_reserved_tms
set_instance_assignment -name IO_STANDARD "1.8 V" -to c4_tx_clock
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_clock_in
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[31]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[32]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[28]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[27]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[26]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[25]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[24]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[30]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[29]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[23]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[22]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[21]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[20]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[19]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[18]
set_instance_assignment -name IO_STANDARD "1.8 V" -to exp_gpio[17]
set_instance_assignment -name IO_STANDARD "1.8 V" -to si_sda
set_instance_assignment -name IO_STANDARD "1.8 V" -to mini_exp1
set_instance_assignment -name IO_STANDARD "1.8 V" -to mini_exp2
set_instance_assignment -name IO_STANDARD "1.8 V" -to ref_1pps
set_instance_assignment -name IO_STANDARD "1.8 V" -to ref_sma_clock
set_instance_assignment -name IO_STANDARD "1.8 V" -to si_scl
set_instance_assignment -name IO_STANDARD "1.8 V" -to fx3_pclk
set_instance_assignment -name IO_STANDARD "1.8 V" -to led[1]
set_instance_assignment -name IO_STANDARD "1.8 V" -to led[2]
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_rx_v[2]
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_rx_v[1]
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_tx_v[2]
set_instance_assignment -name IO_STANDARD "1.8 V" -to lms_tx_v[1]

# Location Assignments
set_location_assignment PIN_A11 -to c4_clock
set_location_assignment PIN_B10 -to dac_csx
set_location_assignment PIN_B9 -to dac_sclk
set_location_assignment PIN_A10 -to dac_sdi
set_location_assignment PIN_A9 -to dac_sdo
set_location_assignment PIN_M1 -to fx3_ctl[12]
set_location_assignment PIN_J4 -to fx3_ctl[11]
set_location_assignment PIN_J6 -to fx3_ctl[10]
set_location_assignment PIN_N1 -to fx3_ctl[8]
set_location_assignment PIN_G3 -to fx3_ctl[7]
set_location_assignment PIN_F7 -to fx3_ctl[6]
set_location_assignment PIN_B1 -to fx3_ctl[5]
set_location_assignment PIN_C1 -to fx3_ctl[4]
set_location_assignment PIN_H1 -to fx3_ctl[3]
set_location_assignment PIN_H6 -to fx3_ctl[2]
set_location_assignment PIN_F2 -to fx3_ctl[1]
set_location_assignment PIN_F1 -to fx3_ctl[0]
set_location_assignment PIN_AA1 -to fx3_gpif[31]
set_location_assignment PIN_Y1 -to fx3_gpif[30]
set_location_assignment PIN_AB3 -to fx3_gpif[29]
set_location_assignment PIN_P1 -to fx3_gpif[28]
set_location_assignment PIN_W7 -to fx3_gpif[27]
set_location_assignment PIN_R2 -to fx3_gpif[26]
set_location_assignment PIN_W6 -to fx3_gpif[25]
set_location_assignment PIN_W1 -to fx3_gpif[24]
set_location_assignment PIN_R1 -to fx3_gpif[23]
set_location_assignment PIN_V5 -to fx3_gpif[22]
set_location_assignment PIN_T4 -to fx3_gpif[21]
set_location_assignment PIN_M4 -to fx3_gpif[20]
set_location_assignment PIN_T5 -to fx3_gpif[19]
set_location_assignment PIN_P4 -to fx3_gpif[18]
set_location_assignment PIN_M6 -to fx3_gpif[17]
set_location_assignment PIN_N6 -to fx3_gpif[16]
set_location_assignment PIN_D1 -to fx3_gpif[15]
set_location_assignment PIN_C8 -to fx3_gpif[14]
set_location_assignment PIN_C7 -to fx3_gpif[13]
set_location_assignment PIN_C4 -to fx3_gpif[12]
set_location_assignment PIN_C6 -to fx3_gpif[11]
set_location_assignment PIN_B3 -to fx3_gpif[10]
set_location_assignment PIN_F10 -to fx3_gpif[9]
set_location_assignment PIN_B4 -to fx3_gpif[8]
set_location_assignment PIN_K1 -to fx3_gpif[7]
set_location_assignment PIN_A8 -to fx3_gpif[6]
set_location_assignment PIN_A3 -to fx3_gpif[5]
set_location_assignment PIN_F8 -to fx3_gpif[4]
set_location_assignment PIN_A5 -to fx3_gpif[3]
set_location_assignment PIN_B8 -to fx3_gpif[2]
set_location_assignment PIN_B7 -to fx3_gpif[1]
set_location_assignment PIN_B6 -to fx3_gpif[0]
set_location_assignment PIN_AA4 -to fx3_uart_cts
set_location_assignment PIN_AA5 -to fx3_uart_rxd
set_location_assignment PIN_AA3 -to fx3_uart_txd
set_location_assignment PIN_T21 -to lms_pll_out
set_location_assignment PIN_T22 -to lms_rx_clock_out
set_location_assignment PIN_B22 -to lms_rx_data[11]
set_location_assignment PIN_B21 -to lms_rx_data[10]
set_location_assignment PIN_C22 -to lms_rx_data[9]
set_location_assignment PIN_C21 -to lms_rx_data[8]
set_location_assignment PIN_D22 -to lms_rx_data[7]
set_location_assignment PIN_E21 -to lms_rx_data[6]
set_location_assignment PIN_D21 -to lms_rx_data[5]
set_location_assignment PIN_F21 -to lms_rx_data[4]
set_location_assignment PIN_E22 -to lms_rx_data[3]
set_location_assignment PIN_H21 -to lms_rx_data[2]
set_location_assignment PIN_F22 -to lms_rx_data[1]
set_location_assignment PIN_J21 -to lms_rx_data[0]
set_location_assignment PIN_H22 -to lms_rx_iq_select
set_location_assignment PIN_U21 -to lms_tx_data[11]
set_location_assignment PIN_P22 -to lms_tx_data[10]
set_location_assignment PIN_U22 -to lms_tx_data[9]
set_location_assignment PIN_R21 -to lms_tx_data[8]
set_location_assignment PIN_N22 -to lms_tx_data[7]
set_location_assignment PIN_R22 -to lms_tx_data[6]
set_location_assignment PIN_M22 -to lms_tx_data[5]
set_location_assignment PIN_P21 -to lms_tx_data[4]
set_location_assignment PIN_L22 -to lms_tx_data[3]
set_location_assignment PIN_N21 -to lms_tx_data[2]
set_location_assignment PIN_K22 -to lms_tx_data[1]
set_location_assignment PIN_M21 -to lms_tx_data[0]
set_location_assignment PIN_AA13 -to lms_reset
set_location_assignment PIN_AA14 -to lms_rx_enable
set_location_assignment PIN_AA20 -to lms_tx_v[2]
set_location_assignment PIN_AB13 -to lms_sdio
set_location_assignment PIN_AB14 -to lms_sdo
set_location_assignment PIN_AB15 -to lms_tx_enable
set_location_assignment PIN_AB16 -to lms_sen
set_location_assignment PIN_AB19 -to lms_rx_v[2]
set_location_assignment PIN_AB20 -to lms_rx_v[1]
set_location_assignment PIN_J22 -to lms_tx_iq_select
set_location_assignment PIN_F11 -to exp_spi_mosi
set_location_assignment PIN_D13 -to exp_gpio[2]
set_location_assignment PIN_E13 -to exp_gpio[3]
set_location_assignment PIN_E14 -to exp_gpio[4]
set_location_assignment PIN_E15 -to exp_gpio[5]
set_location_assignment PIN_E16 -to exp_gpio[6]
set_location_assignment PIN_D17 -to exp_gpio[7]
set_location_assignment PIN_D19 -to exp_gpio[8]
set_location_assignment PIN_C19 -to exp_gpio[9]
set_location_assignment PIN_D20 -to exp_gpio[10]
set_location_assignment PIN_C20 -to exp_gpio[11]
set_location_assignment PIN_F13 -to exp_gpio[12]
set_location_assignment PIN_F14 -to exp_gpio[13]
set_location_assignment PIN_F15 -to exp_gpio[14]
set_location_assignment PIN_D15 -to exp_gpio[15]
set_location_assignment PIN_A13 -to exp_gpio[16]
set_location_assignment PIN_C13 -to exp_present
set_location_assignment PIN_G1 -to fx3_pclk
set_location_assignment PIN_AA15 -to lms_sclk
set_location_assignment PIN_B13 -to exp_spi_miso
set_location_assignment PIN_E11 -to exp_spi_clock
set_location_assignment PIN_AA7 -to led[1]
set_location_assignment PIN_AB7 -to led[2]
set_location_assignment PIN_AA21 -to lms_tx_v[1]
set_location_assignment PIN_J3 -to fx3_ctl[9]
set_location_assignment PIN_G22 -to c4_tx_clock
set_location_assignment PIN_A12 -to exp_clock_in
set_location_assignment PIN_A14 -to exp_gpio[25]
set_location_assignment PIN_A15 -to exp_gpio[26]
set_location_assignment PIN_A16 -to exp_gpio[27]
set_location_assignment PIN_A17 -to exp_gpio[28]
set_location_assignment PIN_A18 -to exp_gpio[32]
set_location_assignment PIN_A19 -to exp_gpio[31]
set_location_assignment PIN_A20 -to exp_gpio[30]
set_location_assignment PIN_B20 -to exp_gpio[29]
set_location_assignment PIN_B14 -to exp_gpio[17]
set_location_assignment PIN_B15 -to exp_gpio[18]
set_location_assignment PIN_B16 -to exp_gpio[20]
set_location_assignment PIN_B17 -to exp_gpio[21]
set_location_assignment PIN_B18 -to exp_gpio[23]
set_location_assignment PIN_B19 -to exp_gpio[24]
set_location_assignment PIN_C15 -to exp_gpio[19]
set_location_assignment PIN_C17 -to exp_gpio[22]
set_location_assignment PIN_AB10 -to led[3]
set_location_assignment PIN_AB8 -to mini_exp1
set_location_assignment PIN_AB9 -to mini_exp2
set_location_assignment PIN_AB11 -to ref_1pps
set_location_assignment PIN_AB12 -to ref_sma_clock
set_location_assignment PIN_A6 -to si_scl
set_location_assignment PIN_A7 -to si_sda

# Configuration information
set_global_assignment -name CYCLONEIII_CONFIGURATION_SCHEME "FAST PASSIVE PARALLEL"
set_global_assignment -name USE_CONFIGURATION_DEVICE OFF
set_global_assignment -name GENERATE_TTF_FILE ON
set_global_assignment -name GENERATE_RBF_FILE ON
set_global_assignment -name CRC_ERROR_OPEN_DRAIN OFF
set_global_assignment -name ON_CHIP_BITSTREAM_DECOMPRESSION OFF
set_global_assignment -name OUTPUT_IO_TIMING_NEAR_END_VMEAS "HALF VCCIO" -rise
set_global_assignment -name OUTPUT_IO_TIMING_NEAR_END_VMEAS "HALF VCCIO" -fall
set_global_assignment -name OUTPUT_IO_TIMING_FAR_END_VMEAS "HALF SIGNAL SWING" -rise
set_global_assignment -name OUTPUT_IO_TIMING_FAR_END_VMEAS "HALF SIGNAL SWING" -fall
set_global_assignment -name RESERVE_DATA0_AFTER_CONFIGURATION "USE AS REGULAR IO"
set_global_assignment -name RESERVE_DATA1_AFTER_CONFIGURATION "USE AS REGULAR IO"
set_global_assignment -name RESERVE_DATA7_THROUGH_DATA2_AFTER_CONFIGURATION "USE AS REGULAR IO"
set_global_assignment -name RESERVE_FLASH_NCE_AFTER_CONFIGURATION "USE AS REGULAR IO"
set_global_assignment -name RESERVE_OTHER_AP_PINS_AFTER_CONFIGURATION "USE AS REGULAR IO"
set_global_assignment -name CYCLONEII_RESERVE_NCEO_AFTER_CONFIGURATION "USE AS REGULAR IO"
set_global_assignment -name RESERVE_DCLK_AFTER_CONFIGURATION "USE AS REGULAR IO"

# Drive Strength and Slew Rate
set_instance_assignment -name SLEW_RATE 0 -to fx3_gpif[*]
#set_instance_assignment -name CURRENT_STRENGTH_NEW 16MA -to fx3_gpif[*]
set_instance_assignment -name CURRENT_STRENGTH_NEW 4MA -to fx3_uart_rxd

#set_instance_assignment -name WEAK_PULL_UP_RESISTOR ON -to ref_1pps
