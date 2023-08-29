set status 0
onerror {set status 1}
set do_file $1
set params ""

for {set i 2} {$i <= 9} {incr i} {
    if {[info exists $i]} {
        append params [eval set $i]
        append params " "
    }
}

do $do_file {*}$params

quit -code $status
