load_package flow

# Make a revision of the bladeRF FPGA based on a qip file in the platforms directory
proc make_revision { rev } {
    if [revision_exists $rev] {
        project_open -revision $rev bladerf
    } else {
        create_revision -based_on base -set_current $rev
    }
    set_global_assignment -name QIP_FILE [file normalize ../../fpga/platforms/bladerf/bladerf-$rev.qip]
    export_assignments
}

# Create the base project
if [project_exists bladerf] {
    project_open -revision base bladerf
} else {
    project_new -family "Cyclone IV E" -revision base bladerf
}
set_global_assignment -name DEVICE EP4CE40F23C8
set_global_assignment -name VHDL_INPUT_VERSION VHDL_2008
set_global_assignment -name TOP_LEVEL_ENTITY bladerf
set_global_assignment -name PROJECT_OUTPUT_DIRECTORY output_files

# Configuration settings
set_global_assignment -name CYCLONEIII_CONFIGURATION_SCHEME "FAST PASSIVE PARALLEL"
set_global_assignment -name USE_CONFIGURATION_DEVICE OFF
set_global_assignment -name GENERATE_TTF_FILE ON
set_global_assignment -name GENERATE_RBF_FILE ON
set_global_assignment -name CRC_ERROR_OPEN_DRAIN OFF
set_global_assignment -name ON_CHIP_BITSTREAM_DECOMPRESSION OFF
set_global_assignment -name RESERVE_DATA0_AFTER_CONFIGURATION "USE AS REGULAR IO"
set_global_assignment -name RESERVE_DATA1_AFTER_CONFIGURATION "USE AS REGULAR IO"
set_global_assignment -name RESERVE_DATA7_THROUGH_DATA2_AFTER_CONFIGURATION "USE AS REGULAR IO"
set_global_assignment -name RESERVE_FLASH_NCE_AFTER_CONFIGURATION "USE AS REGULAR IO"
set_global_assignment -name RESERVE_OTHER_AP_PINS_AFTER_CONFIGURATION "USE AS REGULAR IO"
set_global_assignment -name CYCLONEII_RESERVE_NCEO_AFTER_CONFIGURATION "USE AS REGULAR IO"
set_global_assignment -name RESERVE_DCLK_AFTER_CONFIGURATION "USE AS REGULAR IO"
set_global_assignment -name OUTPUT_IO_TIMING_NEAR_END_VMEAS "HALF VCCIO" -rise
set_global_assignment -name OUTPUT_IO_TIMING_NEAR_END_VMEAS "HALF VCCIO" -fall
set_global_assignment -name OUTPUT_IO_TIMING_FAR_END_VMEAS "HALF SIGNAL SWING" -rise
set_global_assignment -name OUTPUT_IO_TIMING_FAR_END_VMEAS "HALF SIGNAL SWING" -fall
set_global_assignment -name CYCLONEII_OPTIMIZATION_TECHNIQUE SPEED
set_global_assignment -name PHYSICAL_SYNTHESIS_COMBO_LOGIC ON
set_global_assignment -name PHYSICAL_SYNTHESIS_REGISTER_DUPLICATION ON
set_global_assignment -name PHYSICAL_SYNTHESIS_REGISTER_RETIMING ON
set_global_assignment -name ROUTER_LCELL_INSERTION_AND_LOGIC_DUPLICATION ON
set_global_assignment -name ROUTER_TIMING_OPTIMIZATION_LEVEL MAXIMUM
set_global_assignment -name AUTO_PACKED_REGISTERS_STRATIXII NORMAL
set_global_assignment -name FITTER_EFFORT "STANDARD FIT"
set_global_assignment -name ADV_NETLIST_OPT_SYNTH_WYSIWYG_REMAP ON
set_global_assignment -name ROUTER_CLOCKING_TOPOLOGY_ANALYSIS ON
set_global_assignment -name ENABLE_DRC_SETTINGS ON
set_instance_assignment -name FAST_INPUT_REGISTER ON -to lms_rx_data[*]
set_instance_assignment -name FAST_OUTPUT_REGISTER ON -to fx3_gpif[*]
set_instance_assignment -name FAST_OUTPUT_REGISTER ON -to lms_tx_data[*]
set_instance_assignment -name FAST_OUTPUT_REGISTER ON -to fx3_ctl[*]
set_instance_assignment -name FAST_OUTPUT_ENABLE_REGISTER ON -to fx3_gpif[*]
set_global_assignment -name OPTIMIZE_POWER_DURING_FITTING "EXTRA EFFORT"
set_global_assignment -name FITTER_AUTO_EFFORT_DESIRED_SLACK_MARGIN "1 ns"
#set_global_assignment -name RESERVE_ALL_UNUSED_PINS_WEAK_PULLUP "AS OUTPUT DRIVING GROUND"

# Create an base revision
set_global_assignment -name QIP_FILE [file normalize ../../fpga/platforms/bladerf/bladerf.qip]
source [file normalize ../../fpga/platforms/bladerf/constraints/pins.tcl]
file copy -force ../ip.ipx ./ip.ipx
export_assignments

# Create the hosted
make_revision hosted

# Create headless
make_revision headless

# Create fsk_bridge
make_revision fsk_bridge

# Create QPSK transmitter
make_revision qpsk_tx

# Create ATSC transmitter
make_revision atsc_tx

# Projects created!
puts "bladeRF projects created.  Please use the build.tcl script to build images.\n"
puts "Revisions:"
foreach rev [get_project_revisions] {
    puts "    $rev"
}

project_close

