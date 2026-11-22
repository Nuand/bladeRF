# qsys scripting (.tcl) file for nios_system
package require -exact qsys 16.0

create_system {nios_system}

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

# Soft CPU: nios2 (Nios II Gen2, default) or niosv (Nios V/m)
if { [info exists cpu_type] == 0 } {
    set cpu_type nios2
}
switch -regexp $cpu_type {
    "^[Nn][Ii][Oo][Ss](2|[Ii][Ii])$" { set cpu_type nios2 }
    "^[Nn][Ii][Oo][Ss][Vv]$"         { set cpu_type niosv }
    default { error "Invalid CPU type: ${cpu_type} (expected nios2 or niosv)." }
}
puts "using CPU: ${cpu_type}"

# Nios V manager bus: avalon (default where supported) or axi4lite
if { [info exists niosv_bus] == 0 } {
    set niosv_bus avalon
}
switch -regexp $niosv_bus {
    "^[Aa][Vv][Aa][Ll][Oo][Nn]$" { set niosv_bus avalon }
    "^[Aa][Xx][Ii]"              { set niosv_bus axi4lite }
    default { error "Invalid Nios V bus: ${niosv_bus} (expected avalon or axi4lite)." }
}

if { [info exists platform_revision] == 0 } {
    set platform_revision "default"
} else {
    puts "platform revision: ${platform_revision}"
}

# RAM base address is equal to the RAM size
set ram_base_addr "0x[format %08x $ram_size]"
puts "RAM size: ${ram_size}, base address: ${ram_base_addr}"

# Instances and instance parameters
# (disabled instances are intentionally culled)
add_instance axi_ad9361_0 axi_ad9361 1.0
set_instance_parameter_value axi_ad9361_0 {ID} {0}
set_instance_parameter_value axi_ad9361_0 {DEVICE_TYPE} {0}
set_instance_parameter_value axi_ad9361_0 {DAC_DDS_DISABLE} {1}

add_instance cb_ad9361_0_data altera_clock_bridge
set_instance_parameter_value cb_ad9361_0_data {EXPLICIT_CLOCK_RATE} {0.0}
set_instance_parameter_value cb_ad9361_0_data {NUM_CLOCK_OUTPUTS} {1}

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

add_instance gpio_rffe_0 altera_avalon_pio
set_instance_parameter_value gpio_rffe_0 {bitClearingEdgeCapReg} {0}
set_instance_parameter_value gpio_rffe_0 {bitModifyingOutReg} {1}
set_instance_parameter_value gpio_rffe_0 {captureEdge} {0}
set_instance_parameter_value gpio_rffe_0 {direction} {InOut}
set_instance_parameter_value gpio_rffe_0 {edgeType} {RISING}
set_instance_parameter_value gpio_rffe_0 {generateIRQ} {1}
set_instance_parameter_value gpio_rffe_0 {irqType} {LEVEL}
set_instance_parameter_value gpio_rffe_0 {resetValue} {0x00018000}
set_instance_parameter_value gpio_rffe_0 {simDoTestBenchWiring} {0}
set_instance_parameter_value gpio_rffe_0 {simDrivenValue} {0.0}
set_instance_parameter_value gpio_rffe_0 {width} {32}

add_instance wishbone_master_0 wishbone_master 1.0
set_instance_parameter_value wishbone_master_0 {ADDR_BITS} {32}
set_instance_parameter_value wishbone_master_0 {DATA_BITS} {32}

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

