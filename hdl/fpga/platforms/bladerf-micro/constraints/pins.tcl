# Top level pins for bladeRF
package require ::quartus::project

##########
# BANK 3A/3B
##########
set_location_assignment PIN_R6   -to fx3_gpif[6]
set_location_assignment PIN_U7   -to fx3_gpif[5]
set_location_assignment PIN_R5   -to fx3_gpif[8]
set_location_assignment PIN_U8   -to fx3_gpif[7]
set_location_assignment PIN_P6   -to fx3_gpif[4]
set_location_assignment PIN_W8   -to fx3_gpif[9]
set_location_assignment PIN_N6   -to fx3_gpif[10]
set_location_assignment PIN_W9   -to fx3_gpif[11]
set_location_assignment PIN_T7   -to fx3_gpif[12]
set_location_assignment PIN_U6   -to fx3_gpif[3]
set_location_assignment PIN_T8   -to fx3_gpif[13]
set_location_assignment PIN_V6   -to fx3_ctl[9]
set_location_assignment PIN_M6   -to fx3_gpif[15]
set_location_assignment PIN_R7   -to fx3_gpif[16]
set_location_assignment PIN_M7   -to fx3_gpif[17]
set_location_assignment PIN_P7   -to fx3_gpif[14]
set_location_assignment PIN_AB6  -to fx3_gpif[0]
set_location_assignment PIN_V9   -to fx3_gpif[18]
set_location_assignment PIN_AB5  -to fx3_gpif[1]
set_location_assignment PIN_V10  -to fx3_gpif[19]
set_location_assignment PIN_P8   -to fx3_gpif[20]
set_location_assignment PIN_AA7  -to fx3_gpif[2]
set_location_assignment PIN_N8   -to fx3_gpif[21]
set_location_assignment PIN_AB7  -to fx3_gpif[22]
set_location_assignment PIN_AA8  -to fx3_gpif[23]
set_location_assignment PIN_T9   -to fx3_gpif[24]
set_location_assignment PIN_AB8  -to fx3_gpif[25]
set_location_assignment PIN_U10  -to fx3_gpif[26]
set_location_assignment PIN_M8   -to fx3_gpif[27]
set_location_assignment PIN_AA10 -to fx3_gpif[28]
set_location_assignment PIN_M9   -to fx3_gpif[29]
set_location_assignment PIN_AA9  -to fx3_gpif[30]
set_location_assignment PIN_Y10  -to fx3_gpif[31]
set_location_assignment PIN_Y9   -to fx3_ctl[0]
set_location_assignment PIN_R9   -to fx3_ctl[1]
set_location_assignment PIN_U11  -to fx3_ctl[2]
set_location_assignment PIN_R12  -to fx3_ctl[3]
set_location_assignment PIN_U12  -to fx3_ctl[4]
set_location_assignment PIN_P12  -to fx3_ctl[5]
set_location_assignment PIN_AB10 -to fx3_ctl[6]
set_location_assignment PIN_R10  -to fx3_ctl[7]
set_location_assignment PIN_AB11 -to fx3_ctl[8]
set_location_assignment PIN_P9   -to fx3_ctl[10]
set_location_assignment PIN_Y11  -to fx3_ctl[11]
set_location_assignment PIN_N9   -to fx3_pclk
set_location_assignment PIN_AA12 -to fx3_ctl[12]
set_location_assignment PIN_R11  -to pwr_status


# Bank Voltage
set_global_assignment -name IOBANK_VCCIO "1.8V" -section_id "3A"
set_global_assignment -name IOBANK_VCCIO "1.8V" -section_id "3B"

# Single-ended output constraints
set outs {}

foreach pin ${outs} {
    set_instance_assignment -name IO_STANDARD          "1.8 V"           -to ${pin}
    set_instance_assignment -name CURRENT_STRENGTH_NEW "MAXIMUM CURRENT" -to ${pin}
    set_instance_assignment -name SLEW_RATE            1                 -to ${pin}
}

