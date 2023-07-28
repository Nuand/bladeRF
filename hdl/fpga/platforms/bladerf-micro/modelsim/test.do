################################################################################
# This file is part of the bladeRF project:
#   http://www.github.com/nuand/bladeRF

# Copyright (C) 2023 Nuand LLC

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
################################################################################

################################################################################
# Runs testbench simulations or executes "do" files.
# Exit Status: The script exits with the status code indicating success (0) or failure (1).
################################################################################
set status 0

if {$argc == 0 || $1 == "-h" || $1 == "--help"} {
    # If "-h" is passed, print the usage information and exit
    puts "Usage: do test.do vsim <testbench_name>  \[testbench parameters\] <time_in_microseconds>"
    puts "       do test.do do <file_name> \[additional_parameters\]"
    puts ""
    puts "Arguments:"
    puts "  vsim:   Run a testbench simulation."
    puts "  do:     Execute a 'do' file with parameters."
    puts "  -h:     Print this help message."
    return
}

set params ""
for {set i 2} {$i <= 9} {incr i} {
    if {[info exists $i]} {
        append params [eval set $i]
        append params " "
    }
}

if { $1 == "vsim" } {
    set testbench [lindex $params 0]
    set tb_args [lrange $params 1 end-1]
    set time_us [lindex $params end]
    set assertion_log log.txt

    vsim $testbench -assertfile $assertion_log {*}$tb_args; run ${time_us}us;

    set f [open $assertion_log]
    set file_content [read $f]
    close $f

    if {[catch {exec grep -c Failure << $file_content} result]} {
        set fail_count 0
    } else {
        set fail_count $result
    }
    echo "fail_count:" $fail_count
    if { $fail_count > 0 } {
        set status 1
        echo "\nSee log.txt for failure\n"
    } else {
        exec rm $assertion_log
    }
} else {
    onerror {set status 1}
    set do_file $1
    do $do_file {*}$params
}

quit -code $status
