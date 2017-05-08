# Create work library
vlib nuand

# Compile design files
vcom -work nuand -2008 ../vhdl/arbiter.vhd

vcom -work nuand -2008 ../vhdl/tb/arbiter_tb.vhd

# Elaborate design
vsim nuand.arbiter_tb

# Add waves
add wave -hexadecimal "sim:/arbiter_tb/U_arbiter/*"

# Run
run -all
