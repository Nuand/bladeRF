set vcom_lib nuand
set vcom_opts {-2008}

vlib ${vcom_lib}

# Support
vcom {*}$vcom_opts -work ${vcom_lib} ../../synthesis/synchronizer.vhd
vcom {*}$vcom_opts -work ${vcom_lib} ../../synthesis/handshake.vhd

# UUT
vcom {*}$vcom_opts -work ${vcom_lib} ../vhdl/pps_counter.vhd
vcom {*}$vcom_opts -work ${vcom_lib} ../vhdl/ppscal.vhd

# Testbench
vcom {*}$vcom_opts -work ${vcom_lib} ../vhdl/tb/asvtx12a384m_model.vhd
vcom {*}$vcom_opts -work ${vcom_lib} ../vhdl/tb/dac161s055_model.vhd
vcom {*}$vcom_opts -work ${vcom_lib} ../vhdl/tb/mm_driver.vhd
vcom {*}$vcom_opts -work ${vcom_lib} ../vhdl/tb/ppscal_tb.vhd

vcom {*}$vcom_opts -work ${vcom_lib} ../vhdl/tb/dacxo_tb.vhd

vsim ${vcom_lib}.ppscal_tb
#vsim ${vcom_lib}.dacxo_tb

#log -r *

# Default radix
radix -hexadecimal

# Change radixes for a select few signals
radix signal sim:/ppscal_tb/uut/pps_1s.count decimal
radix signal sim:/ppscal_tb/uut/pps_1s.target decimal
radix signal sim:/ppscal_tb/uut/pps_1s.count_mm_hold decimal
radix signal sim:/ppscal_tb/uut/pps_10s.count decimal
radix signal sim:/ppscal_tb/uut/pps_10s.target decimal
radix signal sim:/ppscal_tb/uut/pps_10s.count_mm_hold decimal
radix signal sim:/ppscal_tb/uut/pps_100s.count decimal
radix signal sim:/ppscal_tb/uut/pps_100s.target decimal
radix signal sim:/ppscal_tb/uut/pps_100s.count_mm_hold decimal

radix signal sim:/ppscal_tb/driver/current.pps_1s_count decimal
radix signal sim:/ppscal_tb/driver/current.pps_10s_count decimal
radix signal sim:/ppscal_tb/driver/current.pps_100s_count decimal

radix signal sim:/ppscal_tb/driver/current.holdoff_count decimal

# Add signals to waveform
#add wave sim:/ppscal_tb/uut/*
add wave sim:/ppscal_tb/uut/pps_1s*
add wave sim:/ppscal_tb/uut/pps_10s*
add wave sim:/ppscal_tb/uut/pps_100s*
#add wave sim:/ppscal_tb/uut/int_req_proc/*
add wave sim:/ppscal_tb/driver/*
add wave sim:/ppscal_tb/dac/*
add wave sim:/ppscal_tb/vctcxo/*
add wave sim:/ppscal_tb/*

run 10 ms
#run 500 ms

wave zoomfull