if { [string equal $cpu_type "nios2"] } {
    # Nios II Gen2 (Quartus Prime 24.1 and earlier)
    set cpu     nios2
    set cpu_dm  nios2.data_master
    set cpu_im  nios2.instruction_master
    set cpu_irq nios2.irq
    # CPU-internal agents: {agent base_address ...}
    set cpu_dm_agents [list nios2.debug_mem_slave 0x8800]
    set cpu_im_agents [list nios2.debug_mem_slave 0x8800]

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
} else {
    # Nios V/m (RV32IA). Quartus Prime 25.1 and later no longer ship Nios II.
    set cpu     niosv
    set cpu_dm  niosv.data_manager
    set cpu_im  niosv.instruction_manager
    set cpu_irq niosv.platform_irq_rx
    # The machine timer (mtime/mtimecmp, 64 bytes) takes the address of the
    # Nios II debug memory. The debug module needs a 64 KiB-aligned 64 KiB
    # window, so it goes just above the RAM.
    set niosv_dm_base "0x[format %08x [expr {2 * $ram_size}]]"
    set cpu_dm_agents [list niosv.timer_sw_agent 0x8800 niosv.dm_agent ${niosv_dm_base}]
    set cpu_im_agents [list niosv.dm_agent ${niosv_dm_base}]

    add_instance niosv intel_niosv_m
    # Tiny: non-pipelined core (smallest). Fast: pipelined core.
    if { [string equal $nios_impl "Fast"] } {
        set_instance_parameter_value niosv {pipelineArch} {true}
    } else {
        set_instance_parameter_value niosv {pipelineArch} {false}
    }
    set_instance_parameter_value niosv {enableDebug} {true}
    set_instance_parameter_value niosv {enableDebugReset} {false}
    set_instance_parameter_value niosv {useResetReq} {false}
    set_instance_parameter_value niosv {numGpr} {32}
    set_instance_parameter_value niosv {resetSlave} {ram.s1}
    set_instance_parameter_value niosv {resetOffset} {0}
    set_instance_parameter_value niosv {enableECCLite} {false}
    # Newer Nios V/m releases (e.g. Quartus 25.1) can use Avalon-MM managers
    # instead of AXI4-Lite, which avoids AXI-to-Avalon adapters in front of
    # every peripheral. Older ones (e.g. Quartus 23.1) are AXI4-Lite only.
    if { [lsearch -exact [get_instance_parameters niosv] enableAvalonInterface] >= 0 } {
        if { [string equal $niosv_bus "avalon"] } {
            set_instance_parameter_value niosv {enableAvalonInterface} {true}
        } else {
            set_instance_parameter_value niosv {enableAvalonInterface} {false}
        }
        puts "Nios V manager interfaces: ${niosv_bus}"
    } elseif { [string equal $niosv_bus "avalon"] } {
        puts "Nios V manager interfaces: axi4lite (this Quartus version has no Avalon option)"
    }
}

add_instance opencores_i2c bladerf_oc_i2c_master 1.0
set_instance_parameter_value opencores_i2c {ARST_LVL} {1}

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
set_instance_parameter_value ram {memorySize} ${ram_size}
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

add_instance rb_ad9361_0_data altera_reset_bridge
set_instance_parameter_value rb_ad9361_0_data {ACTIVE_LOW_RESET} {0}
set_instance_parameter_value rb_ad9361_0_data {SYNCHRONOUS_EDGES} {deassert}
set_instance_parameter_value rb_ad9361_0_data {NUM_RESET_OUTPUTS} {1}
set_instance_parameter_value rb_ad9361_0_data {USE_RESET_REQUEST} {0}

add_instance rffe_spi altera_avalon_spi
set_instance_parameter_value rffe_spi {clockPhase} {0}
set_instance_parameter_value rffe_spi {clockPolarity} {1}
set_instance_parameter_value rffe_spi {dataWidth} {8}
set_instance_parameter_value rffe_spi {disableAvalonFlowControl} {0}
set_instance_parameter_value rffe_spi {insertDelayBetweenSlaveSelectAndSClk} {0}
set_instance_parameter_value rffe_spi {insertSync} {0}
set_instance_parameter_value rffe_spi {lsbOrderedFirst} {0}
set_instance_parameter_value rffe_spi {masterSPI} {1}
set_instance_parameter_value rffe_spi {numberOfSlaves} {1}
set_instance_parameter_value rffe_spi {syncRegDepth} {2}
set_instance_parameter_value rffe_spi {targetClockRate} {40000000.0}
set_instance_parameter_value rffe_spi {targetSlaveSelectToSClkDelay} {0.0}

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

