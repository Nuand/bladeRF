# bladeRF Micro Design Constraints File for peripheral SPI/I2C interfaces
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
# * Analog Devices ADF4002 datasheet
# * Analog Devices AD5601/AD5611/AD5621 datasheet
# * http://www.alterawiki.com/wiki/Constrain_SPI_Core

### Peripheral SPI ###

# This bus services the ADF4002 and the AD5621.

# Create generated clocks. Input clock to the SPI block in the common_system
# is 80 MHz, actual output clock is 8 MHz. Note: the ADF4002 clock is inverted.
set peri_sclk_divisor 10

create_generated_clock -name peri_sclk_reg -source [get_pins $system_clock] -divide_by $peri_sclk_divisor [get_registers {*peripheral_spi|SCLK_reg}]
create_generated_clock -name adf_sclk_pin  -source [get_registers -no_duplicates {*peripheral_spi|SCLK_reg}] -phase 180 [get_ports {adf_sclk}]
create_generated_clock -name dac_sclk_pin  -source [get_registers -no_duplicates {*peripheral_spi|SCLK_reg}]            [get_ports {dac_sclk}]

set_max_skew -from [get_clocks {adf_sclk_pin}] -to [get_ports {adf_sclk}] 0.2
set_max_skew -from [get_clocks {dac_sclk_pin}] -to [get_ports {dac_sclk}] 0.2

# Data-in to Clock setup and hold times
set adf_spi_data_ts     10.0
set adf_spi_data_th     10.0

# Data-in to Clock setup and hold times
set dac_spi_data_ts     5.0
set dac_spi_data_th     4.5

# Enable to Clock setup and hold times
set dac_spi_nsync_ts    10.0
set dac_spi_nsync_th    0.0

# Constraints for ADF data/enable (FPGA -> ADF4002)
# Max output delay = setup + (data_trace_delay + src_clock_trace_delay)
set v [expr {$adf_spi_data_ts + ($adf_spi_data_trace_delay + $adf_spi_clk_trace_delay)}]
set_output_delay -clock [get_clocks adf_sclk_pin] -max $v [get_ports {adf_sdi}]

set v [expr {$adf_spi_data_ts + ($adf_spi_le_trace_delay + $adf_spi_clk_trace_delay)}]
set_output_delay -clock [get_clocks adf_sclk_pin] -max $v [get_ports {adf_csn}]

# Min output delay = -1 * hold + (data_trace_delay + src_clock_trace_delay)
set v [expr {(-1 * $adf_spi_data_th) + ($adf_spi_data_trace_delay + $adf_spi_clk_trace_delay)}]
set_output_delay -clock [get_clocks adf_sclk_pin] -min $v [get_ports {adf_sdi}]

set v [expr {(-1 * $adf_spi_data_th) + ($adf_spi_le_trace_delay + $adf_spi_clk_trace_delay)}]
set_output_delay -clock [get_clocks adf_sclk_pin] -min $v [get_ports {adf_csn}]

# Multicycle constraints
set_multicycle_path -setup -start -from [get_clocks $system_clock] -to [get_clocks {adf_sclk_pin}] [expr {$peri_sclk_divisor / 2}]
set_multicycle_path -hold  -start -from [get_clocks $system_clock] -to [get_clocks {adf_sclk_pin}] [expr {$peri_sclk_divisor - 1}]

# Constraints for DAC data/enable (FPGA -> AD5621)
# Max output delay = setup + (data_trace_delay + src_clock_trace_delay)
set v [expr {$dac_spi_data_ts + ($dac_spi_sdin_trace_delay + $dac_spi_sclk_trace_delay)}]
set_output_delay -clock [get_clocks dac_sclk_pin] -max $v [get_ports {dac_sdi}]

set v [expr {$dac_spi_nsync_ts + ($dac_spi_nsync_trace_delay + $dac_spi_sclk_trace_delay)}]
set_output_delay -clock [get_clocks dac_sclk_pin] -max $v [get_ports {dac_csn}]

# Min output delay = -1 * hold + (data_trace_delay + src_clock_trace_delay)
set v [expr {(-1 * $dac_spi_data_th) + ($dac_spi_sdin_trace_delay + $dac_spi_sclk_trace_delay)}]
set_output_delay -clock [get_clocks dac_sclk_pin] -min $v [get_ports {dac_sdi}]

set v [expr {(-1 * $dac_spi_nsync_th) + ($dac_spi_nsync_trace_delay + $dac_spi_sclk_trace_delay)}]
set_output_delay -clock [get_clocks dac_sclk_pin] -min $v [get_ports {dac_csn}]

# Multicycle constraints
set_multicycle_path -setup -start -from [get_clocks $system_clock] -to [get_clocks {dac_sclk_pin}] [expr {$peri_sclk_divisor / 2}]
set_multicycle_path -hold  -start -from [get_clocks $system_clock] -to [get_clocks {dac_sclk_pin}] [expr {$peri_sclk_divisor - 1}]
