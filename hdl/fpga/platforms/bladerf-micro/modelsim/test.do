set status 0

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
