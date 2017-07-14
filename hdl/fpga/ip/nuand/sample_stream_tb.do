# Start dir
set start_dir [pwd]

# Make sim dir and cd into it
set simdir sample_stream_tb
file mkdir ${simdir}
cd ${simdir}

# Platform settings
set platform "bladerf-micro"

if { ${platform} == "bladerf" } {
    set NUM_MIMO_STREAMS          1
    set FIFO_READER_READ_THROTTLE 1
} elseif { ${platform} == "bladerf-micro" } {
    set NUM_MIMO_STREAMS          2
    set FIFO_READER_READ_THROTTLE 0
} else {
    error "Unknown platform: ${platform}"
}

# Add signals to waveform viewer
proc addwaves { } {
    if { [batch_mode] == 0 } {
        add wave -divider "FIFO WRITER"
        add wave -hexadecimal  sim:/sample_stream_tb/U_fifo_writer/*
        add wave -divider "FIFO READER"
        add wave -hexadecimal  sim:/sample_stream_tb/U_fifo_reader/*
    } else {
        # Command-line mode, log everything to a file
        log -r *
    }
}

# Prettify the wave pane
proc prettify { } {
    if { [batch_mode] == 0 } {
        wave zoom full
        configure wave -namecolwidth     400 -valuecolwidth   150
        configure wave -waveselectenable 1   -waveselectcolor grey20
    }
}

# Post-simulation cleanup tasks
proc cleanup { } {
    global start_dir
    if { [batch_mode] == 1 } {
        quit -sim
        cd ${start_dir}
        quit
    }
}

# Load common functions
do ../nuand.do

# Compile HDL
compile_nuand ../ ${platform}

# Elaborate design
vsim -GNUM_MIMO_STREAMS=${NUM_MIMO_STREAMS} \
    -GFIFO_READER_READ_THROTTLE=${FIFO_READER_READ_THROTTLE} \
    nuand.sample_stream_tb

addwaves
run 4 ms; # May run longer if needed
prettify
cleanup
