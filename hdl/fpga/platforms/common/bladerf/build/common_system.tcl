# qsys scripting (.tcl) file for nios_system
package require -exact qsys 16.0

create_system {common_system}

set_project_property HIDE_FROM_IP_CATALOG {false}

if { [info exists device_family] == 0 } {
    error "Device family variable not set."
} else {
    set_project_property DEVICE_FAMILY $device_family
}

if { [info exists device] == 0 } {
    error "Device variable not set."
} else {
    set_project_property DEVICE $device
}

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
add_instance command_uart command_uart 1.0

add_instance control altera_avalon_pio
set_instance_parameter_value control {bitClearingEdgeCapReg} {0}
set_instance_parameter_value control {bitModifyingOutReg} {1}
set_instance_parameter_value control {captureEdge} {0}
set_instance_parameter_value control {direction} {InOut}
set_instance_parameter_value control {edgeType} {RISING}
set_instance_parameter_value control {generateIRQ} {0}
set_instance_parameter_value control {irqType} {LEVEL}
set_instance_parameter_value control {resetValue} {0.0}
set_instance_parameter_value control {simDoTestBenchWiring} {0}
set_instance_parameter_value control {simDrivenValue} {0.0}
set_instance_parameter_value control {width} {32}

add_instance irq_bridge_0 altera_irq_bridge
set_instance_parameter_value irq_bridge_0 {IRQ_WIDTH} {4}
set_instance_parameter_value irq_bridge_0 {IRQ_N} {0}

add_instance jtag_uart altera_avalon_jtag_uart
set_instance_parameter_value jtag_uart {allowMultipleConnections} {0}
set_instance_parameter_value jtag_uart {hubInstanceID} {0}
set_instance_parameter_value jtag_uart {readBufferDepth} {64}
set_instance_parameter_value jtag_uart {readIRQThreshold} {8}
set_instance_parameter_value jtag_uart {simInputCharacterStream} {}
set_instance_parameter_value jtag_uart {simInteractiveOptions} {INTERACTIVE_INPUT_OUTPUT}
set_instance_parameter_value jtag_uart {useRegistersForReadBuffer} {0}
set_instance_parameter_value jtag_uart {useRegistersForWriteBuffer} {0}
set_instance_parameter_value jtag_uart {useRelativePathForSimFile} {0}
set_instance_parameter_value jtag_uart {writeBufferDepth} {64}
set_instance_parameter_value jtag_uart {writeIRQThreshold} {8}