add_instance vctcxo_tamer_0 altera_avalon_onchip_memory2
set_instance_parameter_value vctcxo_tamer_0 {allowInSystemMemoryContentEditor} {0}
set_instance_parameter_value vctcxo_tamer_0 {blockType} {AUTO}
set_instance_parameter_value vctcxo_tamer_0 {dataWidth} {8}
set_instance_parameter_value vctcxo_tamer_0 {dataWidth2} {32}
set_instance_parameter_value vctcxo_tamer_0 {dualPort} {0}
set_instance_parameter_value vctcxo_tamer_0 {enableDiffWidth} {0}
set_instance_parameter_value vctcxo_tamer_0 {initMemContent} {0}
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

add_instance arbiter_0 arbiter 1.0
set_instance_parameter_value arbiter_0 {N} {2}

# exported interfaces
add_interface ad9361_adc_i0 conduit end
set_interface_property ad9361_adc_i0 EXPORT_OF axi_ad9361_0.fifo_ch_0_in
add_interface ad9361_adc_i1 conduit end
set_interface_property ad9361_adc_i1 EXPORT_OF axi_ad9361_0.fifo_ch_2_in
add_interface ad9361_adc_overflow conduit end
set_interface_property ad9361_adc_overflow EXPORT_OF axi_ad9361_0.if_adc_dovf
add_interface ad9361_adc_q0 conduit end
set_interface_property ad9361_adc_q0 EXPORT_OF axi_ad9361_0.fifo_ch_1_in
add_interface ad9361_adc_q1 conduit end
set_interface_property ad9361_adc_q1 EXPORT_OF axi_ad9361_0.fifo_ch_3_in
add_interface ad9361_adc_underflow conduit end
set_interface_property ad9361_adc_underflow EXPORT_OF axi_ad9361_0.if_adc_dunf
add_interface ad9361_dac_i0 conduit end
set_interface_property ad9361_dac_i0 EXPORT_OF axi_ad9361_0.fifo_ch_0_out
add_interface ad9361_dac_i1 conduit end
set_interface_property ad9361_dac_i1 EXPORT_OF axi_ad9361_0.fifo_ch_2_out
add_interface ad9361_dac_overflow conduit end
set_interface_property ad9361_dac_overflow EXPORT_OF axi_ad9361_0.if_dac_dovf
add_interface ad9361_dac_q0 conduit end
set_interface_property ad9361_dac_q0 EXPORT_OF axi_ad9361_0.fifo_ch_1_out
add_interface ad9361_dac_q1 conduit end
set_interface_property ad9361_dac_q1 EXPORT_OF axi_ad9361_0.fifo_ch_3_out
add_interface ad9361_dac_sync_in conduit end
set_interface_property ad9361_dac_sync_in EXPORT_OF axi_ad9361_0.if_dac_sync_in
add_interface ad9361_dac_sync_out conduit end
set_interface_property ad9361_dac_sync_out EXPORT_OF axi_ad9361_0.if_dac_sync_out
add_interface ad9361_dac_underflow conduit end
set_interface_property ad9361_dac_underflow EXPORT_OF axi_ad9361_0.if_dac_dunf
add_interface ad9361_data_clock clock source
set_interface_property ad9361_data_clock EXPORT_OF cb_ad9361_0_data.out_clk
add_interface ad9361_data_reset reset source
set_interface_property ad9361_data_reset EXPORT_OF rb_ad9361_0_data.out_reset
add_interface ad9361_device_if conduit end
set_interface_property ad9361_device_if EXPORT_OF axi_ad9361_0.device_if
add_interface clk clock sink
set_interface_property clk EXPORT_OF system_clock.clk_in
add_interface command conduit end
set_interface_property command EXPORT_OF command_uart.rs232
add_interface dac conduit end
set_interface_property dac EXPORT_OF peripheral_spi.external
add_interface gpio conduit end
set_interface_property gpio EXPORT_OF control.external_connection
add_interface gpio_rffe_0 conduit end
set_interface_property gpio_rffe_0 EXPORT_OF gpio_rffe_0.external_connection
add_interface oc_i2c conduit end
set_interface_property oc_i2c EXPORT_OF opencores_i2c.conduit_end
add_interface reset reset sink
set_interface_property reset EXPORT_OF system_clock.clk_in_reset
add_interface rx_tamer conduit end
set_interface_property rx_tamer EXPORT_OF rx_tamer.conduit_end
add_interface rx_trigger_ctl conduit end
set_interface_property rx_trigger_ctl EXPORT_OF rx_trigger_ctl.external_connection
add_interface spi conduit end
set_interface_property spi EXPORT_OF rffe_spi.external
add_interface tx_tamer conduit end
set_interface_property tx_tamer EXPORT_OF tx_tamer.conduit_end
add_interface tx_trigger_ctl conduit end
set_interface_property tx_trigger_ctl EXPORT_OF tx_trigger_ctl.external_connection
add_interface xb_gpio conduit end
set_interface_property xb_gpio EXPORT_OF xb_gpio.external_connection
add_interface xb_gpio_dir conduit end
set_interface_property xb_gpio_dir EXPORT_OF xb_gpio_dir.external_connection
add_interface arbiter conduit end
set_interface_property arbiter EXPORT_OF arbiter_0.conduit_end
add_interface wbm conduit end
set_interface_property wbm EXPORT_OF wishbone_master_0.conduit_end