# Single-ended inout constraints
set inouts {}
for { set i 0 } { $i < 32 } { incr i } {
    lappend inouts "fx3_gpif\[${i}\]"
}
for { set i 0 } { $i < 13 } { incr i } {
    lappend inouts "fx3_ctl\[${i}\]"
}

foreach pin ${inouts} {
    set_instance_assignment -name IO_STANDARD          "1.8 V"           -to ${pin}
    set_instance_assignment -name CURRENT_STRENGTH_NEW "MAXIMUM CURRENT" -to ${pin}
    set_instance_assignment -name SLEW_RATE            1                 -to ${pin}
}

# Single-ended input constraints
set ins {
    fx3_pclk
    pwr_status
}

foreach pin ${ins} {
    set_instance_assignment -name IO_STANDARD          "1.8 V"           -to ${pin}
}

set_instance_assignment -name WEAK_PULL_UP_RESISTOR "ON" -to pwr_status


##########
# BANK 4A & 5B
##########
# Bank 4A
set_location_assignment PIN_V13  -to "adi_rx_data[0](n)"
set_location_assignment PIN_AB12 -to adi_ctrl_out[7]
set_location_assignment PIN_U13  -to adi_rx_data[0]
set_location_assignment PIN_T12  -to "adi_rx_data[1](n)"
set_location_assignment PIN_AA14 -to adi_ctrl_out[6]
set_location_assignment PIN_T13  -to adi_rx_data[1]
set_location_assignment PIN_AA13 -to adi_ctrl_out[5]
set_location_assignment PIN_AB15 -to adi_ctrl_out[4]
set_location_assignment PIN_Y14  -to "adi_rx_data[2](n)"
set_location_assignment PIN_Y15  -to adi_rx_data[2]
set_location_assignment PIN_V14  -to "adi_rx_clock(n)"
set_location_assignment PIN_V15  -to adi_rx_clock
set_location_assignment PIN_AB18 -to adi_tx_spdt1_v[1]
set_location_assignment PIN_AB20 -to "adi_tx_clock(n)"
set_location_assignment PIN_Y16  -to "adi_rx_data[3](n)"
set_location_assignment PIN_AB21 -to adi_tx_clock
set_location_assignment PIN_Y17  -to adi_rx_data[3]
set_location_assignment PIN_T14  -to "adi_rx_data[4](n)"
set_location_assignment PIN_AA17 -to "adi_tx_frame(n)"
set_location_assignment PIN_U15  -to adi_rx_data[4]
set_location_assignment PIN_AA18 -to adi_tx_frame
set_location_assignment PIN_AA19 -to "adi_tx_data[5](n)"
set_location_assignment PIN_V20  -to "adi_rx_data[5](n)"
set_location_assignment PIN_AA20 -to adi_tx_data[5]
set_location_assignment PIN_W19  -to adi_rx_data[5]
set_location_assignment PIN_AB22 -to "adi_tx_data[4](n)"
set_location_assignment PIN_AA22 -to adi_tx_data[4]
set_location_assignment PIN_Y22  -to "adi_tx_data[3](n)"
set_location_assignment PIN_Y20  -to "adi_rx_frame(n)"
set_location_assignment PIN_W22  -to adi_tx_data[3]
set_location_assignment PIN_Y19  -to adi_rx_frame
set_location_assignment PIN_Y21  -to "adi_tx_data[2](n)"
set_location_assignment PIN_R14  -to adi_tx_spdt1_v[2]
set_location_assignment PIN_W21  -to adi_tx_data[2]
set_location_assignment PIN_U22  -to "adi_tx_data[1](n)"
set_location_assignment PIN_V19  -to adi_ctrl_out[3]
set_location_assignment PIN_V21  -to adi_tx_data[1]
set_location_assignment PIN_V18  -to adi_ctrl_out[2]
set_location_assignment PIN_U16  -to adi_ctrl_out[1]
set_location_assignment PIN_U21  -to "adi_tx_data[0](n)"
set_location_assignment PIN_U17  -to adi_ctrl_out[0]
set_location_assignment PIN_U20  -to adi_tx_data[0]
set_location_assignment PIN_P14  -to adi_rx_spdt2_v[2]
set_location_assignment PIN_AB17 -to adi_tx_spdt2_v[1]
set_location_assignment PIN_AA15 -to adi_tx_spdt2_v[2]

