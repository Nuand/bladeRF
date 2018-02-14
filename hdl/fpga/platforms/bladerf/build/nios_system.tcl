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

add_instance iq_corr_rx_phase_gain altera_avalon_pio
set_instance_parameter_value iq_corr_rx_phase_gain {bitClearingEdgeCapReg} {0}
set_instance_parameter_value iq_corr_rx_phase_gain {bitModifyingOutReg} {0}
set_instance_parameter_value iq_corr_rx_phase_gain {captureEdge} {0}
set_instance_parameter_value iq_corr_rx_phase_gain {direction} {Output}
set_instance_parameter_value iq_corr_rx_phase_gain {edgeType} {RISING}
set_instance_parameter_value iq_corr_rx_phase_gain {generateIRQ} {0}
set_instance_parameter_value iq_corr_rx_phase_gain {irqType} {LEVEL}
set_instance_parameter_value iq_corr_rx_phase_gain {resetValue} {0.0}
set_instance_parameter_value iq_corr_rx_phase_gain {simDoTestBenchWiring} {0}
set_instance_parameter_value iq_corr_rx_phase_gain {simDrivenValue} {0.0}
set_instance_parameter_value iq_corr_rx_phase_gain {width} {32}

add_instance iq_corr_tx_phase_gain altera_avalon_pio
set_instance_parameter_value iq_corr_tx_phase_gain {bitClearingEdgeCapReg} {0}
set_instance_parameter_value iq_corr_tx_phase_gain {bitModifyingOutReg} {0}
set_instance_parameter_value iq_corr_tx_phase_gain {captureEdge} {0}
set_instance_parameter_value iq_corr_tx_phase_gain {direction} {Output}
set_instance_parameter_value iq_corr_tx_phase_gain {edgeType} {RISING}
set_instance_parameter_value iq_corr_tx_phase_gain {generateIRQ} {0}
set_instance_parameter_value iq_corr_tx_phase_gain {irqType} {LEVEL}
set_instance_parameter_value iq_corr_tx_phase_gain {resetValue} {0.0}
set_instance_parameter_value iq_corr_tx_phase_gain {simDoTestBenchWiring} {0}
set_instance_parameter_value iq_corr_tx_phase_gain {simDrivenValue} {0.0}
set_instance_parameter_value iq_corr_tx_phase_gain {width} {32}

add_instance rffe_spi lms6_spi_controller 1.0
set_instance_parameter_value rffe_spi {CLOCK_DIV} {4}
set_instance_parameter_value rffe_spi {ADDR_WIDTH} {8}
set_instance_parameter_value rffe_spi {DATA_WIDTH} {8}

add_instance vctcxo_tamer_0 vctcxo_tamer 1.0

add_instance arbiter_0 arbiter 1.0
set_instance_parameter_value arbiter_0 {N} {2}

add_instance agc_dc_i_max altera_avalon_pio
set_instance_parameter_value agc_dc_i_max {bitClearingEdgeCapReg} {0}
set_instance_parameter_value agc_dc_i_max {bitModifyingOutReg} {0}
set_instance_parameter_value agc_dc_i_max {captureEdge} {0}
set_instance_parameter_value agc_dc_i_max {direction} {Output}
set_instance_parameter_value agc_dc_i_max {edgeType} {RISING}
set_instance_parameter_value agc_dc_i_max {generateIRQ} {0}
set_instance_parameter_value agc_dc_i_max {irqType} {LEVEL}
set_instance_parameter_value agc_dc_i_max {resetValue} {0.0}
set_instance_parameter_value agc_dc_i_max {simDoTestBenchWiring} {0}
set_instance_parameter_value agc_dc_i_max {simDrivenValue} {0.0}
set_instance_parameter_value agc_dc_i_max {width} {16}

add_instance agc_dc_i_mid altera_avalon_pio
set_instance_parameter_value agc_dc_i_mid {bitClearingEdgeCapReg} {0}
set_instance_parameter_value agc_dc_i_mid {bitModifyingOutReg} {0}
set_instance_parameter_value agc_dc_i_mid {captureEdge} {0}
set_instance_parameter_value agc_dc_i_mid {direction} {Output}
set_instance_parameter_value agc_dc_i_mid {edgeType} {RISING}
set_instance_parameter_value agc_dc_i_mid {generateIRQ} {0}
set_instance_parameter_value agc_dc_i_mid {irqType} {LEVEL}
set_instance_parameter_value agc_dc_i_mid {resetValue} {0.0}
set_instance_parameter_value agc_dc_i_mid {simDoTestBenchWiring} {0}
set_instance_parameter_value agc_dc_i_mid {simDrivenValue} {0.0}
set_instance_parameter_value agc_dc_i_mid {width} {16}