# connections and connection parameters
add_connection axi_ad9361_0.if_l_clk axi_ad9361_0.device_clock

add_connection axi_ad9361_0.if_l_clk cb_ad9361_0_data.in_clk

add_connection axi_ad9361_0.if_l_clk rb_ad9361_0_data.clk

add_connection axi_ad9361_0.if_rst rb_ad9361_0_data.in_reset

add_connection ${cpu_dm} axi_ad9361_0.s_axi
set_connection_parameter_value ${cpu_dm}/axi_ad9361_0.s_axi arbitrationPriority {1}
set_connection_parameter_value ${cpu_dm}/axi_ad9361_0.s_axi baseAddress {0x00010000}
set_connection_parameter_value ${cpu_dm}/axi_ad9361_0.s_axi defaultConnection {0}

add_connection ${cpu_dm} command_uart.avalon_slave
set_connection_parameter_value ${cpu_dm}/command_uart.avalon_slave arbitrationPriority {1}
set_connection_parameter_value ${cpu_dm}/command_uart.avalon_slave baseAddress {0x9120}
set_connection_parameter_value ${cpu_dm}/command_uart.avalon_slave defaultConnection {0}

add_connection ${cpu_dm} control.s1
set_connection_parameter_value ${cpu_dm}/control.s1 arbitrationPriority {1}
set_connection_parameter_value ${cpu_dm}/control.s1 baseAddress {0x9040}
set_connection_parameter_value ${cpu_dm}/control.s1 defaultConnection {0}

add_connection ${cpu_dm} gpio_rffe_0.s1
set_connection_parameter_value ${cpu_dm}/gpio_rffe_0.s1 arbitrationPriority {1}
set_connection_parameter_value ${cpu_dm}/gpio_rffe_0.s1 baseAddress {0x9440}
set_connection_parameter_value ${cpu_dm}/gpio_rffe_0.s1 defaultConnection {0}

add_connection ${cpu_dm} jtag_uart.avalon_jtag_slave
set_connection_parameter_value ${cpu_dm}/jtag_uart.avalon_jtag_slave arbitrationPriority {1}
set_connection_parameter_value ${cpu_dm}/jtag_uart.avalon_jtag_slave baseAddress {0x9100}
set_connection_parameter_value ${cpu_dm}/jtag_uart.avalon_jtag_slave defaultConnection {0}

# CPU-internal agents on the data side (see cpu_dm_agents above)
foreach {agent base} $cpu_dm_agents {
    add_connection ${cpu_dm} ${agent}
    set_connection_parameter_value ${cpu_dm}/${agent} arbitrationPriority {1}
    set_connection_parameter_value ${cpu_dm}/${agent} baseAddress ${base}
    set_connection_parameter_value ${cpu_dm}/${agent} defaultConnection {0}
}

