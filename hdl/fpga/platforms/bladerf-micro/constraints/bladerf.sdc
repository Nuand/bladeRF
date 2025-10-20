# bladeRF Micro Design Constraints File
#
# Copyright (c) 2013-2017 Nuand LLC
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

# Clock inputs
create_clock -period "38.4 MHz"  [get_ports c5_clock2]
create_clock -period "250.0 MHz" [get_ports adi_rx_clock]

# Generate the appropriate PLL clocks
derive_pll_clocks
derive_clock_uncertainty

# Platform-specific clock aliases
set fx3_clock    {U_fx3_pll|altera_pll_i|general[0].gpll~PLL_OUTPUT_COUNTER|divclk}
set system_clock {U_system_pll|altera_pll_i|general[0].gpll~PLL_OUTPUT_COUNTER|divclk}

# Trace delays between AD9361 and FPGA (bladeRF Micro)
set adi_spi_clk_trace_delay     0.127
set adi_spi_di_trace_delay      0.168
set adi_spi_do_trace_delay      0.119
set adi_spi_enb_trace_delay     0.137

# Trace delays between FX3 and FPGA (bladeRF Micro)
set fx3_data_trace_delay        0.260
set fx3_sclk_trace_delay        0.000
set fx3_dclk_trace_delay        0.226

# Trace delays between ADF4002 and FPGA (bladeRF Micro)
set adf_spi_clk_trace_delay     0.383
set adf_spi_data_trace_delay    0.373
set adf_spi_le_trace_delay      0.311

# Trace delays between AD5621 and FPGA (bladeRF Micro)
set dac_spi_sclk_trace_delay    0.164
set dac_spi_sdin_trace_delay    0.181
set dac_spi_nsync_trace_delay   0.193

# First flop synchronizer false path
set_false_path -from [get_registers {*source_holding[*]}] -to *

# Slow Interfaces
set_false_path -from *             -to [get_ports ps_sync_1p*]
set_false_path -from *             -to [get_ports {rx_bias_en tx_bias_en}]
set_false_path -from *             -to [get_ports {exp_gpio[*] exp_clock_oe}]
set_false_path -from *             -to [get_ports adf_ce]
set_false_path -from *             -to [get_ports led*]
set_false_path -from *             -to [get_ports {adi_*x_spdt*_v*}]
set_false_path -from *             -to [get_ports si_clock_sel]
set_false_path -from *             -to [get_ports ufl_clock_oe]
set_false_path -from adf_muxout    -to *
set_false_path -from exp_clock_req -to *
set_false_path -from exp_present   -to *
set_false_path -from pwr_status    -to *

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

# Mini Expansion Port (J51)
set_false_path -from [get_ports mini_exp*] -to *
set_false_path -from *                     -to [get_ports mini_exp*]
