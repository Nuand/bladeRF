onerror {resume}
quietly WaveActivateNextPane {} 0
add wave -noupdate /rx_counter_8bit_tb/U_fx3_gpif/gpif_oe
add wave -noupdate /rx_counter_8bit_tb/U_fx3_gpif/gpif_out
add wave -noupdate -radix decimal -expand -subitemconfig {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0) {-height 17 -expand} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i {-height 17 -childformat {{/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(15) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(14) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(13) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(12) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(11) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(10) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(9) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(8) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(7) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(6) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(5) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(4) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(3) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(2) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(1) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(0) -radix decimal}}} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(15) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(14) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(13) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(12) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(11) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(10) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(9) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(8) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(7) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(6) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(5) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(4) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(3) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(2) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(1) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(0) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q {-height 17 -childformat {{/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(15) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(14) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(13) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(12) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(11) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(10) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(9) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(8) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(7) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(6) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(5) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(4) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(3) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(2) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(1) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(0) -radix decimal}}} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(15) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(14) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(13) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(12) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(11) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(10) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(9) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(8) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(7) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(6) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(5) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(4) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(3) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(2) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(1) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q(0) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_v {-height 17} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(1) {-height 17 -expand} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(1).data_i {-height 17} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(1).data_q {-height 17} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(1).data_v {-height 17}} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples
add wave -noupdate /rx_counter_8bit_tb/U_rx/U_fifo_writer/fifo_data
add wave -noupdate -group {fx3 model} /rx_counter_8bit_tb/U_fx3_model/gpif_state_rx
add wave -noupdate -group {fx3 model} /rx_counter_8bit_tb/U_fx3_model/gpif_state_tx
add wave -noupdate -group {fx3 model} /rx_counter_8bit_tb/U_fx3_model/fx3_tx_meta_en
add wave -noupdate -group {fx3 model} /rx_counter_8bit_tb/U_fx3_model/fx3_rx_meta_en
add wave -noupdate -group {fx3 model} /rx_counter_8bit_tb/U_fx3_model/fx3_rx_en
add wave -noupdate -group {fx3 model} /rx_counter_8bit_tb/U_fx3_model/fx3_uart_rxd
add wave -noupdate -group {fx3 model} /rx_counter_8bit_tb/U_fx3_model/fx3_tx_en
add wave -noupdate -group {fx3 model} /rx_counter_8bit_tb/U_fx3_model/fx3_ctl
add wave -noupdate -group {fx3 model} /rx_counter_8bit_tb/U_fx3_model/fx3_gpif
add wave -noupdate -group {fx3 model} /rx_counter_8bit_tb/U_fx3_model/fx3_pclk
add wave -noupdate -group {fx3 model} /rx_counter_8bit_tb/U_fx3_model/done
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/pclk
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/current
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/ctl_in
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/gpif_in
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/meta_enable
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/packet_enable
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/reset
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/rx_fifo_data
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/rx_fifo_empty
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/rx_fifo_full
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/rx_fifo_usedw
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/rx_meta_fifo_data
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/rx_meta_fifo_empty
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/rx_meta_fifo_full
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/rx_meta_fifo_usedr
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/tx_fifo_empty
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/tx_fifo_full
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/tx_fifo_usedw
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/tx_meta_fifo_empty
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/tx_meta_fifo_full
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/tx_meta_fifo_usedw
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/tx_timestamp
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/usb_speed
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/ctl_oe
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/ctl_out
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/gpif_oe
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/gpif_out
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/rx_enable
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/rx_fifo_read
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/rx_meta_fifo_read
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/tx_enable
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/tx_fifo_data
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/tx_fifo_write
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/tx_meta_fifo_data
add wave -noupdate -group {fx3 gpif} /rx_counter_8bit_tb/U_fx3_gpif/tx_meta_fifo_write
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/rdclk
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/rdreq
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/q
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/ADD_RAM_OUTPUT_REGISTER
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/ADD_USEDW_MSB_BIT
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/CLOCKS_ARE_SYNCHRONIZED
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/DELAY_RDUSEDW
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/DELAY_WRUSEDW
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/INTENDED_DEVICE_FAMILY
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/LPM_NUMWORDS
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/LPM_SHOWAHEAD
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/LPM_WIDTH
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/LPM_WIDTH_R
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/OVERFLOW_CHECKING
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/RDSYNC_DELAYPIPE
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/READ_ACLR_SYNCH
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/UNDERFLOW_CHECKING
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/USE_EAB
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/WRITE_ACLR_SYNCH
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/WRSYNC_DELAYPIPE
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/aclr
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/data
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/wrclk
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/wrreq
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/rdempty
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/rdfull
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/rdusedw
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/wrempty
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/wrfull
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/wrusedw
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/rdclk
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/rdreq
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/q
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/ADD_RAM_OUTPUT_REGISTER
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/ADD_USEDW_MSB_BIT
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/CLOCKS_ARE_SYNCHRONIZED
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/DELAY_RDUSEDW
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/DELAY_WRUSEDW
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/INTENDED_DEVICE_FAMILY
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/LPM_NUMWORDS
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/LPM_SHOWAHEAD
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/LPM_WIDTH
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/LPM_WIDTH_R
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/OVERFLOW_CHECKING
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/RDSYNC_DELAYPIPE
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/READ_ACLR_SYNCH
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/UNDERFLOW_CHECKING
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/USE_EAB
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/WRITE_ACLR_SYNCH
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/WRSYNC_DELAYPIPE
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/aclr
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/data
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/wrclk
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/wrreq
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/rdempty
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/rdfull
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/rdusedw
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/wrempty
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/wrfull
add wave -noupdate -group {tx sample fifo} /rx_counter_8bit_tb/U_tx/U_tx_sample_fifo/wrusedw
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/clock
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/eight_bit_mode_en
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/enable
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/FIFO_DATA_WIDTH
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/fifo_full
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/fifo_usedw
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/FIFO_USEDW_WIDTH
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_sample_controls
add wave -noupdate -expand -group {fifo writer} -radix decimal -childformat {{/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0) -radix decimal -childformat {{/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i -radix decimal -childformat {{/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(15) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(14) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(13) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(12) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(11) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(10) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(9) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(8) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(7) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(6) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(5) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(4) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(3) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(2) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(1) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(0) -radix decimal}}} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_v -radix decimal}}} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(1) -radix decimal -childformat {{/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(1).data_i -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(1).data_q -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(1).data_v -radix decimal}}}} -expand -subitemconfig {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0) {-height 17 -radix decimal -childformat {{/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i -radix decimal -childformat {{/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(15) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(14) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(13) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(12) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(11) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(10) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(9) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(8) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(7) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(6) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(5) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(4) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(3) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(2) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(1) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(0) -radix decimal}}} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_v -radix decimal}} -expand} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i {-height 17 -radix decimal -childformat {{/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(15) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(14) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(13) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(12) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(11) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(10) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(9) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(8) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(7) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(6) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(5) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(4) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(3) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(2) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(1) -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(0) -radix decimal}}} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(15) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(14) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(13) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(12) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(11) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(10) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(9) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(8) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(7) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(6) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(5) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(4) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(3) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(2) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(1) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_i(0) {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_q {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(0).data_v {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(1) {-height 17 -radix decimal -childformat {{/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(1).data_i -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(1).data_q -radix decimal} {/rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(1).data_v -radix decimal}} -expand} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(1).data_i {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(1).data_q {-height 17 -radix decimal} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples(1).data_v {-height 17 -radix decimal}} /rx_counter_8bit_tb/U_rx/U_fifo_writer/in_samples
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/meta_en
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/META_FIFO_DATA_WIDTH
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/meta_fifo_full
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/meta_fifo_usedw
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/META_FIFO_USEDW_WIDTH
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/mini_exp
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/NUM_STREAMS
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/overflow_duration
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/packet_control
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/packet_en
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/reset
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/timestamp
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/usb_speed
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/fifo_clear
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/fifo_data
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/fifo_write
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/meta_fifo_data
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/meta_fifo_write
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/overflow_count
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/overflow_led
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/packet_ready
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/dma_buf_size
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/DMA_BUF_SIZE_HS
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/DMA_BUF_SIZE_SS
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/fifo_current
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/fifo_enough
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/FIFO_FSM_RESET_VALUE
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/fifo_future
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/meta_current
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/META_FSM_RESET_VALUE
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/meta_future
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/overflow_detected
add wave -noupdate -expand -group {fifo writer} /rx_counter_8bit_tb/U_rx/U_fifo_writer/sync_mini_exp
TreeUpdate [SetDefaultTree]
WaveRestoreCursors {{GPIF start} {269209329 ps} 1} {in_samples {41444776 ps} 1} {{Cursor 5} {723894680 ps} 0}
quietly wave cursor active 3
configure wave -namecolwidth 165
configure wave -valuecolwidth 106
configure wave -justifyvalue left
configure wave -signalnamewidth 1
configure wave -snapdistance 10
configure wave -datasetprefix 0
configure wave -rowmargin 4
configure wave -childrowmargin 2
configure wave -gridoffset 0
configure wave -gridperiod 1
configure wave -griddelta 40
configure wave -timeline 0
configure wave -timelineunits us
update
WaveRestoreZoom {0 ps} {1016086528 ps}
