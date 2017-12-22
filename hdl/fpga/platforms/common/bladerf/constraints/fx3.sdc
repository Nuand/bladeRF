# bladeRF Design Constraints File for Cypress FX3 Interface
#
# Copyright (c) 2017 Nuand LLC
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

# References:
# * EZ-USB FX3 SuperSpeed USB Controller datasheet
# * EZ-USB FX3 Technical Reference Manual
# * Altera AN 433 (Constraining and Analyzing Source-Synchronous Interfaces)
# * I/O Timing Constrainer (quartus_sta --ssc hosted)

### FX3 PCLK ###

# Clock frequency
set clock_frequency "100.0 MHz"

# Virtual clock for the FX3's PCLK output pin
create_clock -period $clock_frequency -name fx3_virtual

# Clock input for the FPGA end of the FX3 PCLK
create_clock -period $clock_frequency [get_ports fx3_pclk]

### FX3 GPIF and CTL Interfaces ###

# Clock to data-out propagation delays
set fx3_data_tco_max    9.0
set fx3_data_tco_min    7.0

# Data-in to Clock setup and hold times
set fx3_data_ts         2.0
set fx3_data_th         0.5

# Constraints for fx3_gpif_in (FX3 -> FPGA)
# Max input delay = tco_max + (data_trace_delay - dest_clock_trace_delay)
set v [expr {$fx3_data_tco_max + ($fx3_data_trace_delay - $fx3_dclk_trace_delay)}]

set_input_delay -clock [get_clocks fx3_virtual] -max $v [get_ports {fx3_gpif[*]}]
set_input_delay -clock [get_clocks fx3_virtual] -max $v [get_ports {fx3_ctl[*]}]

# Min input delay = tco_min + (data_trace_delay - dest_clock_trace_delay)
set v [expr {$fx3_data_tco_min + ($fx3_data_trace_delay - $fx3_dclk_trace_delay)}]

set_input_delay -clock [get_clocks fx3_virtual] -min $v [get_ports {fx3_gpif[*]}]
set_input_delay -clock [get_clocks fx3_virtual] -min $v [get_ports {fx3_ctl[*]}]

# Constraints for fx3_gpif_out (FPGA -> FX3)
# Max output delay = setup + (data_trace_delay + src_clock_trace_delay)
set v [expr {$fx3_data_ts + ($fx3_data_trace_delay + $fx3_sclk_trace_delay)}]

set_output_delay -clock [get_clocks fx3_virtual] -max $v [get_ports {fx3_gpif[*]}]
set_output_delay -clock [get_clocks fx3_virtual] -max $v [get_ports {fx3_ctl[*]}]

# Min output delay = -1 * hold + (data_trace_delay + src_clock_trace_delay)
set v [expr {(-1 * $fx3_data_th) + ($fx3_data_trace_delay + $fx3_sclk_trace_delay)}]

set_output_delay -clock [get_clocks fx3_virtual] -min $v [get_ports {fx3_gpif[*]}]
set_output_delay -clock [get_clocks fx3_virtual] -min $v [get_ports {fx3_ctl[*]}]

# Multicycle exceptions
set_multicycle_path -setup -start -from [get_clocks fx3_pclk] -to [get_clocks fx3_virtual] 1
set_multicycle_path -hold  -start -from [get_clocks fx3_pclk] -to [get_clocks fx3_virtual] 1

set_multicycle_path -setup -start -from [get_clocks $fx3_clock] -to [get_clocks {fx3_virtual}] 2
set_multicycle_path -hold  -start -from [get_clocks $fx3_clock] -to [get_clocks {fx3_virtual}] 2

set_multicycle_path -setup -start -from [get_clocks {fx3_virtual}] -to [get_clocks $fx3_clock] 2
set_multicycle_path -hold  -start -from [get_clocks {fx3_virtual}] -to [get_clocks $fx3_clock] 2

### FX3 UART interface ###

# This is an asynchronous serial interface that runs at 4 MHz.
set_false_path -from [get_ports {fx3_uart_*}] -to *
set_false_path -from *                        -to [get_ports {fx3_uart_*}]

### FX3 signalling ###

# Asynchronous clear on the RX FIFO when disabling RX
set_false_path -from {fx3_gpif*rx_enable} -to {*rx_meta_fifo*:wraclr|*}
set_false_path -from {fx3_gpif*rx_enable} -to {*rx_fifo*:wraclr|*}
