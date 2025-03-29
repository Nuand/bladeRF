source ../../../ip/nuand/nuand.do
compile_nuand ../../../ip/nuand bladerf

vcom -work nuand -2008 ../../../ip/altera/pll/pll.vhd
vcom -work nuand -2008 ../../../ip/altera/fx3_pll/fx3_pll.vhd

vcom -work nuand -2008 ../vhdl/tb/nios_system.vhd

vcom -work nuand -2008 ../../common/bladerf/vhdl/fx3_gpif_p.vhd
vcom -work nuand -2008 ../../common/bladerf/vhdl/fx3_gpif.vhd

vcom -work nuand -2008 ../../../ip/nuand/lms6_spi_controller/vhdl/lms6_spi_controller.vhd
vcom -work nuand -2008 ../../../ip/nuand/synthesis/lms6002d/vhdl/bladerf_agc_lms_drv.vhd
vcom -work nuand -2008 ../../../ip/nuand/synthesis/lms6002d/vhdl/bladerf_agc.vhd

vcom -work nuand -2008 ../vhdl/bladerf.vhd
vcom -work nuand -2008 ../vhdl/bladerf_p.vhd
vcom -work nuand -2008 ../vhdl/bladerf-hosted.vhd

vcom -work nuand -2008 ../vhdl/tb/bladerf_tb.vhd

vsim -t 1ps nuand.bladerf_tb

