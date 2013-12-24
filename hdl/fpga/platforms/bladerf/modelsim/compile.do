vlib nuand

vcom -work nuand -2008 ../../../ip/nuand/simulation/util.vhd
vcom -work nuand -2008 ../../../ip/nuand/simulation/fx3_model.vhd
vcom -work nuand -2008 ../../../ip/nuand/simulation/lms6002d_model.vhd

vcom -work nuand -2008 ../../../ip/nuand/synthesis/fifo_reader.vhd
vcom -work nuand -2008 ../../../ip/nuand/synthesis/fifo_writer.vhd
vcom -work nuand -2008 ../../../ip/nuand/synthesis/synchronizer.vhd
vcom -work nuand -2008 ../../../ip/nuand/synthesis/reset_synchronizer.vhd
vcom -work nuand -2008 ../../../ip/nuand/synthesis/signal_generator.vhd
vcom -work nuand -2008 ../../../ip/nuand/synthesis/tan_table.vhd
vcom -work nuand -2008 ../../../ip/nuand/synthesis/iq_correction.vhd
vcom -work nuand -2008 ../../../ip/nuand/synthesis/lms6002d/vhdl/lms6002d.vhd

vcom -work nuand -2008 ../../../ip/altera/pll/pll.vhd
vcom -work nuand -2008 ../../../ip/altera/rx_fifo/rx_fifo.vhd
vcom -work nuand -2008 ../../../ip/altera/tx_fifo/tx_fifo.vhd
vcom -work nuand -2008 ../../../ip/altera/tx_meta_fifo/tx_meta_fifo.vhd
vcom -work nuand -2008 ../../../ip/altera/rx_meta_fifo/rx_meta_fifo.vhd
vcom -work nuand -2008 ../../../ip/altera/nios_system/simulation/nios_system.vhd

vcom -work nuand -2008 ../vhdl/fx3_gpif.vhd

vcom -work nuand -2008 ../vhdl/bladerf.vhd
vcom -work nuand -2008 ../vhdl/bladerf-hosted.vhd

vcom -work nuand -2008 ../vhdl/tb/bladerf_tb.vhd

vsim -t 1ps nuand.bladerf_tb

