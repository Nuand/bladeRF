proc compile_nuand { root } {
    vlib nuand

    vcom -work nuand -2008 [file join $root ../altera/tx_fifo/tx_fifo.vhd]


    vcom -work nuand -2008 [file join $root ./synthesis/constellation_mapper.vhd]
    vcom -work nuand -2008 [file join $root ./synthesis/sync_fifo.vhd]
    vcom -work nuand -2008 [file join $root ./synthesis/uart.vhd]

    vcom -work nuand -2008 [file join $root ./synthesis/cordic.vhd]
    vcom -work nuand -2008 [file join $root ./synthesis/nco.vhd]
    vcom -work nuand -2008 [file join $root ./synthesis/fsk_modulator.vhd]
    vcom -work nuand -2008 [file join $root ./synthesis/fsk_demodulator.vhd]
    vcom -work nuand -2008 [file join $root ./synthesis/uart.vhd]

    vcom -work nuand -2008 [file join $root ./synthesis/tan_table.vhd]
    vcom -work nuand -2008 [file join $root ./synthesis/iq_correction.vhd]

    vcom -work nuand -2008 [file join $root ./synthesis/synchronizer.vhd]
    vcom -work nuand -2008 [file join $root ./synthesis/handshake.vhd]
    vcom -work nuand -2008 [file join $root ./synthesis/tb/handshake_tb.vhd]

    vcom -work nuand -2008 [file join $root ./synthesis/signal_processing_p.vhd]

    vcom -work nuand -2008 [file join $root ./synthesis/bit_stripper.vhd]
    vcom -work nuand -2008 [file join $root ./synthesis/fir_filter.vhd]
    vcom -work nuand -2008 [file join $root ./synthesis/atsc_tx.vhd]
    vcom -work nuand -2008 [file join $root ./simulation/util.vhd]
    vcom -work nuand -2008 [file join $root ./synthesis/tb/fir_filter_tb.vhd]

    vcom -work nuand -2008 [file join $root ./synthesis/tb/atsc_tx_tb.vhd]

    vcom -work nuand -2008 [file join $root ./simulation/fx3_model.vhd]
    vcom -work nuand -2008 [file join $root ./simulation/lms6002d_model.vhd]

    vcom -work nuand -2008 [file join $root ./synthesis/lms6002d/vhdl/lms6002d.vhd]
    vcom -work nuand -2008 [file join $root ./synthesis/lms6002d/vhdl/tb/lms6002d_tb.vhd]

    vcom -work nuand -2008 [file join $root ../altera/rx_fifo/rx_fifo.vhd]
    vcom -work nuand -2008 [file join $root ../altera/tx_fifo/tx_fifo.vhd]
    vcom -work nuand -2008 [file join $root ../altera/rx_meta_fifo/rx_meta_fifo.vhd]
    vcom -work nuand -2008 [file join $root ../altera/tx_meta_fifo/tx_meta_fifo.vhd]

    vcom -work nuand -2008 [file join $root ./synthesis/fifo_reader.vhd]
    vcom -work nuand -2008 [file join $root ./synthesis/fifo_writer.vhd]
    vcom -work nuand -2008 [file join $root ./simulation/sample_stream_tb.vhd]
}

