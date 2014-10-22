# Clock inputs
create_clock -period "100.0 MHz" -waveform {0.6 5.6} [get_ports fx3_pclk]
create_clock -period "38.4 MHz"  [get_ports c4_clock]
create_clock -period "38.4 MHz"  [get_ports lms_pll_out]
create_clock -period "19.2 MHz"  [get_ports lms_sclk]
create_clock -period "80.0 MHz"  [get_ports lms_rx_clock_out]
create_clock -period "80.0 MHz"  -waveform {0.34 6.59} [get_ports c4_tx_clock]

# Virtual clocks
create_clock -period "100.0 MHz" -name fx3_virtual
create_clock -period "80 MHz" -name c4_tx_virtual
create_clock -period "80 MHz" -name lms_rx_virtual

# Generate the appropriate PLL clocks
derive_pll_clocks
derive_clock_uncertainty

# First flop synchronizer false path
set_false_path -from * -to [get_registers *synchronize:reg0]
set_false_path -from * -to [get_registers reset_synchronizer*]
set_false_path -from [get_registers {*source_holding[*]}] -to *

### Fast Interfaces ###

# FX3 GPIF/CTL interface
set_input_delay -clock [get_clocks fx3_virtual] -max 8.225 [get_ports {fx3_gpif* fx3_ctl*}]
set_input_delay -clock [get_clocks fx3_virtual] -min 0.225 [get_ports {fx3_gpif* fx3_ctl*}] -add_delay

set_output_delay -clock [get_clocks fx3_virtual] -max 2.5 [get_ports {fx3_gpif* fx3_ctl*}]
set_output_delay -clock [get_clocks fx3_virtual] -min 0.75 [get_ports {fx3_gpif* fx3_ctl*}] -add_delay

set_multicycle_path -from [get_clocks {fx3_virtual}] -to [get_clocks {U_fx3_pll|altpll_component|auto_generated|pll1|clk[0]}] -setup -start 2
set_multicycle_path -from [get_clocks {fx3_virtual}] -to [get_clocks {U_fx3_pll|altpll_component|auto_generated|pll1|clk[0]}] -hold -start 2


# LMS sample interface
set_input_delay -clock [get_clocks lms_rx_virtual] -max  5.832 [get_ports {lms_rx_data* lms_rx_iq_select}]
set_input_delay -clock [get_clocks lms_rx_virtual] -min  -0.168 [get_ports {lms_rx_data* lms_rx_iq_select}] -add_delay

set_output_delay -clock [get_clocks c4_tx_virtual] -min  -0.014 [get_ports {lms_tx_data* lms_tx_iq_select}]
set_output_delay -clock [get_clocks c4_tx_virtual] -max  1.185 [get_ports {lms_tx_data* lms_tx_iq_select}] -add_delay

### Slow Interfaces ###

# FX3 UART interface
#set_false_path -from * -to [get_ports fx3_uart_rxd]
#set_false_path -from [get_ports fx3_uart_txd] -to *
#set_false_path -from * -to [get_ports fx3_uart_cts]
set_output_delay -clock [get_clocks {U_pll|altpll_component|auto_generated|pll1|clk[0]}] -max 5.0 [get_ports {fx3_uart_rxd}]
set_output_delay -clock [get_clocks {U_pll|altpll_component|auto_generated|pll1|clk[0]}] -min 0.0 [get_ports {fx3_uart_rxd}] -add_delay
set_input_delay -clock [get_clocks {U_pll|altpll_component|auto_generated|pll1|clk[0]}] -max 5.0 [get_ports {fx3_uart_txd}]
set_input_delay -clock [get_clocks {U_pll|altpll_component|auto_generated|pll1|clk[0]}] -min 0.0 [get_ports {fx3_uart_txd}] -add_delay

# LMS SPI interface
set_input_delay  -clock [get_clocks U_pll*0*] -min  1.0 [get_ports {lms_sdo}]
set_input_delay  -clock [get_clocks U_pll*0*] -max  9.0 [get_ports {lms_sdo}] -add_delay

set_output_delay -clock [get_clocks U_pll*0*] -min  1.0 [get_ports {lms_sen lms_sdio lms_sclk}]
set_output_delay -clock [get_clocks U_pll*0*] -max  4.0 [get_ports {lms_sen lms_sdio lms_sclk}] -add_delay

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

# Long lived correction parameters
set_false_path -from * -to [get_keepers {iq_correction:*} ]

# JTAG settings
set_clock_groups -exclusive -group [get_clocks altera_reserved_tck]
set_input_delay -clock [get_clocks altera_reserved_tck] 20 [get_ports altera_reserved_tdi]
set_input_delay -clock [get_clocks altera_reserved_tck] 20 [get_ports altera_reserved_tms]
set_output_delay -clock [get_clocks altera_reserved_tck] 20 [get_ports altera_reserved_tdo]

# Exceptions

# The dcfifo which goes from the C4 TX domain to the PCLK domain seems to have an issue with the clear signal.  It isn't generally a issue so ignore it?
set_false_path -from {reset_synchronizer:U_tx_reset|sync} -to {tx_*fifo:U_tx_*_fifo|dcfifo*:dcfifo_*|dcfifo_*:auto_generated|dffpipe_3dc:wraclr|dffe12a[0]}
set_false_path -from {reset_synchronizer:U_tx_reset|sync} -to {tx_*fifo:U_tx_*_fifo|dcfifo*:dcfifo_*|dcfifo_*:auto_generated|dffpipe_3dc:wraclr|dffe13a[0]}

