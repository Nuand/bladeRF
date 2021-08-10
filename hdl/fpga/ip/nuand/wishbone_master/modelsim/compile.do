# Create work library
vlib nuand

# Compile design files
vcom -work nuand -2008 ../vhdl/wishbone_master.vhd

vcom -work nuand -2008 ../vhdl/tb/wishbone_master_tb.vhd

# Elaborate design
vsim nuand.wishbone_master_tb

# Add waves
add wave -hexadecimal "sim:/wishbone_master_tb/U_wishbone_master/*"

# Run
run -all