add_instance agc_dc_i_min altera_avalon_pio
set_instance_parameter_value agc_dc_i_min {bitClearingEdgeCapReg} {0}
set_instance_parameter_value agc_dc_i_min {bitModifyingOutReg} {0}
set_instance_parameter_value agc_dc_i_min {captureEdge} {0}
set_instance_parameter_value agc_dc_i_min {direction} {Output}
set_instance_parameter_value agc_dc_i_min {edgeType} {RISING}
set_instance_parameter_value agc_dc_i_min {generateIRQ} {0}
set_instance_parameter_value agc_dc_i_min {irqType} {LEVEL}
set_instance_parameter_value agc_dc_i_min {resetValue} {0.0}
set_instance_parameter_value agc_dc_i_min {simDoTestBenchWiring} {0}
set_instance_parameter_value agc_dc_i_min {simDrivenValue} {0.0}
set_instance_parameter_value agc_dc_i_min {width} {16}

add_instance agc_dc_q_max altera_avalon_pio
set_instance_parameter_value agc_dc_q_max {bitClearingEdgeCapReg} {0}
set_instance_parameter_value agc_dc_q_max {bitModifyingOutReg} {0}
set_instance_parameter_value agc_dc_q_max {captureEdge} {0}
set_instance_parameter_value agc_dc_q_max {direction} {Output}
set_instance_parameter_value agc_dc_q_max {edgeType} {RISING}
set_instance_parameter_value agc_dc_q_max {generateIRQ} {0}
set_instance_parameter_value agc_dc_q_max {irqType} {LEVEL}
set_instance_parameter_value agc_dc_q_max {resetValue} {0.0}
set_instance_parameter_value agc_dc_q_max {simDoTestBenchWiring} {0}
set_instance_parameter_value agc_dc_q_max {simDrivenValue} {0.0}
set_instance_parameter_value agc_dc_q_max {width} {16}

add_instance agc_dc_q_mid altera_avalon_pio
set_instance_parameter_value agc_dc_q_mid {bitClearingEdgeCapReg} {0}
set_instance_parameter_value agc_dc_q_mid {bitModifyingOutReg} {0}
set_instance_parameter_value agc_dc_q_mid {captureEdge} {0}
set_instance_parameter_value agc_dc_q_mid {direction} {Output}
set_instance_parameter_value agc_dc_q_mid {edgeType} {RISING}
set_instance_parameter_value agc_dc_q_mid {generateIRQ} {0}
set_instance_parameter_value agc_dc_q_mid {irqType} {LEVEL}
set_instance_parameter_value agc_dc_q_mid {resetValue} {0.0}
set_instance_parameter_value agc_dc_q_mid {simDoTestBenchWiring} {0}
set_instance_parameter_value agc_dc_q_mid {simDrivenValue} {0.0}
set_instance_parameter_value agc_dc_q_mid {width} {16}

add_instance agc_dc_q_min altera_avalon_pio
set_instance_parameter_value agc_dc_q_min {bitClearingEdgeCapReg} {0}
set_instance_parameter_value agc_dc_q_min {bitModifyingOutReg} {0}
set_instance_parameter_value agc_dc_q_min {captureEdge} {0}
set_instance_parameter_value agc_dc_q_min {direction} {Output}
set_instance_parameter_value agc_dc_q_min {edgeType} {RISING}
set_instance_parameter_value agc_dc_q_min {generateIRQ} {0}
set_instance_parameter_value agc_dc_q_min {irqType} {LEVEL}
set_instance_parameter_value agc_dc_q_min {resetValue} {0.0}
set_instance_parameter_value agc_dc_q_min {simDoTestBenchWiring} {0}
set_instance_parameter_value agc_dc_q_min {simDrivenValue} {0.0}
set_instance_parameter_value agc_dc_q_min {width} {16}

