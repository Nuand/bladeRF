# bladeRF Design Constraints File for Analog Devices AD9361 discretes/SPI
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
# * AD9361 Reference Manual UG-570
# * Intel LVDS SERDES Transmitter/Receiver IP Cores User Guide UG-MF9504

### AD9361 LVDS ###

# Per Altera's documentation, the ALTLVDS_RX and ALTLVDS_TX IP cores are
# guaranteed to function correctly, and timing constraints are not required.
# (Exception: if they do not function correctly, then timing constraints are
# required.)

### AD9361 SPI ###

# NOTE: This relies on the Qsys SPI component having synchronizer stages
# enabled (timing verified w/ depth of 2). This creates a false path for
# the adi_spi_sdo port.

# Create generated clocks. Input clock to the SPI block in the nios_system
# is 80 MHz, output clock is 40 MHz, so it's a division by 2.
set adi_sclk_divisor 2

create_generated_clock -name adi_sclk_reg -source [get_pins $system_clock] -divide_by $adi_sclk_divisor [get_registers {*rffe_spi|SCLK_reg}]
create_generated_clock -name adi_sclk_pin -source [get_registers -no_duplicates {*rffe_spi|SCLK_reg}] [get_ports {adi_spi_sclk}]

set_max_skew -from [get_clocks {adi_sclk_pin}] -to [get_ports {adi_spi_sclk}] 0.2

# Clock to Data-out times
set adi_spi_data_tco_min 3.0
set adi_spi_data_tco_max 8.0

# Data-in to Clock setup and hold times
set adi_spi_data_ts 2.0
set adi_spi_data_th 1.0

# Enable to Clock setup and hold times
set adi_spi_enb_ts  1.0
set adi_spi_enb_th  0.0

# Constraints for adi_spi_do (AD9361 -> FPGA)
# Max input delay = tco_max + (data_trace_delay + src_clock_trace_delay)
set v [expr {$adi_spi_data_tco_max + ($adi_spi_do_trace_delay + $adi_spi_clk_trace_delay)}]
set_input_delay -clock [get_clocks adi_sclk_pin] -max $v [get_ports adi_spi_sdo]

# Min input delay = tco_min + (data_trace_delay + src_clock_trace_delay)
set v [expr {$adi_spi_data_tco_min + ($adi_spi_do_trace_delay + $adi_spi_clk_trace_delay)}]
set_input_delay -clock [get_clocks adi_sclk_pin] -min 3.119 [get_ports adi_spi_sdo]

# Multicycle constraints
set_multicycle_path -setup -start -from [get_clocks {adi_sclk_pin}] -to [get_clocks $system_clock] [expr {$adi_sclk_divisor}]
set_multicycle_path -hold  -start -from [get_clocks {adi_sclk_pin}] -to [get_clocks $system_clock] [expr {$adi_sclk_divisor}]


# Constraints for adi_spi_di/enb (FPGA -> AD9361)
# Max output delay = setup + (data_trace_delay + src_clock_trace_delay)
set v [expr {$adi_spi_data_ts + ($adi_spi_di_trace_delay + $adi_spi_clk_trace_delay)}]
set_output_delay -clock [get_clocks adi_sclk_pin] -max $v [get_ports {adi_spi_sdi}]

set v [expr {$adi_spi_enb_ts + ($adi_spi_enb_trace_delay + $adi_spi_clk_trace_delay)}]
set_output_delay -clock [get_clocks adi_sclk_pin] -max $v [get_ports {adi_spi_csn}]

# Min output delay = -1 * hold + (data_trace_delay + src_clock_trace_delay)
set v [expr {(-1 * $adi_spi_data_th) + ($adi_spi_di_trace_delay + $adi_spi_clk_trace_delay)}]
set_output_delay -clock [get_clocks adi_sclk_pin] -min $v [get_ports {adi_spi_sdi}]

set v [expr {(-1 * $adi_spi_enb_th) + ($adi_spi_enb_trace_delay + $adi_spi_clk_trace_delay)}]
set_output_delay -clock [get_clocks adi_sclk_pin] -min $v [get_ports {adi_spi_csn}]

### AD9361 Discretes ###

# adi_ctrl_in is asynchronous, but must be held for at least two ClkRF cycles
set_false_path -from * -to [get_ports {adi_ctrl_in[*]}]

# adi_sync_in does have timing requirements, but we do not use it at this time
set_false_path -from * -to [get_ports {adi_sync_in}]

# Long-lived control lines
set_false_path -from * -to [get_ports {adi_en_agc}]
set_false_path -from * -to [get_ports {adi_enable}]
set_false_path -from * -to [get_ports {adi_txnrx}]
set_false_path -from * -to [get_ports {adi_reset_n}]
