

package require -exact qsys 13.0
source ../scripts/adi_env.tcl
source ../scripts/adi_ip_alt.tcl

set_module_property NAME axi_ad9361
set_module_property DESCRIPTION "AXI AD9361 Interface"
set_module_property VERSION 1.0
set_module_property GROUP "Analog Devices"
set_module_property DISPLAY_NAME axi_ad9361

# files

add_fileset quartus_synth QUARTUS_SYNTH "" "Quartus Synthesis"
set_fileset_property quartus_synth TOP_LEVEL axi_ad9361
add_fileset_file axi_ad9361.qip OTHER PATH ./axi_ad9361.qip

# parameters

add_parameter ID INTEGER 0
set_parameter_property ID DEFAULT_VALUE 0
set_parameter_property ID DISPLAY_NAME ID
set_parameter_property ID TYPE INTEGER
set_parameter_property ID UNITS None
set_parameter_property ID HDL_PARAMETER true

add_parameter DEVICE_TYPE INTEGER 0
set_parameter_property DEVICE_TYPE DEFAULT_VALUE 0
set_parameter_property DEVICE_TYPE DISPLAY_NAME DEVICE_TYPE
set_parameter_property DEVICE_TYPE TYPE INTEGER
set_parameter_property DEVICE_TYPE UNITS None
set_parameter_property DEVICE_TYPE HDL_PARAMETER true

add_parameter DAC_DDS_DISABLE INTEGER 0
set_parameter_property DAC_DDS_DISABLE DEFAULT_VALUE 0
set_parameter_property DAC_DDS_DISABLE DISPLAY_NAME DAC_DDS_DISABLE
set_parameter_property DAC_DDS_DISABLE TYPE INTEGER
set_parameter_property DAC_DDS_DISABLE UNITS None
set_parameter_property DAC_DDS_DISABLE HDL_PARAMETER true

# axi4 slave

add_interface s_axi_clock clock end
add_interface_port s_axi_clock s_axi_aclk clk Input 1

add_interface s_axi_reset reset end
set_interface_property s_axi_reset associatedClock s_axi_clock
add_interface_port s_axi_reset s_axi_aresetn reset_n Input 1

add_interface s_axi axi4lite end
set_interface_property s_axi associatedClock s_axi_clock
set_interface_property s_axi associatedReset s_axi_reset
add_interface_port s_axi s_axi_awvalid awvalid Input 1
add_interface_port s_axi s_axi_awaddr awaddr Input 16
add_interface_port s_axi s_axi_awprot awprot Input 3
add_interface_port s_axi s_axi_awready awready Output 1
add_interface_port s_axi s_axi_wvalid wvalid Input 1
add_interface_port s_axi s_axi_wdata wdata Input 32
add_interface_port s_axi s_axi_wstrb wstrb Input 4
add_interface_port s_axi s_axi_wready wready Output 1
add_interface_port s_axi s_axi_bvalid bvalid Output 1
add_interface_port s_axi s_axi_bresp bresp Output 2
add_interface_port s_axi s_axi_bready bready Input 1
add_interface_port s_axi s_axi_arvalid arvalid Input 1
add_interface_port s_axi s_axi_araddr araddr Input 16
add_interface_port s_axi s_axi_arprot arprot Input 3
add_interface_port s_axi s_axi_arready arready Output 1
add_interface_port s_axi s_axi_rvalid rvalid Output 1
add_interface_port s_axi s_axi_rresp rresp Output 2
add_interface_port s_axi s_axi_rdata rdata Output 32
add_interface_port s_axi s_axi_rready rready Input 1

# device interface

add_interface device_clock clock end
add_interface_port device_clock clk clk Input 1

