source ../../../ip/nuand/nuand.do
compile_nuand ../../../ip/nuand bladerf-micro

vcom -work nuand -2008 ../vhdl/tb/nios_system.vhd

vcom -work nuand -2008 ../../../platforms/common/bladerf/vhdl/fx3_gpif.vhd

vcom -work nuand -2008 ../vhdl/bladerf_p.vhd
vcom -work nuand -2008 ../vhdl/bladerf.vhd
vcom -work nuand -2008 ../vhdl/rx.vhd
vcom -work nuand -2008 ../vhdl/tx.vhd
vcom -work nuand -2008 ../vhdl/bladerf-hosted.vhd

vcom -work nuand -2008 ../vhdl/tb/fx3_pll.vhd
vcom -work nuand -2008 ../vhdl/tb/system_pll.vhd
vcom -work nuand -2008 ../vhdl/tb/bladerf_tb.vhd

vsim -t 1ps nuand.bladerf_tb
