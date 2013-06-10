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

# Projects created!
puts "bladeRF projects created.  Please use the build.tcl script to build images.\n"
puts "Revisions:"
foreach rev [get_project_revisions] {
    puts "    $rev"
}

project_close

