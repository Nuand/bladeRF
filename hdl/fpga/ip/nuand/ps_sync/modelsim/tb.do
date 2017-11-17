set vcom_lib nuand
set vcom_opts {-2008}

vlib ${vcom_lib}

# Support


# UUT
vcom {*}$vcom_opts -work ${vcom_lib} ../vhdl/ps_sync.vhd

# Testbench
vcom {*}$vcom_opts -work ${vcom_lib} ../vhdl/tb/ps_sync_tb.vhd

vsim ${vcom_lib}.ps_sync_tb

log -r *

# Add signals to waveform
add wave sim:/ps_sync_tb/u_uut0/*
add wave sim:/ps_sync_tb/u_uut0/sync_proc/*

add wave sim:/ps_sync_tb/u_uut1/*
add wave sim:/ps_sync_tb/u_uut1/sync_proc/*

add wave sim:/ps_sync_tb/u_uut2/*
add wave sim:/ps_sync_tb/u_uut2/sync_proc/*

# Default radix
radix -hexadecimal

# Change radixes for a select few signals
radix signal sim:/ps_sync_tb/u_uut0/sync_proc/count      decimal
radix signal sim:/ps_sync_tb/u_uut0/sync_proc/count_rate decimal
radix signal sim:/ps_sync_tb/u_uut0/sync_proc/sel        decimal

radix signal sim:/ps_sync_tb/u_uut1/sync_proc/count      decimal
radix signal sim:/ps_sync_tb/u_uut1/sync_proc/count_rate decimal
radix signal sim:/ps_sync_tb/u_uut1/sync_proc/sel        decimal

radix signal sim:/ps_sync_tb/u_uut2/sync_proc/count      decimal
radix signal sim:/ps_sync_tb/u_uut2/sync_proc/count_rate decimal
radix signal sim:/ps_sync_tb/u_uut2/sync_proc/sel        decimal

run 1 ms

wave zoomfull
