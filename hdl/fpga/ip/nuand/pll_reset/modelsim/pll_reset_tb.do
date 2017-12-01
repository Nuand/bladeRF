set vcom_lib nuand
set vcom_opts {-2008}

vlib ${vcom_lib}

# UUT
vcom {*}$vcom_opts -work ${vcom_lib} ../vhdl/pll_reset.vhd

# Testbench
vcom {*}$vcom_opts -work ${vcom_lib} ../vhdl/tb/pll_reset_tb.vhd

vsim ${vcom_lib}.pll_reset_tb

add wave sim:/pll_reset_tb/*
add wave sim:/pll_reset_tb/U_pll_reset/*

run -all
wave zoomfull
