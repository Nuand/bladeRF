source ../../../ip/nuand/nuand.do

vcom -work nuand -2008 ../vhdl/rx.vhd
vcom -work nuand -2008 ../vhdl/tx.vhd

compile_nuand ../../../ip/nuand bladerf-micro

vcom -work nuand -2008 ../vhdl/tb/loopback_tb.vhd

vsim -t 1ps nuand.loopback_tb