add_instance nios2 altera_nios2_gen2
set_instance_parameter_value nios2 {tmr_enabled} {0}
set_instance_parameter_value nios2 {setting_disable_tmr_inj} {0}
set_instance_parameter_value nios2 {setting_showUnpublishedSettings} {0}
set_instance_parameter_value nios2 {setting_showInternalSettings} {0}
set_instance_parameter_value nios2 {setting_preciseIllegalMemAccessException} {0}
set_instance_parameter_value nios2 {setting_exportPCB} {0}
set_instance_parameter_value nios2 {setting_exportdebuginfo} {0}
set_instance_parameter_value nios2 {setting_clearXBitsLDNonBypass} {1}
set_instance_parameter_value nios2 {setting_bigEndian} {0}
set_instance_parameter_value nios2 {setting_export_large_RAMs} {0}
set_instance_parameter_value nios2 {setting_asic_enabled} {0}
set_instance_parameter_value nios2 {setting_asic_synopsys_translate_on_off} {0}
set_instance_parameter_value nios2 {setting_asic_third_party_synthesis} {0}
set_instance_parameter_value nios2 {setting_asic_add_scan_mode_input} {0}
set_instance_parameter_value nios2 {setting_oci_version} {1}
set_instance_parameter_value nios2 {setting_fast_register_read} {0}
set_instance_parameter_value nios2 {setting_exportHostDebugPort} {0}
set_instance_parameter_value nios2 {setting_oci_export_jtag_signals} {0}
set_instance_parameter_value nios2 {setting_avalonDebugPortPresent} {0}
set_instance_parameter_value nios2 {setting_alwaysEncrypt} {1}
set_instance_parameter_value nios2 {io_regionbase} {0}
set_instance_parameter_value nios2 {io_regionsize} {0}
set_instance_parameter_value nios2 {setting_support31bitdcachebypass} {1}
set_instance_parameter_value nios2 {setting_activateTrace} {1}
set_instance_parameter_value nios2 {setting_allow_break_inst} {0}
set_instance_parameter_value nios2 {setting_activateTestEndChecker} {0}
set_instance_parameter_value nios2 {setting_ecc_sim_test_ports} {0}
set_instance_parameter_value nios2 {setting_disableocitrace} {0}
set_instance_parameter_value nios2 {setting_activateMonitors} {1}
set_instance_parameter_value nios2 {setting_HDLSimCachesCleared} {1}
set_instance_parameter_value nios2 {setting_HBreakTest} {0}
set_instance_parameter_value nios2 {setting_breakslaveoveride} {0}
set_instance_parameter_value nios2 {mpu_useLimit} {0}
set_instance_parameter_value nios2 {mpu_enabled} {0}
set_instance_parameter_value nios2 {mmu_enabled} {0}
set_instance_parameter_value nios2 {mmu_autoAssignTlbPtrSz} {1}
set_instance_parameter_value nios2 {cpuReset} {0}
set_instance_parameter_value nios2 {resetrequest_enabled} {1}
set_instance_parameter_value nios2 {setting_removeRAMinit} {0}
set_instance_parameter_value nios2 {setting_tmr_output_disable} {0}
set_instance_parameter_value nios2 {setting_shadowRegisterSets} {0}
set_instance_parameter_value nios2 {mpu_numOfInstRegion} {8}
set_instance_parameter_value nios2 {mpu_numOfDataRegion} {8}
set_instance_parameter_value nios2 {mmu_TLBMissExcOffset} {0}
set_instance_parameter_value nios2 {resetOffset} {0}
set_instance_parameter_value nios2 {exceptionOffset} {32}
set_instance_parameter_value nios2 {cpuID} {0}
set_instance_parameter_value nios2 {breakOffset} {32}
set_instance_parameter_value nios2 {userDefinedSettings} {}
set_instance_parameter_value nios2 {tracefilename} {}
set_instance_parameter_value nios2 {resetSlave} {ram.s1}
set_instance_parameter_value nios2 {mmu_TLBMissExcSlave} {}
set_instance_parameter_value nios2 {exceptionSlave} {ram.s1}
set_instance_parameter_value nios2 {breakSlave} {nios2_qsys_0.jtag_debug_module}
set_instance_parameter_value nios2 {setting_interruptControllerType} {Internal}
set_instance_parameter_value nios2 {setting_branchpredictiontype} {Dynamic}
set_instance_parameter_value nios2 {setting_bhtPtrSz} {8}
set_instance_parameter_value nios2 {cpuArchRev} {1}
set_instance_parameter_value nios2 {mul_shift_choice} {1}
set_instance_parameter_value nios2 {mul_32_impl} {2}
set_instance_parameter_value nios2 {mul_64_impl} {1}
set_instance_parameter_value nios2 {shift_rot_impl} {1}
set_instance_parameter_value nios2 {dividerType} {srt2}
set_instance_parameter_value nios2 {mpu_minInstRegionSize} {12}
set_instance_parameter_value nios2 {mpu_minDataRegionSize} {12}
set_instance_parameter_value nios2 {mmu_uitlbNumEntries} {4}
set_instance_parameter_value nios2 {mmu_udtlbNumEntries} {6}
set_instance_parameter_value nios2 {mmu_tlbPtrSz} {7}
set_instance_parameter_value nios2 {mmu_tlbNumWays} {16}
set_instance_parameter_value nios2 {mmu_processIDNumBits} {8}
set_instance_parameter_value nios2 {impl} ${nios_impl}
set_instance_parameter_value nios2 {icache_size} {2048}
set_instance_parameter_value nios2 {fa_cache_line} {2}
set_instance_parameter_value nios2 {fa_cache_linesize} {0}
set_instance_parameter_value nios2 {icache_tagramBlockType} {Automatic}
set_instance_parameter_value nios2 {icache_ramBlockType} {Automatic}
set_instance_parameter_value nios2 {icache_numTCIM} {0}
set_instance_parameter_value nios2 {icache_burstType} {None}
set_instance_parameter_value nios2 {dcache_bursts} {false}
set_instance_parameter_value nios2 {dcache_victim_buf_impl} {ram}
set_instance_parameter_value nios2 {dcache_size} {2048}
set_instance_parameter_value nios2 {dcache_tagramBlockType} {Automatic}
set_instance_parameter_value nios2 {dcache_ramBlockType} {Automatic}
set_instance_parameter_value nios2 {dcache_numTCDM} {0}
set_instance_parameter_value nios2 {setting_exportvectors} {0}
set_instance_parameter_value nios2 {setting_usedesignware} {0}
set_instance_parameter_value nios2 {setting_ecc_present} {0}
set_instance_parameter_value nios2 {setting_ic_ecc_present} {1}
set_instance_parameter_value nios2 {setting_rf_ecc_present} {1}
set_instance_parameter_value nios2 {setting_mmu_ecc_present} {1}
set_instance_parameter_value nios2 {setting_dc_ecc_present} {0}
set_instance_parameter_value nios2 {setting_itcm_ecc_present} {0}
set_instance_parameter_value nios2 {setting_dtcm_ecc_present} {0}
set_instance_parameter_value nios2 {regfile_ramBlockType} {Automatic}
set_instance_parameter_value nios2 {ocimem_ramBlockType} {Automatic}
set_instance_parameter_value nios2 {ocimem_ramInit} {0}
set_instance_parameter_value nios2 {mmu_ramBlockType} {Automatic}
set_instance_parameter_value nios2 {bht_ramBlockType} {Automatic}
set_instance_parameter_value nios2 {cdx_enabled} {0}
set_instance_parameter_value nios2 {mpx_enabled} {0}
set_instance_parameter_value nios2 {debug_enabled} {1}
set_instance_parameter_value nios2 {debug_triggerArming} {1}
set_instance_parameter_value nios2 {debug_debugReqSignals} {0}
set_instance_parameter_value nios2 {debug_assignJtagInstanceID} {0}
set_instance_parameter_value nios2 {debug_jtagInstanceID} {0}
set_instance_parameter_value nios2 {debug_OCIOnchipTrace} {_128}
set_instance_parameter_value nios2 {debug_hwbreakpoint} {0}
set_instance_parameter_value nios2 {debug_datatrigger} {0}
set_instance_parameter_value nios2 {debug_traceType} {none}
set_instance_parameter_value nios2 {debug_traceStorage} {onchip_trace}
set_instance_parameter_value nios2 {master_addr_map} {0}
set_instance_parameter_value nios2 {instruction_master_paddr_base} {0}
set_instance_parameter_value nios2 {instruction_master_paddr_size} {0.0}
set_instance_parameter_value nios2 {flash_instruction_master_paddr_base} {0}
set_instance_parameter_value nios2 {flash_instruction_master_paddr_size} {0.0}
set_instance_parameter_value nios2 {data_master_paddr_base} {0}
set_instance_parameter_value nios2 {data_master_paddr_size} {0.0}
set_instance_parameter_value nios2 {tightly_coupled_instruction_master_0_paddr_base} {0}
set_instance_parameter_value nios2 {tightly_coupled_instruction_master_0_paddr_size} {0.0}
set_instance_parameter_value nios2 {tightly_coupled_instruction_master_1_paddr_base} {0}
set_instance_parameter_value nios2 {tightly_coupled_instruction_master_1_paddr_size} {0.0}
set_instance_parameter_value nios2 {tightly_coupled_instruction_master_2_paddr_base} {0}
set_instance_parameter_value nios2 {tightly_coupled_instruction_master_2_paddr_size} {0.0}
set_instance_parameter_value nios2 {tightly_coupled_instruction_master_3_paddr_base} {0}
set_instance_parameter_value nios2 {tightly_coupled_instruction_master_3_paddr_size} {0.0}
set_instance_parameter_value nios2 {tightly_coupled_data_master_0_paddr_base} {0}
set_instance_parameter_value nios2 {tightly_coupled_data_master_0_paddr_size} {0.0}
set_instance_parameter_value nios2 {tightly_coupled_data_master_1_paddr_base} {0}
set_instance_parameter_value nios2 {tightly_coupled_data_master_1_paddr_size} {0.0}
set_instance_parameter_value nios2 {tightly_coupled_data_master_2_paddr_base} {0}
set_instance_parameter_value nios2 {tightly_coupled_data_master_2_paddr_size} {0.0}
set_instance_parameter_value nios2 {tightly_coupled_data_master_3_paddr_base} {0}
set_instance_parameter_value nios2 {tightly_coupled_data_master_3_paddr_size} {0.0}
set_instance_parameter_value nios2 {instruction_master_high_performance_paddr_base} {0}
set_instance_parameter_value nios2 {instruction_master_high_performance_paddr_size} {0.0}
set_instance_parameter_value nios2 {data_master_high_performance_paddr_base} {0}
set_instance_parameter_value nios2 {data_master_high_performance_paddr_size} {0.0}

