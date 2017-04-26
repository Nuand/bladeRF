# Create work library
vlib nuand

# Compile design files
vcom -work nuand -2008 ../../synthesis/synchronizer.vhd
vcom -work nuand -2008 ../../synthesis/handshake.vhd
vcom -work nuand -2008 ../vhdl/time_tamer.vhd

vcom -work nuand -2008 ../vhdl/tb/time_tamer_tb.vhd

# Elaborate design
vsim nuand.time_tamer_tb

# Add waves
add wave -hexadecimal sim:/time_tamer_tb/U_tamer/clock
add wave -hexadecimal sim:/time_tamer_tb/U_tamer/reset
add wave -hexadecimal sim:/time_tamer_tb/U_tamer/ts_clock
add wave -hexadecimal sim:/time_tamer_tb/U_tamer/ts_reset
add wave -hexadecimal sim:/time_tamer_tb/U_tamer/timestamp
add wave -hexadecimal sim:/time_tamer_tb/U_tamer/compare_time
add wave -hexadecimal sim:/time_tamer_tb/U_tamer/check_compare/fsm

# Run
run -all
