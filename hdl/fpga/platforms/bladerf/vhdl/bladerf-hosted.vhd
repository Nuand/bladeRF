-- Copyright (c) 2013 Nuand LLC
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

library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;
    use ieee.math_real.all ;
    use ieee.math_complex.all ;

library work;
    use work.bladerf_p.all;
    use work.fifo_readwrite_p.all;

architecture hosted_bladerf of bladerf is

    attribute noprune   : boolean ;
    attribute keep      : boolean ;

    alias sys_rst   is fx3_ctl(7) ;
    alias tx_clock  is c4_tx_clock ;
    alias rx_clock  is lms_rx_clock_out ;

    constant NUM_MIMO_STREAMS : natural := 1;

    -- Can be set from libbladeRF using bladerf_set_rx_mux()
    type rx_mux_mode_t is (RX_MUX_NORMAL, RX_MUX_12BIT_COUNTER, RX_MUX_32BIT_COUNTER, RX_MUX_ENTROPY, RX_MUX_DIGITAL_LOOPBACK) ;

    signal rx_mux_sel       : unsigned(2 downto 0) ;
    signal rx_mux_mode      : rx_mux_mode_t ;

    signal \80MHz\          : std_logic ;

    signal nios_gpio        : nios_gpio_t;
    signal nios_gpo_slv     : std_logic_vector(31 downto 0);

    signal nios_xb_gpio_in  : std_logic_vector(31 downto 0) ;
    signal nios_xb_gpio_out : std_logic_vector(31 downto 0) ;
    signal nios_xb_gpio_dir : std_logic_vector(31 downto 0) ;
    signal xb_gpio_dir      : std_logic_vector(31 downto 0) ;

    signal correction_rx_phase_gain :  std_logic_vector(31 downto 0);
    signal correction_tx_phase_gain :  std_logic_vector(31 downto 0);

    signal i2c_scl_in       : std_logic ;
    signal i2c_scl_out      : std_logic ;
    signal i2c_scl_oen      : std_logic ;

    signal i2c_sda_in       : std_logic ;
    signal i2c_sda_out      : std_logic ;
    signal i2c_sda_oen      : std_logic ;

    signal rx_sample_fifo   : rx_fifo_t       := RX_FIFO_T_DEFAULT;
    signal tx_sample_fifo   : tx_fifo_t       := TX_FIFO_T_DEFAULT;
    signal rx_meta_fifo     : meta_fifo_rx_t  := META_FIFO_RX_T_DEFAULT;
    signal tx_meta_fifo     : meta_fifo_tx_t  := META_FIFO_TX_T_DEFAULT;
    signal rx_loopback_fifo : loopback_fifo_t := LOOPBACK_FIFO_T_DEFAULT;

    signal sys_rst_sync     : std_logic ;
    signal sys_rst_80M      : std_logic ;

    signal usb_speed        : std_logic ;
    signal usb_speed_rx     : std_logic ;
    signal usb_speed_tx     : std_logic ;

    signal tx_reset         : std_logic ;
    signal rx_reset         : std_logic ;

    signal pclk_tx_enable   :   std_logic ;
    signal pclk_rx_enable   :   std_logic ;

    signal tx_enable        : std_logic ;
    signal rx_enable        : std_logic ;

    signal meta_en_tx       : std_logic ;
    signal meta_en_rx       : std_logic ;
    signal meta_en_fx3      : std_logic ;
    signal tx_timestamp     : unsigned(63 downto 0) ;
    signal rx_timestamp     : unsigned(63 downto 0) ;
    signal timestamp_sync   : std_logic ;

    signal rx_sample_i      : signed(15 downto 0) ;
    signal rx_sample_q      : signed(15 downto 0) ;
    signal rx_sample_valid  : std_logic ;

    signal rx_samples       : sample_streams_t(0 to NUM_MIMO_STREAMS-1) := (others => ZERO_SAMPLE);

    signal rx_gen_mode      : std_logic ;
    signal rx_gen_i         : signed(15 downto 0) ;
    signal rx_gen_q         : signed(15 downto 0) ;
    signal rx_gen_valid     : std_logic ;

    signal rx_loopback_i     : signed(15 downto 0) := (others =>'0') ;
    signal rx_loopback_q     : signed(15 downto 0) := (others =>'0') ;
    signal rx_loopback_valid : std_logic := '0' ;
    signal rx_loopback_enabled : std_logic := '0' ;
    signal tx_loopback_enabled : std_logic := '0' ;

    signal tx_sample_raw_i : signed(15 downto 0);
    signal tx_sample_raw_q : signed(15 downto 0);
    signal tx_sample_raw_valid : std_logic;

    signal tx_sample_i      : signed(15 downto 0) ;
    signal tx_sample_q      : signed(15 downto 0) ;
    signal tx_sample_valid  : std_logic ;

    signal tx_samples       : sample_streams_t(0 to NUM_MIMO_STREAMS-1) := (others => ZERO_SAMPLE);

    signal fx3_gpif_in      : std_logic_vector(31 downto 0) ;
    signal fx3_gpif_out     : std_logic_vector(31 downto 0) ;
    signal fx3_gpif_oe      : std_logic ;

    signal fx3_ctl_in       : std_logic_vector(12 downto 0) ;
    signal fx3_ctl_out      : std_logic_vector(12 downto 0) ;
    signal fx3_ctl_oe       : std_logic_vector(12 downto 0) ;

    signal tx_underflow_led     :   std_logic ;

    signal rx_overflow_led      :   std_logic ;

    signal lms_rx_data_reg      :   signed(11 downto 0) ;
    signal lms_rx_iq_select_reg :   std_logic ;

    signal rx_mux_i             :   signed(15 downto 0) ;
    signal rx_mux_q             :   signed(15 downto 0) ;
    signal rx_mux_valid         :   std_logic ;

    signal rx_sample_corrected_i : signed(15 downto 0);
    signal rx_sample_corrected_q : signed(15 downto 0);
    signal rx_sample_corrected_valid : std_logic;

    signal led1_blink : std_logic;

    signal nios_sdo : std_logic;
    signal nios_sdio : std_logic;
    signal nios_sclk : std_logic;
    signal nios_ss_n : std_logic_vector(1 downto 0);

    signal correction_valid : std_logic;

    signal correction_tx_phase :  signed(15 downto 0);--to_signed(integer(round(real(2**Q_SCALE) * PHASE_OFFSET)),DC_WIDTH);
    signal correction_tx_gain  :  signed(15 downto 0);--to_signed(integer(round(real(2**Q_SCALE) * DC_OFFSET_REAL)),DC_WIDTH);
    signal correction_rx_phase :  signed(15 downto 0);--to_signed(integer(round(real(2**Q_SCALE) * PHASE_OFFSET)),DC_WIDTH);
    signal correction_rx_gain  :  signed(15 downto 0);--to_signed(integer(round(real(2**Q_SCALE) * DC_OFFSET_REAL)),DC_WIDTH);

    signal command_serial_in    :   std_logic ;
    signal command_serial_out   :   std_logic ;

    constant FPGA_DC_CORRECTION :  signed(15 downto 0) := to_signed(integer(0), 16);

    signal fx3_pclk_pll     :   std_logic ;

    signal timestamp_req    :   std_logic ;
    signal timestamp_ack    :   std_logic ;
    signal fx3_timestamp    :   unsigned(63 downto 0) ;

    signal rx_ts_reset      :   std_logic ;
    signal tx_ts_reset      :   std_logic ;

    -- Trigger Control interfaces
    signal rx_trigger_ctl       : std_logic_vector(7 downto 0);
    signal tx_trigger_ctl       : std_logic_vector(7 downto 0);

    -- Trigger Control breakdown
    alias rx_trigger_arm        : std_logic is rx_trigger_ctl(0);
    alias rx_trigger_fire       : std_logic is rx_trigger_ctl(1);
    alias rx_trigger_master     : std_logic is rx_trigger_ctl(2);
    alias rx_trigger_line       : std_logic is mini_exp1;

    alias tx_trigger_arm        : std_logic is tx_trigger_ctl(0);
    alias tx_trigger_fire       : std_logic is tx_trigger_ctl(1);
    alias tx_trigger_master     : std_logic is tx_trigger_ctl(2);
    alias tx_trigger_line       : std_logic is mini_exp1;

    signal tx_trigger_arm_sync  :   std_logic ;

    -- Trigger Control readback interfaces
    signal rx_trigger_ctl_rb    : std_logic_vector(7 downto 0);
    signal tx_trigger_ctl_rb    : std_logic_vector(7 downto 0);

    -- Arbiter signal
    signal arbiter_request      : std_logic_vector(1 downto 0) := (others => '0');
    signal arbiter_granted      : std_logic_vector(1 downto 0) := (others => '0');
    signal arbiter_ack          : std_logic_vector(1 downto 0) := (others => '0');

    -- AGC control signal
    alias agc_en is nios_gpio.o.agc_en ;

    signal gain_inc_req         : std_logic ;
    signal gain_dec_req         : std_logic ;
    signal gain_rst_req         : std_logic ;
    signal gain_ack             : std_logic ;
    signal gain_nack            : std_logic ;

    signal agc_lms_sdio         : std_logic ;
    signal agc_lms_sclk         : std_logic ;
    signal agc_lms_sen          : std_logic ;
    signal nios_lms_sdio        : std_logic ;
    signal nios_lms_sclk        : std_logic ;
    signal nios_lms_sen         : std_logic ;

    -- Trigger Control readback breakdown
    alias rx_trigger_arm_rb         : std_logic is rx_trigger_ctl_rb(0);
    alias rx_trigger_fire_rb        : std_logic is rx_trigger_ctl_rb(1);
    alias rx_trigger_master_rb      : std_logic is rx_trigger_ctl_rb(2);
    alias rx_trigger_line_rb        : std_logic is rx_trigger_ctl_rb(3);

    alias tx_trigger_arm_rb         : std_logic is tx_trigger_ctl_rb(0);
    alias tx_trigger_fire_rb        : std_logic is tx_trigger_ctl_rb(1);
    alias tx_trigger_master_rb      : std_logic is tx_trigger_ctl_rb(2);
    alias tx_trigger_line_rb        : std_logic is tx_trigger_ctl_rb(3);

    -- Trigger Outputs
    signal lms_rx_enable_sig                        : std_logic;
    signal lms_rx_enable_qualified                  : std_logic;
    signal lms_rx_enable_qualified_rxclk            : std_logic;
    signal tx_sample_fifo_rempty_untriggered        : std_logic;

    -- AGC signals
    alias agc_band_sel is nios_gpio.o.agc_band_sel ;

    signal gain_inc_req_80M          : std_logic;
    signal gain_dec_req_80M          : std_logic;
    signal gain_rst_req_80M          : std_logic;

    signal gain_ack_rxclk            : std_logic;
    signal gain_nack_rxclk           : std_logic;

    signal agc_gain_high             : std_logic;
    signal agc_gain_mid              : std_logic;
    signal agc_gain_low              : std_logic;

    signal agc_gain_high_rxclk       : std_logic;
    signal agc_gain_mid_rxclk        : std_logic;
    signal agc_gain_low_rxclk        : std_logic;

    signal fpga_dc_i_correction      : signed(15 downto 0);
    signal fpga_dc_q_correction      : signed(15 downto 0);

    signal corr_dc_i_max             : std_logic_vector(15 downto 0);
    signal corr_dc_i_mid             : std_logic_vector(15 downto 0);
    signal corr_dc_i_min             : std_logic_vector(15 downto 0);
    signal corr_dc_q_max             : std_logic_vector(15 downto 0);
    signal corr_dc_q_mid             : std_logic_vector(15 downto 0);
    signal corr_dc_q_min             : std_logic_vector(15 downto 0);