add_connection ${cpu_dm} opencores_i2c.bladerf_oc_i2c_master
set_connection_parameter_value ${cpu_dm}/opencores_i2c.bladerf_oc_i2c_master arbitrationPriority {1}
set_connection_parameter_value ${cpu_dm}/opencores_i2c.bladerf_oc_i2c_master baseAddress {0x90f0}
set_connection_parameter_value ${cpu_dm}/opencores_i2c.bladerf_oc_i2c_master defaultConnection {0}

add_connection ${cpu_dm} peripheral_spi.spi_control_port
set_connection_parameter_value ${cpu_dm}/peripheral_spi.spi_control_port arbitrationPriority {1}
set_connection_parameter_value ${cpu_dm}/peripheral_spi.spi_control_port baseAddress {0x9060}
set_connection_parameter_value ${cpu_dm}/peripheral_spi.spi_control_port defaultConnection {0}

add_connection ${cpu_dm} ram.s1
set_connection_parameter_value ${cpu_dm}/ram.s1 arbitrationPriority {1}
set_connection_parameter_value ${cpu_dm}/ram.s1 baseAddress ${ram_base_addr}
set_connection_parameter_value ${cpu_dm}/ram.s1 defaultConnection {0}

add_connection ${cpu_dm} rffe_spi.spi_control_port
set_connection_parameter_value ${cpu_dm}/rffe_spi.spi_control_port arbitrationPriority {1}
set_connection_parameter_value ${cpu_dm}/rffe_spi.spi_control_port baseAddress {0x9200}
set_connection_parameter_value ${cpu_dm}/rffe_spi.spi_control_port defaultConnection {0}

add_connection ${cpu_dm} rx_tamer.avalon_slave_0
set_connection_parameter_value ${cpu_dm}/rx_tamer.avalon_slave_0 arbitrationPriority {1}
set_connection_parameter_value ${cpu_dm}/rx_tamer.avalon_slave_0 baseAddress {0x9160}
set_connection_parameter_value ${cpu_dm}/rx_tamer.avalon_slave_0 defaultConnection {0}

add_connection ${cpu_dm} rx_trigger_ctl.s1
set_connection_parameter_value ${cpu_dm}/rx_trigger_ctl.s1 arbitrationPriority {1}
set_connection_parameter_value ${cpu_dm}/rx_trigger_ctl.s1 baseAddress {0x9400}
set_connection_parameter_value ${cpu_dm}/rx_trigger_ctl.s1 defaultConnection {0}

add_connection ${cpu_dm} tx_tamer.avalon_slave_0
set_connection_parameter_value ${cpu_dm}/tx_tamer.avalon_slave_0 arbitrationPriority {1}
set_connection_parameter_value ${cpu_dm}/tx_tamer.avalon_slave_0 baseAddress {0x9140}
set_connection_parameter_value ${cpu_dm}/tx_tamer.avalon_slave_0 defaultConnection {0}

add_connection ${cpu_dm} tx_trigger_ctl.s1
set_connection_parameter_value ${cpu_dm}/tx_trigger_ctl.s1 arbitrationPriority {1}
set_connection_parameter_value ${cpu_dm}/tx_trigger_ctl.s1 baseAddress {0x9420}
set_connection_parameter_value ${cpu_dm}/tx_trigger_ctl.s1 defaultConnection {0}

add_connection ${cpu_dm} vctcxo_tamer_0.s1
set_connection_parameter_value ${cpu_dm}/vctcxo_tamer_0.s1 arbitrationPriority {1}
set_connection_parameter_value ${cpu_dm}/vctcxo_tamer_0.s1 baseAddress {0x9300}
set_connection_parameter_value ${cpu_dm}/vctcxo_tamer_0.s1 defaultConnection {0}

