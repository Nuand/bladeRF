load_package flow
package require cmdline

set options {\
    { "size.arg" "40" "FPGA Size - 40 or 115" } \
    { "rev.arg" "" "Revision name" } \
    { "stp.arg" "" "SignalTap II File to use" } \
    { "force" "" "Force using SignalTap II" }
}

# Read the revision from the commandline
array set opts [::cmdline::getoptions quartus(args) $options]

proc print_revisions { } {
    project_open -force bladerf -revision base
    puts stderr "Revisions"
    puts stderr "---------"
    foreach $rev [get_project_revisions] {
        if { $rev == "base" } {
            continue
        }
        puts stderr "    $rev"
    }
    project_close
}

# Check to make sure the project exists
if { ![project_exists bladerf] } {
    puts stderr "ERROR: bladeRF project does not exist.  Please run:"
    puts stderr "  $ quartus_sh -t ../bladerf.tcl"
    exit 1
}

# Check to make sure the revison was used on the commandline.
# If it wasn't, print out the revisions available.
if { $opts(rev) == "" } {
    puts stderr "ERROR: Revision required to build\n"
    print_revisions
    exit 1
}

# Check to make sure the revision exists
if { ![revision_exists -project bladerf $opts(rev)] } {
    puts stderr "ERROR: No revision named $opts(rev) in bladeRF project\n"
    print_revisions
    exit 1
}

# Open the project with the specific revision
project_open -revision $opts(rev) bladerf

# Check the size of the FPGA
if { $opts(size) != 115 && $opts(size) != 40 } {
    puts stderr "ERROR: Size must be either 40 or 115, not $opts(size)"
    exit 1
}

# Add signaltap file
set forced_talkback 0
if { $opts(stp) != "" } {
    if { ([get_user_option -name TALKBACK_ENABLED] == off || [get_user_option -name TALKBACK_ENABLED] == "") && $opts(force) } {
        puts "Enabling TalkBack to include SignalTap file"
        set_user_option -name TALKBACK_ENABLED on
        set forced_talkback 1
    }
    if { [get_user_option -name TALKBACK_ENABLED] == on } {
        puts "Adding SignalTap file: [file normalize $opts(stp)]"
        set_global_assignment -name ENABLE_SIGNALTAP on
        set_global_assignment -name USE_SIGNALTAP_FILE [file normalize $opts(stp)]
        set_global_assignment -name SIGNALTAP_FILE [file normalize $opts(stp)]
    } else {
        puts stderr "ERROR: Cannot add $opts(stp) to project without enabling TalkBack."
        puts stderr "         Use -force to enable and add SignalTap to project."
        exit 1
    }
} else {
    set_global_assignment -name ENABLE_SIGNALTAP off
}

set_global_assignment -name DEVICE EP4CE$opts(size)F23C8
set failed 0

# Save all the options
export_assignments
project_close

# Open the project with the specific revision
project_open -revision $opts(rev) bladerf

# Run Analysis and Synthesis
if { [catch {execute_module -tool map} result] } {
    puts "Result: $result"
    puts stderr "ERROR: Analysis & Synthesis Failed"
    set failed 1
}

# Run Fitter
if { $failed == 0 && [catch {execute_module -tool fit} result ] } {
    puts "Result: $result"
    puts stderr "ERROR: Fitter failed"
    set failed 1
}

# Run Static Timing Analysis
if { $failed == 0 && [catch {execute_module -tool sta} result] } {
    puts "Result: $result"
    puts stderr "ERROR: Timing Analysis Failed!"
    set failed 1
}

# Run Assembler
if { $failed == 0 && [catch {execute_module -tool asm} result] } {
    puts "Result: $result"
    puts stderr "ERROR: Assembler Failed!"
    set failed 1
}

# Run EDA Output
#if { $failed == 0 && [catch {execute_module -tool eda} result] } {
#    puts "Result: $result"
#    puts stderr "ERROR: EDA failed!"
#    set failed 1
#} elseif { $failed == 0 } {
#    puts "INFO: EDA OK!"
#}

# If we were forced to turn on TALKBACK .. turn it back off
if { $forced_talkback == 1 } {
    puts "Disabling TalkBack back to original state"
    set_user_option -name TALKBACK_ENABLED off
}

project_close

