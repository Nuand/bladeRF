# qsys scripting (.tcl) file for nios_system
package require -exact qsys 16.0

create_system {nios_system}

set_project_property DEVICE_FAMILY {Cyclone V}
set_project_property DEVICE {5CEBA4F23C8}
set_project_property HIDE_FROM_IP_CATALOG {false}

if { [info exists nios_impl] == 0 } {
    error "Nios implementation variable not set."
} else {
    switch -regexp $nios_impl {
        "[Tt][Ii][Nn][Yy]"     { puts "using tiny";  set nios_impl Tiny  }
        "[Ff][Aa][Ss][Tt]"     { puts "using fast";  set nios_impl Fast  }
        default { error "Invalid NIOS implementation: ${nios_impl}." }
    }
}

# Instances and instance parameters
# (disabled instances are intentionally culled)
add_instance clk_0 clock_source
set_instance_parameter_value clk_0 {clockFrequency} {80000000.0}
set_instance_parameter_value clk_0 {clockFrequencyKnown} {1}
set_instance_parameter_value clk_0 {resetSynchronousEdges} {DEASSERT}

add_instance common_system_0 common_system 1.0

add_instance gpio_rffe_0 altera_avalon_pio
set_instance_parameter_value gpio_rffe_0 {bitClearingEdgeCapReg} {0}
set_instance_parameter_value gpio_rffe_0 {bitModifyingOutReg} {1}
set_instance_parameter_value gpio_rffe_0 {captureEdge} {0}
set_instance_parameter_value gpio_rffe_0 {direction} {InOut}
set_instance_parameter_value gpio_rffe_0 {edgeType} {RISING}
set_instance_parameter_value gpio_rffe_0 {generateIRQ} {1}
set_instance_parameter_value gpio_rffe_0 {irqType} {LEVEL}
set_instance_parameter_value gpio_rffe_0 {resetValue} {0x00000000}
set_instance_parameter_value gpio_rffe_0 {simDoTestBenchWiring} {0}
set_instance_parameter_value gpio_rffe_0 {simDrivenValue} {0.0}
set_instance_parameter_value gpio_rffe_0 {width} {32}

add_instance lms_spi altera_avalon_spi
set_instance_parameter_value lms_spi {clockPhase} {0}
set_instance_parameter_value lms_spi {clockPolarity} {1}
set_instance_parameter_value lms_spi {dataWidth} {8}
set_instance_parameter_value lms_spi {disableAvalonFlowControl} {0}
set_instance_parameter_value lms_spi {insertDelayBetweenSlaveSelectAndSClk} {0}
set_instance_parameter_value lms_spi {insertSync} {1}
set_instance_parameter_value lms_spi {lsbOrderedFirst} {0}
set_instance_parameter_value lms_spi {masterSPI} {1}
set_instance_parameter_value lms_spi {numberOfSlaves} {1}
set_instance_parameter_value lms_spi {syncRegDepth} {2}
set_instance_parameter_value lms_spi {targetClockRate} {40000000.0}
set_instance_parameter_value lms_spi {targetSlaveSelectToSClkDelay} {0.0}

add_instance vctcxo_tamer_0 altera_avalon_onchip_memory2
set_instance_parameter_value vctcxo_tamer_0 {allowInSystemMemoryContentEditor} {0}
set_instance_parameter_value vctcxo_tamer_0 {blockType} {AUTO}
set_instance_parameter_value vctcxo_tamer_0 {dataWidth} {8}
set_instance_parameter_value vctcxo_tamer_0 {dataWidth2} {32}
set_instance_parameter_value vctcxo_tamer_0 {dualPort} {0}
set_instance_parameter_value vctcxo_tamer_0 {enableDiffWidth} {0}
set_instance_parameter_value vctcxo_tamer_0 {initMemContent} {1}
set_instance_parameter_value vctcxo_tamer_0 {initializationFileName} {onchip_mem.hex}
set_instance_parameter_value vctcxo_tamer_0 {instanceID} {NONE}
set_instance_parameter_value vctcxo_tamer_0 {memorySize} {256.0}
set_instance_parameter_value vctcxo_tamer_0 {readDuringWriteMode} {DONT_CARE}
set_instance_parameter_value vctcxo_tamer_0 {simAllowMRAMContentsFile} {0}
set_instance_parameter_value vctcxo_tamer_0 {simMemInitOnlyFilename} {0}
set_instance_parameter_value vctcxo_tamer_0 {singleClockOperation} {0}
set_instance_parameter_value vctcxo_tamer_0 {slave1Latency} {1}
set_instance_parameter_value vctcxo_tamer_0 {slave2Latency} {1}
set_instance_parameter_value vctcxo_tamer_0 {useNonDefaultInitFile} {0}
set_instance_parameter_value vctcxo_tamer_0 {copyInitFile} {0}
set_instance_parameter_value vctcxo_tamer_0 {useShallowMemBlocks} {0}
set_instance_parameter_value vctcxo_tamer_0 {writable} {1}
set_instance_parameter_value vctcxo_tamer_0 {ecc_enabled} {0}
set_instance_parameter_value vctcxo_tamer_0 {resetrequest_enabled} {1}