add_connection ${cpu_dm} wishbone_master_0.avalon_slave_0 avalon
set_connection_parameter_value ${cpu_dm}/wishbone_master_0.avalon_slave_0 arbitrationPriority {1}
set_connection_parameter_value ${cpu_dm}/wishbone_master_0.avalon_slave_0 baseAddress {0x10000000}
set_connection_parameter_value ${cpu_dm}/wishbone_master_0.avalon_slave_0 defaultConnection {0}

add_connection ${cpu_dm} xb_gpio.s1
set_connection_parameter_value ${cpu_dm}/xb_gpio.s1 arbitrationPriority {1}
set_connection_parameter_value ${cpu_dm}/xb_gpio.s1 baseAddress {0x90b0}
set_connection_parameter_value ${cpu_dm}/xb_gpio.s1 defaultConnection {0}

add_connection ${cpu_dm} xb_gpio_dir.s1
set_connection_parameter_value ${cpu_dm}/xb_gpio_dir.s1 arbitrationPriority {1}
set_connection_parameter_value ${cpu_dm}/xb_gpio_dir.s1 baseAddress {0x90a0}
set_connection_parameter_value ${cpu_dm}/xb_gpio_dir.s1 defaultConnection {0}

add_connection ${cpu_dm} arbiter_0.avalon_slave_0 avalon
set_connection_parameter_value ${cpu_dm}/arbiter_0.avalon_slave_0 arbitrationPriority {1}
set_connection_parameter_value ${cpu_dm}/arbiter_0.avalon_slave_0 baseAddress {0x9500}
set_connection_parameter_value ${cpu_dm}/arbiter_0.avalon_slave_0 defaultConnection {0}

# CPU-internal agents on the instruction side (see cpu_im_agents above)
foreach {agent base} $cpu_im_agents {
    add_connection ${cpu_im} ${agent}
    set_connection_parameter_value ${cpu_im}/${agent} arbitrationPriority {1}
    set_connection_parameter_value ${cpu_im}/${agent} baseAddress ${base}
    set_connection_parameter_value ${cpu_im}/${agent} defaultConnection {0}
}

add_connection ${cpu_im} ram.s1
set_connection_parameter_value ${cpu_im}/ram.s1 arbitrationPriority {1}
set_connection_parameter_value ${cpu_im}/ram.s1 baseAddress ${ram_base_addr}
set_connection_parameter_value ${cpu_im}/ram.s1 defaultConnection {0}

add_connection ${cpu_irq} command_uart.interrupt
set_connection_parameter_value ${cpu_irq}/command_uart.interrupt irqNumber {7}

add_connection ${cpu_irq} arbiter_0.interrupt_sender interrupt
set_connection_parameter_value ${cpu_irq}/arbiter_0.interrupt_sender irqNumber {6}

add_connection ${cpu_irq} gpio_rffe_0.irq
set_connection_parameter_value ${cpu_irq}/gpio_rffe_0.irq irqNumber {9}

add_connection ${cpu_irq} jtag_uart.irq
set_connection_parameter_value ${cpu_irq}/jtag_uart.irq irqNumber {1}

add_connection ${cpu_irq} opencores_i2c.interrupt_sender
set_connection_parameter_value ${cpu_irq}/opencores_i2c.interrupt_sender irqNumber {5}

add_connection ${cpu_irq} peripheral_spi.irq
set_connection_parameter_value ${cpu_irq}/peripheral_spi.irq irqNumber {4}

add_connection ${cpu_irq} rffe_spi.irq
set_connection_parameter_value ${cpu_irq}/rffe_spi.irq irqNumber {8}

add_connection ${cpu_irq} rx_tamer.interrupt_sender
set_connection_parameter_value ${cpu_irq}/rx_tamer.interrupt_sender irqNumber {3}

add_connection ${cpu_irq} wishbone_master_0.interrupt_sender interrupt
set_connection_parameter_value ${cpu_irq}/wishbone_master_0.interrupt_sender irqNumber {10}

add_connection ${cpu_irq} tx_tamer.interrupt_sender
set_connection_parameter_value ${cpu_irq}/tx_tamer.interrupt_sender irqNumber {2}

add_connection system_clock.clk axi_ad9361_0.delay_clock

