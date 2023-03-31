onerror {resume}
quietly WaveActivateNextPane {} 0
add wave -noupdate -color Pink /rx_timestamp_tb/U_rx/U_fifo_writer/eight_bit_mode_en
add wave -noupdate /rx_timestamp_tb/U_fx3_gpif/pclk
add wave -noupdate /rx_timestamp_tb/U_fx3_gpif/gpif_out
add wave -noupdate /rx_timestamp_tb/U_fx3_gpif/gpif_oe
add wave -noupdate /rx_timestamp_tb/U_rx/U_fifo_writer/clock
add wave -noupdate -radix decimal /rx_timestamp_tb/U_rx/U_fifo_writer/timestamp
add wave -noupdate -expand -subitemconfig {/rx_timestamp_tb/U_rx/U_fifo_writer/meta_current.dma_downcount {-color {Cornflower Blue} -height 17} /rx_timestamp_tb/U_rx/U_fifo_writer/meta_current.meta_write {-color Salmon -height 17}} /rx_timestamp_tb/U_rx/U_fifo_writer/meta_current
add wave -noupdate -expand -subitemconfig {/rx_timestamp_tb/U_rx/U_fifo_writer/fifo_current.fifo_write {-color {Steel Blue} -height 17} /rx_timestamp_tb/U_rx/U_fifo_writer/fifo_current.samples_left {-color Gold} /rx_timestamp_tb/U_rx/U_fifo_writer/fifo_current.eight_bit_delay {-color Gold -height 17}} /rx_timestamp_tb/U_rx/U_fifo_writer/fifo_current
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/pclk
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/reset
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/usb_speed
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/gpif_in
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/ctl_in
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/meta_enable
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/packet_enable
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/tx_fifo_full
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/tx_fifo_empty
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/tx_fifo_usedw
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/tx_timestamp
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/tx_meta_fifo_full
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/tx_meta_fifo_empty
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/tx_meta_fifo_usedw
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/rx_fifo_full
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/rx_fifo_empty
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/rx_fifo_usedw
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/rx_fifo_data
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/rx_meta_fifo_full
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/rx_meta_fifo_empty
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/rx_meta_fifo_usedr
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/rx_meta_fifo_data
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/current
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/future
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/can_rx
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/should_rx
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/can_tx
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/should_tx
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/rx_fifo_enough
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/tx_fifo_enough
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/rx_fifo_critical
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/underrun
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/dma_req
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/gpif_buf_size
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/dma3_tx_reqx
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/dma2_tx_reqx
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/dma1_rx_reqx
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/dma0_rx_reqx
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/dma_idle
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/dma_tx_enable
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/dma_rx_enable
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/dma3_tx_ack
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/dma2_tx_ack
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/dma1_rx_ack
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/dma0_rx_ack
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/gpif_out
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/gpif_oe
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/ctl_out
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/ctl_oe
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/tx_enable
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/rx_enable
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/tx_fifo_write
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/tx_fifo_data
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/tx_meta_fifo_write
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/tx_meta_fifo_data
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/rx_fifo_read
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/rx_meta_fifo_read
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/pclk
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/reset
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/usb_speed
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/gpif_in
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/ctl_in
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/meta_enable
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/packet_enable
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/tx_fifo_full
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/tx_fifo_empty
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/tx_fifo_usedw
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/tx_timestamp
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/tx_meta_fifo_full
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/tx_meta_fifo_empty
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/tx_meta_fifo_usedw
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/rx_fifo_full
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/rx_fifo_empty
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/rx_fifo_usedw
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/rx_fifo_data
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/rx_meta_fifo_full
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/rx_meta_fifo_empty
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/rx_meta_fifo_usedr
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/rx_meta_fifo_data
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/current
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/future
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/can_rx
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/should_rx
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/can_tx
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/should_tx
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/rx_fifo_enough
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/tx_fifo_enough
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/rx_fifo_critical
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/underrun
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/dma_req
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/gpif_buf_size
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/dma3_tx_reqx
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/dma2_tx_reqx
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/dma1_rx_reqx
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/dma0_rx_reqx
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/dma_idle
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/dma_tx_enable
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/dma_rx_enable
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/dma3_tx_ack
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/dma2_tx_ack
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/dma1_rx_ack
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/dma0_rx_ack
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/gpif_out
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/gpif_oe
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/ctl_out
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/ctl_oe
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/tx_enable
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/rx_enable
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/tx_fifo_write
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/tx_fifo_data
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/tx_meta_fifo_write
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/tx_meta_fifo_data
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/rx_fifo_read
add wave -noupdate -group fx3_gpif /rx_timestamp_tb/U_fx3_gpif/rx_meta_fifo_read
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/clock
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/NUM_STREAMS
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/FIFO_USEDW_WIDTH
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/FIFO_DATA_WIDTH
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/META_FIFO_USEDW_WIDTH
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/META_FIFO_DATA_WIDTH
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/reset
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/enable
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/usb_speed
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/meta_en
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/packet_en
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/eight_bit_mode_en
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/timestamp
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/mini_exp
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/in_sample_controls
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/in_samples
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/fifo_usedw
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/fifo_full
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/packet_control
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/meta_fifo_full
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/meta_fifo_usedw
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/overflow_duration
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/dma_buf_size
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/fifo_enough
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/overflow_detected
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/meta_current
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/meta_future
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/fifo_current
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/fifo_future
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/sync_mini_exp
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/DMA_BUF_SIZE_SS
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/DMA_BUF_SIZE_HS
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/META_FSM_RESET_VALUE
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/FIFO_FSM_RESET_VALUE
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/fifo_clear
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/fifo_write
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/fifo_data
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/packet_ready
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/meta_fifo_data
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/meta_fifo_write
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/overflow_led
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/overflow_count
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/clock
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/NUM_STREAMS
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/FIFO_USEDW_WIDTH
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/FIFO_DATA_WIDTH
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/META_FIFO_USEDW_WIDTH
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/META_FIFO_DATA_WIDTH
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/reset
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/enable
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/usb_speed
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/meta_en
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/packet_en
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/eight_bit_mode_en
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/timestamp
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/mini_exp
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/in_sample_controls
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/in_samples
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/fifo_usedw
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/fifo_full
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/packet_control
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/meta_fifo_full
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/meta_fifo_usedw
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/overflow_duration
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/dma_buf_size
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/fifo_enough
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/overflow_detected
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/meta_current
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/meta_future
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/fifo_current
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/fifo_future
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/sync_mini_exp
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/DMA_BUF_SIZE_SS
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/DMA_BUF_SIZE_HS
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/META_FSM_RESET_VALUE
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/FIFO_FSM_RESET_VALUE
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/fifo_clear
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/fifo_write
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/fifo_data
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/packet_ready
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/meta_fifo_data
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/meta_fifo_write
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/overflow_led
add wave -noupdate -expand -group fifo_writer /rx_timestamp_tb/U_rx/U_fifo_writer/overflow_count
TreeUpdate [SetDefaultTree]
WaveRestoreCursors {{Cursor 11} {42111448 ps} 1} {{Cursor 2} {42555896 ps} 1} {{Meta_wr expected} {267335472 ps} 1} {{Cursor 4} {492979686 ps} 0}
quietly wave cursor active 4
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
WaveRestoreZoom {492180935 ps} {508008039 ps}
