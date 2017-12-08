set vcom_lib nuand
set vcom_opts {-2008}

vlib ${vcom_lib}

# Support
vcom {*}$vcom_opts -work ${vcom_lib} ../../synthesis/synchronizer.vhd
vcom {*}$vcom_opts -work ${vcom_lib} ../../synthesis/handshake.vhd
vcom {*}$vcom_opts -work ${vcom_lib} ../../synthesis/edge_detector.vhd
vcom {*}$vcom_opts -work ${vcom_lib} ../../synthesis/reset_synchronizer.vhd

# UUT
vcom {*}$vcom_opts -work ${vcom_lib} ../vhdl/pps_counter.vhd
vcom {*}$vcom_opts -work ${vcom_lib} ../vhdl/vctcxo_tamer.vhd

# Testbench
vcom {*}$vcom_opts -work ${vcom_lib} ../vhdl/tb/asvtx12a384m_model.vhd
vcom {*}$vcom_opts -work ${vcom_lib} ../vhdl/tb/dac161s055_model.vhd
vcom {*}$vcom_opts -work ${vcom_lib} ../vhdl/tb/mm_driver.vhd
vcom {*}$vcom_opts -work ${vcom_lib} ../vhdl/tb/vctcxo_tamer_tb.vhd

vsim ${vcom_lib}.vctcxo_tamer_tb
#vsim ${vcom_lib}.dacxo_tb

#log -r *

# Default radix
radix -hexadecimal

# Change radixes for a select few signals
radix signal sim:/vctcxo_tamer_tb/uut/pps_1s.count decimal
radix signal sim:/vctcxo_tamer_tb/uut/pps_1s.target decimal
radix signal sim:/vctcxo_tamer_tb/uut/pps_1s.error decimal
radix signal sim:/vctcxo_tamer_tb/uut/pps_1s.error_mm_hold decimal
radix signal sim:/vctcxo_tamer_tb/uut/pps_10s.count decimal
radix signal sim:/vctcxo_tamer_tb/uut/pps_10s.target decimal
radix signal sim:/vctcxo_tamer_tb/uut/pps_10s.error decimal
radix signal sim:/vctcxo_tamer_tb/uut/pps_10s.error_mm_hold decimal
radix signal sim:/vctcxo_tamer_tb/uut/pps_100s.count decimal
radix signal sim:/vctcxo_tamer_tb/uut/pps_100s.target decimal
radix signal sim:/vctcxo_tamer_tb/uut/pps_100s.error decimal
radix signal sim:/vctcxo_tamer_tb/uut/pps_100s.error_mm_hold decimal

radix signal sim:/vctcxo_tamer_tb/driver/current.pps_1s_count decimal
radix signal sim:/vctcxo_tamer_tb/driver/current.pps_10s_count decimal
radix signal sim:/vctcxo_tamer_tb/driver/current.pps_100s_count decimal

radix signal sim:/vctcxo_tamer_tb/driver/current.holdoff_count decimal

# Add signals to waveform
#add wave sim:/vctcxo_tamer_tb/uut/*
add wave sim:/vctcxo_tamer_tb/uut/pps_1s*
add wave sim:/vctcxo_tamer_tb/uut/pps_10s*
add wave sim:/vctcxo_tamer_tb/uut/pps_100s*
#add wave sim:/vctcxo_tamer_tb/uut/int_req_proc/*
add wave sim:/vctcxo_tamer_tb/driver/*
add wave sim:/vctcxo_tamer_tb/dac/*
add wave sim:/vctcxo_tamer_tb/vctcxo/*
add wave sim:/vctcxo_tamer_tb/*

run 10 ms
#run 500 ms

wave zoomfull