add_interface device_if conduit end
set_interface_property device_if associatedClock device_clock
add_interface_port device_if rx_clk_in_p rx_clk_in_p Input 1
add_interface_port device_if rx_clk_in_n rx_clk_in_n Input 1
add_interface_port device_if rx_frame_in_p rx_frame_in_p Input 1
add_interface_port device_if rx_frame_in_n rx_frame_in_n Input 1
add_interface_port device_if rx_data_in_p rx_data_in_p Input 6
add_interface_port device_if rx_data_in_n rx_data_in_n Input 6
add_interface_port device_if tx_clk_out_p tx_clk_out_p Output 1
add_interface_port device_if tx_clk_out_n tx_clk_out_n Output 1
add_interface_port device_if tx_frame_out_p tx_frame_out_p Output 1
add_interface_port device_if tx_frame_out_n tx_frame_out_n Output 1
add_interface_port device_if tx_data_out_p tx_data_out_p Output 6
add_interface_port device_if tx_data_out_n tx_data_out_n Output 6

ad_alt_intf signal  dac_sync_in     input   1   sync
ad_alt_intf signal  dac_sync_out    output  1   sync

ad_alt_intf clock   l_clk           output  1
ad_alt_intf reset   rst             output  1   if_l_clk

add_interface fifo_ch_0_in conduit end
#set_interface_property fifo_ch_0_in associatedClock if_l_clk
add_interface_port fifo_ch_0_in  adc_enable_i0 enable   Output  1
add_interface_port fifo_ch_0_in  adc_valid_i0  valid    Output  1
add_interface_port fifo_ch_0_in  adc_data_i0   data     Output  16

add_interface fifo_ch_1_in conduit end
#set_interface_property fifo_ch_1_in associatedClock if_l_clk
add_interface_port fifo_ch_1_in  adc_enable_q0 enable   Output  1
add_interface_port fifo_ch_1_in  adc_valid_q0  valid    Output  1
add_interface_port fifo_ch_1_in  adc_data_q0   data     Output  16

add_interface fifo_ch_2_in conduit end
#set_interface_property fifo_ch_2_in associatedClock if_l_clk
add_interface_port fifo_ch_2_in  adc_enable_i1 enable   Output  1
add_interface_port fifo_ch_2_in  adc_valid_i1  valid    Output  1
add_interface_port fifo_ch_2_in  adc_data_i1   data     Output  16

add_interface fifo_ch_3_in conduit end
#set_interface_property fifo_ch_3_in associatedClock if_l_clk
add_interface_port fifo_ch_3_in  adc_enable_q1 enable   Output  1
add_interface_port fifo_ch_3_in  adc_valid_q1  valid    Output  1
add_interface_port fifo_ch_3_in  adc_data_q1   data     Output  16

ad_alt_intf signal  adc_dovf        input  1   ovf
ad_alt_intf signal  adc_dunf        input  1   unf

add_interface fifo_ch_0_out conduit end
#set_interface_property fifo_ch_0_out associatedClock if_l_clk
add_interface_port fifo_ch_0_out  dac_enable_i0 enable   Output  1
add_interface_port fifo_ch_0_out  dac_valid_i0  valid    Output  1
add_interface_port fifo_ch_0_out  dac_data_i0   data     Input   16

add_interface fifo_ch_1_out conduit end
#set_interface_property fifo_ch_1_out associatedClock if_l_clk
add_interface_port fifo_ch_1_out  dac_enable_q0 enable   Output  1
add_interface_port fifo_ch_1_out  dac_valid_q0  valid    Output  1
add_interface_port fifo_ch_1_out  dac_data_q0   data     Input   16

add_interface fifo_ch_2_out conduit end
#set_interface_property fifo_ch_2_out associatedClock if_l_clk
add_interface_port fifo_ch_2_out  dac_enable_i1 enable   Output  1
add_interface_port fifo_ch_2_out  dac_valid_i1  valid    Output  1
add_interface_port fifo_ch_2_out  dac_data_i1   data     Input   16

add_interface fifo_ch_3_out conduit end
#set_interface_property fifo_ch_3_out associatedClock if_l_clk
add_interface_port fifo_ch_3_out  dac_enable_q1 enable   Output  1
add_interface_port fifo_ch_3_out  dac_valid_q1  valid    Output  1
add_interface_port fifo_ch_3_out  dac_data_q1   data     Input   16

ad_alt_intf signal  dac_dovf       input   1 ovf
ad_alt_intf signal  dac_dunf       input   1 unf

add_interface delay_clock clock end
add_interface_port delay_clock delay_clk clk Input 1

