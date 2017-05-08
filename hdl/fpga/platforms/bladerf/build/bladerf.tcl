load_package flow
package require cmdline

# Define the command argument structure as specified here:
# https://www.altera.com/support/support-resources/design-examples/design-software/tcl/open_project.html
set options { \
    { "projname.arg" "bladerf"      "Project name" } \
    { "part.arg"     ""             "FPGA part number" } \
    { "platdir.arg"  "0"            "Platform directory" } \
    { "outdir.arg"   "output_files" "Directory to place generated outputs (.sof, .rbf, .rpt, etc.)" } \
}

# Parse the command arguments, store them into 'opts'
array set opts [::cmdline::getoptions quartus(args) $options]

set PROJECT_NAME $opts(projname)
set TOP_LEVEL    "bladerf"
set BASE_REV     "base"

# Make a revision of the bladeRF FPGA based on a qip file in the platforms directory
proc make_revision { rev } {
    global PROJECT_NAME
    global BASE_REV
    global opts

    if [revision_exists ${rev}] {
        project_open -revision ${rev} ${PROJECT_NAME}
    } else {
        create_revision -based_on ${BASE_REV} -set_current ${rev}
    }
    set_global_assignment -name QIP_FILE [file normalize $opts(platdir)/${PROJECT_NAME}-$rev.qip]
    export_assignments
}

# Create the base project revision
if [project_exists ${PROJECT_NAME}] {
    project_open -revision ${BASE_REV} ${PROJECT_NAME}
} else {
    project_new -part $opts(part) -revision ${BASE_REV} ${PROJECT_NAME}
}
set_global_assignment -name DEVICE                   $opts(part)
set_global_assignment -name VHDL_INPUT_VERSION       VHDL_2008
set_global_assignment -name TOP_LEVEL_ENTITY         ${TOP_LEVEL}
set_global_assignment -name PROJECT_OUTPUT_DIRECTORY $opts(outdir)
set_global_assignment -name NUM_PARALLEL_PROCESSORS  ALL

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

# Add IP files and pin assignments
set_global_assignment -name QIP_FILE [file normalize $opts(platdir)/${PROJECT_NAME}.qip]
source [file normalize $opts(platdir)/constraints/pins.tcl]
file copy -force $opts(platdir)/build/ip.ipx ./ip.ipx
export_assignments

# At this point, we can add custom revisions

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

# Create ADSB receiver
make_revision adsb


# Projects created!
puts "${PROJECT_NAME} projects created!"
puts "Please use the build.tcl script to build images.\n"
puts "Revisions:"
foreach rev [get_project_revisions] {
    puts "    $rev"
}

project_close