begin

    correction_tx_phase <= signed(correction_tx_phase_gain(31 downto 16));
    correction_tx_gain  <= signed(correction_tx_phase_gain(15 downto 0));
    correction_rx_phase <= signed(correction_rx_phase_gain(31 downto 16));
    correction_rx_gain  <= signed(correction_rx_phase_gain(15 downto 0));
    correction_valid <= '1';


    -- Create 80MHz from 38.4MHz coming from the c4_clock source
    U_pll : entity work.pll
      port map (
        inclk0              =>  c4_clock,
        c0                  =>  \80MHz\,
        locked              =>  open
      ) ;

    U_fx3_pll : entity work.fx3_pll
      port map (
        inclk0              =>  fx3_pclk,
        c0                  =>  fx3_pclk_pll,
        locked              =>  open
      ) ;

    -- Cross domain synchronizer chains
    U_usb_speed : entity work.synchronizer
      generic map (
        RESET_LEVEL         =>  '0'
      ) port map (
        reset               =>  '0',
        clock               =>  fx3_pclk_pll,
        async               =>  nios_gpio.o.usb_speed,
        sync                =>  usb_speed
      ) ;

    U_usb_speed_rx : entity work.synchronizer
      generic map (
        RESET_LEVEL         =>  '0'
      ) port map (
        reset               =>  '0',
        clock               =>  rx_clock,
        async               =>  nios_gpio.o.usb_speed,
        sync                =>  usb_speed_rx
      ) ;

    U_usb_speed_tx : entity work.synchronizer
      generic map (
        RESET_LEVEL         =>  '0'
      ) port map (
        reset               =>  '0',
        clock               =>  tx_clock,
        async               =>  nios_gpio.o.usb_speed,
        sync                =>  usb_speed_tx
      ) ;

    generate_mux_sel : for i in rx_mux_sel'range generate
        U_rx_source : entity work.synchronizer
          generic map (
            RESET_LEVEL         =>  '0'
          ) port map (
            reset               =>  '0',
            clock               =>  rx_clock,
            async               =>  nios_gpio.o.rx_mux_sel(i),
            sync                =>  rx_mux_sel(i)
          ) ;
    end generate ;

    U_meta_sync_fx3 : entity work.synchronizer
      generic map (
        RESET_LEVEL         =>  '0'
      ) port map (
        reset               =>  '0',
        clock               =>  fx3_pclk_pll,
        async               =>  nios_gpio.o.meta_en,
        sync                =>  meta_en_fx3
      ) ;

    U_meta_sync_tx : entity work.synchronizer
      generic map (
        RESET_LEVEL         =>  '0'
      ) port map (
        reset               =>  '0',
        clock               =>  tx_clock,
        async               =>  nios_gpio.o.meta_en,
        sync                =>  meta_en_tx
      ) ;

    U_meta_sync_rx : entity work.synchronizer
      generic map (
        RESET_LEVEL         =>  '0'
      ) port map (
        reset               =>  '0',
        clock               =>  rx_clock,
        async               =>  nios_gpio.o.meta_en,
        sync                =>  meta_en_rx
      ) ;

    U_sys_reset_sync : entity work.reset_synchronizer
      generic map (
        INPUT_LEVEL         =>  '1',
        OUTPUT_LEVEL        =>  '1'
      ) port map (
        clock               =>  fx3_pclk_pll,
        async               =>  sys_rst,
        sync                =>  sys_rst_sync
      ) ;

    U_80M_reset_sync : entity work.reset_synchronizer
      generic map (
        INPUT_LEVEL         =>  '1',
        OUTPUT_LEVEL        =>  '1'
      ) port map (
        clock               =>  \80MHz\,
        async               =>  sys_rst,
        sync                =>  sys_rst_80M
      ) ;

    U_tx_reset : entity work.reset_synchronizer
      generic map (
        INPUT_LEVEL         =>  '1',
        OUTPUT_LEVEL        =>  '1'
      ) port map (
        clock               =>  c4_tx_clock,
        async               =>  sys_rst_sync,
        sync                =>  tx_reset
      ) ;

    U_rx_clock_reset : entity work.reset_synchronizer
      generic map (
        INPUT_LEVEL         =>  '1',
        OUTPUT_LEVEL        =>  '1'
      ) port map (
        clock               =>  rx_clock,
        async               =>  sys_rst_sync,
        sync                =>  rx_reset
      ) ;

    U_rx_enable_sync : entity work.synchronizer
      generic map (
        RESET_LEVEL =>  '0'
      ) port map (
        reset       =>  rx_reset,
        clock       =>  rx_clock,
        async       =>  pclk_rx_enable,
        sync        =>  rx_enable
      ) ;

    U_tx_enable_sync : entity work.synchronizer
      generic map (
        RESET_LEVEL =>  '0'
      ) port map (
        reset       =>  tx_reset,
        clock       =>  tx_clock,
        async       =>  pclk_tx_enable,
        sync        =>  tx_enable
      ) ;

    U_gain_inc_req_80M_sync : entity work.synchronizer
      generic map (
        RESET_LEVEL =>  '0'
      ) port map (
        reset       =>  sys_rst_80M,
        clock       =>  \80MHz\,
        async       =>  gain_inc_req,
        sync        =>  gain_inc_req_80M
      );

    U_gain_dec_req_80M_sync : entity work.synchronizer
      generic map (
        RESET_LEVEL =>  '0'
      ) port map (
        reset       =>  sys_rst_80M,
        clock       =>  \80MHz\,
        async       =>  gain_dec_req,
        sync        =>  gain_dec_req_80M
      );

    U_gain_rst_req_80M_sync : entity work.synchronizer
      generic map (
        RESET_LEVEL =>  '0'
      ) port map (
        reset       =>  sys_rst_80M,
        clock       =>  \80MHz\,
        async       =>  gain_rst_req,
        sync        =>  gain_rst_req_80M
      );

    U_gain_ack_rxclk_sync : entity work.synchronizer
      generic map (
        RESET_LEVEL =>  '0'
      ) port map (
        reset       =>  rx_reset,
        clock       =>  rx_clock,
        async       =>  gain_ack,
        sync        =>  gain_ack_rxclk
      );

    U_gain_nack_rxclk_sync : entity work.synchronizer
      generic map (
        RESET_LEVEL =>  '0'
      ) port map (
        reset       =>  rx_reset,
        clock       =>  rx_clock,
        async       =>  gain_nack,
        sync        =>  gain_nack_rxclk
      );

    U_agc_gain_high_rxclk_sync : entity work.synchronizer
      generic map (
        RESET_LEVEL =>  '0'
      ) port map (
        reset       =>  rx_reset,
        clock       =>  rx_clock,
        async       =>  agc_gain_high,
        sync        =>  agc_gain_high_rxclk
      );

    U_agc_gain_mid_rxclk_sync : entity work.synchronizer
      generic map (
        RESET_LEVEL =>  '0'
      ) port map (
        reset       =>  rx_reset,
        clock       =>  rx_clock,
        async       =>  agc_gain_mid,
        sync        =>  agc_gain_mid_rxclk
      );

    U_agc_gain_low_rxclk_sync : entity work.synchronizer
      generic map (
        RESET_LEVEL =>  '0'
      ) port map (
        reset       =>  rx_reset,
        clock       =>  rx_clock,
        async       =>  agc_gain_low,
        sync        =>  agc_gain_low_rxclk
      );

    U_lms_rx_enable_qualified_rxclk_sync : entity work.synchronizer
      generic map (
        RESET_LEVEL =>  '0'
      ) port map (
        reset       =>  rx_reset,
        clock       =>  rx_clock,
        async       =>  lms_rx_enable_qualified,
        sync        =>  lms_rx_enable_qualified_rxclk
      );

    -- TX sample fifo
    tx_sample_fifo.aclr <= tx_reset ;
    tx_sample_fifo.wclock <= fx3_pclk_pll ;
    tx_sample_fifo.rclock <= tx_clock ;
    U_tx_sample_fifo : entity work.tx_fifo
      generic map (
        LPM_NUMWORDS        => TX_FIFO_LENGTH
      )
      port map (
        aclr                => tx_sample_fifo.aclr,
        data                => tx_sample_fifo.wdata,
        rdclk               => tx_sample_fifo.rclock,
        rdreq               => tx_sample_fifo.rreq,
        wrclk               => tx_sample_fifo.wclock,
        wrreq               => tx_sample_fifo.wreq,
        q                   => tx_sample_fifo.rdata,
        --rdempty             => tx_sample_fifo.rempty,
        rdempty             => tx_sample_fifo_rempty_untriggered,
        rdfull              => tx_sample_fifo.rfull,
        rdusedw             => tx_sample_fifo.rused,
        wrempty             => tx_sample_fifo.wempty,
        wrfull              => tx_sample_fifo.wfull,
        wrusedw             => tx_sample_fifo.wused
      );

    -- TX meta fifo
    tx_meta_fifo.aclr <= tx_reset ;
    tx_meta_fifo.wclock <= fx3_pclk_pll ;
    tx_meta_fifo.rclock <= tx_clock ;
    U_tx_meta_fifo : entity work.tx_meta_fifo
      generic map (
        LPM_NUMWORDS        => META_FIFO_TX_LENGTH
      )
      port map (
        aclr                => tx_meta_fifo.aclr,
        data                => tx_meta_fifo.wdata,
        rdclk               => tx_meta_fifo.rclock,
        rdreq               => tx_meta_fifo.rreq,
        wrclk               => tx_meta_fifo.wclock,
        wrreq               => tx_meta_fifo.wreq,
        q                   => tx_meta_fifo.rdata,
        rdempty             => tx_meta_fifo.rempty,
        rdfull              => tx_meta_fifo.rfull,
        rdusedw             => tx_meta_fifo.rused,
        wrempty             => tx_meta_fifo.wempty,
        wrfull              => tx_meta_fifo.wfull,
        wrusedw             => tx_meta_fifo.wused
      );

    -- RX sample fifo
    rx_sample_fifo.wclock <= rx_clock ;
    rx_sample_fifo.rclock <= fx3_pclk_pll ;
    U_rx_sample_fifo : entity work.rx_fifo
      generic map (
        LPM_NUMWORDS        => RX_FIFO_LENGTH
      )
      port map (
        aclr                => "not"(pclk_rx_enable),
        data                => rx_sample_fifo.wdata,
        rdclk               => rx_sample_fifo.rclock,
        rdreq               => rx_sample_fifo.rreq,
        wrclk               => rx_sample_fifo.wclock,
        wrreq               => rx_sample_fifo.wreq,
        q                   => rx_sample_fifo.rdata,
        rdempty             => rx_sample_fifo.rempty,
        rdfull              => rx_sample_fifo.rfull,
        rdusedw             => rx_sample_fifo.rused,
        wrempty             => rx_sample_fifo.wempty,
        wrfull              => rx_sample_fifo.wfull,
        wrusedw             => rx_sample_fifo.wused
      );

    -- RX meta fifo
    rx_meta_fifo.aclr <= rx_reset ;
    rx_meta_fifo.wclock <= rx_clock ;
    rx_meta_fifo.rclock <= fx3_pclk_pll ;
    U_rx_meta_fifo : entity work.rx_meta_fifo
      generic map (
        LPM_NUMWORDS        => META_FIFO_RX_LENGTH
      )
      port map (
        aclr                => "not"(pclk_rx_enable),
        data                => rx_meta_fifo.wdata,
        rdclk               => rx_meta_fifo.rclock,
        rdreq               => rx_meta_fifo.rreq,
        wrclk               => rx_meta_fifo.wclock,
        wrreq               => rx_meta_fifo.wreq,
        q                   => rx_meta_fifo.rdata,
        rdempty             => rx_meta_fifo.rempty,
        rdfull              => rx_meta_fifo.rfull,
        rdusedw             => rx_meta_fifo.rused,
        wrempty             => rx_meta_fifo.wempty,
        wrfull              => rx_meta_fifo.wfull,
        wrusedw             => rx_meta_fifo.wused
      );


    -- RX loopback fifo
    rx_loopback_fifo.aclr <= '1' when tx_reset = '1' or tx_loopback_enabled = '0' else '0' ;
    rx_loopback_fifo.wclock <= tx_clock ;
    rx_loopback_fifo.wdata <= std_logic_vector(tx_sample_i & tx_sample_q) when tx_loopback_enabled = '1' else (others => '0') ;
    rx_loopback_fifo.wreq <= tx_sample_valid when tx_loopback_enabled = '1' else '0';

    rx_loopback_fifo.rclock <= rx_clock ;
    rx_loopback_fifo.rreq <= '1' when rx_loopback_enabled = '1' and rx_loopback_fifo.rempty = '0' else '0' ;

    U_rx_loopack_fifo : entity work.lb_fifo
      generic map (
        LPM_NUMWORDS        => LOOPBACK_FIFO_LENGTH
      )
      port map (
        aclr                => rx_loopback_fifo.aclr,
        data                => rx_loopback_fifo.wdata,
        rdclk               => rx_loopback_fifo.rclock,
        rdreq               => rx_loopback_fifo.rreq,
        wrclk               => rx_loopback_fifo.wclock,
        wrreq               => rx_loopback_fifo.wreq,
        q                   => rx_loopback_fifo.rdata,
        rdempty             => rx_loopback_fifo.rempty,
        rdfull              => rx_loopback_fifo.rfull,
        rdusedw             => rx_loopback_fifo.rused,
        wrempty             => rx_loopback_fifo.wempty,
        wrfull              => rx_loopback_fifo.wfull,
        wrusedw             => rx_loopback_fifo.wused
      );

    U_loopback_sync : entity work.reset_synchronizer
      generic map (
        INPUT_LEVEL         => '0',
        OUTPUT_LEVEL        => '0'
      ) port map (
        clock               =>  tx_clock,
        async               =>  rx_loopback_enabled,
        sync                =>  tx_loopback_enabled
      ) ;

    rx_loopback_enabled <= '1' when rx_enable = '1' and rx_mux_mode = RX_MUX_DIGITAL_LOOPBACK else '0' ;
    rx_loopback_i <= resize(signed(rx_loopback_fifo.rdata(31 downto 16)), rx_loopback_i'length) ;
    rx_loopback_q <= resize(signed(rx_loopback_fifo.rdata(15 downto 0)), rx_loopback_q'length) ;

    loopback_valid : process(rx_reset, rx_clock)
    begin
        if (rx_reset = '1') then
            rx_loopback_valid <= '0';
        elsif rising_edge(rx_clock) then
             if (rx_loopback_fifo.rempty = '0') then
                -- Delay fifo read request by one clock to indicate sample validity
                rx_loopback_valid <= rx_loopback_fifo.rreq ;
            else
                rx_loopback_valid <= '0' ;
            end if;
        end if;
    end process loopback_valid;

    -- FX3 GPIF
    U_fx3_gpif : entity work.fx3_gpif
      port map (
        pclk                =>  fx3_pclk_pll,
        reset               =>  sys_rst_sync,

        usb_speed           =>  usb_speed,

        meta_enable         =>  meta_en_fx3,
        rx_enable           =>  pclk_rx_enable,
        tx_enable           =>  pclk_tx_enable,

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
      ) ;

    -- Sample bridges
    U_fifo_writer : entity work.fifo_writer
      generic map (
        NUM_STREAMS           => NUM_MIMO_STREAMS,
        FIFO_USEDW_WIDTH      => rx_sample_fifo.wused'length,
        FIFO_DATA_WIDTH       => rx_sample_fifo.wdata'length,
        META_FIFO_USEDW_WIDTH => rx_meta_fifo.wused'length,
        META_FIFO_DATA_WIDTH  => rx_meta_fifo.wdata'length
      )
      port map (
        clock               =>  rx_clock,
        reset               =>  rx_reset,
        enable              =>  rx_enable,

        usb_speed           =>  usb_speed_rx,
        meta_en             =>  meta_en_rx,
        timestamp           =>  rx_timestamp,
        mini_exp            =>  mini_exp2 & mini_exp1,

        fifo_clear          =>  rx_sample_fifo.aclr,
        fifo_full           =>  rx_sample_fifo.wfull,
        fifo_usedw          =>  rx_sample_fifo.wused,
        fifo_data           =>  rx_sample_fifo.wdata,
        fifo_write          =>  rx_sample_fifo.wreq,

        meta_fifo_full      =>  rx_meta_fifo.wfull,
        meta_fifo_usedw     =>  rx_meta_fifo.wused,
        meta_fifo_data      =>  rx_meta_fifo.wdata,
        meta_fifo_write     =>  rx_meta_fifo.wreq,

        in_sample_controls  =>  (others => SAMPLE_CONTROL_ENABLE),
        in_samples          =>  rx_samples,

        overflow_led        =>  rx_overflow_led,
        overflow_count      =>  open,
        overflow_duration   =>  x"ffff"
      ) ;

    process(all)
    begin
        if( agc_gain_high_rxclk = '1' ) then
            fpga_dc_i_correction <= signed( corr_dc_i_max ) ;
            fpga_dc_q_correction <= signed( corr_dc_q_max ) ;
        elsif( agc_gain_mid_rxclk = '1' ) then
            fpga_dc_i_correction <= signed( corr_dc_i_mid ) ;
            fpga_dc_q_correction <= signed( corr_dc_q_mid ) ;
        elsif( agc_gain_low_rxclk = '1' ) then
            fpga_dc_i_correction <= signed( corr_dc_i_min ) ;
            fpga_dc_q_correction <= signed( corr_dc_q_min ) ;
        else
            fpga_dc_i_correction <= ( others => '0' ) ;
            fpga_dc_q_correction <= ( others => '0' ) ;
        end if;
    end process ;

    rx_samples(0).data_i <= rx_sample_corrected_i;
    rx_samples(0).data_q <= rx_sample_corrected_q;
    rx_samples(0).data_v <= rx_sample_corrected_valid;

    U_rx_iq_correction : entity work.iq_correction(rx)
      generic map(
        INPUT_WIDTH         => rx_sample_corrected_i'length
      ) port map(
        reset               => rx_reset,
        clock               => rx_clock,

        in_real             => resize(rx_mux_i,16),
        in_imag             => resize(rx_mux_q,16),
        in_valid            => rx_mux_valid,

        out_real            => rx_sample_corrected_i,
        out_imag            => rx_sample_corrected_q,
        out_valid           => rx_sample_corrected_valid,

        dc_real             => fpga_dc_i_correction,
        dc_imag             => fpga_dc_q_correction,
        gain                => correction_rx_gain,
        phase               => correction_rx_phase,
        correction_valid    => correction_valid
      );


    U_agc : entity work.bladerf_agc
      port map (
        clock          => rx_clock,
        reset          => rx_reset,

        agc_hold_req   => '0',

        gain_inc_req   => gain_inc_req,
        gain_dec_req   => gain_dec_req,
        gain_rst_req   => gain_rst_req,
        gain_ack       => gain_ack_rxclk,
        gain_nack      => gain_nack_rxclk,
        gain_max       => '0',

        rst_gains      => open,
        burst          => open,

        sample_i       => rx_sample_corrected_i,
        sample_q       => rx_sample_corrected_q,
        sample_valid   => rx_sample_corrected_valid
      ) ;

    U_agc_lms : entity work.bladerf_agc_lms_drv
      port map (
        clock          => \80MHz\,
        reset          => sys_rst_80M,

        enable         => agc_en,
        gain_inc_req   => gain_inc_req_80M,
        gain_dec_req   => gain_dec_req_80M,
        gain_rst_req   => gain_rst_req_80M,
        gain_ack       => gain_ack,
        gain_nack      => gain_nack,

        gain_high      => agc_gain_high,
        gain_mid       => agc_gain_mid,
        gain_low       => agc_gain_low,

        arbiter_req    => arbiter_request(1),
        arbiter_grant  => arbiter_granted(1),
        arbiter_done   => arbiter_ack(1),

        band_sel       => agc_band_sel,

        sclk           => agc_lms_sclk,
        miso           => lms_sdo,
        mosi           => agc_lms_sdio,
        cs_n           => agc_lms_sen
      ) ;

    U_agc_mux : process(all)
    begin
        if( arbiter_granted(1) = '1' ) then
            lms_sdio <= agc_lms_sdio ;
            lms_sclk <= agc_lms_sclk ;
            lms_sen  <= agc_lms_sen ;
        else
            lms_sdio <= nios_lms_sdio ;
            lms_sclk <= nios_lms_sclk ;
            lms_sen  <= nios_lms_sen ;
        end if ;
    end process ;

    U_fifo_reader : entity work.fifo_reader
      generic map (
        NUM_STREAMS           => NUM_MIMO_STREAMS,
        FIFO_READ_THROTTLE    => 1, -- TODO: can this be 0 ?
        FIFO_USEDW_WIDTH      => tx_sample_fifo.rused'length,
        FIFO_DATA_WIDTH       => tx_sample_fifo.rdata'length,
        META_FIFO_USEDW_WIDTH => tx_meta_fifo.rused'length,
        META_FIFO_DATA_WIDTH  => tx_meta_fifo.rdata'length
      )
      port map (
        clock               =>  tx_clock,
        reset               =>  tx_reset,
        enable              =>  tx_enable,

        usb_speed           =>  usb_speed_tx,
        meta_en             =>  meta_en_tx,
        timestamp           =>  tx_timestamp,

        fifo_empty          =>  tx_sample_fifo.rempty,
        fifo_usedw          =>  tx_sample_fifo.rused,
        fifo_data           =>  tx_sample_fifo.rdata,
        fifo_read           =>  tx_sample_fifo.rreq,

        meta_fifo_empty     =>  tx_meta_fifo.rempty,
        meta_fifo_usedw     =>  tx_meta_fifo.rused,
        meta_fifo_data      =>  tx_meta_fifo.rdata,
        meta_fifo_read      =>  tx_meta_fifo.rreq,

        in_sample_controls  =>  (others => SAMPLE_CONTROL_ENABLE),
        out_samples         =>  tx_samples,

        underflow_led       =>  tx_underflow_led,
        underflow_count     =>  open,
        underflow_duration  =>  x"ffff"
      ) ;

    tx_sample_raw_i     <= tx_samples(0).data_i;
    tx_sample_raw_q     <= tx_samples(0).data_q;
    tx_sample_raw_valid <= tx_samples(0).data_v;

    U_tx_iq_correction : entity work.iq_correction(tx)
      generic map (
        INPUT_WIDTH         => tx_sample_raw_i'length
      ) port map (
        reset               => tx_reset,
        clock               => tx_clock,

        in_real             => tx_sample_raw_i,
        in_imag             => tx_sample_raw_q,
        in_valid            => tx_sample_raw_valid,

        out_real            => tx_sample_i,
        out_imag            => tx_sample_q,
        out_valid           => tx_sample_valid,

        dc_real             => FPGA_DC_CORRECTION,
        dc_imag             => FPGA_DC_CORRECTION,
        gain                => correction_tx_gain,
        phase               => correction_tx_phase,
        correction_valid    => correction_valid
      );

    -- RX Trigger
    rxtrig : entity work.trigger(async)
      generic map (
        DEFAULT_OUTPUT  => '0'
      ) port map (
        armed           => rx_trigger_arm,
        fired           => rx_trigger_fire,
        master          => rx_trigger_master,
        trigger_in      => rx_trigger_line,
        trigger_out     => rx_trigger_line,
        signal_in       => lms_rx_enable_sig,
        signal_out      => lms_rx_enable_qualified
      );

    rx_trigger_arm_rb             <= rx_trigger_arm;
    rx_trigger_fire_rb            <= rx_trigger_fire;
    rx_trigger_master_rb          <= rx_trigger_master;
    rx_trigger_line_rb            <= rx_trigger_line;
    rx_trigger_ctl_rb(7 downto 4) <= rx_trigger_ctl(7 downto 4);

    -- TX Trigger
    U_tx_arm_sync : entity work.reset_synchronizer
      generic map (
        INPUT_LEVEL     =>  '0',
        OUTPUT_LEVEL    =>  '0'
      ) port map (
        clock           =>  tx_clock,
        async           =>  tx_trigger_arm,
        sync            =>  tx_trigger_arm_sync
      ) ;

    txtrig : entity work.trigger(async)
      generic map (
        DEFAULT_OUTPUT  => '1'
      ) port map (
        armed           => tx_trigger_arm_sync,
        fired           => tx_trigger_fire,
        master          => tx_trigger_master,
        trigger_in      => tx_trigger_line,
        trigger_out     => tx_trigger_line,
        signal_in       => tx_sample_fifo_rempty_untriggered,
        signal_out      => tx_sample_fifo.rempty
      );

    tx_trigger_arm_rb             <= tx_trigger_arm;
    tx_trigger_fire_rb            <= tx_trigger_fire;
    tx_trigger_master_rb          <= tx_trigger_master;
    tx_trigger_line_rb            <= tx_trigger_line;
    tx_trigger_ctl_rb(7 downto 4) <= tx_trigger_ctl(7 downto 4);

    -- LMS6002D IQ interface
    rx_sample_i(15 downto 12) <= (others => rx_sample_i(11)) ;
    rx_sample_q(15 downto 12) <= (others => rx_sample_q(11)) ;

    U_lms6002d : entity work.lms6002d
      port map (
        rx_clock            =>  rx_clock,
        rx_reset            =>  rx_reset,
        rx_enable           =>  rx_enable,

        rx_lms_data         =>  lms_rx_data_reg,
        rx_lms_iq_sel       =>  lms_rx_iq_select_reg,
        rx_lms_enable       =>  open,

        rx_sample_i         =>  rx_sample_i(11 downto 0),
        rx_sample_q         =>  rx_sample_q(11 downto 0),
        rx_sample_valid     =>  rx_sample_valid,

        tx_clock            =>  tx_clock,
        tx_reset            =>  tx_reset,
        tx_enable           =>  tx_enable,

        tx_sample_i         =>  tx_sample_i(11 downto 0),
        tx_sample_q         =>  tx_sample_q(11 downto 0),
        tx_sample_valid     =>  tx_sample_valid,

        tx_lms_data         =>  lms_tx_data,
        tx_lms_iq_sel       =>  lms_tx_iq_select,
        tx_lms_enable       =>  open
      ) ;

    U_rx_siggen : entity work.signal_generator
      port map (
        clock           =>  rx_clock,
        reset           =>  rx_reset,
        enable          =>  rx_enable,

        mode            =>  rx_gen_mode,

        sample_i        =>  rx_gen_i,
        sample_q        =>  rx_gen_q,
        sample_valid    =>  rx_gen_valid
      ) ;

    rx_mux_mode <= rx_mux_mode_t'val(to_integer(rx_mux_sel)) ;

    rx_mux : process(rx_reset, rx_clock)
    begin
        if( rx_reset = '1' ) then
            rx_mux_i <= (others =>'0') ;
            rx_mux_q <= (others =>'0') ;
            rx_mux_valid <= '0' ;
            rx_gen_mode <= '0' ;
        elsif( rising_edge(rx_clock) ) then
            case rx_mux_mode is
                when RX_MUX_NORMAL =>
                    rx_mux_i <= rx_sample_i ;
                    rx_mux_q <= rx_sample_q ;
                    if( lms_rx_enable_qualified_rxclk = '1' ) then
                        rx_mux_valid <= rx_sample_valid ;
                    else
                        rx_mux_valid <= '0' ;
                    end if ;
                when RX_MUX_12BIT_COUNTER | RX_MUX_32BIT_COUNTER =>
                    rx_mux_i <= rx_gen_i ;
                    rx_mux_q <= rx_gen_q ;
                    rx_mux_valid <= rx_gen_valid ;
                    if( rx_mux_mode = RX_MUX_32BIT_COUNTER ) then
                        rx_gen_mode <= '1' ;
                    else
                        rx_gen_mode <= '0' ;
                    end if ;
                when RX_MUX_ENTROPY =>
                    rx_mux_i     <= (others => '0');
                    rx_mux_q     <= (others => '0');
                    rx_mux_valid <= '0';
                when RX_MUX_DIGITAL_LOOPBACK =>
                    rx_mux_i <= rx_loopback_i ;
                    rx_mux_q <= rx_loopback_q ;
                    rx_mux_valid <= rx_loopback_valid ;
                when others =>
                    rx_mux_i <= (others =>'0') ;
                    rx_mux_q <= (others =>'0') ;
                    rx_mux_valid <= '0' ;
            end case ;
        end if ;
    end process ;

    -- Register the inputs immediately
    lms_rx_data_reg         <= lms_rx_data when rising_edge(rx_clock) ;
    lms_rx_iq_select_reg    <= lms_rx_iq_select when rising_edge(rx_clock) ;

    -- FX3 GPIF bidirectional signals
    register_gpif : process(sys_rst_sync, fx3_pclk_pll)
    begin
        if( sys_rst_sync = '1' ) then
            fx3_gpif <= (others =>'Z') ;
            fx3_gpif_in <= (others =>'0') ;
        elsif( rising_edge(fx3_pclk_pll) ) then
            fx3_gpif_in <= fx3_gpif ;
            if( fx3_gpif_oe = '1' ) then
                fx3_gpif <= fx3_gpif_out ;
            else
                fx3_gpif <= (others =>'Z') ;
            end if ;
        end if ;
    end process ;

    generate_ctl : for i in fx3_ctl'range generate
        fx3_ctl(i) <= fx3_ctl_out(i) when fx3_ctl_oe(i) = '1' else 'Z';
    end generate ;

    -- synchronize fx3_ctl_in to the fx3_pclk_pll domain
    generate_fx3_ctl_in_synch: for i in fx3_ctl'range generate
        U_fx3_ctl_in_sync : entity work.synchronizer
          generic map (
            RESET_LEVEL =>  '0'
          ) port map (
            reset               =>  sys_rst_sync,
            clock               =>  fx3_pclk_pll,
            async               =>  fx3_ctl(i),
            sync                =>  fx3_ctl_in(i)
          );
    end generate generate_fx3_ctl_in_synch;

    register_uart : process(sys_rst_80M, \80MHz\) is
    begin
        if (sys_rst_80M = '1') then
            command_serial_in <= '1';
            fx3_uart_rxd <= 'Z';
        elsif (rising_edge(\80MHz\)) then
            command_serial_in <= fx3_uart_txd;
            fx3_uart_rxd <= command_serial_out;
        end if;
    end process register_uart;

    -- NIOS control system for si5338, vctcxo trim and lms control
    U_nios_system : component nios_system
      port map (
        clk_clk                         => \80MHz\,
        reset_reset_n                   => '1',
        dac_MISO                        => nios_sdo,
        dac_MOSI                        => nios_sdio,
        dac_SCLK                        => nios_sclk,
        dac_SS_n                        => nios_ss_n,
        spi_MISO                        => lms_sdo,
        spi_MOSI                        => nios_lms_sdio,
        spi_SCLK                        => nios_lms_sclk,
        spi_SS_n                        => nios_lms_sen,
        gpio_in_port                    => pack(nios_gpio.i),
        gpio_out_port                   => nios_gpo_slv,
        xb_gpio_in_port                 => nios_xb_gpio_in,
        xb_gpio_out_port                => nios_xb_gpio_out,
        xb_gpio_dir_export              => nios_xb_gpio_dir,
        command_serial_in               => command_serial_in,
        command_serial_out              => command_serial_out,
        correction_tx_phase_gain_export => correction_tx_phase_gain,
        correction_rx_phase_gain_export => correction_rx_phase_gain,
        oc_i2c_scl_pad_o                => i2c_scl_out,
        oc_i2c_scl_padoen_o             => i2c_scl_oen,
        oc_i2c_sda_pad_i                => i2c_sda_in,
        oc_i2c_sda_pad_o                => i2c_sda_out,
        oc_i2c_sda_padoen_o             => i2c_sda_oen,
        oc_i2c_arst_i                   => '0',
        oc_i2c_scl_pad_i                => i2c_scl_in,
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
        vctcxo_tamer_tune_ref           => ref_vctcxo_tune,
        vctcxo_tamer_vctcxo_clock       => c4_clock,
        rx_trigger_ctl_out_port         => rx_trigger_ctl,
        tx_trigger_ctl_out_port         => tx_trigger_ctl,
        rx_trigger_ctl_in_port          => rx_trigger_ctl_rb,
        tx_trigger_ctl_in_port          => tx_trigger_ctl_rb,
        arbiter_request                 => arbiter_request,
        arbiter_granted                 => arbiter_granted,
        arbiter_ack                     => arbiter_ack,
        agc_dc_i_max_export             => corr_dc_i_max,
        agc_dc_q_max_export             => corr_dc_q_max,
        agc_dc_i_mid_export             => corr_dc_i_mid,
        agc_dc_q_mid_export             => corr_dc_q_mid,
        agc_dc_i_min_export             => corr_dc_i_min,
        agc_dc_q_min_export             => corr_dc_q_min
      ) ;

    -- Unpack the Nios general-purpose outputs into a record
    nios_gpio.o <= unpack(nios_gpo_slv);

    -- Readback of Nios general-purpose outputs
    nios_gpio.i.gpo_readback <= nios_gpio.o;

    xb_gpio_direction : process(all)
    begin
        for i in 0 to 31 loop
            if (xb_gpio_dir(i) = '1') then
                nios_xb_gpio_in(i) <= nios_xb_gpio_out(i);
                if (nios_gpio.o.xb_mode = "10" and i + 1 = 2) then
                    exp_gpio(i+1) <= nios_ss_n(1);
                elsif (i + 1 /= 1) then
                    exp_gpio(i+1) <= nios_xb_gpio_out(i);
                end if;
            else
                if (i + 1 = 1) then
                    nios_xb_gpio_in(i) <= exp_clock_in;
                else
                    nios_xb_gpio_in(i) <= exp_gpio(i + 1);
                    exp_gpio(i + 1) <= 'Z';
                end if;
            end if;
        end loop ;
    end process ;

    nios_gpio.i.nios_ss_n <= nios_ss_n;
    nios_gpio.i.xb_mode2  <= nios_gpio.o.xb_mode;

    dac_sclk    <= nios_sclk;
    dac_sdi     <= nios_sdio;
    nios_sdo    <= dac_sdo;

    dac_cs_selection : process(sys_rst_80M, \80MHz\) is
    begin
        if (sys_rst_80M = '1') then
            xb_gpio_dir <= (others => '0');
            dac_csx     <= '1';
        elsif (rising_edge(\80MHz\)) then
            case nios_gpio.o.xb_mode is
                when "00" =>
                    xb_gpio_dir <= nios_xb_gpio_dir(31 downto 0);
                    dac_csx     <= nios_ss_n(0);

                when "10" =>
                    xb_gpio_dir <= nios_xb_gpio_dir(31 downto 0);

                    case nios_ss_n(1 downto 0) is
                        when "10" =>
                            dac_csx <= '0';
                        when "01" =>
                            dac_csx <= '1';
                        when others =>
                            dac_csx <= '1';
                    end case;

                when others =>
                    xb_gpio_dir <= nios_xb_gpio_dir(31 downto 0);
                    dac_csx     <= nios_ss_n(0);
            end case;
        end if;
    end process dac_cs_selection;

    -- IO for NIOS
    si_scl <= i2c_scl_out when i2c_scl_oen = '0' else 'Z' ;
    si_sda <= i2c_sda_out when i2c_sda_oen = '0' else 'Z' ;

    i2c_scl_in <= si_scl ;
    i2c_sda_in <= si_sda ;

    toggle_led1 : process(fx3_pclk_pll)
        variable count : natural range 0 to 100_000_000 := 100_000_000 ;
    begin
        if( rising_edge(fx3_pclk_pll) ) then
            count := count - 1 ;
            if( count = 0 ) then
                count := 100_000_000 ;
                led1_blink <= not led1_blink;
            end if ;
        end if ;
    end process ;

    led(1) <= led1_blink        when nios_gpio.o.led_mode = '0' else not nios_gpio.o.leds(1);
    led(2) <= tx_underflow_led  when nios_gpio.o.led_mode = '0' else not nios_gpio.o.leds(2);
    led(3) <= rx_overflow_led   when nios_gpio.o.led_mode = '0' else not nios_gpio.o.leds(3);

    lms_reset               <= nios_gpio.o.lms_reset;

    lms_rx_enable_sig       <= nios_gpio.o.lms_rx_enable;
    lms_rx_enable           <= nios_gpio.o.lms_rx_enable;
    lms_tx_enable           <= nios_gpio.o.lms_tx_enable;

    lms_tx_v                <= nios_gpio.o.lms_tx_v;
    lms_rx_v                <= nios_gpio.o.lms_rx_v;

    -- CTS and the SPI CSx are tied to the same signal.  When we are in reset,
    -- allow for SPI accesses.
    set_fx3_uart_cts : process(sys_rst_sync, fx3_pclk_pll) is
    begin
        if (sys_rst_sync = '1') then
            fx3_uart_cts <= 'Z';
        elsif (rising_edge(fx3_pclk_pll)) then
            fx3_uart_cts <= '1';
        end if;
    end process set_fx3_uart_cts;

    exp_spi_clock           <= nios_sclk when ( nios_ss_n(1 downto 0) = "01" ) else '0' ;
    exp_spi_mosi            <= nios_sdio when ( nios_ss_n(1 downto 0) = "01" ) else '0' ;

    --mini_exp1               <= 'Z';
    mini_exp2               <= 'Z';

    set_tx_ts_reset : process(tx_clock, tx_reset)
    begin
        if( tx_reset = '1' ) then
            tx_ts_reset <= '1' ;
        elsif( rising_edge(tx_clock) ) then
            if( meta_en_tx = '1' ) then
                tx_ts_reset <= '0' ;
            else
                tx_ts_reset <= '1' ;
            end if ;
        end if ;
    end process ;

    set_rx_ts_reset : process(rx_clock, rx_reset)
    begin
        if( rx_reset = '1' ) then
            rx_ts_reset <= '1' ;
        elsif( rising_edge(rx_clock) ) then
            if( meta_en_rx = '1' ) then
                rx_ts_reset <= '0' ;
            else
                rx_ts_reset <= '1' ;
            end if ;
        end if ;
    end process ;

    drive_handshake : process(fx3_pclk_pll, sys_rst_sync)
    begin
        if( sys_rst_sync = '1' ) then
            timestamp_req <= '0' ;
        elsif( rising_edge(fx3_pclk_pll) ) then
            if( meta_en_fx3 = '0' ) then
                timestamp_req <= '0' ;
            else
                if( timestamp_ack = '0' ) then
                    timestamp_req <= '1' ;
                elsif( timestamp_ack = '1' ) then
                    timestamp_req <= '0' ;
                end if ;
            end if ;
        end if ;
    end process ;

    U_timestamp_handshake : entity work.handshake
      generic map (
        DATA_WIDTH          =>  tx_timestamp'length
      ) port map (
        source_clock        =>  tx_clock,
        source_reset        =>  tx_reset,
        source_data         =>  std_logic_vector(tx_timestamp),

        dest_clock          =>  fx3_pclk_pll,
        dest_reset          =>  sys_rst_sync,
        unsigned(dest_data) =>  fx3_timestamp,
        dest_req            =>  timestamp_req,
        dest_ack            =>  timestamp_ack
      ) ;

end architecture ; -- arch
