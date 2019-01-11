# Initializes simulation of tone generator

proc compile_tone_generator { root } {
    vlib work

    vcom -2008 -work work [file join $root . synthesis cordic.vhd]
    vcom -2008 -work work [file join $root . synthesis nco.vhd]
    vcom -2008 -work work [file join $root . simulation util.vhd]

    vcom -2008 -work work [file join $root tone_generator vhdl tone_generator.vhd]
    vcom -2008 -work work [file join $root tone_generator vhdl tone_generator_hw.vhd]
    vcom -2008 -work work [file join $root tone_generator vhdl tb tone_generator_tb.vhd]
}

proc simulate_tone_generator { } {
    vsim -t 100ns -L altera_lnsim tone_generator_tb
}

proc waves_tone_generator { } {
    add wave sim:/tone_generator_tb/clock
    add wave -format analog -max 2048 -min -2047 -height 100 sim:/tone_generator_tb/outputs.re
    add wave -format analog -max 2048 -min -2047 -height 100 sim:/tone_generator_tb/outputs.im
    add wave sim:/tone_generator_tb/outputs.valid
    add wave -expand -group tb *
    add wave -expand -group tone_generator sim:/tone_generator_tb/U_tone_generator/*
}

if [info exists root] {
    compile_tone_generator $root
} else {
    # we assume we're in hdl/fpga/ip/nuand/tone_generator/modelsim
    compile_tone_generator [file join [pwd] .. ..]
}

# simulate_tone_generator
# waves_tone_generator
# run -all