add_instance opencores_i2c bladerf_oc_i2c_master 1.0
set_instance_parameter_value opencores_i2c {ARST_LVL} {1}

add_instance pb_0 altera_avalon_mm_bridge
set_instance_parameter_value pb_0 {DATA_WIDTH} {32}
set_instance_parameter_value pb_0 {SYMBOL_WIDTH} {8}
set_instance_parameter_value pb_0 {ADDRESS_WIDTH} {9}
set_instance_parameter_value pb_0 {USE_AUTO_ADDRESS_WIDTH} {0}
set_instance_parameter_value pb_0 {ADDRESS_UNITS} {SYMBOLS}
set_instance_parameter_value pb_0 {MAX_BURST_SIZE} {1}
set_instance_parameter_value pb_0 {MAX_PENDING_RESPONSES} {1}
set_instance_parameter_value pb_0 {LINEWRAPBURSTS} {0}
set_instance_parameter_value pb_0 {PIPELINE_COMMAND} {1}
set_instance_parameter_value pb_0 {PIPELINE_RESPONSE} {1}
set_instance_parameter_value pb_0 {USE_RESPONSE} {0}

add_instance pb_1 altera_avalon_mm_bridge
set_instance_parameter_value pb_1 {DATA_WIDTH} {32}
set_instance_parameter_value pb_1 {SYMBOL_WIDTH} {8}
set_instance_parameter_value pb_1 {ADDRESS_WIDTH} {5}
set_instance_parameter_value pb_1 {USE_AUTO_ADDRESS_WIDTH} {0}
set_instance_parameter_value pb_1 {ADDRESS_UNITS} {SYMBOLS}
set_instance_parameter_value pb_1 {MAX_BURST_SIZE} {1}
set_instance_parameter_value pb_1 {MAX_PENDING_RESPONSES} {1}
set_instance_parameter_value pb_1 {LINEWRAPBURSTS} {0}
set_instance_parameter_value pb_1 {PIPELINE_COMMAND} {1}
set_instance_parameter_value pb_1 {PIPELINE_RESPONSE} {1}
set_instance_parameter_value pb_1 {USE_RESPONSE} {0}

