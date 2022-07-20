onerror {resume}
quietly WaveActivateNextPane {} 0
add wave -noupdate -expand -group Focus /fx3_gpif_meta_8bit_tb/U_fx3_gpif/pclk
add wave -noupdate -expand -group Focus /fx3_gpif_meta_8bit_tb/U_fx3_gpif/gpif_in
add wave -noupdate -expand -group Focus /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/clock
add wave -noupdate -expand -group Focus /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/fifo_read
add wave -noupdate -expand -group Focus /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/fifo_current.eight_bit_sample_sel
add wave -noupdate -expand -group Focus -label out_samples /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/fifo_future.out_samples
add wave -noupdate -expand -group Focus /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/out_samples
add wave -noupdate -expand -group Focus /fx3_gpif_meta_8bit_tb/U_fx3_gpif/pclk
add wave -noupdate -expand -group Focus /fx3_gpif_meta_8bit_tb/U_fx3_gpif/gpif_in
add wave -noupdate -expand -group Focus /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/clock
add wave -noupdate -expand -group Focus /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/fifo_read
add wave -noupdate -expand -group Focus /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/fifo_current.eight_bit_sample_sel
add wave -noupdate -expand -group Focus -label out_samples -expand -subitemconfig {/fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/fifo_future.out_samples(0) -expand /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/fifo_future.out_samples(1) -expand} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/fifo_future.out_samples
add wave -noupdate -expand -group Focus /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/out_samples
add wave -noupdate -group {fx3 model} /fx3_gpif_meta_8bit_tb/U_fx3_model/gpif_state_rx
add wave -noupdate -group {fx3 model} /fx3_gpif_meta_8bit_tb/U_fx3_model/gpif_state_tx
add wave -noupdate -group {fx3 model} /fx3_gpif_meta_8bit_tb/U_fx3_model/fx3_tx_meta_en
add wave -noupdate -group {fx3 model} /fx3_gpif_meta_8bit_tb/U_fx3_model/fx3_rx_meta_en
add wave -noupdate -group {fx3 model} /fx3_gpif_meta_8bit_tb/U_fx3_model/fx3_rx_en
add wave -noupdate -group {fx3 model} /fx3_gpif_meta_8bit_tb/U_fx3_model/fx3_uart_rxd
add wave -noupdate -group {fx3 model} /fx3_gpif_meta_8bit_tb/U_fx3_model/fx3_tx_en
add wave -noupdate -group {fx3 model} /fx3_gpif_meta_8bit_tb/U_fx3_model/fx3_ctl
add wave -noupdate -group {fx3 model} /fx3_gpif_meta_8bit_tb/U_fx3_model/fx3_gpif
add wave -noupdate -group {fx3 model} /fx3_gpif_meta_8bit_tb/U_fx3_model/fx3_pclk
add wave -noupdate -group {fx3 model} /fx3_gpif_meta_8bit_tb/U_fx3_model/done
add wave -noupdate -group {fx3 model} /fx3_gpif_meta_8bit_tb/U_fx3_model/gpif_state_rx
add wave -noupdate -group {fx3 model} /fx3_gpif_meta_8bit_tb/U_fx3_model/gpif_state_tx
add wave -noupdate -group {fx3 model} /fx3_gpif_meta_8bit_tb/U_fx3_model/fx3_tx_meta_en
add wave -noupdate -group {fx3 model} /fx3_gpif_meta_8bit_tb/U_fx3_model/fx3_rx_meta_en
add wave -noupdate -group {fx3 model} /fx3_gpif_meta_8bit_tb/U_fx3_model/fx3_rx_en
add wave -noupdate -group {fx3 model} /fx3_gpif_meta_8bit_tb/U_fx3_model/fx3_uart_rxd
add wave -noupdate -group {fx3 model} /fx3_gpif_meta_8bit_tb/U_fx3_model/fx3_tx_en
add wave -noupdate -group {fx3 model} /fx3_gpif_meta_8bit_tb/U_fx3_model/fx3_ctl
add wave -noupdate -group {fx3 model} /fx3_gpif_meta_8bit_tb/U_fx3_model/fx3_gpif
add wave -noupdate -group {fx3 model} /fx3_gpif_meta_8bit_tb/U_fx3_model/fx3_pclk
add wave -noupdate -group {fx3 model} /fx3_gpif_meta_8bit_tb/U_fx3_model/done
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/pclk
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/current
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/ctl_in
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/gpif_in
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/meta_enable
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/packet_enable
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/reset
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/rx_fifo_data
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/rx_fifo_empty
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/rx_fifo_full
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/rx_fifo_usedw
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/rx_meta_fifo_data
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/rx_meta_fifo_empty
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/rx_meta_fifo_full
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/rx_meta_fifo_usedr
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/tx_fifo_empty
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/tx_fifo_full
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/tx_fifo_usedw
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/tx_meta_fifo_empty
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/tx_meta_fifo_full
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/tx_meta_fifo_usedw
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/tx_timestamp
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/usb_speed
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/ctl_oe
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/ctl_out
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/gpif_oe
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/gpif_out
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/rx_enable
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/rx_fifo_read
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/rx_meta_fifo_read
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/tx_enable
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/tx_fifo_data
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/tx_fifo_write
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/tx_meta_fifo_data
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/tx_meta_fifo_write
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/pclk
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/current
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/ctl_in
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/gpif_in
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/meta_enable
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/packet_enable
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/reset
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/rx_fifo_data
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/rx_fifo_empty
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/rx_fifo_full
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/rx_fifo_usedw
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/rx_meta_fifo_data
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/rx_meta_fifo_empty
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/rx_meta_fifo_full
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/rx_meta_fifo_usedr
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/tx_fifo_empty
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/tx_fifo_full
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/tx_fifo_usedw
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/tx_meta_fifo_empty
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/tx_meta_fifo_full
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/tx_meta_fifo_usedw
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/tx_timestamp
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/usb_speed
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/ctl_oe
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/ctl_out
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/gpif_oe
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/gpif_out
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/rx_enable
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/rx_fifo_read
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/rx_meta_fifo_read
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/tx_enable
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/tx_fifo_data
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/tx_fifo_write
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/tx_meta_fifo_data
add wave -noupdate -group {fx3 gpif} /fx3_gpif_meta_8bit_tb/U_fx3_gpif/tx_meta_fifo_write
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/clock
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/enable
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/FIFO_DATA_WIDTH
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/fifo_full
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/fifo_usedw
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/FIFO_USEDW_WIDTH
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/in_sample_controls
add wave -noupdate -expand -group {fifo writer} -subitemconfig {/fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/in_samples(0) -expand /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/in_samples(1) -expand} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/in_samples
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/meta_en
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/META_FIFO_DATA_WIDTH
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/meta_fifo_full
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/meta_fifo_usedw
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/META_FIFO_USEDW_WIDTH
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/mini_exp
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/NUM_STREAMS
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/overflow_duration
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/packet_control
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/packet_en
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/reset
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/timestamp
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/usb_speed
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/fifo_clear
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/fifo_data
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/fifo_write
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/meta_fifo_data
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/meta_fifo_write
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/overflow_count
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/overflow_led
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/packet_ready
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/dma_buf_size
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/DMA_BUF_SIZE_HS
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/DMA_BUF_SIZE_SS
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/fifo_current
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/fifo_enough
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/FIFO_FSM_RESET_VALUE
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/fifo_future
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/meta_current
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/META_FSM_RESET_VALUE
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/meta_future
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/overflow_detected
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/sync_mini_exp
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/clock
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/enable
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/FIFO_DATA_WIDTH
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/fifo_full
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/fifo_usedw
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/FIFO_USEDW_WIDTH
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/in_sample_controls
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/in_samples
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/meta_en
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/META_FIFO_DATA_WIDTH
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/meta_fifo_full
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/meta_fifo_usedw
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/META_FIFO_USEDW_WIDTH
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/mini_exp
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/NUM_STREAMS
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/overflow_duration
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/packet_control
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/packet_en
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/reset
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/timestamp
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/usb_speed
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/fifo_clear
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/fifo_data
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/fifo_write
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/meta_fifo_data
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/meta_fifo_write
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/overflow_count
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/overflow_led
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/packet_ready
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/dma_buf_size
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/DMA_BUF_SIZE_HS
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/DMA_BUF_SIZE_SS
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/fifo_current
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/fifo_enough
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/FIFO_FSM_RESET_VALUE
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/fifo_future
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/meta_current
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/META_FSM_RESET_VALUE
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/meta_future
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/overflow_detected
add wave -noupdate -expand -group {fifo writer} /fx3_gpif_meta_8bit_tb/U_rx/U_fifo_writer/sync_mini_exp
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/clock
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/eight_bit_mode_en
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/enable
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/fifo_data
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/fifo_future
add wave -noupdate -group {fifo reader} -expand /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/fifo_current
add wave -noupdate -group {fifo reader} -expand -subitemconfig {/fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/in_sample_controls(0) -expand /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/in_sample_controls(1) -expand} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/in_sample_controls
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/FIFO_DATA_WIDTH
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/fifo_empty
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/fifo_holdoff
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/FIFO_READ_THROTTLE
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/fifo_usedw
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/FIFO_USEDW_WIDTH
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/meta_en
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/meta_fifo_data
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/META_FIFO_DATA_WIDTH
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/meta_fifo_empty
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/meta_fifo_usedw
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/META_FIFO_USEDW_WIDTH
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/NUM_STREAMS
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/packet_en
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/packet_ready
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/reset
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/timestamp
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/underflow_duration
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/usb_speed
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/fifo_read
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/meta_fifo_read
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/out_samples
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/packet_control
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/packet_empty
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/underflow_count
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/underflow_led
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/dma_buf_size
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/DMA_BUF_SIZE_HS
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/DMA_BUF_SIZE_SS
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/FIFO_FSM_RESET_VALUE
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/meta_current
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/META_FSM_RESET_VALUE
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/meta_future
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/underflow_detected
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/clock
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/eight_bit_mode_en
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/enable
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/fifo_data
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/fifo_future
add wave -noupdate -group {fifo reader} -expand /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/fifo_current
add wave -noupdate -group {fifo reader} -expand -subitemconfig {/fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/in_sample_controls(0) -expand /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/in_sample_controls(1) -expand} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/in_sample_controls
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/FIFO_DATA_WIDTH
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/fifo_empty
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/fifo_holdoff
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/FIFO_READ_THROTTLE
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/fifo_usedw
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/FIFO_USEDW_WIDTH
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/meta_en
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/meta_fifo_data
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/META_FIFO_DATA_WIDTH
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/meta_fifo_empty
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/meta_fifo_usedw
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/META_FIFO_USEDW_WIDTH
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/NUM_STREAMS
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/packet_en
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/packet_ready
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/reset
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/timestamp
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/underflow_duration
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/usb_speed
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/fifo_read
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/meta_fifo_read
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/out_samples
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/packet_control
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/packet_empty
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/underflow_count
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/underflow_led
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/dma_buf_size
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/DMA_BUF_SIZE_HS
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/DMA_BUF_SIZE_SS
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/FIFO_FSM_RESET_VALUE
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/meta_current
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/META_FSM_RESET_VALUE
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/meta_future
add wave -noupdate -group {fifo reader} /fx3_gpif_meta_8bit_tb/U_tx/U_fifo_reader/underflow_detected
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/rdclk
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/rdreq
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/q
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/ADD_RAM_OUTPUT_REGISTER
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/ADD_USEDW_MSB_BIT
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/CLOCKS_ARE_SYNCHRONIZED
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/DELAY_RDUSEDW
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/DELAY_WRUSEDW
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/INTENDED_DEVICE_FAMILY
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/LPM_NUMWORDS
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/LPM_SHOWAHEAD
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/LPM_WIDTH
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/LPM_WIDTH_R
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/OVERFLOW_CHECKING
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/RDSYNC_DELAYPIPE
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/READ_ACLR_SYNCH
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/UNDERFLOW_CHECKING
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/USE_EAB
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/WRITE_ACLR_SYNCH
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/WRSYNC_DELAYPIPE
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/aclr
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/data
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/wrclk
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/wrreq
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/rdempty
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/rdfull
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/rdusedw
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/wrempty
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/wrfull
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/wrusedw
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/rdclk
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/rdreq
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/q
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/ADD_RAM_OUTPUT_REGISTER
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/ADD_USEDW_MSB_BIT
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/CLOCKS_ARE_SYNCHRONIZED
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/DELAY_RDUSEDW
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/DELAY_WRUSEDW
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/INTENDED_DEVICE_FAMILY
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/LPM_NUMWORDS
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/LPM_SHOWAHEAD
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/LPM_WIDTH
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/LPM_WIDTH_R
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/OVERFLOW_CHECKING
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/RDSYNC_DELAYPIPE
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/READ_ACLR_SYNCH
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/UNDERFLOW_CHECKING
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/USE_EAB
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/WRITE_ACLR_SYNCH
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/WRSYNC_DELAYPIPE
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/aclr
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/data
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/wrclk
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/wrreq
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/rdempty
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/rdfull
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/rdusedw
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/wrempty
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/wrfull
add wave -noupdate -group {tx sample fifo} /fx3_gpif_meta_8bit_tb/U_tx/U_tx_sample_fifo/wrusedw
TreeUpdate [SetDefaultTree]
WaveRestoreCursors {{Stream fail point} {42333672 ps} 1} {{Cursor 8} {41129500 ps} 0}
quietly wave cursor active 2
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
WaveRestoreZoom {39311955 ps} {45299371 ps}
