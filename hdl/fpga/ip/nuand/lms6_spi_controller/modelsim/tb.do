set vcom_lib nuand
set vcom_opts {-2008}

vlib ${vcom_lib}

# UUT
vcom {*}$vcom_opts -work ${vcom_lib} ../vhdl/lms6_spi_controller.vhd

# Models
vcom {*}$vcom_opts -work ${vcom_lib} ../vhdl/tb/lms6002d_model.vhd
vcom {*}$vcom_opts -work ${vcom_lib} ../vhdl/tb/nios_model.vhd

# Testbench
vcom {*}$vcom_opts -work ${vcom_lib} ../vhdl/tb/spi_master_tb.vhd

vsim ${vcom_lib}.spi_master_tb

log -r *

add wave -r -hex sim:/spi_master_tb/uut/*
add wave    -hex sim:/spi_master_tb/lms/*
add wave    -hex sim:/spi_master_tb/lms/spi_reg_proc/*

run -all

wave zoomfull