add_instance pb_2 altera_avalon_mm_bridge
set_instance_parameter_value pb_2 {DATA_WIDTH} {32}
set_instance_parameter_value pb_2 {SYMBOL_WIDTH} {8}
set_instance_parameter_value pb_2 {ADDRESS_WIDTH} {16}
set_instance_parameter_value pb_2 {USE_AUTO_ADDRESS_WIDTH} {1}
set_instance_parameter_value pb_2 {ADDRESS_UNITS} {SYMBOLS}
set_instance_parameter_value pb_2 {MAX_BURST_SIZE} {1}
set_instance_parameter_value pb_2 {MAX_PENDING_RESPONSES} {4}
set_instance_parameter_value pb_2 {LINEWRAPBURSTS} {0}
set_instance_parameter_value pb_2 {PIPELINE_COMMAND} {1}
set_instance_parameter_value pb_2 {PIPELINE_RESPONSE} {1}
set_instance_parameter_value pb_2 {USE_RESPONSE} {0}

add_instance peripheral_spi altera_avalon_spi
set_instance_parameter_value peripheral_spi {clockPhase} {1}
set_instance_parameter_value peripheral_spi {clockPolarity} {1}
set_instance_parameter_value peripheral_spi {dataWidth} {8}
set_instance_parameter_value peripheral_spi {disableAvalonFlowControl} {0}
set_instance_parameter_value peripheral_spi {insertDelayBetweenSlaveSelectAndSClk} {0}
set_instance_parameter_value peripheral_spi {insertSync} {0}
set_instance_parameter_value peripheral_spi {lsbOrderedFirst} {0}
set_instance_parameter_value peripheral_spi {masterSPI} {1}
set_instance_parameter_value peripheral_spi {numberOfSlaves} {2}
set_instance_parameter_value peripheral_spi {syncRegDepth} {2}
set_instance_parameter_value peripheral_spi {targetClockRate} {9600000.0}
set_instance_parameter_value peripheral_spi {targetSlaveSelectToSClkDelay} {0.0}

