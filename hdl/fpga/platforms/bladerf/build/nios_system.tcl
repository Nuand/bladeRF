# qsys scripting (.tcl) file for nios_system
package require -exact qsys 16.0

create_system {nios_system}

set_project_property DEVICE_FAMILY {Cyclone IV E}
set_project_property DEVICE {EP4CE40F23C8}
set_project_property HIDE_FROM_IP_CATALOG {false}

# Instances and instance parameters
# (disabled instances are intentionally culled)
add_instance clk_0 clock_source
set_instance_parameter_value clk_0 {clockFrequency} {80000000.0}
set_instance_parameter_value clk_0 {clockFrequencyKnown} {1}
set_instance_parameter_value clk_0 {resetSynchronousEdges} {DEASSERT}

add_instance common_system_0 common_system 1.0

add_instance lms_spi lms6_spi_controller 1.0
set_instance_parameter_value lms_spi {CLOCK_DIV} {4}
set_instance_parameter_value lms_spi {ADDR_WIDTH} {8}
set_instance_parameter_value lms_spi {DATA_WIDTH} {8}

add_instance vctcxo_tamer_0 vctcxo_tamer 1.0

# exported interfaces
add_interface command conduit end
set_interface_property command EXPORT_OF common_system_0.command
add_interface correction_rx_phase_gain conduit end
set_interface_property correction_rx_phase_gain EXPORT_OF common_system_0.correction_rx_phase_gain
add_interface correction_tx_phase_gain conduit end
set_interface_property correction_tx_phase_gain EXPORT_OF common_system_0.correction_tx_phase_gain
add_interface dac conduit end
set_interface_property dac EXPORT_OF common_system_0.dac
add_interface gpio conduit end
set_interface_property gpio EXPORT_OF common_system_0.gpio
add_interface oc_i2c conduit end
set_interface_property oc_i2c EXPORT_OF common_system_0.oc_i2c
add_interface rx_tamer conduit end
set_interface_property rx_tamer EXPORT_OF common_system_0.rx_tamer
add_interface rx_trigger_ctl conduit end
set_interface_property rx_trigger_ctl EXPORT_OF common_system_0.rx_trigger_ctl
add_interface spi conduit end
set_interface_property spi EXPORT_OF lms_spi.conduit_end
add_interface sys_clk clock sink
set_interface_property sys_clk EXPORT_OF clk_0.clk_in
add_interface sys_reset reset sink
set_interface_property sys_reset EXPORT_OF clk_0.clk_in_reset
add_interface tx_tamer conduit end
set_interface_property tx_tamer EXPORT_OF common_system_0.tx_tamer
add_interface tx_trigger_ctl conduit end
set_interface_property tx_trigger_ctl EXPORT_OF common_system_0.tx_trigger_ctl
add_interface vctcxo_tamer conduit end
set_interface_property vctcxo_tamer EXPORT_OF vctcxo_tamer_0.conduit_end
add_interface xb_gpio conduit end
set_interface_property xb_gpio EXPORT_OF common_system_0.xb_gpio
add_interface xb_gpio_dir conduit end
set_interface_property xb_gpio_dir EXPORT_OF common_system_0.xb_gpio_dir

# connections and connection parameters
add_connection common_system_0.pb_0_m0 vctcxo_tamer_0.avalon_slave_0
set_connection_parameter_value common_system_0.pb_0_m0/vctcxo_tamer_0.avalon_slave_0 arbitrationPriority {1}
set_connection_parameter_value common_system_0.pb_0_m0/vctcxo_tamer_0.avalon_slave_0 baseAddress {0x0100}
set_connection_parameter_value common_system_0.pb_0_m0/vctcxo_tamer_0.avalon_slave_0 defaultConnection {0}

add_connection common_system_0.pb_0_m0 lms_spi.avalon_slave_0
set_connection_parameter_value common_system_0.pb_0_m0/lms_spi.avalon_slave_0 arbitrationPriority {1}
set_connection_parameter_value common_system_0.pb_0_m0/lms_spi.avalon_slave_0 baseAddress {0x0000}
set_connection_parameter_value common_system_0.pb_0_m0/lms_spi.avalon_slave_0 defaultConnection {0}

add_connection clk_0.clk common_system_0.clk

add_connection clk_0.clk vctcxo_tamer_0.clock_sink

add_connection clk_0.clk lms_spi.clock_sink

add_connection common_system_0.ib0_receiver_irq vctcxo_tamer_0.interrupt_sender
set_connection_parameter_value common_system_0.ib0_receiver_irq/vctcxo_tamer_0.interrupt_sender irqNumber {0}

add_connection clk_0.clk_reset common_system_0.reset

add_connection clk_0.clk_reset vctcxo_tamer_0.reset_sink

add_connection clk_0.clk_reset lms_spi.reset_sink

# interconnect requirements
set_interconnect_requirement {$system} {qsys_mm.clockCrossingAdapter} {HANDSHAKE}
set_interconnect_requirement {$system} {qsys_mm.maxAdditionalLatency} {1}
set_interconnect_requirement {$system} {qsys_mm.enableEccProtection} {FALSE}
set_interconnect_requirement {$system} {qsys_mm.insertDefaultSlave} {FALSE}

save_system {nios_system.qsys}