# Bank 5B
set_location_assignment PIN_N16 -to adi_ctrl_in[0]
set_location_assignment PIN_N20 -to adi_ctrl_in[1]
set_location_assignment PIN_M16 -to adi_ctrl_in[2]
set_location_assignment PIN_N21 -to adi_ctrl_in[3]
set_location_assignment PIN_M18 -to adi_sync_in
set_location_assignment PIN_K17 -to adi_enable
set_location_assignment PIN_M20 -to adi_en_agc
set_location_assignment PIN_L17 -to adi_txnrx
set_location_assignment PIN_M21 -to adi_reset_n
set_location_assignment PIN_L19 -to adi_spi_csn
set_location_assignment PIN_K21 -to adi_spi_sdo
set_location_assignment PIN_L18 -to adi_spi_sdi
set_location_assignment PIN_K22 -to adi_spi_sclk
set_location_assignment PIN_N19 -to adi_rx_spdt1_v[1]
set_location_assignment PIN_M22 -to adi_rx_spdt1_v[2]
set_location_assignment PIN_L22 -to adi_rx_spdt2_v[1]


# Bank voltage
set_global_assignment -name IOBANK_VCCIO "2.5V" -section_id "4A"
set_global_assignment -name IOBANK_VCCIO "2.5V" -section_id "5B"


# Single-ended output constraints
set outs {
    adi_en_agc
    adi_txnrx
    adi_reset_n
    adi_enable
    adi_spi_csn
    adi_spi_sdi
    adi_spi_sclk
    adi_sync_in
}
for { set i 0 } { $i < 4 } { incr i } {
    lappend outs "adi_ctrl_in\[${i}\]"
}
for { set i 1 } { $i < 3 } { incr i } {
    for { set j 1 } { $j < 3 } { incr j } {
        lappend outs "adi_rx_spdt${j}_v\[${i}\]"
        lappend outs "adi_tx_spdt${j}_v\[${i}\]"

        set_instance_assignment -name IO_MAXIMUM_TOGGLE_RATE "1 MHz" -to adi_rx_spdt${j}_v\[${i}\]
        set_instance_assignment -name IO_MAXIMUM_TOGGLE_RATE "1 MHz" -to adi_tx_spdt${j}_v\[${i}\]
    }
}

foreach pin ${outs} {
    set_instance_assignment -name IO_STANDARD          "2.5 V"           -to ${pin}
    set_instance_assignment -name CURRENT_STRENGTH_NEW "MAXIMUM CURRENT" -to ${pin}
    set_instance_assignment -name SLEW_RATE            1                 -to ${pin}
}

# Single-ended inout constraints
set inouts {
}

foreach pin ${inouts} {
    set_instance_assignment -name IO_STANDARD          "2.5 V"           -to ${pin}
    set_instance_assignment -name CURRENT_STRENGTH_NEW "MAXIMUM CURRENT" -to ${pin}
    set_instance_assignment -name SLEW_RATE            1                 -to ${pin}
}

# Single-ended input constraints
set ins {
    adi_spi_sdo
}
for { set i 0 } { $i < 8 } { incr i } {
    lappend ins "adi_ctrl_out\[${i}\]"
}

foreach pin ${ins} {
    set_instance_assignment -name IO_STANDARD          "2.5 V"           -to ${pin}
}