add_instance ram altera_avalon_onchip_memory2
set_instance_parameter_value ram {allowInSystemMemoryContentEditor} {1}
set_instance_parameter_value ram {blockType} {AUTO}
set_instance_parameter_value ram {dataWidth} {32}
set_instance_parameter_value ram {dataWidth2} {32}
set_instance_parameter_value ram {dualPort} {0}
set_instance_parameter_value ram {enableDiffWidth} {0}
set_instance_parameter_value ram {initMemContent} {1}
set_instance_parameter_value ram {initializationFileName} {onchip_memory2_0}
set_instance_parameter_value ram {instanceID} {MED}
set_instance_parameter_value ram {memorySize} {32768.0}
set_instance_parameter_value ram {readDuringWriteMode} {DONT_CARE}
set_instance_parameter_value ram {simAllowMRAMContentsFile} {0}
set_instance_parameter_value ram {simMemInitOnlyFilename} {0}
set_instance_parameter_value ram {singleClockOperation} {0}
set_instance_parameter_value ram {slave1Latency} {1}
set_instance_parameter_value ram {slave2Latency} {1}
set_instance_parameter_value ram {useNonDefaultInitFile} {0}
set_instance_parameter_value ram {copyInitFile} {0}
set_instance_parameter_value ram {useShallowMemBlocks} {0}
set_instance_parameter_value ram {writable} {1}
set_instance_parameter_value ram {ecc_enabled} {0}
set_instance_parameter_value ram {resetrequest_enabled} {1}

add_instance rx_tamer time_tamer 1.0

add_instance rx_trigger_ctl altera_avalon_pio
set_instance_parameter_value rx_trigger_ctl {bitClearingEdgeCapReg} {0}
set_instance_parameter_value rx_trigger_ctl {bitModifyingOutReg} {1}
set_instance_parameter_value rx_trigger_ctl {captureEdge} {0}
set_instance_parameter_value rx_trigger_ctl {direction} {InOut}
set_instance_parameter_value rx_trigger_ctl {edgeType} {RISING}
set_instance_parameter_value rx_trigger_ctl {generateIRQ} {0}
set_instance_parameter_value rx_trigger_ctl {irqType} {LEVEL}
set_instance_parameter_value rx_trigger_ctl {resetValue} {0.0}
set_instance_parameter_value rx_trigger_ctl {simDoTestBenchWiring} {0}
set_instance_parameter_value rx_trigger_ctl {simDrivenValue} {0.0}
set_instance_parameter_value rx_trigger_ctl {width} {8}

add_instance system_clock clock_source
set_instance_parameter_value system_clock {clockFrequency} {80000000.0}
set_instance_parameter_value system_clock {clockFrequencyKnown} {1}
set_instance_parameter_value system_clock {resetSynchronousEdges} {DEASSERT}

add_instance tx_tamer time_tamer 1.0

add_instance tx_trigger_ctl altera_avalon_pio
set_instance_parameter_value tx_trigger_ctl {bitClearingEdgeCapReg} {0}
set_instance_parameter_value tx_trigger_ctl {bitModifyingOutReg} {1}
set_instance_parameter_value tx_trigger_ctl {captureEdge} {0}
set_instance_parameter_value tx_trigger_ctl {direction} {InOut}
set_instance_parameter_value tx_trigger_ctl {edgeType} {RISING}
set_instance_parameter_value tx_trigger_ctl {generateIRQ} {0}
set_instance_parameter_value tx_trigger_ctl {irqType} {LEVEL}
set_instance_parameter_value tx_trigger_ctl {resetValue} {0.0}
set_instance_parameter_value tx_trigger_ctl {simDoTestBenchWiring} {0}
set_instance_parameter_value tx_trigger_ctl {simDrivenValue} {0.0}
set_instance_parameter_value tx_trigger_ctl {width} {8}

add_instance xb_gpio altera_avalon_pio
set_instance_parameter_value xb_gpio {bitClearingEdgeCapReg} {0}
set_instance_parameter_value xb_gpio {bitModifyingOutReg} {0}
set_instance_parameter_value xb_gpio {captureEdge} {0}
set_instance_parameter_value xb_gpio {direction} {InOut}
set_instance_parameter_value xb_gpio {edgeType} {RISING}
set_instance_parameter_value xb_gpio {generateIRQ} {0}
set_instance_parameter_value xb_gpio {irqType} {LEVEL}
set_instance_parameter_value xb_gpio {resetValue} {0.0}
set_instance_parameter_value xb_gpio {simDoTestBenchWiring} {0}
set_instance_parameter_value xb_gpio {simDrivenValue} {0.0}
set_instance_parameter_value xb_gpio {width} {32}

