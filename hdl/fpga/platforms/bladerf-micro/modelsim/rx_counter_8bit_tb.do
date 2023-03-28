source ../../../ip/nuand/nuand.do

vcom -work nuand -2008 ../vhdl/rx.vhd
vcom -work nuand -2008 ../vhdl/tx.vhd

vlib rtl_work
vcom -work rtl_work -2008 ../../../ip/altera/fx3_pll/fx3_pll.vhd

compile_nuand ../../../ip/nuand bladerf-micro

vcom -work nuand -2008 ../vhdl/tb/fx3_model.vhd
vcom -work nuand -2008 ../vhdl/tb/rx_counter_8bit_tb.vhd
