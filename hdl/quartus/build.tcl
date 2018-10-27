load_package flow
package require cmdline

# Define the command argument structure as specified here:
# https://www.altera.com/support/support-resources/design-examples/design-software/tcl/open_project.html
set options { \
    { "projname.arg" ""        "Name of Quartus project" } \
    { "rev.arg"      ""        "Name of revision within the project" } \
    { "flow.arg"     "gen"     "Quartus flow: gen, synth, full" } \
    { "stp.arg"      ""        "SignalTap II file to use" } \
    { "force.arg"    ""        "Force enable TalkBack to use SignalTapII" } \
    { "seed.arg"     ""        "Specify fitter seed" } \
}

# Parse the command arguments, store them into 'opts'
array set opts [::cmdline::getoptions quartus(args) $options]

set BASE_REVISION "base"

proc print_revisions { } {
    global opts
    global BASE_REVISION

    project_open -force $opts(projname) -revision ${BASE_REVISION}
    puts stderr "Revisions"
    puts stderr "---------"
    foreach $rev [get_project_revisions] {
        if { $rev == "${BASE_REVISION}" } {
            continue
        }
        puts stderr "    ${rev}"
    }
    project_close
}

# Check to make sure the project exists
if { ![project_exists $opts(projname)] } {
    puts stderr "ERROR: $opts(projname) project does not exist.  Please run:"
    puts stderr "  $ quartus_sh -t /path/to/bladerf.tcl -opt1 val1 -opt2 val2 ..."
    exit 1
}

# Check to make sure the revison was specified on the commandline.
# If it wasn't, print out the revisions available.
if { $opts(rev) == "" } {
    puts stderr "ERROR: Revision required to build\n"
    print_revisions
    exit 1
}

# Check to make sure the revision exists
if { ![revision_exists -project $opts(projname) $opts(rev)] } {
    puts stderr "ERROR: No revision named $opts(rev) in $opts(projname) project\n"
    print_revisions
    exit 1
}

# Check the flow
if { $opts(flow) != "gen" &&
     $opts(flow) != "synth" &&
     $opts(flow) != "full" } {
    puts stderr "ERROR: Invalid Quartus flow. Valid option are:"
    puts stderr "  gen   - generate project files only"
    puts stderr "  synth - generate project files and run synthesis only"
    puts stderr "  full  - generate project, synth, fit, timing, assemble"
    exit 1
}

# Open the project with the specific revision
project_open -revision $opts(rev) $opts(projname)

# Add signaltap file
set forced_talkback 0
set enable_stp 0
if { $opts(stp) != "" } {
    # Check if we need to force
    if { ([get_user_option -name TALKBACK_ENABLED] == off || [get_user_option -name TALKBACK_ENABLED] == "") && ($opts(force) == "1") } {
        puts "Enabling TalkBack to include SignalTap file"
        set_user_option -name TALKBACK_ENABLED on
        set forced_talkback 1
    }
    # SignalTap requires either TalkBack to be enabled or a non-Web Edition of Quartus (e.g. Standard, Pro)
    if { ([get_user_option -name TALKBACK_ENABLED] == on) || (![string match "*Web Edition*" $quartus(version)]) } {
        puts "Adding SignalTap file: [file normalize $opts(stp)]"
        set_global_assignment -name ENABLE_SIGNALTAP on
        set_global_assignment -name USE_SIGNALTAP_FILE [file normalize "$opts(stp)"]
        set_global_assignment -name SIGNALTAP_FILE     [file normalize "$opts(stp)"]
        set enable_stp 1
    } else {
        puts stderr "\nERROR: Cannot add $opts(stp) to project without enabling TalkBack."
        puts stderr "         Use -force to enable and add SignalTap to project."
        project_close
        exit 1
    }
} else {
    set_global_assignment -name ENABLE_SIGNALTAP off
}

# Configure seed
if { $opts(seed) != "" } {
    set_global_assignment -name SEED "$opts(seed)"
}

# Save all the options
export_assignments
project_close

set failed 0

# The quartus_stp executable creates/edits a Quartus Setting File (.qsf)
# based on the SignalTap II File specified if enabled. It must be run
# successfully before running Analysis & Synthesis.
if { $enable_stp == 1 } {
    if { $failed == 0 && [catch {qexec "quartus_stp --stp_file [file normalize $opts(stp)] --enable $opts(projname) --rev $opts(rev)"} result] } {
        puts "Result: $result"
        puts stderr "ERROR: Adding SignalTap settings to QSF failed."
        set failed 1
    }
}

# Open the project with the specific revision
project_open -revision $opts(rev) $opts(projname)

if { ($opts(flow) == "synth") || ($opts(flow) == "full") } {
    # Run Analysis and Synthesis
    if { $failed == 0 && [catch {execute_module -tool map} result] } {
        puts "Result: $result"
        puts stderr "ERROR: Analysis & Synthesis Failed"
        set failed 1
    }
}

if { $opts(flow) == "full" } {
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
}

# If we were forced to turn on TALKBACK .. turn it back off
if { $forced_talkback == 1 } {
    puts "Disabling TalkBack back to original state"
    set_user_option -name TALKBACK_ENABLED off
}

project_close
