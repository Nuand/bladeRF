# Initializes simulation of FX3 GPIF interface

proc compile_fx3_gpif { root } {
    vlib work

    vcom -2008 -work work [file join $root ./synthesis/set_clear_ff.vhd]
    vcom -2008 -work work [file join $root ../../platforms/common/bladerf/vhdl/fx3_gpif.vhd]
    vcom -2008 -work work [file join $root ./simulation/util.vhd]
    vcom -2008 -work work [file join $root ./simulation/fx3_model.vhd]
    vcom -2008 -work work [file join $root ../../platforms/common/bladerf/vhdl/tb/fx3_gpif_tb.vhd]
}

proc simulate_fx3_gpif { } {
    vsim -L altera_lnsim fx3_gpif_tb
}

proc waves_fx3_gpif { } {
    add wave sim:/fx3_gpif_tb/sys_clk
    add wave sim:/fx3_gpif_tb/fx3_pclk
    add wave sim:/fx3_gpif_tb/fx3_pclk_pll
    add wave -radix hexadecimal -expand -group tb *
    add wave -radix hexadecimal -expand -group gpif sim:/fx3_gpif_tb/U_fx3_gpif/*
    add wave -radix hexadecimal -expand -group model sim:/fx3_gpif_tb/U_fx3_model/*
}

if [info exists root] {
    compile_fx3_gpif $root
} else {
    # We assume we're in hdl/quartus/work/bladerf-micro/simulation/modelsim
    compile_fx3_gpif [file join [pwd] .. .. .. .. .. fpga ip nuand]
}

simulate_fx3_gpif
waves_fx3_gpif
