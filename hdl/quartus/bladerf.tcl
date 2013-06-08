load_package flow

project_open bladerf -force

set failed 0

if { [catch {execute_module -tool map} result] } {
    puts "Result: $result"
    puts stderr "ERROR: Analysis & Synthesis Failed"
    set failed 1
} else {
    puts "INFO: Analysis & Synthesis OK!"
}

if { $failed == 0 && [catch {execute_module -tool fit} result ] } {
    puts "Result: $result"
    puts stderr "ERROR: Fitter failed"
    set failed 1
} else {
    puts "INFO: Fitter OK!"
}

if { $failed == 0 && [catch {execute_module -tool sta} result] } {
    puts "Result: $result"
    puts stderr "ERROR: Timing Analysis Failed!"
    set failed 1
} else {
    puts "INFO: Timing Analysis OK!"
}

if { $failed == 0 && [catch {execute_module -tool asm} result] } {
    puts "Result: $result"
    puts stderr "ERROR: Assembler Failed!"
    set failed 1
} else {
    puts "INFO: Assembler OK!"
}

if { $failed == 0 && [catch {execute_module -tool eda} result] } {
    puts "Result: $result"
    puts stderr "ERROR: EDA failed!"
    set failed 1
} else {
    puts "INFO: EDA OK!"
}

project_close
