-- Copyright (c) 2017 Nuand LLC
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
-- copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in
-- all copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
-- THE SOFTWARE.
--
-- bladerf_core: shared implementation body for the hosted and sweep FPGA
-- revisions. bladerf-hosted.vhd and bladerf-sweep.vhd are thin wrappers that
-- instantiate this entity with revision-specific generics.

library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;
    use ieee.math_real.all;
    use ieee.math_complex.all;

library work;
    use work.bladerf;
    use work.bladerf_p.all;
    use work.fifo_readwrite_p.all;

entity bladerf_core is
  generic (
    -- Feature switches for revision-specific blocks. All false in hosted.
    -- Nothing reads them yet: they exist so that the sweep revision has a
    -- place to turn things on without a second copy of this file.
    ENABLE_SWEEP_ANALYZER  : boolean := false;
    ENABLE_TRIGGER_CAPTURE : boolean := false;
    ENABLE_GAIN_SEQUENCER  : boolean := false
  );
  port (
    -- Main 38.4MHz system clock (3.3 V)
    c5_clock2           :   in      std_logic ;

    -- SI53304 clock controls (3.3 V)
    si_clock_sel        :   out     std_logic := '0';
    c5_clock2_oe        :   out     std_logic := '0';
    ufl_clock_oe        :   out     std_logic := '0';
    exp_clock_oe        :   out     std_logic := '0';

    -- VCTCXO DAC (3.3 V)
    dac_sclk            :   out     std_logic := '0' ;
    dac_sdi             :   out     std_logic := '0' ;
    dac_csn             :   out     std_logic := '1' ;

    -- LEDs (3.3 V)
    led                 :   buffer  std_logic_vector(3 downto 1) := (others =>'0') ;

    -- Power supply sync (2.5 V)
    ps_sync_1p1         :   out     std_logic := '0';
    ps_sync_1p8         :   out     std_logic := '0';

    -- INA219 power monitor (3.3 V)
    pwr_sda             :   inout   std_logic := 'Z';
    pwr_scl             :   out     std_logic := 'Z';

    -- TPS2115A power mux status
    pwr_status          :   in      std_logic;

    -- AD9361 RX Interface (2.5 V, LVDS)
    adi_rx_clock        :   in      std_logic ;
    adi_rx_data         :   in      std_logic_vector(5 downto 0) ;
    adi_rx_frame        :   in      std_logic ;

    -- RX RF Switching (3.3 V)
    adi_rx_spdt1_v      :   out     std_logic_vector(2 downto 1) ;
    adi_rx_spdt2_v      :   out     std_logic_vector(2 downto 1) ;

    -- RX Bias-T Enable
    rx_bias_en          :   out     std_logic := '0';

    -- AD9361 TX Interface (2.5 V, LVDS)
    adi_tx_clock        :   out     std_logic ;
    adi_tx_data         :   out     std_logic_vector(5 downto 0) ;
    adi_tx_frame        :   out     std_logic ;

    -- TX RF Switching (3.3 V)
    adi_tx_spdt1_v      :   out     std_logic_vector(2 downto 1) ;
    adi_tx_spdt2_v      :   out     std_logic_vector(2 downto 1) ;

    -- TX Bias-T Enable
    tx_bias_en          :   out     std_logic := '0';

    -- AD9361 SPI Interface (2.5 V)
    adi_spi_sclk        :   buffer  std_logic := '0' ;
    adi_spi_csn         :   out     std_logic := '1' ;
    adi_spi_sdi         :   out     std_logic := '0' ;
    adi_spi_sdo         :   in      std_logic := '0' ;

    -- AD9361 Control Interface (2.5 V)
    adi_reset_n         :   buffer  std_logic ;
    adi_enable          :   out     std_logic;
    adi_txnrx           :   out     std_logic := '0';
    adi_en_agc          :   out     std_logic := '0';
    adi_ctrl_in         :   out     std_logic_vector(3 downto 0) := (others => '0');
    adi_ctrl_out        :   in      std_logic_vector(7 downto 0);
    adi_sync_in         :   out     std_logic := '0';

    -- ADF4002 SPI Interface (3.3 V)
    adf_sclk            :   out     std_logic := '0' ;
    adf_csn             :   out     std_logic := '1' ;
    adf_sdi             :   out     std_logic := '0' ;
    adf_ce              :   out     std_logic := '0';
    adf_muxout          :   in      std_logic;

    -- FX3 GPIF Interface - VIO1/2/3 (1.8 V)
    fx3_pclk            :   in      std_logic ;
    fx3_gpif            :   inout   std_logic_vector(31 downto 0) ;
    fx3_ctl             :   inout   std_logic_vector(12 downto 0) ;

    -- FX3 UART/Flash Interface - VIO4 (3.3 V)
    fx3_uart_rxd        :   out     std_logic ;
    fx3_uart_txd        :   in      std_logic ;
    fx3_uart_cts        :   out     std_logic ;

    -- Expansion Interface (3.3 V / 2.5 V / 1.8 V)
    exp_present         :   in      std_logic;
    exp_clock_req       :   in      std_logic;
    exp_i2c_sda         :   inout   std_logic;
    exp_i2c_scl         :   inout   std_logic;
    exp_gpio            :   inout   std_logic_vector(31 downto 0);

    -- Mini expansion interface (3.3 V / 2.5 V / 1.8 V)
    mini_exp1           :   inout   std_logic := 'Z';
    mini_exp2           :   inout   std_logic := 'Z';

    -- Hardware revision resistors (3.3 V / 2.5 V / 1.8 V)
    hw_rev              :   in      std_logic_vector(1 downto 0)

  ) ;
end entity ; -- bladerf_core

