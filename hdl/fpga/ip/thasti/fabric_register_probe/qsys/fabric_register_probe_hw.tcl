# 
# fabric_register_probe "BladeRF Fabric Register Probe" v1.0
# Stefan Biereigel 2017.05.18.14:28:15
# Can be used to probe FPGA registers from libbladeRF
# 

# 
# request TCL package from ACDS 15.0
# 
package require -exact qsys 15.0


# 
# module fabric_register_probe
# 
set_module_property DESCRIPTION "Can be used to probe FPGA registers from libbladeRF"
set_module_property NAME fabric_register_probe
set_module_property VERSION 1.0
set_module_property INTERNAL false
set_module_property OPAQUE_ADDRESS_MAP true
set_module_property GROUP Peripherals
set_module_property AUTHOR "Stefan Biereigel"
set_module_property DISPLAY_NAME "BladeRF Fabric Register Probe"
set_module_property INSTANTIATE_IN_SYSTEM_MODULE true
set_module_property EDITABLE true
set_module_property REPORT_TO_TALKBACK false
set_module_property ALLOW_GREYBOX_GENERATION false
set_module_property REPORT_HIERARCHY false


# 
# file sets
# 
add_fileset QUARTUS_SYNTH QUARTUS_SYNTH "" ""
set_fileset_property QUARTUS_SYNTH TOP_LEVEL fabric_register_probe
set_fileset_property QUARTUS_SYNTH ENABLE_RELATIVE_INCLUDE_PATHS false
set_fileset_property QUARTUS_SYNTH ENABLE_FILE_OVERWRITE_MODE false
add_fileset_file fabric_register_probe.vhd VHDL PATH ../vhdl/fabric_register_probe.vhd TOP_LEVEL_FILE


# 
# parameters
# 
add_parameter enable_outputs BOOLEAN false
set_parameter_property enable_outputs DEFAULT_VALUE false
set_parameter_property enable_outputs TYPE BOOLEAN
set_parameter_property enable_outputs HDL_PARAMETER false
set_parameter_property enable_outputs AFFECTS_ELABORATION true
set_parameter_property enable_outputs VISIBLE true

add_parameter enable_inputs BOOLEAN false
set_parameter_property enable_inputs DEFAULT_VALUE false
set_parameter_property enable_inputs TYPE BOOLEAN
set_parameter_property enable_inputs HDL_PARAMETER false
set_parameter_property enable_inputs AFFECTS_ELABORATION true
set_parameter_property enable_inputs VISIBLE true

# 
# display items
# 

# 
# connection point reset
# 
add_interface reset reset end
set_interface_property reset associatedClock clock
set_interface_property reset synchronousEdges DEASSERT
set_interface_property reset ENABLED true
set_interface_property reset EXPORT_OF ""
set_interface_property reset PORT_NAME_MAP ""
set_interface_property reset CMSIS_SVD_VARIABLES ""
set_interface_property reset SVD_ADDRESS_GROUP ""

add_interface_port reset reset reset Input 1

# 
# connection point clock
# 
add_interface clock clock end
set_interface_property clock clockRate 0
set_interface_property clock ENABLED true
set_interface_property clock EXPORT_OF ""
set_interface_property clock PORT_NAME_MAP ""
set_interface_property clock CMSIS_SVD_VARIABLES ""
set_interface_property clock SVD_ADDRESS_GROUP ""

add_interface_port clock clk clk Input 1

# 
# connection point mm
# 
add_interface mm avalon end
set_interface_property mm addressUnits SYMBOLS
set_interface_property mm associatedClock clock
set_interface_property mm associatedReset reset
set_interface_property mm bitsPerSymbol 8
set_interface_property mm burstOnBurstBoundariesOnly false
set_interface_property mm burstcountUnits WORDS
set_interface_property mm explicitAddressSpan 0
set_interface_property mm holdTime 0
set_interface_property mm linewrapBursts false
set_interface_property mm maximumPendingReadTransactions 1
set_interface_property mm maximumPendingWriteTransactions 0
set_interface_property mm readLatency 0
set_interface_property mm readWaitTime 1
set_interface_property mm setupTime 0
set_interface_property mm timingUnits Cycles
set_interface_property mm writeWaitTime 0
set_interface_property mm ENABLED true
set_interface_property mm EXPORT_OF ""
set_interface_property mm PORT_NAME_MAP ""
set_interface_property mm CMSIS_SVD_VARIABLES ""
set_interface_property mm SVD_ADDRESS_GROUP ""

add_interface_port mm mm_begintransfer begintransfer Input 1
add_interface_port mm mm_write write Input 1
add_interface_port mm mm_address address Input 5
add_interface_port mm mm_writedata writedata Input 32
add_interface_port mm mm_read read Input 1
add_interface_port mm mm_readdata readdata Output 32
add_interface_port mm mm_readdatavalid readdatavalid Output 1
set_interface_assignment mm embeddedsw.configuration.isFlash 0
set_interface_assignment mm embeddedsw.configuration.isMemoryDevice 0
set_interface_assignment mm embeddedsw.configuration.isNonVolatileStorage 0
set_interface_assignment mm embeddedsw.configuration.isPrintableDevice 0

# 
# connection point conduit_end
# 
add_interface out conduit end
set_interface_property out associatedClock clock
set_interface_property out associatedReset ""
set_interface_property out ENABLED false
set_interface_property out EXPORT_OF ""
set_interface_property out PORT_NAME_MAP ""
set_interface_property out CMSIS_SVD_VARIABLES ""
set_interface_property out SVD_ADDRESS_GROUP ""

add_interface_port out register0_out register0_out Output 32
add_interface_port out register1_out register1_out Output 32
add_interface_port out register2_out register2_out Output 32
add_interface_port out register3_out register3_out Output 32
add_interface_port out register4_out register4_out Output 32
add_interface_port out register5_out register5_out Output 32
add_interface_port out register6_out register6_out Output 32
add_interface_port out register7_out register7_out Output 32

add_interface in conduit end
set_interface_property in associatedClock clock
set_interface_property in associatedReset ""
set_interface_property in ENABLED false
set_interface_property in EXPORT_OF ""
set_interface_property in PORT_NAME_MAP ""
set_interface_property in CMSIS_SVD_VARIABLES ""
set_interface_property in SVD_ADDRESS_GROUP ""

add_interface_port in register0_in register0_in Input 32
add_interface_port in register1_in register1_in Input 32
add_interface_port in register2_in register2_in Input 32
add_interface_port in register3_in register3_in Input 32
add_interface_port in register4_in register4_in Input 32
add_interface_port in register5_in register5_in Input 32
add_interface_port in register6_in register6_in Input 32
add_interface_port in register7_in register7_in Input 32

set_module_property ELABORATION_CALLBACK cb_elaborate

proc cb_elaborate {} {
    if { [get_parameter_value enable_inputs] == false } {
        set_interface_property in ENABLED false
    } else {
        set_interface_property in ENABLED true
    }
    
    if { [get_parameter_value enable_outputs] == false } {
        set_interface_property out ENABLED false
    } else {
        set_interface_property out ENABLED true
    }
}


