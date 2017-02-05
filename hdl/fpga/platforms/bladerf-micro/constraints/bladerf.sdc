# Clock inputs
create_clock -period "100.0 MHz" -waveform {0.6 5.6} [get_ports fx3_pclk]
create_clock -period "38.4 MHz"  [get_ports c5_clock_1]
create_clock -period "38.4 MHz"  [get_ports c5_clock_2]
create_clock -period "250.0 MHz" [get_ports adi_rx_clock]
create_clock -period "38.4 MHz"  [get_ports exp_clock_in]

## Virtual clocks
create_clock -period "100.0 MHz" -name fx3_virtual

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

set_multicycle_path -from [get_clocks {fx3_virtual}] -to [get_clocks {U_fx3_pll*divclk} ] -setup -start 2
set_multicycle_path -from [get_clocks {fx3_virtual}] -to [get_clocks {U_fx3_pll*divclk}] -hold -start 2


### Slow Interfaces ###

# FX3 UART interface
#set_false_path -from * -to [get_ports fx3_uart_rxd]
#set_false_path -from [get_ports fx3_uart_txd] -to *
#set_false_path -from * -to [get_ports fx3_uart_cts]
set_output_delay -clock [get_clocks {U_pll*divclk}] -max 1.0 [get_ports {fx3_uart_rxd}]
set_output_delay -clock [get_clocks {U_pll*divclk}] -min 0.0 [get_ports {fx3_uart_rxd}] -add_delay
set_input_delay  -clock [get_clocks {U_pll*divclk}] -max 1.0 [get_ports {fx3_uart_txd}]
set_input_delay  -clock [get_clocks {U_pll*divclk}] -min 0.0 [get_ports {fx3_uart_txd}] -add_delay

## VCTCXO trimdac
#set_output_delay -clock [get_clocks U_pll*0*] -min 0.0 [get_ports {dac_csn dac_sclk dac_sdi}]
#set_output_delay -clock [get_clocks U_pll*0*] -max 0.2 [get_ports {dac_csn dac_sclk dac_sdi}] -add_delay


# LED's
set_false_path -from * -to [get_ports led*]

# LMS long lived GPIO and RF Switches
set_false_path -from * -to [get_ports {adi_*x_sp4t*_v* adi_reset_n}]

# Long lived correction parameters
set_false_path -from * -to [get_keepers {iq_correction:*} ]

# JTAG settings
set_clock_groups -exclusive -group [get_clocks altera_reserved_tck]
set_input_delay  -clock [get_clocks altera_reserved_tck] 20 [get_ports altera_reserved_tdi]
set_input_delay  -clock [get_clocks altera_reserved_tck] 20 [get_ports altera_reserved_tms]
set_output_delay -clock [get_clocks altera_reserved_tck] 20 [get_ports altera_reserved_tdo]

# Exceptions

# The dcfifo which goes from the C4 TX domain to the PCLK domain seems to have an issue with the clear signal.  It isn't generally a issue so ignore it?
set_false_path -from {reset_synchronizer:U_tx_reset|sync} -to {tx_*fifo:U_tx_*_fifo|dcfifo*:dcfifo_*|dcfifo_*:auto_generated|dffpipe_3dc:wraclr|dffe12a[0]}
set_false_path -from {reset_synchronizer:U_tx_reset|sync} -to {tx_*fifo:U_tx_*_fifo|dcfifo*:dcfifo_*|dcfifo_*:auto_generated|dffpipe_3dc:wraclr|dffe13a[0]}

# False path between hold_time and compare_time due to the way the FSM is setup
set_false_path -from {*x_tamer|hold_time[*]} -to {*x_tamer|compare_time[*]}

# Asynchronous clear on the RX FIFO when disabling RX
set_false_path -from {fx3_gpif*rx_enable} -to {rx_meta_fifo*:wraclr|*}
set_false_path -from {fx3_gpif*rx_enable} -to {rx_fifo*:wraclr|*}