# exported interfaces
add_interface clk clock sink
set_interface_property clk EXPORT_OF clk_0.clk_in
add_interface command conduit end
set_interface_property command EXPORT_OF common_system_0.command
add_interface correction_rx_phase_gain conduit end
set_interface_property correction_rx_phase_gain EXPORT_OF iq_corr_rx_phase_gain.external_connection
add_interface correction_tx_phase_gain conduit end
set_interface_property correction_tx_phase_gain EXPORT_OF iq_corr_tx_phase_gain.external_connection
add_interface dac conduit end
set_interface_property dac EXPORT_OF common_system_0.dac
add_interface gpio conduit end
set_interface_property gpio EXPORT_OF common_system_0.gpio
add_interface oc_i2c conduit end
set_interface_property oc_i2c EXPORT_OF common_system_0.oc_i2c
add_interface reset reset sink
set_interface_property reset EXPORT_OF clk_0.clk_in_reset
add_interface rx_tamer conduit end
set_interface_property rx_tamer EXPORT_OF common_system_0.rx_tamer
add_interface rx_trigger_ctl conduit end
set_interface_property rx_trigger_ctl EXPORT_OF common_system_0.rx_trigger_ctl
add_interface spi conduit end
set_interface_property spi EXPORT_OF rffe_spi.conduit_end
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
add_interface arbiter conduit end
set_interface_property arbiter EXPORT_OF arbiter_0.conduit_end
add_interface agc_dc_i_max conduit end
set_interface_property agc_dc_i_max EXPORT_OF agc_dc_i_max.external_connection
add_interface agc_dc_i_mid conduit end
set_interface_property agc_dc_i_mid EXPORT_OF agc_dc_i_mid.external_connection
add_interface agc_dc_i_min conduit end
set_interface_property agc_dc_i_min EXPORT_OF agc_dc_i_min.external_connection
add_interface agc_dc_q_max conduit end
set_interface_property agc_dc_q_max EXPORT_OF agc_dc_q_max.external_connection
add_interface agc_dc_q_mid conduit end
set_interface_property agc_dc_q_mid EXPORT_OF agc_dc_q_mid.external_connection
add_interface agc_dc_q_min conduit end
set_interface_property agc_dc_q_min EXPORT_OF agc_dc_q_min.external_connection

# connections and connection parameters
add_connection common_system_0.pb_0_m0 vctcxo_tamer_0.avalon_slave_0
set_connection_parameter_value common_system_0.pb_0_m0/vctcxo_tamer_0.avalon_slave_0 arbitrationPriority {1}
set_connection_parameter_value common_system_0.pb_0_m0/vctcxo_tamer_0.avalon_slave_0 baseAddress {0x0100}
set_connection_parameter_value common_system_0.pb_0_m0/vctcxo_tamer_0.avalon_slave_0 defaultConnection {0}

add_connection common_system_0.pb_0_m0 rffe_spi.avalon_slave_0
set_connection_parameter_value common_system_0.pb_0_m0/rffe_spi.avalon_slave_0 arbitrationPriority {1}
set_connection_parameter_value common_system_0.pb_0_m0/rffe_spi.avalon_slave_0 baseAddress {0x0000}
set_connection_parameter_value common_system_0.pb_0_m0/rffe_spi.avalon_slave_0 defaultConnection {0}

add_connection common_system_0.pb_1_m0 iq_corr_rx_phase_gain.s1
set_connection_parameter_value common_system_0.pb_1_m0/iq_corr_rx_phase_gain.s1 arbitrationPriority {1}
set_connection_parameter_value common_system_0.pb_1_m0/iq_corr_rx_phase_gain.s1 baseAddress {0x0000}
set_connection_parameter_value common_system_0.pb_1_m0/iq_corr_rx_phase_gain.s1 defaultConnection {0}

add_connection common_system_0.pb_1_m0 iq_corr_tx_phase_gain.s1
set_connection_parameter_value common_system_0.pb_1_m0/iq_corr_tx_phase_gain.s1 arbitrationPriority {1}
set_connection_parameter_value common_system_0.pb_1_m0/iq_corr_tx_phase_gain.s1 baseAddress {0x0010}
set_connection_parameter_value common_system_0.pb_1_m0/iq_corr_tx_phase_gain.s1 defaultConnection {0}

add_connection common_system_0.pb_2_m0 arbiter_0.avalon_slave_0
set_connection_parameter_value common_system_0.pb_2_m0/arbiter_0.avalon_slave_0 arbitrationPriority {1}
set_connection_parameter_value common_system_0.pb_2_m0/arbiter_0.avalon_slave_0 baseAddress {0x0200}
set_connection_parameter_value common_system_0.pb_2_m0/arbiter_0.avalon_slave_0 defaultConnection {0}

add_connection common_system_0.pb_2_m0 agc_dc_i_max.s1
set_connection_parameter_value common_system_0.pb_2_m0/agc_dc_i_max.s1 arbitrationPriority {1}
set_connection_parameter_value common_system_0.pb_2_m0/agc_dc_i_max.s1 baseAddress {0x0300}
set_connection_parameter_value common_system_0.pb_2_m0/agc_dc_i_max.s1 defaultConnection {0}

