# Clock inputs
create_clock -period "38.4 MHz"  [get_ports c5_clock1]
create_clock -period "38.4 MHz"  [get_ports c5_clock2]
create_clock -period "250.0 MHz" [get_ports adi_rx_clock]

# Generate the appropriate PLL clocks
derive_pll_clocks
derive_clock_uncertainty

# First flop synchronizer false path
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

set_false_path -from * -to [get_ports ps_sync_1p*]
set_false_path -from * -to [get_ports {rx_bias_en tx_bias_en}]
set_false_path -from * -to [get_ports exp_gpio[*]]
set_false_path -from * -to [get_ports adf_ce]

# FX3 UART interface
set_false_path -from * -to [get_ports fx3_uart_cts]
set_false_path -from * -to [get_ports fx3_uart_rxd]
set_false_path -from [get_ports fx3_uart_txd] -to *

## VCTCXO trimdac
#set_output_delay -clock [get_clocks U_system_pll*0*] -min 0.0 [get_ports {dac_csn dac_sclk dac_sdi}]
#set_output_delay -clock [get_clocks U_system_pll*0*] -max 0.2 [get_ports {dac_csn dac_sclk dac_sdi}] -add_delay

# LEDs
set_false_path -from * -to [get_ports led*]

# LMS long lived GPIO and RF Switches
set_false_path -from * -to [get_ports {adi_*x_spdt*_v* adi_reset_n}]

# JTAG settings
set_clock_groups -exclusive -group [get_clocks altera_reserved_tck]
set_input_delay  -clock [get_clocks altera_reserved_tck] 2.0 [get_ports altera_reserved_tdi]
set_input_delay  -clock [get_clocks altera_reserved_tck] 2.0 [get_ports altera_reserved_tms]
set_output_delay -clock [get_clocks altera_reserved_tck] 2.0 [get_ports altera_reserved_tdo]

# Exceptions

# The TX dcfifo aclr (TX clock) has recovery violation going to the write clock domain (PCLK)
# The DCFIFO documentation says to false path aclr-->rdclk, but we need to do it to wrclk.
# Has not been an issue so far, so probably safe?
# With the LVDS cores, the TX PLL clock got merged with the RX PLL clock
set_false_path -from {reset_synchronizer:U_reset_sync_rx|sync} -to {tx:U_tx|tx_*fifo*common_dcfifo*dffpipe_3dc:wraclr|dffe12a[0]}
set_false_path -from {reset_synchronizer:U_reset_sync_rx|sync} -to {tx:U_tx|tx_*fifo*common_dcfifo*dffpipe_3dc:wraclr|dffe13a[0]}
set_false_path -from {reset_synchronizer:U_reset_sync_tx|sync} -to {tx:U_tx|tx_*fifo*common_dcfifo*dffpipe_3dc:wraclr|dffe12a[0]}
set_false_path -from {reset_synchronizer:U_reset_sync_tx|sync} -to {tx:U_tx|tx_*fifo*common_dcfifo*dffpipe_3dc:wraclr|dffe13a[0]}

# False path between hold_time and compare_time due to the way the FSM is setup
set_false_path -from {*x_tamer|hold_time[*]} -to {*x_tamer|compare_time[*]}

# Asynchronous clear on the RX FIFO when disabling RX
set_false_path -from {fx3_gpif*rx_enable} -to {rx:U_rx|rx_meta_fifo*:wraclr|*}
set_false_path -from {fx3_gpif*rx_enable} -to {rx:U_rx|rx_fifo*:wraclr|*}
