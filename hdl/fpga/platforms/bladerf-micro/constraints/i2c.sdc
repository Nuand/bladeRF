# bladeRF Micro Design Constraints File for I2C interfaces
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

# Create generated clocks. Input clock to the SPI block in the common_system
# is 80 MHz, actual output clock is 400 kHz
set pwr_scl_divisor 200

create_generated_clock -name i2c_scl_reg -source [get_pins $system_clock] -divide_by $pwr_scl_divisor [get_registers {*bit_controller|scl_oen}]
create_generated_clock -name pwr_scl_pin -source [get_registers -no_duplicates {*bit_controller|scl_oen}] [get_ports {pwr_scl}]

set_max_skew -from [get_clocks {pwr_scl_pin}] -to [get_ports {pwr_scl}] 0.2

# Constraints for INA219 data
set ina_i2c_data_ts 100.0
set ina_i2c_data_th 900.0

set_input_delay  -clock [get_clocks pwr_scl_pin] -max $ina_i2c_data_ts [get_ports {pwr_sda}]
set_input_delay  -clock [get_clocks pwr_scl_pin] -min $ina_i2c_data_th [get_ports {pwr_sda}]

set_output_delay -clock [get_clocks pwr_scl_pin] -max $ina_i2c_data_ts [get_ports {pwr_sda}]
set_output_delay -clock [get_clocks pwr_scl_pin] -min $ina_i2c_data_th [get_ports {pwr_sda}]

# Multicycle constraints
set_multicycle_path -setup -start -from [get_clocks $system_clock] -to [get_clocks {pwr_scl_pin}] [expr {$pwr_scl_divisor / 2}]
set_multicycle_path -hold  -start -from [get_clocks $system_clock] -to [get_clocks {pwr_scl_pin}] [expr {$pwr_scl_divisor - 1}]
set_multicycle_path -setup -end   -from [get_clocks {pwr_scl_pin}] -to [get_clocks $system_clock] [expr {$pwr_scl_divisor / 2}]
set_multicycle_path -hold  -end   -from [get_clocks {pwr_scl_pin}] -to [get_clocks $system_clock] [expr {$pwr_scl_divisor - 1}]