add_instance xb_gpio_dir altera_avalon_pio
set_instance_parameter_value xb_gpio_dir {bitClearingEdgeCapReg} {0}
set_instance_parameter_value xb_gpio_dir {bitModifyingOutReg} {0}
set_instance_parameter_value xb_gpio_dir {captureEdge} {0}
set_instance_parameter_value xb_gpio_dir {direction} {Output}
set_instance_parameter_value xb_gpio_dir {edgeType} {RISING}
set_instance_parameter_value xb_gpio_dir {generateIRQ} {0}
set_instance_parameter_value xb_gpio_dir {irqType} {LEVEL}
set_instance_parameter_value xb_gpio_dir {resetValue} {0.0}
set_instance_parameter_value xb_gpio_dir {simDoTestBenchWiring} {0}
set_instance_parameter_value xb_gpio_dir {simDrivenValue} {0.0}
set_instance_parameter_value xb_gpio_dir {width} {32}

# exported interfaces
add_interface clk clock sink
set_interface_property clk EXPORT_OF system_clock.clk_in
add_interface command conduit end
set_interface_property command EXPORT_OF command_uart.rs232
add_interface dac conduit end
set_interface_property dac EXPORT_OF peripheral_spi.external
add_interface gpio conduit end
set_interface_property gpio EXPORT_OF control.external_connection
add_interface ib0_receiver_irq interrupt receiver
set_interface_property ib0_receiver_irq EXPORT_OF irq_bridge_0.receiver_irq
add_interface oc_i2c conduit end
set_interface_property oc_i2c EXPORT_OF opencores_i2c.conduit_end
add_interface pb_0_m0 avalon master
set_interface_property pb_0_m0 EXPORT_OF pb_0.m0
add_interface pb_1_m0 avalon master
set_interface_property pb_1_m0 EXPORT_OF pb_1.m0
add_interface pb_2_m0 avalon master
set_interface_property pb_2_m0 EXPORT_OF pb_2.m0
add_interface reset reset sink
set_interface_property reset EXPORT_OF system_clock.clk_in_reset
add_interface rx_tamer conduit end
set_interface_property rx_tamer EXPORT_OF rx_tamer.conduit_end
add_interface rx_trigger_ctl conduit end
set_interface_property rx_trigger_ctl EXPORT_OF rx_trigger_ctl.external_connection
add_interface tx_tamer conduit end
set_interface_property tx_tamer EXPORT_OF tx_tamer.conduit_end
add_interface tx_trigger_ctl conduit end
set_interface_property tx_trigger_ctl EXPORT_OF tx_trigger_ctl.external_connection
add_interface xb_gpio conduit end
set_interface_property xb_gpio EXPORT_OF xb_gpio.external_connection
add_interface xb_gpio_dir conduit end
set_interface_property xb_gpio_dir EXPORT_OF xb_gpio_dir.external_connection

# connections and connection parameters
add_connection nios2.data_master jtag_uart.avalon_jtag_slave
set_connection_parameter_value nios2.data_master/jtag_uart.avalon_jtag_slave arbitrationPriority {1}
set_connection_parameter_value nios2.data_master/jtag_uart.avalon_jtag_slave baseAddress {0x9100}
set_connection_parameter_value nios2.data_master/jtag_uart.avalon_jtag_slave defaultConnection {0}

add_connection nios2.data_master command_uart.avalon_slave
set_connection_parameter_value nios2.data_master/command_uart.avalon_slave arbitrationPriority {1}
set_connection_parameter_value nios2.data_master/command_uart.avalon_slave baseAddress {0x9120}
set_connection_parameter_value nios2.data_master/command_uart.avalon_slave defaultConnection {0}

add_connection nios2.data_master tx_tamer.avalon_slave_0
set_connection_parameter_value nios2.data_master/tx_tamer.avalon_slave_0 arbitrationPriority {1}
set_connection_parameter_value nios2.data_master/tx_tamer.avalon_slave_0 baseAddress {0x9140}
set_connection_parameter_value nios2.data_master/tx_tamer.avalon_slave_0 defaultConnection {0}

add_connection nios2.data_master rx_tamer.avalon_slave_0
set_connection_parameter_value nios2.data_master/rx_tamer.avalon_slave_0 arbitrationPriority {1}
set_connection_parameter_value nios2.data_master/rx_tamer.avalon_slave_0 baseAddress {0x9160}
set_connection_parameter_value nios2.data_master/rx_tamer.avalon_slave_0 defaultConnection {0}