# LVDS outputs
set lvds_outs {
    adi_tx_frame
    adi_tx_clock
}
for { set i 0 } { $i < 6 } { incr i } {
    lappend lvds_outs "adi_tx_data\[${i}\]"
}

foreach pin ${lvds_outs} {
    set_instance_assignment -name IO_STANDARD "LVDS" -to ${pin}
    #set_instance_assignment -name IO_STANDARD "LVDS" -to "${pin}(n)"
}

# LVDS inputs
set lvds_ins {
    adi_rx_frame
    adi_rx_clock
}
for { set i 0 } { $i < 6 } { incr i } {
    lappend lvds_ins "adi_rx_data\[${i}\]"
}

foreach pin ${lvds_ins} {
    set_instance_assignment -name IO_STANDARD       "LVDS"         -to ${pin}
    #set_instance_assignment -name IO_STANDARD       "LVDS"         -to "${pin}(n)"
    set_instance_assignment -name INPUT_TERMINATION "DIFFERENTIAL" -to ${pin}
    #set_instance_assignment -name INPUT_TERMINATION "DIFFERENTIAL" -to "${pin}(n)"
}


##########
# BANK 5A
##########
set_location_assignment PIN_T19 -to ufl_clock_oe
set_location_assignment PIN_T18 -to adf_ce
set_location_assignment PIN_T17 -to c5_clock2_oe
set_location_assignment PIN_T22 -to si_clock_sel
set_location_assignment PIN_P16 -to fx3_uart_cts
set_location_assignment PIN_P18 -to fx3_uart_rxd
set_location_assignment PIN_P17 -to fx3_uart_txd
set_location_assignment PIN_T20 -to exp_clock_oe
set_location_assignment PIN_P19  -to pwr_sda
set_location_assignment PIN_P22 -to pwr_scl
set_location_assignment PIN_T15 -to adf_muxout
set_location_assignment PIN_R15 -to adf_sclk
set_location_assignment PIN_R17 -to adf_sdi
set_location_assignment PIN_R16 -to adf_csn
set_location_assignment PIN_R22 -to rx_bias_en
set_location_assignment PIN_R21 -to tx_bias_en


# Bank voltage
set_global_assignment -name IOBANK_VCCIO "3.3V" -section_id "5A"

# Single-ended output constraints
set outs {
    adf_sclk
    adf_sdi
    adf_csn
    fx3_uart_cts
    fx3_uart_rxd
    adf_ce
    exp_clock_oe
    ufl_clock_oe
    c5_clock2_oe
    si_clock_sel
    pwr_scl
    rx_bias_en
    tx_bias_en
}

foreach pin ${outs} {
    set_instance_assignment -name IO_STANDARD          "3.3-V LVCMOS"    -to ${pin}
    set_instance_assignment -name CURRENT_STRENGTH_NEW "MAXIMUM CURRENT" -to ${pin}
    set_instance_assignment -name SLEW_RATE            1                 -to ${pin}
}

# Single-ended inout constraints
set inouts {
    pwr_sda
}

foreach pin ${inouts} {
    set_instance_assignment -name IO_STANDARD          "3.3-V LVCMOS"    -to ${pin}
    set_instance_assignment -name CURRENT_STRENGTH_NEW "MAXIMUM CURRENT" -to ${pin}
    set_instance_assignment -name SLEW_RATE            1                 -to ${pin}
}

# Single-ended input constraints
set ins {
    fx3_uart_txd
    adf_muxout
}

foreach pin ${ins} {
    set_instance_assignment -name IO_STANDARD          "3.3-V LVCMOS"    -to ${pin}
}

set_instance_assignment -name WEAK_PULL_UP_RESISTOR "ON" -to adf_muxout
set_instance_assignment -name WEAK_PULL_UP_RESISTOR "ON" -to fx3_uart_rxd


##########
# BANK 7A & 8A
##########