# exported interfaces
add_interface clk clock sink
set_interface_property clk EXPORT_OF clk_0.clk_in
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
add_interface gpio_rffe_0 conduit end
set_interface_property gpio_rffe_0 EXPORT_OF gpio_rffe_0.external_connection
add_interface oc_i2c conduit end
set_interface_property oc_i2c EXPORT_OF common_system_0.oc_i2c
add_interface reset reset sink
set_interface_property reset EXPORT_OF clk_0.clk_in_reset
add_interface rx_tamer conduit end
set_interface_property rx_tamer EXPORT_OF common_system_0.rx_tamer
add_interface rx_trigger_ctl conduit end
set_interface_property rx_trigger_ctl EXPORT_OF common_system_0.rx_trigger_ctl
add_interface spi conduit end
set_interface_property spi EXPORT_OF lms_spi.external
add_interface tx_tamer conduit end
set_interface_property tx_tamer EXPORT_OF common_system_0.tx_tamer
add_interface tx_trigger_ctl conduit end
set_interface_property tx_trigger_ctl EXPORT_OF common_system_0.tx_trigger_ctl
add_interface xb_gpio conduit end
set_interface_property xb_gpio EXPORT_OF common_system_0.xb_gpio
add_interface xb_gpio_dir conduit end
set_interface_property xb_gpio_dir EXPORT_OF common_system_0.xb_gpio_dir

# connections and connection parameters
add_connection common_system_0.pb_0_m0 vctcxo_tamer_0.s1
set_connection_parameter_value common_system_0.pb_0_m0/vctcxo_tamer_0.s1 arbitrationPriority {1}
set_connection_parameter_value common_system_0.pb_0_m0/vctcxo_tamer_0.s1 baseAddress {0x0100}
set_connection_parameter_value common_system_0.pb_0_m0/vctcxo_tamer_0.s1 defaultConnection {0}

add_connection common_system_0.pb_0_m0 lms_spi.spi_control_port
set_connection_parameter_value common_system_0.pb_0_m0/lms_spi.spi_control_port arbitrationPriority {1}
set_connection_parameter_value common_system_0.pb_0_m0/lms_spi.spi_control_port baseAddress {0x0000}
set_connection_parameter_value common_system_0.pb_0_m0/lms_spi.spi_control_port defaultConnection {0}

add_connection common_system_0.pb_1_m0 gpio_rffe_0.s1
set_connection_parameter_value common_system_0.pb_1_m0/gpio_rffe_0.s1 arbitrationPriority {1}
set_connection_parameter_value common_system_0.pb_1_m0/gpio_rffe_0.s1 baseAddress {0x0000}
set_connection_parameter_value common_system_0.pb_1_m0/gpio_rffe_0.s1 defaultConnection {0}

add_connection clk_0.clk common_system_0.clk

add_connection clk_0.clk lms_spi.clk

add_connection clk_0.clk gpio_rffe_0.clk

add_connection clk_0.clk vctcxo_tamer_0.clk1

add_connection common_system_0.ib0_receiver_irq lms_spi.irq
set_connection_parameter_value common_system_0.ib0_receiver_irq/lms_spi.irq irqNumber {2}

add_connection common_system_0.ib0_receiver_irq gpio_rffe_0.irq
set_connection_parameter_value common_system_0.ib0_receiver_irq/gpio_rffe_0.irq irqNumber {3}

add_connection clk_0.clk_reset common_system_0.reset

add_connection clk_0.clk_reset lms_spi.reset

add_connection clk_0.clk_reset gpio_rffe_0.reset

add_connection clk_0.clk_reset vctcxo_tamer_0.reset1

# interconnect requirements
set_interconnect_requirement {$system} {qsys_mm.clockCrossingAdapter} {HANDSHAKE}
set_interconnect_requirement {$system} {qsys_mm.maxAdditionalLatency} {1}
set_interconnect_requirement {$system} {qsys_mm.enableEccProtection} {FALSE}
set_interconnect_requirement {$system} {qsys_mm.insertDefaultSlave} {FALSE}

save_system {nios_system.qsys}
