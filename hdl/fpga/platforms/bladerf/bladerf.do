proc compile_bladerf { root } {
	vlib pll
	vcom -work pll  [file join $root ../../ip/altera/pll/pll.vhd]

	vlib serial_pll
	vcom -work serial_pll [file join $root ../../ip/altera/serial_pll/serial_pll.vhd]

    vlib nuand
    vcom -work nuand -2008 [file join $root ./vhdl/fx3.vhd]
    vcom -work nuand -2008 [file join $root ./vhdl/bladerf_debug_p.vhd]
    vcom -work nuand -2008 [file join $root ./vhdl/ramp.vhd]
    vcom -work nuand -2008 [file join $root ./vhdl/bladerf_p.vhd]
    vcom -work nuand -2008 [file join $root ./vhdl/bladerf.vhd]
	
    vcom -work nuand -2008 [file join $root ./vhdl/tb/bladerf_tb.vhd]
}