add_connection nios2.data_master opencores_i2c.bladerf_oc_i2c_master
set_connection_parameter_value nios2.data_master/opencores_i2c.bladerf_oc_i2c_master arbitrationPriority {1}
set_connection_parameter_value nios2.data_master/opencores_i2c.bladerf_oc_i2c_master baseAddress {0x90f0}
set_connection_parameter_value nios2.data_master/opencores_i2c.bladerf_oc_i2c_master defaultConnection {0}

add_connection nios2.data_master nios2.debug_mem_slave
set_connection_parameter_value nios2.data_master/nios2.debug_mem_slave arbitrationPriority {1}
set_connection_parameter_value nios2.data_master/nios2.debug_mem_slave baseAddress {0x8800}
set_connection_parameter_value nios2.data_master/nios2.debug_mem_slave defaultConnection {0}

add_connection nios2.data_master pb_0.s0
set_connection_parameter_value nios2.data_master/pb_0.s0 arbitrationPriority {1}
set_connection_parameter_value nios2.data_master/pb_0.s0 baseAddress {0x9200}
set_connection_parameter_value nios2.data_master/pb_0.s0 defaultConnection {0}

add_connection nios2.data_master pb_1.s0
set_connection_parameter_value nios2.data_master/pb_1.s0 arbitrationPriority {1}
set_connection_parameter_value nios2.data_master/pb_1.s0 baseAddress {0x9440}
set_connection_parameter_value nios2.data_master/pb_1.s0 defaultConnection {0}

add_connection nios2.data_master pb_2.s0
set_connection_parameter_value nios2.data_master/pb_2.s0 arbitrationPriority {1}
set_connection_parameter_value nios2.data_master/pb_2.s0 baseAddress {0x00010000}
set_connection_parameter_value nios2.data_master/pb_2.s0 defaultConnection {0}

add_connection nios2.data_master ram.s1
set_connection_parameter_value nios2.data_master/ram.s1 arbitrationPriority {1}
set_connection_parameter_value nios2.data_master/ram.s1 baseAddress {0x20000}
set_connection_parameter_value nios2.data_master/ram.s1 defaultConnection {0}

add_connection nios2.data_master control.s1
set_connection_parameter_value nios2.data_master/control.s1 arbitrationPriority {1}
set_connection_parameter_value nios2.data_master/control.s1 baseAddress {0x9040}
set_connection_parameter_value nios2.data_master/control.s1 defaultConnection {0}

add_connection nios2.data_master xb_gpio.s1
set_connection_parameter_value nios2.data_master/xb_gpio.s1 arbitrationPriority {1}
set_connection_parameter_value nios2.data_master/xb_gpio.s1 baseAddress {0x90b0}
set_connection_parameter_value nios2.data_master/xb_gpio.s1 defaultConnection {0}

add_connection nios2.data_master xb_gpio_dir.s1
set_connection_parameter_value nios2.data_master/xb_gpio_dir.s1 arbitrationPriority {1}
set_connection_parameter_value nios2.data_master/xb_gpio_dir.s1 baseAddress {0x90a0}
set_connection_parameter_value nios2.data_master/xb_gpio_dir.s1 defaultConnection {0}

add_connection nios2.data_master tx_trigger_ctl.s1
set_connection_parameter_value nios2.data_master/tx_trigger_ctl.s1 arbitrationPriority {1}
set_connection_parameter_value nios2.data_master/tx_trigger_ctl.s1 baseAddress {0x9420}
set_connection_parameter_value nios2.data_master/tx_trigger_ctl.s1 defaultConnection {0}

add_connection nios2.data_master rx_trigger_ctl.s1
set_connection_parameter_value nios2.data_master/rx_trigger_ctl.s1 arbitrationPriority {1}
set_connection_parameter_value nios2.data_master/rx_trigger_ctl.s1 baseAddress {0x9400}
set_connection_parameter_value nios2.data_master/rx_trigger_ctl.s1 defaultConnection {0}

add_connection nios2.data_master peripheral_spi.spi_control_port
set_connection_parameter_value nios2.data_master/peripheral_spi.spi_control_port arbitrationPriority {1}
set_connection_parameter_value nios2.data_master/peripheral_spi.spi_control_port baseAddress {0x9060}
set_connection_parameter_value nios2.data_master/peripheral_spi.spi_control_port defaultConnection {0}

add_connection nios2.instruction_master nios2.debug_mem_slave
set_connection_parameter_value nios2.instruction_master/nios2.debug_mem_slave arbitrationPriority {1}
set_connection_parameter_value nios2.instruction_master/nios2.debug_mem_slave baseAddress {0x8800}
set_connection_parameter_value nios2.instruction_master/nios2.debug_mem_slave defaultConnection {0}