add_connection system_clock.clk axi_ad9361_0.s_axi_clock

add_connection system_clock.clk command_uart.clock

add_connection system_clock.clk control.clk

add_connection system_clock.clk gpio_rffe_0.clk

add_connection system_clock.clk jtag_uart.clk

add_connection system_clock.clk ${cpu}.clk

add_connection system_clock.clk opencores_i2c.clock_sink

add_connection system_clock.clk peripheral_spi.clk

add_connection system_clock.clk ram.clk1

add_connection system_clock.clk rffe_spi.clk

add_connection system_clock.clk rx_tamer.clock_sink

add_connection system_clock.clk rx_trigger_ctl.clk

add_connection system_clock.clk tx_tamer.clock_sink

add_connection system_clock.clk tx_trigger_ctl.clk

add_connection system_clock.clk vctcxo_tamer_0.clk1

add_connection system_clock.clk xb_gpio.clk

add_connection system_clock.clk xb_gpio_dir.clk

add_connection system_clock.clk arbiter_0.clock_sink clock

add_connection system_clock.clk wishbone_master_0.clock_sink clock

add_connection system_clock.clk_reset axi_ad9361_0.s_axi_reset

add_connection system_clock.clk_reset command_uart.reset

add_connection system_clock.clk_reset control.reset

add_connection system_clock.clk_reset gpio_rffe_0.reset

add_connection system_clock.clk_reset jtag_uart.reset

add_connection system_clock.clk_reset ${cpu}.reset

add_connection system_clock.clk_reset opencores_i2c.reset_sink

add_connection system_clock.clk_reset peripheral_spi.reset

add_connection system_clock.clk_reset ram.reset1

add_connection system_clock.clk_reset rffe_spi.reset

add_connection system_clock.clk_reset rx_tamer.reset

add_connection system_clock.clk_reset rx_trigger_ctl.reset

add_connection system_clock.clk_reset tx_tamer.reset

add_connection system_clock.clk_reset tx_trigger_ctl.reset

add_connection system_clock.clk_reset vctcxo_tamer_0.reset1

add_connection system_clock.clk_reset xb_gpio.reset

add_connection system_clock.clk_reset xb_gpio_dir.reset

add_connection system_clock.clk_reset arbiter_0.reset reset

add_connection system_clock.clk_reset wishbone_master_0.reset reset

# interconnect requirements
set_interconnect_requirement {$system} {qsys_mm.clockCrossingAdapter} {HANDSHAKE}
set_interconnect_requirement {$system} {qsys_mm.maxAdditionalLatency} {1}
set_interconnect_requirement {$system} {qsys_mm.enableEccProtection} {FALSE}
set_interconnect_requirement {$system} {qsys_mm.insertDefaultSlave} {FALSE}

if { $platform_revision == "foxhunt" } {
    puts "Adding tone_generator..."
    add_instance tone_generator_0 tone_generator 1.0
    set_instance_parameter_value tone_generator_0 {ADDR_WIDTH} {2}
    set_instance_parameter_value tone_generator_0 {DATA_WIDTH} {32}
    set_instance_parameter_value tone_generator_0 {QUEUE_LENGTH} {8}

    add_interface tonegen_sample conduit end
    set_interface_property tonegen_sample EXPORT_OF tone_generator_0.samplestream
    add_interface tonegen_status conduit end
    set_interface_property tonegen_status EXPORT_OF tone_generator_0.status

    add_connection system_clock.clk tone_generator_0.clock_sink
    add_connection system_clock.clk_reset tone_generator_0.reset
    add_connection ${cpu_dm} tone_generator_0.avalon_slave_0
    set_connection_parameter_value ${cpu_dm}/tone_generator_0.avalon_slave_0 arbitrationPriority {1}
    set_connection_parameter_value ${cpu_dm}/tone_generator_0.avalon_slave_0 baseAddress {0x9220}
    set_connection_parameter_value ${cpu_dm}/tone_generator_0.avalon_slave_0 defaultConnection {0}
}

save_system {nios_system.qsys}