# Bank 7A
set_location_assignment PIN_K20 -to led[3]
set_location_assignment PIN_B16 -to led[2]
set_location_assignment PIN_K19 -to led[1]
set_location_assignment PIN_G15 -to exp_present
set_location_assignment PIN_B15 -to exp_i2c_scl
set_location_assignment PIN_F14 -to exp_i2c_sda
set_location_assignment PIN_H13 -to c5_clock2
set_location_assignment PIN_A12 -to dac_sclk
set_location_assignment PIN_H11 -to dac_sdi
set_location_assignment PIN_G12 -to dac_csn
set_location_assignment PIN_D12 -to exp_clock_req
set_location_assignment PIN_H16 -to mini_exp1
set_location_assignment PIN_H15 -to mini_exp2
set_location_assignment PIN_E12 -to hw_rev[0]
set_location_assignment PIN_E15 -to hw_rev[1]
set_location_assignment PIN_C11 -to ps_sync_1p1
set_location_assignment PIN_C13 -to ps_sync_1p8

# Bank 8A
set_location_assignment PIN_G10 -to exp_gpio[15]
set_location_assignment PIN_L7  -to exp_gpio[17]
set_location_assignment PIN_F10 -to exp_gpio[14]
set_location_assignment PIN_K7  -to exp_gpio[16]
set_location_assignment PIN_J7  -to exp_gpio[1]
set_location_assignment PIN_H8  -to exp_gpio[31]
set_location_assignment PIN_J8  -to exp_gpio[0]
set_location_assignment PIN_G8  -to exp_gpio[30]
set_location_assignment PIN_J9  -to exp_gpio[3]
set_location_assignment PIN_A10 -to exp_gpio[19]
set_location_assignment PIN_H9  -to exp_gpio[2]
set_location_assignment PIN_A9  -to exp_gpio[18]
set_location_assignment PIN_B10 -to exp_gpio[5]
set_location_assignment PIN_A5  -to exp_gpio[21]
set_location_assignment PIN_C9  -to exp_gpio[4]
set_location_assignment PIN_B5  -to exp_gpio[20]
set_location_assignment PIN_E10 -to exp_gpio[7]
set_location_assignment PIN_B6  -to exp_gpio[23]
set_location_assignment PIN_F9  -to exp_gpio[6]
set_location_assignment PIN_B7  -to exp_gpio[22]
set_location_assignment PIN_A8  -to exp_gpio[9]
set_location_assignment PIN_C6  -to exp_gpio[25]
set_location_assignment PIN_A7  -to exp_gpio[8]
set_location_assignment PIN_D6  -to exp_gpio[24]
set_location_assignment PIN_E9  -to exp_gpio[11]
set_location_assignment PIN_D7  -to exp_gpio[27]
set_location_assignment PIN_D9  -to exp_gpio[10]
set_location_assignment PIN_C8  -to exp_gpio[26]
set_location_assignment PIN_G6  -to exp_gpio[13]
set_location_assignment PIN_F7  -to exp_gpio[29]
set_location_assignment PIN_H6  -to exp_gpio[12]
set_location_assignment PIN_E7  -to exp_gpio[28]


# Bank Voltage
set_global_assignment -name IOBANK_VCCIO "3.3V" -section_id "7A"
set_global_assignment -name IOBANK_VCCIO "3.3V" -section_id "8A"

# Single-ended output constraints
set outs {
    dac_sclk
    dac_sdi
    dac_csn
    ps_sync_1p1
    ps_sync_1p8
}
for { set i 1 } { $i < 4 } { incr i } {
    lappend outs "led\[${i}\]"
}

foreach pin ${outs} {
    set_instance_assignment -name IO_STANDARD          "3.3-V LVCMOS"    -to ${pin}
    if { ${pin} == "exp_clock_out" } {
        set_instance_assignment -name CURRENT_STRENGTH_NEW "MAXIMUM CURRENT" -to ${pin}
        set_instance_assignment -name SLEW_RATE            1                 -to ${pin}
    } else {
        set_instance_assignment -name CURRENT_STRENGTH_NEW "MINIMUM CURRENT" -to ${pin}
        set_instance_assignment -name SLEW_RATE            0                 -to ${pin}
    }
}

