# Clock inputs
create_clock -period "100.0 MHz" [get_ports fx3_pclk]
create_clock -period "38.4 MHz"  [get_ports c4_clock]
create_clock -period "38.4 MHz"  [get_ports lms_pll_out]
create_clock -period "19.2 MHz"  [get_ports lms_sclk]
create_clock -period "80.0 MHz"  [get_ports lms_rx_clock_out]
create_clock -period "80.0 MHz"  [get_ports c4_tx_clock]

# Virtual clocks
create_clock -period "100.0 MHz" -name fx3_virtual
create_clock -period "80.0 MHz" -name c4_tx_virtual
create_clock -period "80.0 MHz" -name lms_rx_virtual

# Generate the appropriate PLL clocks
derive_pll_clocks
derive_clock_uncertainty

# First flop synchronizer false path
set_false_path -from * -to [get_keepers synchronizer*reg0]
set_false_path -from * -to [get_keepers reset_synchronizer*]

### Fast Interfaces ###

# FX3 GPIF/CTL interface
set_input_delay -clock [get_clocks fx3_virtual] -max 8.0 [get_ports {fx3_gpif* fx3_ctl*}]
set_input_delay -clock [get_clocks fx3_virtual] -min 0.5 [get_ports {fx3_gpif* fx3_ctl*}] -add_delay

set_output_delay -clock [get_clocks fx3_virtual] -max 2.0 [get_ports {fx3_gpif* fx3_ctl*}]
set_output_delay -clock [get_clocks fx3_virtual] -min 0.5 [get_ports {fx3_gpif* fx3_ctl*}] -add_delay

# LMS sample interface
set_input_delay -clock [get_clocks lms_rx_virtual] -max  6.0 [get_ports {lms_rx_data* lms_rx_iq_select}]
set_input_delay -clock [get_clocks lms_rx_virtual] -min  0.2 [get_ports {lms_rx_data* lms_rx_iq_select}] -add_delay

set_output_delay -clock [get_clocks c4_tx_virtual] -min  0.2 [get_ports {lms_tx_data* lms_tx_iq_select}]
set_output_delay -clock [get_clocks c4_tx_virtual] -max  1.0 [get_ports {lms_tx_data* lms_tx_iq_select}] -add_delay

### Slow Interfaces ###

# FX3 UART interface
set_input_delay -clock [get_clocks U_pll*0*] -min 0.0   [get_ports fx3_uart_txd]
set_input_delay -clock [get_clocks U_pll*0*] -max 1.0  [get_ports fx3_uart_txd] -add_delay

set_output_delay -clock [get_clocks U_pll*0*] -min  0.0 [get_ports fx3_uart_rxd]
set_output_delay -clock [get_clocks U_pll*0*] -max  1.0 [get_ports fx3_uart_rxd] -add_delay

# LMS SPI interface
set_input_delay  -clock [get_clocks U_pll*0*] -min  0.2 [get_ports lms_sdo]
set_input_delay  -clock [get_clocks U_pll*0*] -max  1.0 [get_ports lms_sdo] -add_delay

set_output_delay -clock [get_clocks U_pll*0*] -min  0.2 [get_ports {lms_sen lms_sdio lms_sclk}]
set_output_delay -clock [get_clocks U_pll*0*] -max  1.0 [get_ports {lms_sen lms_sdio lms_sclk}] -add_delay

# Si5338 interface
set_input_delay -clock [get_clocks U_pll*0*] -min  1.0 [get_ports {si_scl si_sda}]
set_input_delay -clock [get_clocks U_pll*0*] -max  1.0 [get_ports {si_scl si_sda}] -add_delay

set_output_delay -clock [get_clocks U_pll*0*] -min  0.0 [get_ports {si_scl si_sda}]
set_output_delay -clock [get_clocks U_pll*0*] -max  1.0 [get_ports {si_scl si_sda}] -add_delay

# VCTCXO trimdac
set_output_delay -clock [get_clocks U_pll*0*] -min 0.0 [get_ports {dac_csx dac_sclk dac_sdi}]
set_output_delay -clock [get_clocks U_pll*0*] -max 1.0 [get_ports {dac_csx dac_sclk dac_sdi}] -add_delay

set_input_delay -clock [get_clocks U_pll*0*] -min 1.0 [get_ports dac_sdo]
set_input_delay -clock [get_clocks U_pll*0*] -max 1.0 [get_ports dac_sdo] -add_delay

# LED's
set_false_path -from * -to [get_ports led*]

# LMS long lived GPIO and RF Switches
set_false_path -from * -to [get_ports {lms_*x_v* lms_*x_enable lms_reset}]

# JTAG settings
set_clock_groups -exclusive -group [get_clocks altera_reserved_tck]
set_input_delay -clock [get_clocks altera_reserved_tck] 20 [get_ports altera_reserved_tdi]
set_input_delay -clock [get_clocks altera_reserved_tck] 20 [get_ports altera_reserved_tms]
set_output_delay -clock [get_clocks altera_reserved_tck] 20 [get_ports altera_reserved_tdo]