add_connection nios2.instruction_master ram.s1
set_connection_parameter_value nios2.instruction_master/ram.s1 arbitrationPriority {1}
set_connection_parameter_value nios2.instruction_master/ram.s1 baseAddress {0x20000}
set_connection_parameter_value nios2.instruction_master/ram.s1 defaultConnection {0}

add_connection system_clock.clk nios2.clk

add_connection system_clock.clk jtag_uart.clk

add_connection system_clock.clk peripheral_spi.clk

add_connection system_clock.clk control.clk

add_connection system_clock.clk xb_gpio.clk

add_connection system_clock.clk xb_gpio_dir.clk

add_connection system_clock.clk rx_trigger_ctl.clk

add_connection system_clock.clk tx_trigger_ctl.clk

add_connection system_clock.clk pb_0.clk

add_connection system_clock.clk irq_bridge_0.clk

add_connection system_clock.clk pb_1.clk

add_connection system_clock.clk pb_2.clk

add_connection system_clock.clk ram.clk1

add_connection system_clock.clk command_uart.clock

add_connection system_clock.clk opencores_i2c.clock_sink

add_connection system_clock.clk tx_tamer.clock_sink

add_connection system_clock.clk rx_tamer.clock_sink

add_connection nios2.irq command_uart.interrupt
set_connection_parameter_value nios2.irq/command_uart.interrupt irqNumber {7}

add_connection nios2.irq opencores_i2c.interrupt_sender
set_connection_parameter_value nios2.irq/opencores_i2c.interrupt_sender irqNumber {5}

add_connection nios2.irq tx_tamer.interrupt_sender
set_connection_parameter_value nios2.irq/tx_tamer.interrupt_sender irqNumber {2}

add_connection nios2.irq rx_tamer.interrupt_sender
set_connection_parameter_value nios2.irq/rx_tamer.interrupt_sender irqNumber {3}

add_connection nios2.irq jtag_uart.irq
set_connection_parameter_value nios2.irq/jtag_uart.irq irqNumber {1}

add_connection nios2.irq peripheral_spi.irq
set_connection_parameter_value nios2.irq/peripheral_spi.irq irqNumber {4}

add_connection nios2.irq irq_bridge_0.sender0_irq
set_connection_parameter_value nios2.irq/irq_bridge_0.sender0_irq irqNumber {0}

add_connection nios2.irq irq_bridge_0.sender1_irq
set_connection_parameter_value nios2.irq/irq_bridge_0.sender1_irq irqNumber {6}

add_connection nios2.irq irq_bridge_0.sender2_irq
set_connection_parameter_value nios2.irq/irq_bridge_0.sender2_irq irqNumber {8}

add_connection nios2.irq irq_bridge_0.sender3_irq
set_connection_parameter_value nios2.irq/irq_bridge_0.sender3_irq irqNumber {9}

add_connection system_clock.clk_reset irq_bridge_0.clk_reset

add_connection system_clock.clk_reset nios2.reset

add_connection system_clock.clk_reset jtag_uart.reset

add_connection system_clock.clk_reset peripheral_spi.reset

add_connection system_clock.clk_reset control.reset

add_connection system_clock.clk_reset xb_gpio.reset

add_connection system_clock.clk_reset xb_gpio_dir.reset

add_connection system_clock.clk_reset command_uart.reset

add_connection system_clock.clk_reset tx_tamer.reset

add_connection system_clock.clk_reset rx_tamer.reset

add_connection system_clock.clk_reset rx_trigger_ctl.reset

add_connection system_clock.clk_reset tx_trigger_ctl.reset

add_connection system_clock.clk_reset pb_0.reset

add_connection system_clock.clk_reset pb_1.reset

add_connection system_clock.clk_reset pb_2.reset

add_connection system_clock.clk_reset ram.reset1

add_connection system_clock.clk_reset opencores_i2c.reset_sink

# interconnect requirements
set_interconnect_requirement {$system} {qsys_mm.clockCrossingAdapter} {HANDSHAKE}
set_interconnect_requirement {$system} {qsys_mm.maxAdditionalLatency} {1}
set_interconnect_requirement {$system} {qsys_mm.enableEccProtection} {FALSE}
set_interconnect_requirement {$system} {qsys_mm.insertDefaultSlave} {FALSE}

save_system {common_system.qsys}