# Single-ended inout constraints
set inouts {
    exp_i2c_scl
    exp_i2c_sda
    mini_exp1
    mini_exp2
}
for { set i 0 } { $i < 32 } { incr i } {
    lappend inouts "exp_gpio\[${i}\]"
}

foreach pin ${inouts} {
    set_instance_assignment -name IO_STANDARD          "3.3-V LVCMOS"    -to ${pin}
    set_instance_assignment -name CURRENT_STRENGTH_NEW "MAXIMUM CURRENT" -to ${pin}
    set_instance_assignment -name SLEW_RATE            1                 -to ${pin}
}

# Single-ended input constraints
set ins {
    c5_clock2
    exp_present
    exp_clock_req
    hw_rev[0]
    hw_rev[1]
}

foreach pin ${ins} {
    set_instance_assignment -name IO_STANDARD          "3.3-V LVCMOS"    -to ${pin}
}

set_instance_assignment -name WEAK_PULL_UP_RESISTOR "ON" -to hw_rev[0]
set_instance_assignment -name WEAK_PULL_UP_RESISTOR "ON" -to hw_rev[1]




# JTAG
set_instance_assignment -name IO_STANDARD          "1.8 V"           -to altera_reserved_tck
set_instance_assignment -name IO_STANDARD          "1.8 V"           -to altera_reserved_tdi
set_instance_assignment -name IO_STANDARD          "1.8 V"           -to altera_reserved_tms

set_instance_assignment -name IO_STANDARD          "1.8 V"           -to altera_reserved_tdo
set_instance_assignment -name CURRENT_STRENGTH_NEW "Default"         -to altera_reserved_tdo
set_instance_assignment -name SLEW_RATE            1                 -to altera_reserved_tdo



# Configuration information
set_global_assignment -name STRATIXV_CONFIGURATION_SCHEME "PASSIVE PARALLEL X8"
set_global_assignment -name USE_CONFIGURATION_DEVICE OFF
set_global_assignment -name GENERATE_TTF_FILE ON
set_global_assignment -name GENERATE_RBF_FILE ON
set_global_assignment -name CRC_ERROR_OPEN_DRAIN OFF
set_global_assignment -name FORCE_CONFIGURATION_VCCIO ON
set_global_assignment -name CONFIGURATION_VCCIO_LEVEL "1.8V"
set_global_assignment -name ON_CHIP_BITSTREAM_DECOMPRESSION OFF
set_global_assignment -name OUTPUT_IO_TIMING_NEAR_END_VMEAS "HALF VCCIO" -rise
set_global_assignment -name OUTPUT_IO_TIMING_NEAR_END_VMEAS "HALF VCCIO" -fall
set_global_assignment -name OUTPUT_IO_TIMING_FAR_END_VMEAS "HALF SIGNAL SWING" -rise
set_global_assignment -name OUTPUT_IO_TIMING_FAR_END_VMEAS "HALF SIGNAL SWING" -fall
set_global_assignment -name RESERVE_DATA7_THROUGH_DATA5_AFTER_CONFIGURATION "USE AS REGULAR IO"
set_global_assignment -name CVP_CONFDONE_OPEN_DRAIN OFF
set_global_assignment -name RESERVE_FLASH_NCE_AFTER_CONFIGURATION "USE AS REGULAR IO"
set_global_assignment -name RESERVE_OTHER_AP_PINS_AFTER_CONFIGURATION "USE AS REGULAR IO"
set_global_assignment -name CYCLONEII_RESERVE_NCEO_AFTER_CONFIGURATION "USE AS REGULAR IO"
set_global_assignment -name RESERVE_DCLK_AFTER_CONFIGURATION "USE AS REGULAR IO"