architecture core_bladerf of bladerf_core is

    attribute noprune          : boolean;
    attribute keep             : boolean;

    alias  sys_reset_async     : std_logic is fx3_ctl(7);
    signal sys_reset_pclk      : std_logic;
    signal sys_reset           : std_logic;

    signal sys_clock           : std_logic;
    signal sys_clock_out       : std_logic;
    signal sys_pll_locked      : std_logic;
    signal sys_pll_reset       : std_logic;

    signal fx3_pclk_pll        : std_logic;
    signal fx3_pclk_pll_out    : std_logic;
    signal fx3_pclk_pll_locked : std_logic;
    signal fx3_pclk_pll_reset  : std_logic;

    signal rx_mux_sel             : unsigned(2 downto 0);

    signal nios_xb_gpio_in        : std_logic_vector(31 downto 0) := (others => '0');
    signal nios_xb_gpio_out       : std_logic_vector(31 downto 0) := (others => '0');
    signal nios_xb_gpio_oe        : std_logic_vector(31 downto 0) := (others => '0');

    signal nios_gpio              : nios_gpio_t;
    signal nios_gpo_slv           : std_logic_vector(31 downto 0);

    -- RF link status (RF_LINK_STATUS host register)
    signal link_start_toggle_tx   : std_logic;
    signal link_start_toggle_rx   : std_logic;

    signal tx_usb_speed_mismatch       : std_logic;
    signal tx_link_active              : std_logic;
    signal tx_speed_latched            : std_logic;
    signal tx_protocol_start_violation : std_logic;
    signal tx_link_epoch_counter       : unsigned(7 downto 0);

    signal rx_usb_speed_mismatch       : std_logic;
    signal rx_link_active              : std_logic;
    signal rx_speed_latched            : std_logic;
    signal rx_protocol_start_violation : std_logic;
    signal rx_link_epoch_counter       : unsigned(7 downto 0);

    -- Synchronized (sys_clock domain) copies of the eight single-bit tx_*/
    -- rx_* flags above. rf_link_status is assembled ONLY from these plus
    -- signals already native to sys_clock -- see ADR on RF_LINK_STATUS CDC.
    signal tx_link_active_sys              : std_logic;
    signal tx_speed_latched_sys            : std_logic;
    signal tx_usb_speed_mismatch_sys       : std_logic;
    signal tx_protocol_start_violation_sys : std_logic;

    -- Fault aggregate, reduced in the originating domain then crossed as a
    -- single bit. rx_fault_any lives on rx_clock, its _sys copy on sys_clock.
    signal rx_fault_any                    : std_logic;
    signal tx_fault_any                    : std_logic;
    signal rx_fault_any_sys                : std_logic;
    signal tx_fault_any_sys                : std_logic;
    signal rx_abort_active_sys             : std_logic;

    signal rx_link_active_sys              : std_logic;
    signal rx_speed_latched_sys            : std_logic;
    signal rx_usb_speed_mismatch_sys       : std_logic;
    signal rx_protocol_start_violation_sys : std_logic;

    -- Epoch bookkeeping, entirely in sys_clock: a one-cycle pulse derived
    -- from rf_link_start_toggle (already a sys_clock signal out of
    -- rf_link_controller) drives both the toggle mirror and the count.
    signal rf_link_start_toggle_prev : std_logic := '0';
    signal rf_epoch_count_sys        : unsigned(7 downto 0) := (others => '0');

    -- Per-direction epoch acknowledgement, crossed back into the system
    -- domain. One bit each way instead of an eight-bit counter: the host's
    -- question is "did both directions take the epoch I asked for", not "what
    -- number are they on".
    signal rx_epoch_ack              : std_logic;
    signal tx_epoch_ack              : std_logic;
    signal rx_epoch_valid            : std_logic;
    signal tx_epoch_valid            : std_logic;
    signal rx_epoch_ack_sys          : std_logic;
    signal tx_epoch_ack_sys          : std_logic;
    signal rx_epoch_valid_sys        : std_logic;
    signal tx_epoch_valid_sys        : std_logic;
    signal rx_epoch_current          : std_logic;
    signal tx_epoch_current          : std_logic;

    signal rf_link_status         : std_logic_vector(31 downto 0);

    -- RF link control. The word arrives from the rf_link_cfg PIO in the
    -- system clock domain; rf_link_controller is the only thing that decodes
    -- it, and what leaves it are already-debounced command toggles.
    signal rf_link_cfg_word       : std_logic_vector(31 downto 0);
    signal rf_link_start_toggle   : std_logic;
    signal rf_link_stop_toggle    : std_logic;
    signal rf_link_clear_fault    : std_logic;
    signal rf_link_req_speed      : std_logic;
    signal rf_link_speed_disagree : std_logic;
    signal rf_link_epoch_tag      : std_logic_vector(7 downto 0);

    signal link_stop_toggle_rx    : std_logic;
    signal link_stop_toggle_tx    : std_logic;
    signal clear_fault_toggle_rx  : std_logic;
    signal clear_fault_toggle_tx  : std_logic;

    signal rx_fault_sticky        : std_logic_vector(4 downto 0);
    signal tx_fault_sticky        : std_logic_vector(4 downto 0);
    signal rx_abort_active        : std_logic;
    signal tx_abort_active        : std_logic;

    signal i2c_scl_in             : std_logic;
    signal i2c_scl_out            : std_logic;
    signal i2c_scl_oen            : std_logic;

    signal i2c_sda_in             : std_logic;
    signal i2c_sda_out            : std_logic;
    signal i2c_sda_oen            : std_logic;

    signal tx_sample_fifo         : tx_fifo_t       := TX_FIFO_T_DEFAULT;
    signal rx_sample_fifo         : rx_fifo_t       := RX_FIFO_T_DEFAULT;
    signal tx_loopback_fifo       : loopback_fifo_t := LOOPBACK_FIFO_T_DEFAULT;

    signal tx_meta_fifo           : meta_fifo_tx_t := META_FIFO_TX_T_DEFAULT;
    signal rx_meta_fifo           : meta_fifo_rx_t := META_FIFO_RX_T_DEFAULT;

    signal usb_speed_pclk         : std_logic;
    signal usb_speed_rx           : std_logic;
    signal usb_speed_tx           : std_logic;

    signal tx_reset               : std_logic;
    signal rx_reset               : std_logic;

    signal tx_enable_pclk         : std_logic;
    signal rx_enable_pclk         : std_logic;

    signal tx_enable              : std_logic;
    signal rx_enable              : std_logic;

    signal meta_en_pclk           : std_logic;
    signal meta_en_tx             : std_logic;
    signal meta_en_rx             : std_logic;

    signal eightbit_en_tx         : std_logic;
    signal eightbit_en_rx         : std_logic;

    signal highly_packed_en_txrx  : std_logic;

    signal packet_en_pclk         : std_logic;
    signal packet_en_tx           : std_logic;
    signal packet_en_rx           : std_logic;

    signal tx_timestamp           : unsigned(63 downto 0);
    signal rx_timestamp           : unsigned(63 downto 0);
    signal timestamp_sync         : std_logic;

    signal tx_loopback_enabled    : std_logic := '0';

    signal fx3_gpif_in            : std_logic_vector(31 downto 0);
    signal fx3_gpif_out           : std_logic_vector(31 downto 0);
    signal fx3_gpif_oe            : std_logic;

    signal fx3_ctl_in             : std_logic_vector(12 downto 0);
    signal fx3_ctl_out            : std_logic_vector(12 downto 0);
    signal fx3_ctl_oe             : std_logic_vector(12 downto 0);

    signal tx_underflow_led       : std_logic := '1';
    signal rx_overflow_led        : std_logic := '1';

    -- Loss counters on their way to the host. Each lives in its own sample
    -- domain and is read by Nios in sys_clock, so it crosses through the
    -- same handshake block the timestamp uses -- not a raw synchronizer:
    -- 64 bits synchronised bit by bit would hand the reader a word that
    -- never existed while the counter is incrementing.
    --
    -- `_q` is the capture register REQUIRED by handshake.vhd:25 (dest_data
    -- is a wire, not a flop). Without it the crossing has no capture event
    -- and no skew constraint can bind to it.
    signal rx_overflow_count      : unsigned(63 downto 0);
    signal tx_underflow_count     : unsigned(63 downto 0);
    signal rx_ovf_sys             : unsigned(63 downto 0);
    signal tx_unf_sys             : unsigned(63 downto 0);
    signal rx_ovf_sys_q           : unsigned(63 downto 0) := (others => '0');
    signal tx_unf_sys_q           : unsigned(63 downto 0) := (others => '0');
    signal rx_ovf_lo_word         : std_logic_vector(31 downto 0);
    signal rx_ovf_hi_word         : std_logic_vector(31 downto 0);
    signal tx_unf_lo_word         : std_logic_vector(31 downto 0);
    signal tx_unf_hi_word         : std_logic_vector(31 downto 0);
    signal rx_ovf_req             : std_logic := '0';
    signal rx_ovf_ack             : std_logic;
    signal tx_unf_req             : std_logic := '0';
    signal tx_unf_ack             : std_logic;

    signal led1_blink             : std_logic;

    signal nios_sdo               : std_logic;
    signal nios_sdio              : std_logic;
    signal nios_sclk              : std_logic;
    signal nios_ss_n              : std_logic_vector(1 downto 0);

    signal command_serial_in      : std_logic;
    signal command_serial_out     : std_logic;

    signal timestamp_req          : std_logic;
    signal timestamp_ack          : std_logic;
    signal fx3_timestamp          : unsigned(63 downto 0);

    signal rx_ts_reset            : std_logic;
    signal tx_ts_reset            : std_logic;

    signal rx_trigger_ctl_i       : std_logic_vector(7 downto 0);
    signal rx_trigger_ctl         : trigger_t := TRIGGER_T_DEFAULT;
    alias  rx_trigger_line        : std_logic is mini_exp1;

    signal tx_trigger_ctl_i       : std_logic_vector(7 downto 0);
    signal tx_trigger_ctl         : trigger_t := TRIGGER_T_DEFAULT;
    alias  tx_trigger_line        : std_logic is mini_exp1;

    signal rffe_gpio              : rffe_gpio_t := (
        i => RFFE_GPI_DEFAULT,
        o => pack(RFFE_GPO_DEFAULT)
    );

    signal ad9361                 : mimo_2r2t_t := MIMO_2R2T_T_DEFAULT;
    alias tx_clock  is ad9361.clock;
    alias rx_clock  is ad9361.clock;

    signal mimo_rx_enables        : std_logic_vector(RFFE_GPO_DEFAULT.mimo_rx_en'range) := RFFE_GPO_DEFAULT.mimo_rx_en;
    signal mimo_tx_enables        : std_logic_vector(RFFE_GPO_DEFAULT.mimo_tx_en'range) := RFFE_GPO_DEFAULT.mimo_tx_en;

    signal dac_controls           : sample_controls_t(ad9361.ch'range)    := (others => SAMPLE_CONTROL_DISABLE);
    signal dac_streams            : sample_streams_t(dac_controls'range)  := (others => ZERO_SAMPLE);
    signal adc_controls           : sample_controls_t(ad9361.ch'range)    := (others => SAMPLE_CONTROL_DISABLE);
    signal adc_streams            : sample_streams_t(adc_controls'range)  := (others => ZERO_SAMPLE);
    signal adc_streams_last_v     : std_logic_vector(adc_controls'range)  := (others => '0');

    -- Sweep analyser: measures every dwell on chip and reports a summary
    -- the host cannot compute in time. Present only when the generic is
    -- set, so the hosted revision pays nothing for it.
    signal dwell_sync_in          : std_logic := '0';
    signal dwell_sync_in_r        : std_logic := '0';
    signal dwell_start            : std_logic := '0';
    signal analysis_sample_q      : sample_stream_t := ZERO_SAMPLE;
    signal e3_tap_activity_q      : std_logic := '0';
    attribute preserve : boolean;
    attribute preserve of e3_tap_activity_q : signal is true;
    signal dwell_summary_valid    : std_logic;
    signal dwell_energy_sum       : unsigned(63 downto 0);
    signal dwell_peak             : unsigned(31 downto 0);
    signal dwell_clip_count       : unsigned(31 downto 0);
    signal dwell_sample_count     : unsigned(31 downto 0);
    signal dwell_triggered        : std_logic := '0';
    signal dwell_first_window     : unsigned(15 downto 0);
    signal dwell_first_timestamp  : unsigned(63 downto 0);
    signal dwell_mean_power       : unsigned(31 downto 0);
    signal dwell_noise_floor      : unsigned(47 downto 0);
    signal dwell_peak_window      : unsigned(47 downto 0);
    signal dwell_measure_valid    : std_logic := '0';
    signal dwell_gain_too_high    : std_logic := '0';

    -- Pre-trigger ring: drained by the host, so the read side stays tied
    -- off here until the Nios register window exists.
    signal pretrig_frozen         : std_logic := '0';
    signal pretrig_oldest         : unsigned(11 downto 0) := (others => '0');
    signal pretrig_wrapped        : std_logic := '0';
    signal pretrig_rd_addr        : unsigned(11 downto 0) := (others => '0');
    signal pretrig_rd_data        : std_logic_vector(31 downto 0) := (others => '0');
    signal pretrig_addr_word      : std_logic_vector(31 downto 0);
    signal dwell_status_word      : std_logic_vector(31 downto 0);

    -- Latched dwell summary read window.
    signal dwell_rd_index         : unsigned(3 downto 0) := (others => '0');
    signal dwell_rd_data          : std_logic_vector(31 downto 0) := (others => '0');
    signal dwell_generation       : unsigned(15 downto 0) := (others => '0');

    -- Trigger threshold config: system domain word, its rx_clock copy, and
    -- the decoded 48-bit value.
    signal dwell_cfg_word         : std_logic_vector(31 downto 0);
    -- Wire out of the handshake (source-domain flops) and the rx-domain
    -- register that captures it. Only dwell_cfg_rx may be read by rx logic.
    signal dwell_cfg_wire         : std_logic_vector(31 downto 0);
    signal dwell_cfg_req_rx       : std_logic := '0';
    signal dwell_cfg_ack_rx       : std_logic;
    signal dwell_cfg_rx           : std_logic_vector(31 downto 0) := (others => '0');
    signal dwell_shift            : natural range 0 to 24 := 0;
    signal dwell_threshold        : unsigned(47 downto 0) := (others => '0');

    -- gain_sequencer counts settling in samples; SETTLE_LOG2 defaults to 13,
    -- so the port is 14 bits and the readout word carries 16.
    signal dwell_settle_raw       : unsigned(13 downto 0) := (others => '0');
    signal dwell_settle_elapsed   : unsigned(15 downto 0) := (others => '0');
    signal pretrig_frozen_sys     : std_logic;
    signal pretrig_wrapped_sys    : std_logic;
    signal dwell_triggered_sys    : std_logic;
    signal dwell_measure_valid_sys : std_logic;
    signal dwell_gain_too_high_sys : std_logic;
    signal adc_enable_r           : std_logic_vector(adc_controls'range)  := (others => '0');

    signal   ps_sync              : std_logic_vector(0 downto 0)          := (others => '0');


    signal rx_packet_control      : packet_control_t := PACKET_CONTROL_DEFAULT ;

    signal rx_packet_ready        : std_logic;

    signal tx_packet_ready        : std_logic;

    -- Wishbone master conduit. Nuand exports this as an extension point: the
    -- bridge is mapped into the Nios address space at 0x10000000 on IRQ 10 and
    -- the conduit is brought out of the Qsys system, so an out-of-tree design
    -- can attach a Wishbone target to it. No in-tree revision does, and this
    -- image does not, but the interface is part of the FPGA's ABI and is not
    -- ours to delete -- see the inert termination below.
    signal wbm_wb_clk_i           : std_logic;
    signal wbm_wb_rst_i           : std_logic;
    signal wbm_wb_adr_o           : std_logic_vector(31 downto 0);
    signal wbm_wb_dat_o           : std_logic_vector(31 downto 0);
    signal wbm_wb_dat_i           : std_logic_vector(31 downto 0);
    signal wbm_wb_we_o            : std_logic;
    signal wbm_wb_sel_o           : std_logic;
    signal wbm_wb_stb_o           : std_logic;
    signal wbm_wb_ack_i           : std_logic;
    signal wbm_wb_cyc_o           : std_logic;

    -- SPI arbiter conduit. Bit 0 is the Nios, which drives its own request
    -- and ack through the arbiter's control register rather than these
    -- lines; bit 1 is the second requester, which this revision does not
    -- have. Both default low so an unconnected input cannot look like a
    -- pending request.
    signal arbiter_request        : std_logic_vector(1 downto 0) := (others => '0');
    signal arbiter_granted        : std_logic_vector(1 downto 0);
    signal arbiter_ack            : std_logic_vector(1 downto 0) := (others => '0');

begin

    U_rx_pkt_gen : entity work.rx_packet_generator
        port map(
            rx_clock               => rx_clock,
            rx_reset               => rx_reset,

            rx_packet_ready        => rx_packet_ready,

            rx_enable              => rx_enable,
            rx_packet_enable       => packet_en_rx,

            rx_packet_control      => rx_packet_control
        ) ;


    -- ========================================================================
    -- PLLs
    -- ========================================================================

    -- Create 80 MHz system clock from 38.4 MHz
    U_system_pll : component system_pll
        port map (
            refclk   => c5_clock2,
            rst      => sys_pll_reset,
            outclk_0 => sys_clock_out,
            locked   => sys_pll_locked
        );

    U_system_pll_ctrl : component clk_ctrl
        port map (
            inclk   => sys_clock_out,
            ena     => sys_pll_locked,
            outclk  => sys_clock
        );

    U_pll_reset_pll : entity work.pll_reset
        generic map (
            SYS_CLOCK_FREQ_HZ   => 38_400_000,
            DEVICE_FAMILY       => "Cyclone V"
        )
        port map (
            sys_clock      => c5_clock2,
            pll_locked     => sys_pll_locked,
            pll_reset      => sys_pll_reset
        );

    -- Use PLL to adjust the phase of the FX3 PCLK to
    -- retime the FX3 GPIF interface for timing closure.
    U_fx3_pll : component fx3_pll
        port map (
            refclk   =>  fx3_pclk,
            rst      =>  fx3_pclk_pll_reset,
            outclk_0 =>  fx3_pclk_pll_out,
            locked   =>  fx3_pclk_pll_locked
        );

    U_fx3_pll_ctrl : component clk_ctrl
        port map (
            inclk   => fx3_pclk_pll_out,
            ena     => fx3_pclk_pll_locked,
            outclk  => fx3_pclk_pll
        );

    U_pll_reset_fx3_pll : entity work.pll_reset
        generic map (
            SYS_CLOCK_FREQ_HZ   => 100_000_000,
            DEVICE_FAMILY       => "Cyclone V"
        )
        port map (
            sys_clock      => fx3_pclk,
            pll_locked     => fx3_pclk_pll_locked,
            pll_reset      => fx3_pclk_pll_reset
        );


    -- ========================================================================
    -- POWER SUPPLY SYNCHRONIZATION
    -- ========================================================================

    U_ps_sync : entity work.ps_sync
        generic map (
            OUTPUTS  => 1,
            USE_LFSR => true,
            HOP_LIST => adp2384_sync_divisors( REFCLK_HZ  => 38.4e6,
                                               n_divisors => 7 ),
            HOP_RATE => 100
        )
        port map (
            refclk   => c5_clock2,
            sync     => ps_sync
        );

    ps_sync_1p1 <= ps_sync(0);
    ps_sync_1p8 <= ps_sync(0);

    -- ========================================================================
    -- FX3 GPIF
    -- ========================================================================

    -- FX3 GPIF
    U_fx3_gpif : entity work.fx3_gpif
        port map (
            pclk                =>  fx3_pclk_pll,
            reset               =>  sys_reset_pclk,

            usb_speed           =>  usb_speed_pclk,

            meta_enable         =>  meta_en_pclk,
            packet_enable       =>  packet_en_pclk,
            rx_enable           =>  rx_enable_pclk,
            tx_enable           =>  tx_enable_pclk,

            gpif_in             =>  fx3_gpif_in,
            gpif_out            =>  fx3_gpif_out,
            gpif_oe             =>  fx3_gpif_oe,
            ctl_in              =>  fx3_ctl_in,
            ctl_out             =>  fx3_ctl_out,
            ctl_oe              =>  fx3_ctl_oe,

            tx_fifo_write       =>  tx_sample_fifo.wreq,
            tx_fifo_full        =>  tx_sample_fifo.wfull,
            tx_fifo_empty       =>  tx_sample_fifo.wempty,
            tx_fifo_usedw       =>  tx_sample_fifo.wused,
            tx_fifo_data        =>  tx_sample_fifo.wdata,

            tx_timestamp        =>  fx3_timestamp,
            tx_meta_fifo_write  =>  tx_meta_fifo.wreq,
            tx_meta_fifo_full   =>  tx_meta_fifo.wfull,
            tx_meta_fifo_empty  =>  tx_meta_fifo.wempty,
            tx_meta_fifo_usedw  =>  tx_meta_fifo.wused,
            tx_meta_fifo_data   =>  tx_meta_fifo.wdata,

            rx_fifo_read        =>  rx_sample_fifo.rreq,
            rx_fifo_full        =>  rx_sample_fifo.rfull,
            rx_fifo_empty       =>  rx_sample_fifo.rempty,
            rx_fifo_usedw       =>  rx_sample_fifo.rused,
            rx_fifo_data        =>  rx_sample_fifo.rdata,

            rx_meta_fifo_read   =>  rx_meta_fifo.rreq,
            rx_meta_fifo_full   =>  rx_meta_fifo.rfull,
            rx_meta_fifo_empty  =>  rx_meta_fifo.rempty,
            rx_meta_fifo_usedr  =>  rx_meta_fifo.rused,
            rx_meta_fifo_data   =>  rx_meta_fifo.rdata
        );

    -- FX3 GPIF bidirectional signal control
    register_gpif : process(sys_reset_pclk, fx3_pclk_pll)
    begin
        if( sys_reset_pclk = '1' ) then
            fx3_gpif    <= (others =>'Z');
            fx3_gpif_in <= (others =>'0');
        elsif( rising_edge(fx3_pclk_pll) ) then
            fx3_gpif_in <= fx3_gpif;
            if( fx3_gpif_oe = '1' ) then
                fx3_gpif <= fx3_gpif_out;
            else
                fx3_gpif <= (others =>'Z');
            end if;
        end if;
    end process;

    -- FX3 CTL bidirectional signal control
    generate_ctl : for i in fx3_ctl'range generate
        fx3_ctl(i) <= fx3_ctl_out(i) when fx3_ctl_oe(i) = '1' else 'Z';
    end generate;

    fx3_ctl_in <= fx3_ctl;

    toggle_led1 : process(fx3_pclk_pll)
        variable count : natural range 0 to 10_000_000 := 10_000_000;
    begin
        if( rising_edge(fx3_pclk_pll) ) then
            count := count - 1;
            if( count = 0 ) then
                count := 10_000_000;
                led1_blink <= not led1_blink;
            end if;
        end if;
    end process;


    -- ========================================================================
    -- NIOS SYSTEM
    -- ========================================================================

    U_nios_system : component nios_system
        port map (
            clk_clk                         => sys_clock,
            reset_reset_n                   => '1',
            dac_MISO                        => nios_sdo,
            dac_MOSI                        => nios_sdio,
            dac_SCLK                        => nios_sclk,
            dac_SS_n                        => nios_ss_n,
            spi_MISO                        => adi_spi_sdo,
            spi_MOSI                        => adi_spi_sdi,
            spi_SCLK                        => adi_spi_sclk,
            spi_SS_n                        => adi_spi_csn,
            gpio_in_port                    => pack(nios_gpio.i, '0'),
            gpio_out_port                   => nios_gpo_slv,
            gpio_rffe_0_in_port             => pack(rffe_gpio),
            gpio_rffe_0_out_port            => rffe_gpio.o,
            ad9361_dac_sync_in_sync         => '0',
            ad9361_dac_sync_out_sync        => adi_sync_in,
            ad9361_data_clock_clk           => ad9361.clock, -- out std_logic;
            ad9361_data_reset_reset         => ad9361.reset, -- out std_logic;
            ad9361_device_if_rx_clk_in_p    => adi_rx_clock,
            ad9361_device_if_rx_clk_in_n    => '0',
            ad9361_device_if_rx_frame_in_p  => adi_rx_frame,
            ad9361_device_if_rx_frame_in_n  => '0',
            ad9361_device_if_rx_data_in_p   => adi_rx_data,
            ad9361_device_if_rx_data_in_n   => (others => '0'),
            ad9361_device_if_tx_clk_out_p   => adi_tx_clock,
            ad9361_device_if_tx_clk_out_n   => open,
            ad9361_device_if_tx_frame_out_p => adi_tx_frame,
            ad9361_device_if_tx_frame_out_n => open,
            ad9361_device_if_tx_data_out_p  => adi_tx_data,
            ad9361_device_if_tx_data_out_n  => open,
            ad9361_adc_i0_enable            => ad9361.ch(0).adc.i.enable, -- out sl
            ad9361_adc_i0_valid             => ad9361.ch(0).adc.i.valid,  -- out sl
            ad9361_adc_i0_data              => ad9361.ch(0).adc.i.data,   -- out slv(15:0)
            ad9361_adc_i1_enable            => ad9361.ch(1).adc.i.enable, -- out sl
            ad9361_adc_i1_valid             => ad9361.ch(1).adc.i.valid,  -- out sl
            ad9361_adc_i1_data              => ad9361.ch(1).adc.i.data,   -- out slv(15:0)
            ad9361_adc_overflow_ovf         => ad9361.adc_overflow,       -- in  sl
            ad9361_adc_q0_enable            => ad9361.ch(0).adc.q.enable, -- out sl
            ad9361_adc_q0_valid             => ad9361.ch(0).adc.q.valid,  -- out sl
            ad9361_adc_q0_data              => ad9361.ch(0).adc.q.data,   -- out slv(15:0)
            ad9361_adc_q1_enable            => ad9361.ch(1).adc.q.enable, -- out sl
            ad9361_adc_q1_valid             => ad9361.ch(1).adc.q.valid,  -- out sl
            ad9361_adc_q1_data              => ad9361.ch(1).adc.q.data,   -- out slv(15:0)
            ad9361_adc_underflow_unf        => ad9361.adc_underflow,      -- in  sl
            ad9361_dac_i0_enable            => ad9361.ch(0).dac.i.enable, -- out sl
            ad9361_dac_i0_valid             => ad9361.ch(0).dac.i.valid,  -- out sl
            ad9361_dac_i0_data              => ad9361.ch(0).dac.i.data,   -- in  slv(15:0)
            ad9361_dac_i1_enable            => ad9361.ch(1).dac.i.enable, -- out sl
            ad9361_dac_i1_valid             => ad9361.ch(1).dac.i.valid,  -- out sl
            ad9361_dac_i1_data              => ad9361.ch(1).dac.i.data,   -- in  slv(15:0)
            ad9361_dac_overflow_ovf         => ad9361.dac_overflow,       -- in  sl
            ad9361_dac_q0_enable            => ad9361.ch(0).dac.q.enable, -- out sl
            ad9361_dac_q0_valid             => ad9361.ch(0).dac.q.valid,  -- out sl
            ad9361_dac_q0_data              => ad9361.ch(0).dac.q.data,   -- in  slv(15:0)
            ad9361_dac_q1_enable            => ad9361.ch(1).dac.q.enable, -- out sl
            ad9361_dac_q1_valid             => ad9361.ch(1).dac.q.valid,  -- out sl
            ad9361_dac_q1_data              => ad9361.ch(1).dac.q.data,   -- in  slv(15:0)
            ad9361_dac_underflow_unf        => ad9361.dac_underflow,      -- in  sl
            rf_link_status_export           => rf_link_status,
            -- Both halves come from the same capture register, so the pair
            -- the host reads is always one whole snapshot.
            rx_overflow_count_lo_export     => rx_ovf_lo_word,
            rx_overflow_count_hi_export     => rx_ovf_hi_word,
            tx_underflow_count_lo_export    => tx_unf_lo_word,
            tx_underflow_count_hi_export    => tx_unf_hi_word,
            pretrig_addr_export             => pretrig_addr_word,
            pretrig_data_export             => pretrig_rd_data,
            dwell_status_export             => dwell_status_word,
            dwell_readout_export            => dwell_rd_data,
            dwell_cfg_export                => dwell_cfg_word,
            rf_link_cfg_export              => rf_link_cfg_word,
            xb_gpio_in_port                 => nios_xb_gpio_in,
            xb_gpio_out_port                => nios_xb_gpio_out,
            xb_gpio_dir_export              => nios_xb_gpio_oe,
            command_serial_in               => command_serial_in,
            command_serial_out              => command_serial_out,
            oc_i2c_arst_i                   => '0',
            oc_i2c_scl_pad_i                => i2c_scl_in,
            oc_i2c_scl_pad_o                => i2c_scl_out,
            oc_i2c_scl_padoen_o             => i2c_scl_oen,
            oc_i2c_sda_pad_i                => i2c_sda_in,
            oc_i2c_sda_pad_o                => i2c_sda_out,
            oc_i2c_sda_padoen_o             => i2c_sda_oen,
            rx_tamer_ts_sync_in             => '0',
            rx_tamer_ts_sync_out            => open,
            rx_tamer_ts_pps                 => '0',
            rx_tamer_ts_clock               => rx_clock,
            rx_tamer_ts_reset               => rx_ts_reset,
            unsigned(rx_tamer_ts_time)      => rx_timestamp,
            tx_tamer_ts_sync_in             => '0',
            tx_tamer_ts_sync_out            => open,
            tx_tamer_ts_pps                 => '0',
            tx_tamer_ts_clock               => tx_clock,
            tx_tamer_ts_reset               => tx_ts_reset,
            unsigned(tx_tamer_ts_time)      => tx_timestamp,
            rx_trigger_ctl_out_port         => rx_trigger_ctl_i,
            tx_trigger_ctl_out_port         => tx_trigger_ctl_i,
            rx_trigger_ctl_in_port          => pack(rx_trigger_ctl),
            tx_trigger_ctl_in_port          => pack(tx_trigger_ctl),
            wbm_wb_clk_i                    => wbm_wb_clk_i,
            wbm_wb_rst_i                    => wbm_wb_rst_i,
            wbm_wb_adr_o                    => wbm_wb_adr_o,
            wbm_wb_dat_o                    => wbm_wb_dat_o,
            wbm_wb_dat_i                    => wbm_wb_dat_i,
            wbm_wb_we_o                     => wbm_wb_we_o,
            wbm_wb_sel_o                    => wbm_wb_sel_o,
            wbm_wb_stb_o                    => wbm_wb_stb_o,
            wbm_wb_ack_i                    => wbm_wb_ack_i,
            wbm_wb_cyc_o                    => wbm_wb_cyc_o,
            arbiter_request                 => arbiter_request,
            arbiter_granted                 => arbiter_granted,
            arbiter_ack                     => arbiter_ack
        );

    -- The SPI arbiter is built with N=2. Bit 0 is the Nios, which requests
    -- and acks through the arbiter's own control register, not through this
    -- conduit (arbiter.vhd: nios_request/nios_ack come from nios_csr). Bit 1
    -- is a second requester that only the WLAN revision has
    -- (bladerf-wlan.vhd drives it); this revision has none.
    --
    -- Leaving the conduit unconnected is what made Qsys report
    -- "arbiter_0.conduit_end_1: Interface has no signals", with the VHDL
    -- input defaulting to 'X'. Drive both bits low: no phantom request, and
    -- no ack that the FSM could mistake for the grantee it waits on in
    -- WAIT_FOR_ACK.
    arbiter_request <= (others => '0');
    arbiter_ack     <= (others => '0');

    -- FX3 UART
    command_serial_in <= fx3_uart_txd       when sys_reset = '0' else '1';
    fx3_uart_rxd      <= command_serial_out when sys_reset = '0' else 'Z';

    -- FX3 UART CTS and Flash SPI CSx are tied to the same signal.
    -- Allow SPI accesses when FPGA is in reset
    fx3_uart_cts      <= '1' when sys_reset_pclk = '0' else 'Z';

    -- Unpack the Nios general-purpose outputs into a record
    nios_gpio.o <= unpack(nios_gpo_slv);

    -- Readback of Nios general-purpose outputs
    nios_gpio.i.gpo_readback <= nios_gpio.o;

    -- RFFE GPIO outputs
    adi_ctrl_in    <= unpack(rffe_gpio.o).ctrl_in;
    adi_tx_spdt2_v <= unpack(rffe_gpio.o).tx_spdt2;
    adi_tx_spdt1_v <= unpack(rffe_gpio.o).tx_spdt1;
    tx_bias_en     <= unpack(rffe_gpio.o).tx_bias_en;
    adi_rx_spdt2_v <= unpack(rffe_gpio.o).rx_spdt2;
    adi_rx_spdt1_v <= unpack(rffe_gpio.o).rx_spdt1;
    rx_bias_en     <= unpack(rffe_gpio.o).rx_bias_en;
    --adi_sync_in    <= unpack(rffe_gpio.o).sync_in;

    -- Per-dwell measurement, sweep revision only.
    --
    -- Tapped off adc_streams(0) rather than inserted into the datapath: the
    -- FIFO write path is untouched, so a fault here cannot cost samples.
    --
    -- The dwell boundary is the host's own sync_in strobe, which it already
    -- drives as part of the retune sequence -- there is no hardware retune
    -- signal in this gateware to use instead (checked: no retune/quick_tune
    -- net exists in the core). sync_in is in the Nios/system domain and the
    -- analyser runs on rx_clock, so it crosses through the same synchroniser
    -- every other control bit uses, then the rising edge is taken on
    -- rx_clock. A level would restart the dwell for as long as it is held.
    gen_sweep_analyzer : if( ENABLE_SWEEP_ANALYZER ) generate

        -- Registered tap, one cycle behind adc_streams(0). E1 (constant
        -- ZERO_SAMPLE input to dwell_summary) completed Analysis & Synthesis
        -- in 90s against a build that otherwise stalled quartus_map for
        -- hours; the difference is the live connection, not dwell_summary's
        -- own arithmetic. adc_streams(0) is read combinationally in several
        -- other places in this file (the RX datapath itself, loopback,
        -- diagnostics), so an analyser reading it directly sits in the same
        -- fanout cone the synthesiser has to resolve for all of them
        -- together. This register is the one thing dwell_summary reads;
        -- nothing downstream of it feeds back into fifo_writer, meta_current,
        -- the metadata FIFO, or any transport enable.
        analysis_tap_proc : process( rx_clock )
        begin
            if( rising_edge( rx_clock ) ) then
                analysis_sample_q <= adc_streams(0);
            end if;
        end process;

        -- EXPERIMENT E3c: a live consumer of the tap exists (so it cannot be
        -- optimised away), but dwell_summary itself stays on a constant --
        -- isolates "does tapping adc_streams(0) with data+valid alone
        -- trigger the map pathology" from "does the live tap reaching
        -- dwell_summary's arithmetic trigger it". One-bit XOR sink, cheapest
        -- thing that cannot constant-fold.
        e3_tap_activity_proc : process( rx_clock )
        begin
            if( rising_edge( rx_clock ) ) then
                e3_tap_activity_q <= analysis_sample_q.data_v
                                     xor analysis_sample_q.data_i(0)
                                     xor analysis_sample_q.data_q(0);
            end if;
        end process;

        U_dwell_sync : entity work.synchronizer
            generic map ( RESET_LEVEL => '0' )
            port map (
                reset  => rx_reset,
                clock  => rx_clock,
                async  => unpack(rffe_gpio.o).sync_in,
                sync   => dwell_sync_in
            );

        dwell_edge_proc : process( rx_clock )
        begin
            if( rising_edge( rx_clock ) ) then
                if( rx_reset = '1' ) then
                    dwell_sync_in_r <= '0';
                    dwell_start     <= '0';
                else
                    dwell_sync_in_r <= dwell_sync_in;
                    dwell_start     <= dwell_sync_in and not dwell_sync_in_r;
                end if;
            end if;
        end process;

        U_dwell_summary : entity work.dwell_summary
            port map (
                clock         => rx_clock,
                reset         => rx_reset,
                sample        => analysis_sample_q,
                dwell_start   => dwell_start,
                threshold     => dwell_threshold,
                summary_valid => dwell_summary_valid,
                energy_sum    => dwell_energy_sum,
                peak          => dwell_peak,
                clip_count    => dwell_clip_count,
                sample_count  => dwell_sample_count,
                triggered     => dwell_triggered,
                first_window  => dwell_first_window,
                timestamp       => rx_timestamp,
                first_timestamp => dwell_first_timestamp,
                mean_power    => dwell_mean_power,
                noise_floor   => dwell_noise_floor,
                peak_window   => dwell_peak_window
            );

        -- Latched read window. Without it the host reads thirteen words one
        -- at a time and a dwell boundary between two of them yields a record
        -- that is half one dwell and half the next -- which looks like a
        -- measurement and is not.
        U_dwell_readout : entity work.dwell_readout
            port map (
                clock           => rx_clock,
                reset           => rx_reset,
                summary_valid   => dwell_summary_valid,
                energy_sum      => dwell_energy_sum,
                peak            => dwell_peak,
                clip_count      => dwell_clip_count,
                sample_count    => dwell_sample_count,
                first_timestamp => dwell_first_timestamp,
                first_window    => dwell_first_window,
                mean_power      => dwell_mean_power,
                noise_floor     => dwell_noise_floor,
                peak_window     => dwell_peak_window,
                triggered       => dwell_triggered,
                measure_valid   => dwell_measure_valid,
                gain_too_high   => dwell_gain_too_high,
                settle_elapsed  => dwell_settle_elapsed,
                rd_index        => dwell_rd_index,
                rd_data         => dwell_rd_data,
                generation      => dwell_generation
            );

    end generate;

    -- Settling sequencer. Shares the dwell boundary with the analyser and
    -- consumes its summary, so the clip verdict describes the dwell that
    -- just ended rather than the one starting.
    --
    -- It changes no gain: the AD9361 owns that loop. This only says when the
    -- receiver has settled, so the host knows which part of a dwell is worth
    -- trusting, and whether the previous one clipped past the point where
    -- its numbers mean anything.
    --
    -- Its own generate, not nested inside the analyser's, so
    -- ENABLE_GAIN_SEQUENCER actually decides something. Nested, the generic
    -- would be accepted and ignored -- the worst kind of switch. It does
    -- depend on the analyser for its inputs, hence the "and" below rather
    -- than the generic alone.
    -- Pre-trigger history. The trigger necessarily fires after the onset,
    -- so without this every capture starts mid-burst and the preamble --
    -- the part that identifies an emitter -- is already gone.
    --
    -- 4096 samples is 67 us at 61.44 MHz and 13 M10K, half the 26 free on
    -- the current build. Gated by ENABLE_TRIGGER_CAPTURE, which is what
    -- that generic was reserved for; it stays false in hosted so the
    -- memory is only spent where it is used.
    -- Dwell summary read. The index shares the address register with the
    -- ring: bits 11:0 select a ring entry, bits 19:16 a summary word. One
    -- register because the host reads one or the other, never both at once,
    -- and a second PIO would cost a Qsys instance to save nothing.
    --
    -- Index 15 returns the generation counter, and that multiplexing lives
    -- inside dwell_readout rather than here on purpose: routed through the
    -- block it arrives already latched on rx_clock, so the host cannot catch
    -- it mid-increment. A mux here would carry a 16-bit rx_clock counter
    -- straight into a system-domain word -- the same trap as oldest_index,
    -- just harder to see.
    dwell_rd_index <= unsigned(pretrig_addr_word(19 downto 16));

    -- Widened rather than truncated: gain_sequencer's counter is
    -- SETTLE_LOG2+1 bits and the readout word holds 16, so this cannot lose
    -- a value even if SETTLE_LOG2 is raised. Zero when the sequencer is not
    -- built, which is what its default already says.
    dwell_settle_elapsed <= resize(dwell_settle_raw, 16);

    -- Trigger threshold, host-programmed, as mantissa and shift.
    --
    -- The threshold is compared against a window energy sum: 1024 samples of
    -- I^2+Q^2 at full scale 2048 reaches 2^33, so the field is 48 bits wide
    -- and a PIO carries 32. Rather than a second register for the top bits,
    -- the host writes a 24-bit mantissa in 23:0 and a shift in 29:24, giving
    -- mantissa << shift. That covers the whole range at a resolution far
    -- finer than any threshold is ever set to -- this decides "is there
    -- energy here", not a calibrated level.
    --
    -- ⛔ Zero still means the trigger is disabled and measurement continues.
    -- That is the reset state and the safe one: dwell_summary reports every
    -- dwell either way, and a threshold nobody set must not silently start
    -- classifying dwells.
    -- The register lives in the system domain and the analyser runs on
    -- rx_clock, so the word crosses through the same handshake block every
    -- other bundled-data crossing in this design uses -- and which the SDC
    -- already constrains as a pair. Synchronising 30 bits individually
    -- could hand the analyser a threshold that was never written.
    --
    -- Config word from the Nios export, crossed into the rx domain. See
    -- drive_handshake_dwell_cfg below for why the request has to cycle.
    U_dwell_cfg_handshake : entity work.handshake
        generic map ( DATA_WIDTH => 32 )
        port map (
            source_reset => sys_reset,
            source_clock => sys_clock,
            source_data  => dwell_cfg_word,
            dest_reset   => rx_reset,
            dest_clock   => rx_clock,
            dest_data    => dwell_cfg_wire,
            dest_req     => dwell_cfg_req_rx,
            dest_ack     => dwell_cfg_ack_rx
        );

    -- Request must cycle, or the crossing transfers exactly once and stops.
    --
    -- Measured, not reasoned: with dest_req tied to '1', handshake clears
    -- source_ack only on `source_req = '0'` (handshake.vhd:68), and
    -- source_req is the synchronised dest_req. Held high, source_ack latches
    -- on the first transfer and never clears, so source_holding is loaded
    -- once after reset and never reloaded. A testbench driving this instance
    -- reprogrammed the word from AAAA0001 to BBBB0002 and the destination
    -- stayed at AAAA0001 for the rest of the run.
    --
    -- The effect on the product: the host could set the dwell threshold and
    -- settle select once, and every later write would be silently ignored --
    -- the analyser would keep triggering against whatever was programmed
    -- first. The old comment here claimed the opposite ("holding it high
    -- means the newest word is always on its way across").
    --
    -- Same req/ack cycle as drive_handshake_timestamp above, which is the
    -- working instance of this protocol in this file.
    drive_handshake_dwell_cfg : process( rx_clock, rx_reset )
    begin
        if( rx_reset = '1' ) then
            dwell_cfg_req_rx <= '0';
        elsif( rising_edge(rx_clock) ) then
            if( dwell_cfg_ack_rx = '0' ) then
                dwell_cfg_req_rx <= '1';
            else
                dwell_cfg_req_rx <= '0';
            end if;
        end if;
    end process;

    -- Capture register in the rx domain.
    --
    -- handshake has no destination register of its own -- handshake.vhd:56
    -- is `dest_data <= source_holding`, a plain wire from the source-domain
    -- flops. Consuming that wire combinationally, as this did, means the
    -- crossing has no capture event at all: there is nothing for
    -- set_max_skew to bound, which is exactly what Quartus reported
    -- (`handshake crossing not matched`, 6 instances, 5 pairs written).
    -- Adding a pattern to the SDC could not have fixed that -- an unbounded
    -- wire is not made coherent by constraining it.
    --
    -- This is bundled data, not telemetry: bits [29:24] are a shift and
    -- [23:0] a mantissa, and dwell_threshold builds both into ONE
    -- expression. A word assembled from two different writes -- an old
    -- mantissa with a new shift -- yields a threshold that was never
    -- programmed, and the analyser would act on it.
    --
    -- dest_ack is already synchronised into this domain by handshake's own
    -- U_sync_ack, and it rises only after source_holding has been loaded,
    -- so it is the stable-data indication; no extra synchroniser is needed.
    -- Config changes between dwells, so capturing on the ack edge costs
    -- nothing and the value is steady for the whole dwell that reads it.
    dwell_cfg_capture : process( rx_clock, rx_reset )
    begin
        if( rx_reset = '1' ) then
            dwell_cfg_rx <= (others => '0');
        elsif( rising_edge(rx_clock) ) then
            if( dwell_cfg_ack_rx = '1' ) then
                dwell_cfg_rx <= dwell_cfg_wire;
            end if;
        end if;
    end process;

    -- Shift saturated at 24, not masked: 24 + the 24-bit mantissa is exactly
    -- the 48-bit field. A larger shift left unclamped would push the value
    -- off the top and read as zero -- which means "trigger disabled", the
    -- opposite of the very high threshold that was asked for.
    dwell_shift <= to_integer(unsigned(dwell_cfg_rx(29 downto 24)))
                   when unsigned(dwell_cfg_rx(29 downto 24)) <= 24
                   else 24;

    -- Registered, not combinational.
    --
    -- Measured (job 54, Slow 1100mV 85C): all five worst setup paths in the
    -- LVDS pll_sclk domain started at this crossing and ended in
    -- dwell_summary's trigger registers, slack -11.858, eight logic levels,
    -- 13.124 ns of data delay. The chain was
    --
    --     source_holding[28] -> dwell_shift -> ShiftLeft0 (48-bit barrel)
    --                        -> LessThan5 (4 levels) -> trig_window[8]
    --
    -- The capture register above cut the crossing itself; this cuts the
    -- rest, so the barrel shifter and the threshold comparison no longer
    -- share one edge. Pipelining dwell_summary's arithmetic did nothing for
    -- this domain because none of it was on this path -- job 54 moved the
    -- slack by 36 ps, which is placement noise.
    --
    -- A cycle of latency is free here: the host reprogrammes between
    -- dwells, and the analyser compares against a threshold that has been
    -- steady for the whole dwell either way.
    dwell_threshold_reg : process( rx_clock, rx_reset )
    begin
        if( rx_reset = '1' ) then
            dwell_threshold <= (others => '0');
        elsif( rising_edge(rx_clock) ) then
            dwell_threshold <= shift_left(
                resize(unsigned(dwell_cfg_rx(23 downto 0)), 48), dwell_shift);
        end if;
    end process;

    -- Read address for the ring, from the host. Only the low DEPTH_LOG2
    -- bits mean anything; the rest are ignored rather than checked, since a
    -- wider write can only select an entry that exists.
    pretrig_rd_addr <= unsigned(pretrig_addr_word(11 downto 0));

    -- Dwell status word. Every bit crosses from rx_clock through its own
    -- synchroniser, same rule as RF_LINK_STATUS: no raw rx_* signal is
    -- assembled into a word read in the system domain.
    --
    -- oldest_index is the exception and is deliberately NOT synchronised
    -- bit by bit -- a 12-bit counter crossed that way can be sampled
    -- mid-change and yield an index that never existed. It is only read
    -- while frozen is set, at which point it has been static for however
    -- long the host took to notice, so the host reads it after seeing
    -- frozen and gets a settled value.
    dwell_status_word(0)            <= pretrig_frozen_sys;
    dwell_status_word(1)            <= pretrig_wrapped_sys;
    dwell_status_word(2)            <= dwell_triggered_sys;
    dwell_status_word(3)            <= dwell_measure_valid_sys;
    dwell_status_word(4)            <= dwell_gain_too_high_sys;
    dwell_status_word(15 downto 5)  <= (others => '0');
    dwell_status_word(27 downto 16) <= std_logic_vector(pretrig_oldest);
    dwell_status_word(31 downto 28) <= "0001";

    gen_trigger_capture : if( ENABLE_SWEEP_ANALYZER and ENABLE_TRIGGER_CAPTURE ) generate

        U_pretrigger : entity work.pretrigger_buffer
            generic map ( DEPTH_LOG2 => 12 )
            port map (
                clock        => rx_clock,
                reset        => rx_reset,
                sample       => adc_streams(0),
                trigger      => dwell_triggered,
                dwell_start  => dwell_start,
                frozen       => pretrig_frozen,
                oldest_index => pretrig_oldest,
                wrapped      => pretrig_wrapped,
                rd_addr      => pretrig_rd_addr,
                rd_data      => pretrig_rd_data
            );

    end generate;

    gen_gain_sequencer : if( ENABLE_SWEEP_ANALYZER and ENABLE_GAIN_SEQUENCER ) generate

        U_gain_sequencer : entity work.gain_sequencer
            port map (
                clock          => rx_clock,
                reset          => rx_reset,
                dwell_start    => dwell_start,
                sample_valid   => adc_streams(0).data_v,
                settle_sel     => unsigned(dwell_cfg_rx(31 downto 30)),
                summary_valid  => dwell_summary_valid,
                clip_count     => dwell_clip_count,
                sample_count   => dwell_sample_count,
                measure_valid  => dwell_measure_valid,
                gain_too_high  => dwell_gain_too_high,
                settle_elapsed => dwell_settle_raw
            );

    end generate;
    adi_en_agc     <= unpack(rffe_gpio.o).en_agc;
    adi_txnrx      <= unpack(rffe_gpio.o).txnrx;
    adi_enable     <= unpack(rffe_gpio.o).enable;
    adi_reset_n    <= unpack(rffe_gpio.o).reset_n;

    -- Unpack trigger GPIO bits into records
    rx_trigger_ctl <= unpack(rx_trigger_ctl_i, rx_trigger_line);
    tx_trigger_ctl <= unpack(tx_trigger_ctl_i, tx_trigger_line);

    -- LEDs
    led(1) <= led1_blink        when nios_gpio.o.led_mode = '0' else not nios_gpio.o.leds(1);
    led(2) <= tx_underflow_led  when nios_gpio.o.led_mode = '0' else not nios_gpio.o.leds(2);
    led(3) <= rx_overflow_led   when nios_gpio.o.led_mode = '0' else not nios_gpio.o.leds(3);

    -- DAC SPI (data latched on falling edge)
    dac_sclk <= not nios_sclk when nios_gpio.o.adf_chip_enable = '0' else '0';
    dac_sdi  <= nios_sdio     when nios_gpio.o.adf_chip_enable = '0' else '0';
    dac_csn  <= nios_ss_n(0)  when nios_gpio.o.adf_chip_enable = '0' else '1';

    -- ADF SPI (data latched on rising edge)
    adf_sclk <= nios_sclk    when nios_gpio.o.adf_chip_enable = '1' else '0';
    adf_sdi  <= nios_sdio    when nios_gpio.o.adf_chip_enable = '1' else '0';
    adf_csn  <= nios_ss_n(1) when nios_gpio.o.adf_chip_enable = '1' else '1';
    adf_ce   <= nios_gpio.o.adf_chip_enable;

    nios_sdo <= adf_muxout when ((nios_ss_n(1) = '0') and (nios_gpio.o.adf_chip_enable = '1'))
                else '0';

    -- Power monitor I2C
    pwr_scl     <= i2c_scl_out when i2c_scl_oen = '0' else 'Z';
    pwr_sda     <= i2c_sda_out when i2c_sda_oen = '0' else 'Z';

    i2c_scl_in  <= pwr_scl;
    i2c_sda_in  <= pwr_sda;

    -- TPS2115A status
    nios_gpio.i.pwr_status <= pwr_status;

    -- RF_LINK_STATUS composition. Every source here is EITHER a _sys
    -- synchronized copy of a tx_clock/rx_clock flag, OR already native to
    -- sys_clock (nios_gpio.o.usb_speed, rf_link_speed_disagree, the epoch
    -- mirror above) -- see the grep in the ADR report for the invariant
    -- this is meant to hold.
    rf_link_status(0)            <= tx_link_active_sys;
    rf_link_status(1)            <= tx_speed_latched_sys;
    rf_link_status(2)            <= nios_gpio.o.usb_speed;
    -- bit 3 was tx_usb_speed_mismatch OR rx_usb_speed_mismatch, combining two
    -- unrelated clock domains combinationally. Deleted: redundant with bits
    -- 4/5 below, which the host can OR itself.
    rf_link_status(3)            <= '0';
    rf_link_status(4)            <= rx_usb_speed_mismatch_sys;
    rf_link_status(5)            <= tx_usb_speed_mismatch_sys;
    -- bit 6 was the same OR-of-two-domains pattern as bit 3, over
    -- protocol_start_violation instead. Deleted for the same reason,
    -- redundant with bits 7/18.
    rf_link_status(6)            <= '0';
    rf_link_status(7)            <= rx_protocol_start_violation_sys;
    -- Epoch acknowledgement. Each direction mirrors the toggle it actually
    -- acted on, so comparing that mirror against the toggle we issued proves
    -- the direction consumed THIS epoch -- not merely that it is running.
    --
    -- link_active cannot answer this: it rises with the start but also drops
    -- on stop or abort while the toggle stands still, so it would read "not
    -- applied" after a clean stop even though the start was consumed. Nor can
    -- the top level re-derive an ack from its own copy of the toggle: that
    -- would prove only that the top level knows what it sent.
    --
    -- epoch_valid is separate on purpose. After reset the issued toggle and a
    -- zeroed ack compare equal, so the equality alone would report an applied
    -- epoch before any direction had consumed one. epoch_valid is cleared by
    -- reset and set only by an actual start, so the host has to see both.
    rf_link_status(8)            <= rx_epoch_current;
    rf_link_status(9)            <= tx_epoch_current;
    rf_link_status(10)           <= rx_epoch_current and tx_epoch_current;
    rf_link_status(11)           <= rx_epoch_valid_sys;
    rf_link_status(12)           <= tx_epoch_valid_sys;
    rf_link_status(13)           <= rf_link_speed_disagree;
    -- Sticky faults, one aggregate bit per direction.
    --
    -- Five fault bits exist per direction (fifo_writer/fifo_reader:
    -- SPEED_MISMATCH, START_NO_PROGRESS, GPIF_TIMEOUT, PROTOCOL_ERROR,
    -- FIFO_ABORT) and there are three free bits in this word, so the
    -- per-bit detail cannot go here. The aggregate answers the question the
    -- host asks first -- "did this direction fault since the last clear" --
    -- and the individual bits remain readable where they are latched.
    --
    -- OR-reduced in its OWN clock domain before crossing, not after: an OR
    -- of five separately synchronised bits could glitch on a cycle where
    -- two of them settle differently, which is the two-domain OR this word
    -- already had deleted at bits 3 and 6.
    rf_link_status(14)           <= rx_fault_any_sys;
    rf_link_status(15)           <= tx_fault_any_sys;
    rf_link_status(16)           <= rx_link_active_sys;
    rf_link_status(17)           <= rx_speed_latched_sys;
    rf_link_status(18)           <= tx_protocol_start_violation_sys;
    -- Abort in progress, RX only. Distinct from the sticky fault above: a
    -- fault says something went wrong at some point, this says the direction
    -- is in an aborted state right now and is not moving samples.
    --
    -- Not "rx or tx". One bit is free and the OR of two directions is
    -- exactly the pattern deleted at bits 3 and 6 -- it tells the host
    -- something is aborted without saying which, which is the answer that
    -- needs a second read anyway. RX is the direction this instrument
    -- collects on; TX abort stays readable at its latch and gets its own bit
    -- when the next format version widens the word.
    rf_link_status(19)           <= rx_abort_active_sys;
    -- Full eight bits, not the low four. This is the sequence number the host
    -- reads while retrying a link start, and four bits wrap after sixteen
    -- attempts -- which is well within one bad recovery session, exactly when
    -- the number needs to still mean something.
    rf_link_status(27 downto 20) <= std_logic_vector(rf_epoch_count_sys);
    rf_link_status(31 downto 28) <= "0001";

    -- SI53304 controls / clock output enables
    -- Inert termination for the Wishbone extension conduit.
    --
    -- This image attaches no Wishbone target, but the bridge stays mapped at
    -- 0x10000000 on IRQ 10, so the Nios can still address it. It must not be
    -- left unacknowledged: wishbone_master.vhd leaves WAIT_FOR_REQ only on
    -- wb_ack_i = '1' (line 209), so a permanently low ack makes any access to
    -- that region wait forever and stalls the Avalon data master with it. That
    -- is exactly what the previous "wbm_wb_ack_i <= '0'" here would have done.
    --
    -- So acknowledge immediately instead: writes are discarded, reads return
    -- zero, and a stray access completes rather than hanging the CPU. The
    -- bridge exposes no wb_err_i (ports at lines 59-61 are stb/ack/cyc only),
    -- so a bus error is not available as a cleaner answer. wb_stb_o is tied
    -- high inside the bridge and wb_cyc_o follows its request FIFO, so the
    -- request predicate is cyc and stb.
    --
    -- This is compatibility, not support: an out-of-tree Wishbone peripheral
    -- still will not work against this image, it just fails predictably.
    wbm_wb_clk_i <= sys_clock;
    wbm_wb_rst_i <= sys_reset;
    wbm_wb_ack_i <= wbm_wb_cyc_o and wbm_wb_stb_o;
    wbm_wb_dat_i <= (others => '0');

    si_clock_sel <= nios_gpio.o.si_clock_sel;
    c5_clock2_oe <= '1';
    exp_clock_oe <= exp_present and exp_clock_req;
    ufl_clock_oe <= nios_gpio.o.ufl_clock_oe;

    -- Expansion I2C
    exp_i2c_scl <= 'Z';
    exp_i2c_sda <= 'Z';

    -- Expansion GPIO outputs
    generate_xb_gpio_out : for i in exp_gpio'range generate
        exp_gpio(i) <= nios_xb_gpio_out(i) when nios_xb_gpio_oe(i) = '1' else 'Z';
    end generate;

    tx_packet_ready <= '1';

    -- TX Submodule
    U_tx : entity work.tx
        generic map (
            NUM_STREAMS          => dac_controls'length
        )
        port map (
            tx_reset             => tx_reset,
            tx_clock             => tx_clock,
            tx_enable            => tx_enable,

            meta_en              => meta_en_tx,
            timestamp_reset      => tx_ts_reset,
            usb_speed            => usb_speed_tx,
            tx_underflow_led     => tx_underflow_led,
            tx_underflow_count   => tx_underflow_count,
            tx_timestamp         => tx_timestamp,

            -- Link status
            link_start_toggle          => link_start_toggle_tx,
            link_stop_toggle           => link_stop_toggle_tx,
            clear_fault_toggle         => clear_fault_toggle_tx,
            usb_speed_mismatch         => tx_usb_speed_mismatch,
            link_active                => tx_link_active,
            speed_latched              => tx_speed_latched,
            protocol_start_violation   => tx_protocol_start_violation,
            link_epoch_counter         => tx_link_epoch_counter,
            fault_sticky               => tx_fault_sticky,
            abort_active               => tx_abort_active,
            epoch_ack                  => tx_epoch_ack,
            epoch_valid                => tx_epoch_valid,

            -- Triggering
            trigger_arm          => tx_trigger_ctl.arm,
            trigger_fire         => tx_trigger_ctl.fire,
            trigger_master       => tx_trigger_ctl.master,
            trigger_line         => tx_trigger_line,

            -- Eightbit mode
            eight_bit_mode_en    => eightbit_en_tx,
            highly_packed_mode_en => highly_packed_en_txrx,

            -- Packet FIFO. The two status outputs are left open: TX packet
            -- mode runs without flow control here -- packet_ready is tied
            -- high above -- so nothing consumes them, and wiring them to
            -- signals only produced "assigned but never read" warnings.
            packet_en            => packet_en_tx,
            packet_empty         => open,
            packet_control       => open,
            packet_ready         => tx_packet_ready,

            -- Samples from host via FX3
            sample_fifo_wclock   => fx3_pclk_pll,
            sample_fifo_wreq     => tx_sample_fifo.wreq,
            sample_fifo_wdata    => tx_sample_fifo.wdata,
            sample_fifo_wempty   => tx_sample_fifo.wempty,
            sample_fifo_wfull    => tx_sample_fifo.wfull,
            sample_fifo_wused    => tx_sample_fifo.wused,

            -- Metadata from host via FX3
            meta_fifo_wclock     => fx3_pclk_pll,
            meta_fifo_wreq       => tx_meta_fifo.wreq,
            meta_fifo_wdata      => tx_meta_fifo.wdata,
            meta_fifo_wempty     => tx_meta_fifo.wempty,
            meta_fifo_wfull      => tx_meta_fifo.wfull,
            meta_fifo_wused      => tx_meta_fifo.wused,

            -- Digital Loopback Interface
            loopback_enabled     => tx_loopback_enabled,
            loopback_fifo_wclock => tx_loopback_fifo.wclock,
            loopback_fifo_wdata  => tx_loopback_fifo.wdata,
            loopback_fifo_wreq   => tx_loopback_fifo.wreq,
            loopback_fifo_wfull  => tx_loopback_fifo.wfull,
            loopback_fifo_wused  => tx_loopback_fifo.wused,

            -- RFFE Interface
            dac_controls         => dac_controls,
            dac_streams          => dac_streams
        );

    dac_assignment_proc : process( all )
    begin
        for i in dac_controls'range loop
            dac_controls(i).enable   <= (ad9361.ch(i).dac.i.enable or ad9361.ch(i).dac.q.enable or tx_loopback_enabled) and
                                        mimo_tx_enables(i);
            dac_controls(i).data_req <= (ad9361.ch(i).dac.i.valid  or ad9361.ch(i).dac.q.valid  or tx_loopback_enabled) and
                                        mimo_tx_enables(i);

            if (rising_edge(tx_clock) and dac_streams(i).data_v = '1') then
                ad9361.ch(i).dac.i.data  <= std_logic_vector(dac_streams(i).data_i(11 downto 0)) & "0000";
                ad9361.ch(i).dac.q.data  <= std_logic_vector(dac_streams(i).data_q(11 downto 0)) & "0000";
            end if;
        end loop;
    end process;

    -- RX Submodule
    U_rx : entity work.rx
        generic map (
            NUM_STREAMS            => adc_controls'length
        )
        port map (
            rx_reset               => rx_reset,
            rx_clock               => rx_clock,
            rx_enable              => rx_enable,

            meta_en                => meta_en_rx,
            timestamp_reset        => rx_ts_reset,
            usb_speed              => usb_speed_rx,
            rx_mux_sel             => rx_mux_sel,
            rx_overflow_led        => rx_overflow_led,
            rx_overflow_count      => rx_overflow_count,
            rx_timestamp           => rx_timestamp,

            -- Link status
            link_start_toggle          => link_start_toggle_rx,
            link_stop_toggle           => link_stop_toggle_rx,
            clear_fault_toggle         => clear_fault_toggle_rx,
            usb_speed_mismatch         => rx_usb_speed_mismatch,
            link_active                => rx_link_active,
            speed_latched              => rx_speed_latched,
            protocol_start_violation   => rx_protocol_start_violation,
            link_epoch_counter         => rx_link_epoch_counter,
            fault_sticky               => rx_fault_sticky,
            abort_active               => rx_abort_active,
            epoch_ack                  => rx_epoch_ack,
            epoch_valid                => rx_epoch_valid,

            -- Triggering
            trigger_arm            => rx_trigger_ctl.arm,
            trigger_fire           => rx_trigger_ctl.fire,
            trigger_master         => rx_trigger_ctl.master,
            trigger_line           => rx_trigger_line,

            -- Packed modes
            eight_bit_mode_en      => eightbit_en_rx,
            highly_packed_mode_en  => highly_packed_en_txrx,

            -- Packet FIFO
            packet_en              => packet_en_rx,
            packet_control         => rx_packet_control,
            packet_ready           => rx_packet_ready,

            -- Samples to host via FX3
            sample_fifo_rclock     => fx3_pclk_pll,
            sample_fifo_raclr      => not rx_enable_pclk,
            sample_fifo_rreq       => rx_sample_fifo.rreq,
            sample_fifo_rdata      => rx_sample_fifo.rdata,
            sample_fifo_rempty     => rx_sample_fifo.rempty,
            sample_fifo_rfull      => rx_sample_fifo.rfull,
            sample_fifo_rused      => rx_sample_fifo.rused,

            -- Mini expansion signals
            mini_exp               => mini_exp2 & mini_exp1,

            -- Metadata to host via FX3
            meta_fifo_rclock       => fx3_pclk_pll,
            meta_fifo_raclr        => not rx_enable_pclk,
            meta_fifo_rreq         => rx_meta_fifo.rreq,
            meta_fifo_rdata        => rx_meta_fifo.rdata,
            meta_fifo_rempty       => rx_meta_fifo.rempty,
            meta_fifo_rfull        => rx_meta_fifo.rfull,
            meta_fifo_rused        => rx_meta_fifo.rused,

            -- Digital Loopback Interface
            loopback_fifo_wenabled => tx_loopback_enabled,
            loopback_fifo_wreset   => tx_reset,
            loopback_fifo_wclock   => tx_loopback_fifo.wclock,
            loopback_fifo_wdata    => tx_loopback_fifo.wdata,
            loopback_fifo_wreq     => tx_loopback_fifo.wreq,
            loopback_fifo_wfull    => tx_loopback_fifo.wfull,
            loopback_fifo_wused    => tx_loopback_fifo.wused,

            -- RFFE Interface
            adc_controls           => adc_controls,
            adc_streams            => adc_streams
        );

    -- The per-channel enable is a configuration term: it comes from the AD9361
    -- control-register bundle through up_xfer_cntrl, is combined here with the
    -- MIMO enable, and is then consumed across the hierarchy in fifo_writer's
    -- meta FSM. Left combinational it was the design-wide worst setup path,
    -- with 1.8 ns of the budget spent on the single interconnect hop from this
    -- process into fifo_writer. Registering it in rx_clock keeps that hop
    -- between two flops instead of inside one cycle's logic cone.
    --
    -- Only .enable is registered. adc_streams(*).data_* stay combinational:
    -- they are the sample stream itself, and delaying them by a cycle would
    -- shift data against the valid strobe and the timestamp.
    adc_enable_reg_proc : process( rx_clock )
    begin
        if( rising_edge( rx_clock ) ) then
            for i in adc_controls'range loop
                adc_enable_r(i) <= (ad9361.ch(i).adc.i.enable or
                                    ad9361.ch(i).adc.q.enable) and
                                   mimo_rx_enables(i);
            end loop;
        end if;
    end process;

    adc_assignment_proc : process( all )
    begin
        for i in adc_controls'range loop
            adc_controls(i).enable   <= adc_enable_r(i);
            adc_controls(i).data_req <= '1';
            adc_streams(i).data_i    <= signed(ad9361.ch(i).adc.i.data);
            adc_streams(i).data_q    <= signed(ad9361.ch(i).adc.q.data);
            adc_streams(i).data_v    <= (ad9361.ch(i).adc.i.valid  or ad9361.ch(i).adc.q.valid) and not adc_streams_last_v(i);
        end loop;
    end process;

    process(rx_clock)
    begin
        if( rx_reset = '1' ) then
            adc_streams_last_v  <= ( others => '0' ) ;
        elsif( rising_edge( rx_clock ) ) then
            for i in adc_controls'range loop
                adc_streams_last_v(i)  <= ad9361.ch(i).adc.i.valid  or ad9361.ch(i).adc.q.valid;
            end loop;
        end if;
    end process;

    -- ========================================================================
    -- RESET SYNCHRONIZERS
    -- ========================================================================

    U_reset_sync_pclk : entity work.reset_synchronizer
        generic map (
            INPUT_LEVEL         =>  '1',
            OUTPUT_LEVEL        =>  '1'
        )
        port map (
            clock               =>  fx3_pclk_pll,
            async               =>  sys_reset_async,
            sync                =>  sys_reset_pclk
        );

    U_reset_sync_sys : entity work.reset_synchronizer
        generic map (
            INPUT_LEVEL         =>  '1',
            OUTPUT_LEVEL        =>  '1'
        )
        port map (
            clock               =>  sys_clock,
            async               =>  sys_reset_async,
            sync                =>  sys_reset
        );

    U_reset_sync_rx : entity work.reset_synchronizer
        generic map (
            INPUT_LEVEL         =>  '1',
            OUTPUT_LEVEL        =>  '1'
        )
        port map (
            clock               =>  rx_clock,
            async               =>  sys_reset_pclk,
            sync                =>  rx_reset
        );

    U_reset_sync_tx : entity work.reset_synchronizer
        generic map (
            INPUT_LEVEL         =>  '1',
            OUTPUT_LEVEL        =>  '1'
        )
        port map (
            clock               =>  tx_clock,
            async               =>  sys_reset_pclk,
            sync                =>  tx_reset
        );


    -- ========================================================================
    -- SYNCHRONIZERS
    -- ========================================================================

    U_sync_usb_speed_pclk : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  '0',
            clock               =>  fx3_pclk_pll,
            async               =>  nios_gpio.o.usb_speed,
            sync                =>  usb_speed_pclk
        );

    -- The datapath FIFOs take the speed the host DECLARED for this epoch, not
    -- the live vendor GPIO bit. The two are compared in rf_link_controller
    -- and a disagreement refuses the epoch outright, so by the time a start
    -- reaches the FIFOs the two agree by construction.
    --
    -- The FX3 GPIF path above deliberately still reads nios_gpio.o.usb_speed:
    -- that bit is libbladeRF's own, it maintains it automatically, and taking
    -- it away from that path would be a compatibility regression.
    U_sync_usb_speed_rx : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  '0',
            clock               =>  rx_clock,
            async               =>  rf_link_req_speed,
            sync                =>  usb_speed_rx
        );

    U_sync_usb_speed_tx : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  '0',
            clock               =>  tx_clock,
            async               =>  rf_link_req_speed,
            sync                =>  usb_speed_tx
        );


    -- One decoder for the whole link protocol, in the system clock domain.
    -- Everything downstream of it sees debounced toggles, never raw PIO bits,
    -- so there is exactly one place that knows what a start means.
    --
    -- usb_speed_live is the vendor GPIO bit the host already maintains
    -- (BLADERF_GPIO_FEATURE_SMALL_DMA_XFER). It is an observation here, not
    -- an authority: the controller compares it against the speed the host
    -- requested and refuses the epoch if they disagree, rather than guessing
    -- which of the two is right.
    U_rf_link_controller : entity work.rf_link_controller
        port map (
            clock                    =>  sys_clock,
            reset                    =>  sys_reset,
            cfg_word                 =>  rf_link_cfg_word,
            usb_speed_live           =>  nios_gpio.o.usb_speed,
            start_toggle_out         =>  rf_link_start_toggle,
            stop_toggle_out          =>  rf_link_stop_toggle,
            clear_fault_out          =>  rf_link_clear_fault,
            requested_speed          =>  rf_link_req_speed,
            start_speed_disagreement =>  rf_link_speed_disagree,
            host_epoch_tag           =>  rf_link_epoch_tag
        );

    -- RF_LINK_STATUS CDC: synchronize the eight tx_*/rx_* single-bit flags
    -- from tx_clock/rx_clock into sys_clock. Using the same synchronizer.vhd
    -- entity and instantiation style as every other crossing in this file.
    U_sync_tx_link_active : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  sys_reset,
            clock               =>  sys_clock,
            async               =>  tx_link_active,
            sync                =>  tx_link_active_sys
        );

    U_sync_tx_speed_latched : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  sys_reset,
            clock               =>  sys_clock,
            async               =>  tx_speed_latched,
            sync                =>  tx_speed_latched_sys
        );

    U_sync_tx_usb_speed_mismatch : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  sys_reset,
            clock               =>  sys_clock,
            async               =>  tx_usb_speed_mismatch,
            sync                =>  tx_usb_speed_mismatch_sys
        );

    U_sync_tx_protocol_start_violation : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  sys_reset,
            clock               =>  sys_clock,
            async               =>  tx_protocol_start_violation,
            sync                =>  tx_protocol_start_violation_sys
        );

    -- Epoch acknowledgement coming back. These are levels that change only on
    -- an epoch boundary, so an ordinary two-flop synchroniser is the right
    -- treatment; there is no multi-bit word to keep coherent.
    U_sync_rx_epoch_ack : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  sys_reset,
            clock               =>  sys_clock,
            async               =>  rx_epoch_ack,
            sync                =>  rx_epoch_ack_sys
        );

    U_sync_tx_epoch_ack : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  sys_reset,
            clock               =>  sys_clock,
            async               =>  tx_epoch_ack,
            sync                =>  tx_epoch_ack_sys
        );

    U_sync_rx_epoch_valid : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  sys_reset,
            clock               =>  sys_clock,
            async               =>  rx_epoch_valid,
            sync                =>  rx_epoch_valid_sys
        );

    U_sync_tx_epoch_valid : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  sys_reset,
            clock               =>  sys_clock,
            async               =>  tx_epoch_valid,
            sync                =>  tx_epoch_valid_sys
        );

    -- Compare against the toggle that actually went to the directions, not
    -- against the local mirror of it: the mirror only proves what we sent.
    -- The validity term belongs here, not only in the applied bit: after reset
    -- the issued toggle and a zeroed acknowledgement compare equal, so the
    -- comparison alone would report "current" for a direction that has never
    -- consumed an epoch.
    rx_epoch_current <= '1' when rx_epoch_valid_sys = '1' and
                                 rx_epoch_ack_sys = rf_link_start_toggle else '0';
    tx_epoch_current <= '1' when tx_epoch_valid_sys = '1' and
                                 tx_epoch_ack_sys = rf_link_start_toggle else '0';

    -- Reduced where the bits are latched, so only a settled single bit
    -- crosses. Combinational on purpose: fault_sticky is set-dominant and
    -- holds until an explicit clear, so there is no pulse to miss.
    rx_fault_any <= '1' when rx_fault_sticky /= "00000" else '0';
    tx_fault_any <= '1' when tx_fault_sticky /= "00000" else '0';

    -- Dwell/pre-trigger flags into the system domain. Five instances rather
    -- than one wide crossing: each is an independent single-bit level, and
    -- the host reads them as a snapshot where a one-cycle skew between them
    -- changes nothing it can act on.
    U_sync_pretrig_frozen : entity work.synchronizer
        generic map ( RESET_LEVEL => '0' )
        port map ( reset => sys_reset, clock => sys_clock,
                   async => pretrig_frozen, sync => pretrig_frozen_sys );

    U_sync_pretrig_wrapped : entity work.synchronizer
        generic map ( RESET_LEVEL => '0' )
        port map ( reset => sys_reset, clock => sys_clock,
                   async => pretrig_wrapped, sync => pretrig_wrapped_sys );

    U_sync_dwell_triggered : entity work.synchronizer
        generic map ( RESET_LEVEL => '0' )
        port map ( reset => sys_reset, clock => sys_clock,
                   async => dwell_triggered, sync => dwell_triggered_sys );

    U_sync_dwell_measure_valid : entity work.synchronizer
        generic map ( RESET_LEVEL => '0' )
        port map ( reset => sys_reset, clock => sys_clock,
                   async => dwell_measure_valid,
                   sync  => dwell_measure_valid_sys );

    U_sync_dwell_gain_too_high : entity work.synchronizer
        generic map ( RESET_LEVEL => '0' )
        port map ( reset => sys_reset, clock => sys_clock,
                   async => dwell_gain_too_high,
                   sync  => dwell_gain_too_high_sys );

    U_sync_rx_fault_any : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  sys_reset,
            clock               =>  sys_clock,
            async               =>  rx_fault_any,
            sync                =>  rx_fault_any_sys
        );

    U_sync_tx_fault_any : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  sys_reset,
            clock               =>  sys_clock,
            async               =>  tx_fault_any,
            sync                =>  tx_fault_any_sys
        );

    U_sync_rx_abort_active : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  sys_reset,
            clock               =>  sys_clock,
            async               =>  rx_abort_active,
            sync                =>  rx_abort_active_sys
        );

    U_sync_rx_link_active : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  sys_reset,
            clock               =>  sys_clock,
            async               =>  rx_link_active,
            sync                =>  rx_link_active_sys
        );

    U_sync_rx_speed_latched : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  sys_reset,
            clock               =>  sys_clock,
            async               =>  rx_speed_latched,
            sync                =>  rx_speed_latched_sys
        );

    U_sync_rx_usb_speed_mismatch : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  sys_reset,
            clock               =>  sys_clock,
            async               =>  rx_usb_speed_mismatch,
            sync                =>  rx_usb_speed_mismatch_sys
        );

    U_sync_rx_protocol_start_violation : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  sys_reset,
            clock               =>  sys_clock,
            async               =>  rx_protocol_start_violation,
            sync                =>  rx_protocol_start_violation_sys
        );

    -- Host-visible epoch number, entirely in sys_clock: advances on the same
    -- edge that rf_link_start_toggle changes, i.e. the cycle the controller
    -- accepts a start. rf_link_start_toggle is native to this domain, so no
    -- CDC is involved.
    --
    -- There is deliberately no second toggle register here. An earlier version
    -- kept rf_epoch_toggle_sys as a local mirror and the acknowledgements were
    -- compared against it, which would have made the mirror a second source of
    -- truth: if it ever diverged from the toggle actually sent to the
    -- directions, the comparison would have hidden the divergence instead of
    -- exposing it. The counter is diagnostic only and is not what the
    -- acknowledgement is measured against.
    epoch_count : process( sys_clock, sys_reset )
    begin
        if( sys_reset = '1' ) then
            rf_link_start_toggle_prev <= '0';
            rf_epoch_count_sys        <= (others => '0');
        elsif( rising_edge(sys_clock) ) then
            rf_link_start_toggle_prev <= rf_link_start_toggle;
            if( rf_link_start_toggle /= rf_link_start_toggle_prev ) then
                rf_epoch_count_sys <= rf_epoch_count_sys + 1;
            end if;
        end if;
    end process;

    -- Both directions take the SAME start toggle, so one host command is one
    -- shared epoch. Two independent latches would let RX sit in the old
    -- generation while TX moved to the new one, which is the split-brain the
    -- epoch mechanism exists to prevent.
    U_sync_link_start_toggle_rx : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  '0',
            clock               =>  rx_clock,
            async               =>  rf_link_start_toggle,
            sync                =>  link_start_toggle_rx
        );

    U_sync_link_start_toggle_tx : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  '0',
            clock               =>  tx_clock,
            async               =>  rf_link_start_toggle,
            sync                =>  link_start_toggle_tx
        );

    U_sync_link_stop_toggle_rx : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  '0',
            clock               =>  rx_clock,
            async               =>  rf_link_stop_toggle,
            sync                =>  link_stop_toggle_rx
        );

    U_sync_link_stop_toggle_tx : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  '0',
            clock               =>  tx_clock,
            async               =>  rf_link_stop_toggle,
            sync                =>  link_stop_toggle_tx
        );

    U_sync_clear_fault_rx : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  '0',
            clock               =>  rx_clock,
            async               =>  rf_link_clear_fault,
            sync                =>  clear_fault_toggle_rx
        );

    U_sync_clear_fault_tx : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  '0',
            clock               =>  tx_clock,
            async               =>  rf_link_clear_fault,
            sync                =>  clear_fault_toggle_tx
        );

    U_sync_meta_en_pclk : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  '0',
            clock               =>  fx3_pclk_pll,
            async               =>  nios_gpio.o.meta_sync,
            sync                =>  meta_en_pclk
        );

    U_sync_meta_en_rx : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  '0',
            clock               =>  rx_clock,
            async               =>  nios_gpio.o.meta_sync,
            sync                =>  meta_en_rx
        );

    U_sync_meta_en_tx : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  '0',
            clock               =>  tx_clock,
            async               =>  nios_gpio.o.meta_sync,
            sync                =>  meta_en_tx
        );

    -- No eightbit_en synchroniser into the FX3 domain: fx3_gpif transfers and
    -- counts words, and gpif_buf_size depends only on USB speed, so the FIFOs
    -- hand it ordinary 32-bit words whatever the sample format.

    U_sync_eightbit_en_rx : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  '0',
            clock               =>  rx_clock,
            async               =>  nios_gpio.o.eightbit_en,
            sync                =>  eightbit_en_rx
        );

    U_sync_eightbit_en_tx : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  '0',
            clock               =>  tx_clock,
            async               =>  nios_gpio.o.eightbit_en,
            sync                =>  eightbit_en_tx
        );

    U_sync_highly_packed_en_txrx : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  '0',
            clock               =>  ad9361.clock,
            async               =>  nios_gpio.o.highly_packed_en,
            sync                =>  highly_packed_en_txrx
        );

    U_sync_packet_en_pclk : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  '0',
            clock               =>  fx3_pclk_pll,
            async               =>  nios_gpio.o.packet_en,
            sync                =>  packet_en_pclk
        );

    U_sync_packet_en_rx : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  '0',
            clock               =>  rx_clock,
            async               =>  nios_gpio.o.packet_en,
            sync                =>  packet_en_rx
        );

    U_sync_packet_en_tx : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  '0',
            clock               =>  tx_clock,
            async               =>  nios_gpio.o.packet_en,
            sync                =>  packet_en_tx
        );

    generate_sync_rx_mux_sel : for i in rx_mux_sel'range generate
        U_sync_rx_mux_sel : entity work.synchronizer
            generic map (
                RESET_LEVEL         =>  '0'
            )
            port map (
                reset               =>  '0',
                clock               =>  rx_clock,
                async               =>  nios_gpio.o.rx_mux_sel(i),
                sync                =>  rx_mux_sel(i)
            );
    end generate;

    generate_sync_mimo_rx_en : for i in mimo_rx_enables'range generate
        U_sync_mimo_rx_en : entity work.synchronizer
            generic map (
                RESET_LEVEL         =>  '0'
                )
            port map (
                reset               =>  '0',
                clock               =>  rx_clock,
                async               =>  unpack(rffe_gpio.o).mimo_rx_en(i),
                sync                =>  mimo_rx_enables(i)
            );
    end generate;

    generate_sync_mimo_tx_en : for i in mimo_tx_enables'range generate
        U_sync_mimo_tx_en : entity work.synchronizer
            generic map (
                RESET_LEVEL         =>  '0'
                )
            port map (
                reset               =>  '0',
                clock               =>  tx_clock,
                async               =>  unpack(rffe_gpio.o).mimo_tx_en(i),
                sync                =>  mimo_tx_enables(i)
            );
    end generate;

    generate_sync_adi_ctrl_out : for i in adi_ctrl_out'range generate
        U_sync_adi_ctrl_out : entity work.synchronizer
            generic map (
                RESET_LEVEL         =>  '0'
            )
            port map (
                reset               =>  '0',
                clock               =>  sys_clock,
                async               =>  adi_ctrl_out(i),
                sync                =>  rffe_gpio.i.ctrl_out(i)
            );
    end generate;

    U_sync_adf_muxout : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  '0',
            clock               =>  sys_clock,
            async               =>  adf_muxout,
            sync                =>  rffe_gpio.i.adf_muxout
        );

    generate_sync_xb_gpio_in : for i in exp_gpio'range generate
        U_sync_xb_gpio_in : entity work.synchronizer
          generic map (
            RESET_LEVEL         =>  '0'
          ) port map (
            reset               =>  '0',
            clock               =>  sys_clock,
            async               =>  exp_gpio(i),
            sync                =>  nios_xb_gpio_in(i)
          );
    end generate;

    U_sync_rx_enable : entity work.synchronizer
        generic map (
            RESET_LEVEL =>  '0'
        )
        port map (
            reset       =>  rx_reset,
            clock       =>  rx_clock,
            async       =>  rx_enable_pclk,
            sync        =>  rx_enable
        );

    U_sync_tx_enable : entity work.synchronizer
        generic map (
            RESET_LEVEL =>  '0'
        )
        port map (
            reset       =>  tx_reset,
            clock       =>  tx_clock,
            async       =>  tx_enable_pclk,
            sync        =>  tx_enable
        );


    -- ========================================================================
    -- HANDSHAKES
    -- ========================================================================

    drive_handshake_timestamp : process( fx3_pclk_pll, sys_reset_pclk )
    begin
        if( sys_reset_pclk = '1' ) then
            timestamp_req <= '0';
        elsif( rising_edge(fx3_pclk_pll) ) then
            if( meta_en_pclk = '0' ) then
                timestamp_req <= '0';
            else
                if( timestamp_ack = '0' ) then
                    timestamp_req <= '1';
                elsif( timestamp_ack = '1' ) then
                    timestamp_req <= '0';
                end if;
            end if;
        end if;
    end process;

    U_handshake_timestamp : entity work.handshake
        generic map (
            DATA_WIDTH          =>  tx_timestamp'length
        )
        port map (
            source_clock        =>  tx_clock,
            source_reset        =>  tx_reset,
            source_data         =>  std_logic_vector(tx_timestamp),

            dest_clock          =>  fx3_pclk_pll,
            dest_reset          =>  sys_reset_pclk,
            unsigned(dest_data) =>  fx3_timestamp,
            dest_req            =>  timestamp_req,
            dest_ack            =>  timestamp_ack
        );

    -- Halves of the same capture register, so a host reading both gets one
    -- whole snapshot regardless of the order it asks in.
    rx_ovf_lo_word <= std_logic_vector(rx_ovf_sys_q(31 downto 0));
    rx_ovf_hi_word <= std_logic_vector(rx_ovf_sys_q(63 downto 32));
    tx_unf_lo_word <= std_logic_vector(tx_unf_sys_q(31 downto 0));
    tx_unf_hi_word <= std_logic_vector(tx_unf_sys_q(63 downto 32));

    -- Loss counters into the system domain, one handshake per direction.
    --
    -- The request cycles rather than sitting at '1' -- handshake.vhd:16
    -- says a held request transfers exactly once, which would freeze the
    -- counter at its first value and look exactly like "no drops ever".
    -- That is the failure this whole change exists to remove, so it must
    -- not be reintroduced by the transport.
    --
    -- Free-running: the host polls whenever it wants, and each completed
    -- handshake refreshes the snapshot. A counter is monotonic, so a
    -- slightly stale read understates the loss and never invents one.
    drive_handshake_rx_ovf : process( sys_clock, sys_reset )
    begin
        if( sys_reset = '1' ) then
            rx_ovf_req   <= '0';
            rx_ovf_sys_q <= (others => '0');
        elsif( rising_edge(sys_clock) ) then
            if( rx_ovf_ack = '0' ) then
                rx_ovf_req <= '1';
            else
                rx_ovf_req <= '0';
                -- Capture on ack: the word is guaranteed stable only
                -- between request and acknowledgement.
                rx_ovf_sys_q <= rx_ovf_sys;
            end if;
        end if;
    end process;

    U_handshake_rx_overflow : entity work.handshake
        generic map (
            DATA_WIDTH          =>  rx_overflow_count'length
        )
        port map (
            source_clock        =>  rx_clock,
            source_reset        =>  rx_reset,
            source_data         =>  std_logic_vector(rx_overflow_count),

            dest_clock          =>  sys_clock,
            dest_reset          =>  sys_reset,
            unsigned(dest_data) =>  rx_ovf_sys,
            dest_req            =>  rx_ovf_req,
            dest_ack            =>  rx_ovf_ack
        );

    drive_handshake_tx_unf : process( sys_clock, sys_reset )
    begin
        if( sys_reset = '1' ) then
            tx_unf_req   <= '0';
            tx_unf_sys_q <= (others => '0');
        elsif( rising_edge(sys_clock) ) then
            if( tx_unf_ack = '0' ) then
                tx_unf_req <= '1';
            else
                tx_unf_req <= '0';
                tx_unf_sys_q <= tx_unf_sys;
            end if;
        end if;
    end process;

    U_handshake_tx_underflow : entity work.handshake
        generic map (
            DATA_WIDTH          =>  tx_underflow_count'length
        )
        port map (
            source_clock        =>  tx_clock,
            source_reset        =>  tx_reset,
            source_data         =>  std_logic_vector(tx_underflow_count),

            dest_clock          =>  sys_clock,
            dest_reset          =>  sys_reset,
            unsigned(dest_data) =>  tx_unf_sys,
            dest_req            =>  tx_unf_req,
            dest_ack            =>  tx_unf_ack
        );


end architecture;
