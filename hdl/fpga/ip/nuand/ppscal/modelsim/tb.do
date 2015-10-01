set vcom_lib nuand
set vcom_opts {-2008}

vlib ${vcom_lib}

# Support
vcom {*}$vcom_opts -work ${vcom_lib} ../../synthesis/synchronizer.vhd
vcom {*}$vcom_opts -work ${vcom_lib} ../../synthesis/handshake.vhd

# UUT
vcom {*}$vcom_opts -work ${vcom_lib} ../vhdl/pps_counter.vhd
vcom {*}$vcom_opts -work ${vcom_lib} ../vhdl/ppscal.vhd

# Testbench
vcom {*}$vcom_opts -work ${vcom_lib} ../vhdl/tb/mm_driver.vhd
vcom {*}$vcom_opts -work ${vcom_lib} ../vhdl/tb/ppscal_tb.vhd

vsim ${vcom_lib}.ppscal_tb

log -r *

add wave -hex sim:/ppscal_tb/uut/*
add wave -hex sim:/ppscal_tb/driver/*

run 500 us

wave zoomfull
