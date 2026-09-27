source ../../../ip/nuand/nuand.do

vcom -work nuand -2008 ../vhdl/rx.vhd
vcom -work nuand -2008 ../vhdl/tx.vhd

vlib rtl_work
vcom -work nuand -2008 ../vhdl/tb/fx3_pll.vhd

compile_nuand ../../../ip/nuand bladerf-micro

vcom -work nuand -2008 ../vhdl/tb/fx3_model.vhd
vcom -work nuand -2008 ../vhdl/tb/fx3_gpif_iq_8bit_tb.vhd