add_connection common_system_0.pb_2_m0 agc_dc_q_max.s1
set_connection_parameter_value common_system_0.pb_2_m0/agc_dc_q_max.s1 arbitrationPriority {1}
set_connection_parameter_value common_system_0.pb_2_m0/agc_dc_q_max.s1 baseAddress {0x0310}
set_connection_parameter_value common_system_0.pb_2_m0/agc_dc_q_max.s1 defaultConnection {0}

add_connection common_system_0.pb_2_m0 agc_dc_i_mid.s1
set_connection_parameter_value common_system_0.pb_2_m0/agc_dc_i_mid.s1 arbitrationPriority {1}
set_connection_parameter_value common_system_0.pb_2_m0/agc_dc_i_mid.s1 baseAddress {0x0320}
set_connection_parameter_value common_system_0.pb_2_m0/agc_dc_i_mid.s1 defaultConnection {0}

add_connection common_system_0.pb_2_m0 agc_dc_q_mid.s1
set_connection_parameter_value common_system_0.pb_2_m0/agc_dc_q_mid.s1 arbitrationPriority {1}
set_connection_parameter_value common_system_0.pb_2_m0/agc_dc_q_mid.s1 baseAddress {0x0330}
set_connection_parameter_value common_system_0.pb_2_m0/agc_dc_q_mid.s1 defaultConnection {0}

add_connection common_system_0.pb_2_m0 agc_dc_i_min.s1
set_connection_parameter_value common_system_0.pb_2_m0/agc_dc_i_min.s1 arbitrationPriority {1}
set_connection_parameter_value common_system_0.pb_2_m0/agc_dc_i_min.s1 baseAddress {0x0340}
set_connection_parameter_value common_system_0.pb_2_m0/agc_dc_i_min.s1 defaultConnection {0}

add_connection common_system_0.pb_2_m0 agc_dc_q_min.s1
set_connection_parameter_value common_system_0.pb_2_m0/agc_dc_q_min.s1 arbitrationPriority {1}
set_connection_parameter_value common_system_0.pb_2_m0/agc_dc_q_min.s1 baseAddress {0x0350}
set_connection_parameter_value common_system_0.pb_2_m0/agc_dc_q_min.s1 defaultConnection {0}

add_connection clk_0.clk common_system_0.clk

add_connection clk_0.clk iq_corr_rx_phase_gain.clk

add_connection clk_0.clk iq_corr_tx_phase_gain.clk

add_connection clk_0.clk agc_dc_i_max.clk

add_connection clk_0.clk agc_dc_q_max.clk

add_connection clk_0.clk agc_dc_i_mid.clk

add_connection clk_0.clk agc_dc_q_mid.clk

add_connection clk_0.clk agc_dc_i_min.clk

add_connection clk_0.clk agc_dc_q_min.clk

add_connection clk_0.clk vctcxo_tamer_0.clock_sink

add_connection clk_0.clk rffe_spi.clock_sink

add_connection clk_0.clk arbiter_0.clock_sink

add_connection common_system_0.ib0_receiver_irq vctcxo_tamer_0.interrupt_sender
set_connection_parameter_value common_system_0.ib0_receiver_irq/vctcxo_tamer_0.interrupt_sender irqNumber {0}

add_connection common_system_0.ib0_receiver_irq arbiter_0.interrupt_sender
set_connection_parameter_value common_system_0.ib0_receiver_irq/arbiter_0.interrupt_sender irqNumber {1}

add_connection clk_0.clk_reset common_system_0.reset

add_connection clk_0.clk_reset iq_corr_rx_phase_gain.reset

add_connection clk_0.clk_reset iq_corr_tx_phase_gain.reset

add_connection clk_0.clk_reset vctcxo_tamer_0.reset_sink

add_connection clk_0.clk_reset rffe_spi.reset_sink

add_connection clk_0.clk_reset arbiter_0.reset

add_connection clk_0.clk_reset agc_dc_i_max.reset

add_connection clk_0.clk_reset agc_dc_q_max.reset

add_connection clk_0.clk_reset agc_dc_i_mid.reset

add_connection clk_0.clk_reset agc_dc_q_mid.reset

add_connection clk_0.clk_reset agc_dc_i_min.reset

add_connection clk_0.clk_reset agc_dc_q_min.reset

# interconnect requirements
set_interconnect_requirement {$system} {qsys_mm.clockCrossingAdapter} {HANDSHAKE}
set_interconnect_requirement {$system} {qsys_mm.maxAdditionalLatency} {1}
set_interconnect_requirement {$system} {qsys_mm.enableEccProtection} {FALSE}
set_interconnect_requirement {$system} {qsys_mm.insertDefaultSlave} {FALSE}

save_system {nios_system.qsys}
