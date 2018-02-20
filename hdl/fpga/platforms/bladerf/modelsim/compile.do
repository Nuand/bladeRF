source ../../../ip/nuand/nuand.do
compile_nuand ../../../ip/nuand

vcom -work nuand -2008 ../../../ip/altera/pll/pll.vhd
vcom -work nuand -2008 ../../../ip/altera/fx3_pll/fx3_pll.vhd
vcom -work nuand -2008 ../../../ip/altera/rx_fifo/rx_fifo.vhd
vcom -work nuand -2008 ../../../ip/altera/tx_fifo/tx_fifo.vhd
vcom -work nuand -2008 ../../../ip/altera/tx_meta_fifo/tx_meta_fifo.vhd
vcom -work nuand -2008 ../../../ip/altera/rx_meta_fifo/rx_meta_fifo.vhd

vcom -work nuand -2008 ../vhdl/tb/nios_system.vhd

vcom -work nuand -2008 ../vhdl/fx3_gpif.vhd

vcom -work nuand -2008 ../vhdl/bladerf.vhd
vcom -work nuand -2008 ../vhdl/bladerf_p.vhd
vcom -work nuand -2008 ../vhdl/bladerf-hosted.vhd

vcom -work nuand -2008 ../vhdl/tb/bladerf_tb.vhd

vsim -t 1ps nuand.bladerf_tb

