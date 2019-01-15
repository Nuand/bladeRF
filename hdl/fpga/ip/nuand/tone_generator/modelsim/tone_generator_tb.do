# Initializes simulation of tone generator

proc compile_tone_generator { root } {
    vlib work

    vcom -2008 -work work [file join $root . synthesis cordic.vhd]
    vcom -2008 -work work [file join $root . synthesis nco.vhd]
    vcom -2008 -work work [file join $root . synthesis reset_synchronizer.vhd]
    vcom -2008 -work work [file join $root . synthesis synchronizer.vhd]
    vcom -2008 -work work [file join $root . simulation util.vhd]

    vcom -2008 -work work [file join $root tone_generator vhdl tone_generator.vhd]
    vcom -2008 -work work [file join $root tone_generator vhdl tone_generator_hw.vhd]
    vcom -2008 -work work [file join $root tone_generator vhdl tb tone_generator_tb.vhd]
}

proc simulate_tone_generator { } {
    vsim -t 10ns -L altera_lnsim tone_generator_tb(tb_tgen)
}

proc simulate_tone_generator_hw { } {
    vsim -t 10ns -L altera_lnsim tone_generator_tb(tb_hw)
}

proc waves_tone_generator { } {
    add wave sim:/tone_generator_tb/clock
    add wave -format analog -max 2048 -min -2047 -height 100 sim:/tone_generator_tb/outputs.re
    add wave -format analog -max 2048 -min -2047 -height 100 sim:/tone_generator_tb/outputs.im
    add wave sim:/tone_generator_tb/outputs.valid
    add wave -expand -group tb *
    add wave -expand -group tone_generator sim:/tone_generator_tb/U_tone_generator/*
}

proc waves_tone_generator_hw { } {
    add wave sim:/tone_generator_tb/clock
    add wave sim:/tone_generator_tb/sample_clk
    add wave -format analog -max 2048 -min -2047 -height 100 sim:/tone_generator_tb/sample_i
    add wave -format analog -max 2048 -min -2047 -height 100 sim:/tone_generator_tb/sample_q
    add wave sim:/tone_generator_tb/sample_valid
    add wave -expand -group tb *
    add wave -expand -group tone_generator sim:/tone_generator_tb/U_tone_generator_hw/U_tone_generator/*
    add wave -expand -group mmap sim:/tone_generator_tb/mm_*
    add wave -expand -group tgen_hw sim:/tone_generator_tb/U_tone_generator_hw/*
}

proc run_sim { } {
    simulate_tone_generator
    waves_tone_generator
    run -all
}

proc run_sim_hw { } {
    simulate_tone_generator_hw
    waves_tone_generator_hw
    run -all
}

if [info exists root] {
    compile_tone_generator $root
} else {
    # we assume we're in hdl/fpga/ip/nuand/tone_generator/modelsim
    compile_tone_generator [file join [pwd] .. ..]
}
