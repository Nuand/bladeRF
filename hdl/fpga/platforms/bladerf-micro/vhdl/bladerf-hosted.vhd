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

architecture hosted_bladerf of bladerf is

    attribute noprune   : boolean ;
    attribute keep      : boolean ;

    component nios_system is
      port (
        clk_clk                         :   in  std_logic := 'X';
        reset_reset_n                   :   in  std_logic := 'X';
        dac_MISO                        :   in  std_logic := 'X';
        dac_MOSI                        :   out std_logic;
        dac_SCLK                        :   out std_logic;
        dac_SS_n                        :   out std_logic_vector(1 downto 0);
        spi_MISO                        :   in  std_logic := 'X';
        spi_MOSI                        :   out std_logic;
        spi_SCLK                        :   out std_logic;
        spi_SS_n                        :   out std_logic;
        gpio_export                     :   out std_logic_vector(31 downto 0);
        gpio_rffe_0_in_port             :   in  std_logic_vector(31 downto 0);
        gpio_rffe_0_out_port            :   out std_logic_vector(31 downto 0);
        ad9361_dac_sync_in_sync         :   in  std_logic;
        ad9361_dac_sync_out_sync        :   out std_logic;
        ad9361_data_clock_clk           :   out std_logic;
        ad9361_data_reset_reset         :   out std_logic;
        ad9361_device_if_rx_clk_in_p    :   in  std_logic;
        ad9361_device_if_rx_clk_in_n    :   in  std_logic;
        ad9361_device_if_rx_frame_in_p  :   in  std_logic;
        ad9361_device_if_rx_frame_in_n  :   in  std_logic;
        ad9361_device_if_rx_data_in_p   :   in  std_logic_vector(5 downto 0);
        ad9361_device_if_rx_data_in_n   :   in  std_logic_vector(5 downto 0);
        ad9361_device_if_tx_clk_out_p   :   out std_logic;
        ad9361_device_if_tx_clk_out_n   :   out std_logic;
        ad9361_device_if_tx_frame_out_p :   out std_logic;
        ad9361_device_if_tx_frame_out_n :   out std_logic;
        ad9361_device_if_tx_data_out_p  :   out std_logic_vector(5 downto 0);
        ad9361_device_if_tx_data_out_n  :   out std_logic_vector(5 downto 0);
        ad9361_adc_i0_enable            :   out std_logic;
        ad9361_adc_i0_valid             :   out std_logic;
        ad9361_adc_i0_data              :   out std_logic_vector(15 downto 0);
        ad9361_adc_i1_enable            :   out std_logic;
        ad9361_adc_i1_valid             :   out std_logic;
        ad9361_adc_i1_data              :   out std_logic_vector(15 downto 0);
        ad9361_adc_overflow_ovf         :   in  std_logic;
        ad9361_adc_q0_enable            :   out std_logic;
        ad9361_adc_q0_valid             :   out std_logic;
        ad9361_adc_q0_data              :   out std_logic_vector(15 downto 0);
        ad9361_adc_q1_enable            :   out std_logic;
        ad9361_adc_q1_valid             :   out std_logic;
        ad9361_adc_q1_data              :   out std_logic_vector(15 downto 0);
        ad9361_adc_underflow_unf        :   in  std_logic;
        ad9361_dac_i0_enable            :   out std_logic;
        ad9361_dac_i0_valid             :   out std_logic;
        ad9361_dac_i0_data              :   in  std_logic_vector(15 downto 0);
        ad9361_dac_i1_enable            :   out std_logic;
        ad9361_dac_i1_valid             :   out std_logic;
        ad9361_dac_i1_data              :   in  std_logic_vector(15 downto 0);
        ad9361_dac_overflow_ovf         :   in  std_logic;
        ad9361_dac_q0_enable            :   out std_logic;
        ad9361_dac_q0_valid             :   out std_logic;
        ad9361_dac_q0_data              :   in  std_logic_vector(15 downto 0);
        ad9361_dac_q1_enable            :   out std_logic;
        ad9361_dac_q1_valid             :   out std_logic;
        ad9361_dac_q1_data              :   in  std_logic_vector(15 downto 0);
        ad9361_dac_underflow_unf        :   in  std_logic;
        oc_i2c_arst_i                   :   in  std_logic;
        oc_i2c_scl_pad_i                :   in  std_logic;
        oc_i2c_scl_pad_o                :   out std_logic;
        oc_i2c_scl_padoen_o             :   out std_logic;
        oc_i2c_sda_pad_i                :   in  std_logic;
        oc_i2c_sda_pad_o                :   out std_logic;
        oc_i2c_sda_padoen_o             :   out std_logic;
        xb_gpio_in_port                 :   in  std_logic_vector(31 downto 0) := (others => 'X');
        xb_gpio_out_port                :   out std_logic_vector(31 downto 0);
        xb_gpio_dir_export              :   out std_logic_vector(31 downto 0);
        command_serial_in               :   in  std_logic;
        command_serial_out              :   out std_logic;
        correction_rx_phase_gain_export :   out std_logic_vector(31 downto 0);
        correction_tx_phase_gain_export :   out std_logic_vector(31 downto 0);
        rx_tamer_ts_sync_in             :   in  std_logic;
        rx_tamer_ts_sync_out            :   out std_logic;
        rx_tamer_ts_pps                 :   in  std_logic;
        rx_tamer_ts_clock               :   in  std_logic;
        rx_tamer_ts_reset               :   in  std_logic;
        rx_tamer_ts_time                :   out std_logic_vector(63 downto 0);
        tx_tamer_ts_sync_in             :   in  std_logic;
        tx_tamer_ts_sync_out            :   out std_logic;
        tx_tamer_ts_pps                 :   in  std_logic;
        tx_tamer_ts_clock               :   in  std_logic;
        tx_tamer_ts_reset               :   in  std_logic;
        tx_tamer_ts_time                :   out std_logic_vector(63 downto 0);
        tx_trigger_ctl_in_port          :   in  std_logic_vector(7 downto 0);
        tx_trigger_ctl_out_port         :   out std_logic_vector(7 downto 0);
        rx_trigger_ctl_in_port          :   in  std_logic_vector(7 downto 0);
        rx_trigger_ctl_out_port         :   out std_logic_vector(7 downto 0)
      );
    end component;

    alias sys_rst   is fx3_ctl(7) ;

    -- Can be set from libbladeRF using bladerf_set_rx_mux()
    type rx_mux_mode_t is (RX_MUX_NORMAL, RX_MUX_12BIT_COUNTER, RX_MUX_32BIT_COUNTER, RX_MUX_ENTROPY, RX_MUX_DIGITAL_LOOPBACK) ;

    signal rx_mux_sel       : unsigned(2 downto 0) ;
    signal rx_mux_mode      : rx_mux_mode_t ;

    signal \80MHz\           : std_logic ;
    signal \80MHz locked\    : std_logic ;
    signal \80MHz pll reset\ : std_logic ;


    signal nios_gpio        : std_logic_vector(31 downto 0) ;
    signal nios_xb_gpio_in  : std_logic_vector(31 downto 0) := (others => '0');
    signal nios_xb_gpio_out : std_logic_vector(31 downto 0) := (others => '0');
    signal nios_xb_gpio_dir : std_logic_vector(31 downto 0) := (others => '0');
    signal xb_gpio_dir      : std_logic_vector(31 downto 0) ;

    signal i2c_scl_in       : std_logic ;
    signal i2c_scl_out      : std_logic ;
    signal i2c_scl_oen      : std_logic ;

    signal i2c_sda_in       : std_logic ;
    signal i2c_sda_out      : std_logic ;
    signal i2c_sda_oen      : std_logic ;

    type fifo_t is record
        aclr    :   std_logic ;

        wclock  :   std_logic ;
        wdata   :   std_logic_vector(31 downto 0) ;
        wreq    :   std_logic ;
        wempty  :   std_logic ;
        wfull   :   std_logic ;
        wused   :   std_logic_vector(11 downto 0) ;

        rclock  :   std_logic ;
        rdata   :   std_logic_vector(31 downto 0) ;
        rreq    :   std_logic ;
        rempty  :   std_logic ;
        rfull   :   std_logic ;
        rused   :   std_logic_vector(11 downto 0) ;
    end record ;

    signal rx_sample_fifo   : fifo_t ;
    signal tx_sample_fifo   : fifo_t ;
    signal rx_loopback_fifo : fifo_t ;

    type meta_fifo_tx_t is record
        aclr    :   std_logic ;

        wclock  :   std_logic ;
        wdata   :   std_logic_vector(31 downto 0) ;
        wreq    :   std_logic ;
        wempty  :   std_logic ;
        wfull   :   std_logic ;
        wused   :   std_logic_vector(4 downto 0) ;

        rclock  :   std_logic ;
        rdata   :   std_logic_vector(127 downto 0) ;
        rreq    :   std_logic ;
        rempty  :   std_logic ;
        rfull   :   std_logic ;
        rused   :   std_logic_vector(2 downto 0) ;
    end record ;

    signal tx_meta_fifo     : meta_fifo_tx_t ;

    type meta_fifo_rx_t is record
        aclr    :   std_logic ;

        wclock  :   std_logic ;
        wdata   :   std_logic_vector(127 downto 0) ;
        wreq    :   std_logic ;
        wempty  :   std_logic ;
        wfull   :   std_logic ;
        wused   :   std_logic_vector(4 downto 0) ;

        rclock  :   std_logic ;
        rdata   :   std_logic_vector(31 downto 0) ;
        rreq    :   std_logic ;
        rempty  :   std_logic ;
        rfull   :   std_logic ;
        rused   :   std_logic_vector(6 downto 0) ;
    end record ;

    signal rx_meta_fifo     : meta_fifo_rx_t ;

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

    signal rx_gen_mode      : std_logic ;
    signal rx_gen_i         : signed(15 downto 0) ;
    signal rx_gen_q         : signed(15 downto 0) ;
    signal rx_gen_valid     : std_logic ;

    signal rx_entropy_i     : signed(15 downto 0) := (others =>'0') ;
    signal rx_entropy_q     : signed(15 downto 0) := (others =>'0') ;
    signal rx_entropy_valid : std_logic := '0' ;

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

    signal fx3_gpif_in      : std_logic_vector(31 downto 0) ;
    signal fx3_gpif_out     : std_logic_vector(31 downto 0) ;
    signal fx3_gpif_oe      : std_logic ;

    signal fx3_ctl_in       : std_logic_vector(12 downto 0) ;
    signal fx3_ctl_out      : std_logic_vector(12 downto 0) ;
    signal fx3_ctl_oe       : std_logic_vector(12 downto 0) ;

    signal tx_underflow_led     :   std_logic ;
    signal tx_underflow_count   :   unsigned(63 downto 0) ;

    signal rx_overflow_led      :   std_logic ;
    signal rx_overflow_count    :   unsigned(63 downto 0) ;

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

    signal xb_mode  : std_logic_vector(1 downto 0);

    signal command_serial_in    :   std_logic ;
    signal command_serial_out   :   std_logic ;

    constant FPGA_DC_CORRECTION :  signed(15 downto 0) := to_signed(integer(0), 16);

    signal fx3_pclk_pll        : std_logic;
    signal fx3_pclk_pll_locked : std_logic;
    signal fx3_pclk_pll_reset  : std_logic;

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
    signal rx_trigger_line      : std_logic := '0';

    alias tx_trigger_arm        : std_logic is tx_trigger_ctl(0);
    alias tx_trigger_fire       : std_logic is tx_trigger_ctl(1);
    alias tx_trigger_master     : std_logic is tx_trigger_ctl(2);
    signal tx_trigger_line      : std_logic := '0';

    signal tx_trigger_arm_sync  :   std_logic ;
    signal tx_trigger_line_sync :   std_logic;

    -- Trigger Control readback interfaces
    signal rx_trigger_ctl_rb    : std_logic_vector(7 downto 0);
    signal tx_trigger_ctl_rb    : std_logic_vector(7 downto 0);

    -- Trigger Control readback breakdown
    alias rx_trigger_arm_rb         : std_logic is rx_trigger_ctl_rb(0);
    alias rx_trigger_fire_rb        : std_logic is rx_trigger_ctl_rb(1);
    alias rx_trigger_master_rb      : std_logic is rx_trigger_ctl_rb(2);
    alias rx_trigger_line_rb        : std_logic is rx_trigger_ctl_rb(3);
    alias rx_trigger_unused_rb      : std_logic_vector(7 downto 4) is rx_trigger_ctl_rb(7 downto 4);

    alias tx_trigger_arm_rb         : std_logic is tx_trigger_ctl_rb(0);
    alias tx_trigger_fire_rb        : std_logic is tx_trigger_ctl_rb(1);
    alias tx_trigger_master_rb      : std_logic is tx_trigger_ctl_rb(2);
    alias tx_trigger_line_rb        : std_logic is tx_trigger_ctl_rb(3);
    alias tx_trigger_unused_rb      : std_logic_vector(7 downto 4) is tx_trigger_ctl_rb(7 downto 4);

    -- Trigger Outputs
    signal lms_rx_enable_sig                        : std_logic;
    signal lms_rx_enable_qualified                  : std_logic;
    signal tx_sample_fifo_rempty_untriggered        : std_logic;

    signal tx_lms_data : signed(11 downto 0) := (others => '0');

    signal exp_blink       : std_logic := '1';

    signal rffe_gpio       : rffe_gpio_t := (
        i => RFFE_GPI_DEFAULT,
        o => pack(RFFE_GPO_DEFAULT)
    );

    signal ad9361 : mimo_t;
    alias tx_clock  is ad9361.clock;
    alias rx_clock  is ad9361.clock;

    signal channel_sel : std_logic := '0';

    attribute noprune of ad9361 : signal is true;
    attribute keep    of ad9361 : signal is true;

begin

    -- Create 80MHz from 38.4MHz coming from the c4_clock source
    U_pll : entity work.pll
      port map (
        areset              =>  \80MHz pll reset\,
        inclk0              =>  c5_clock_1,
        c0                  =>  \80MHz\,
        locked              =>  \80MHz locked\
      ) ;

    U_pll_reset_pll : entity work.pll_reset
      generic map (
        SYS_CLOCK_FREQ_HZ   => 38_400_000,
        DEVICE_FAMILY       => "Cyclone V"
      )
      port map (
        sys_clock      => c5_clock_1,
        pll_locked     => \80MHz locked\,
        pll_locked_out => open,
        pll_reset      => \80MHz pll reset\
    );

    U_fx3_pll : entity work.fx3_pll
      port map (
        areset              =>  fx3_pclk_pll_reset,
        inclk0              =>  fx3_pclk,
        c0                  =>  fx3_pclk_pll,
        locked              =>  fx3_pclk_pll_locked
      ) ;

    U_pll_reset_fx3_pll : entity work.pll_reset
      generic map (
        SYS_CLOCK_FREQ_HZ   => 100_000_000,
        DEVICE_FAMILY       => "Cyclone V"
      )
      port map (
        sys_clock      => fx3_pclk,
        pll_locked     => fx3_pclk_pll_locked,
        pll_locked_out => open,
        pll_reset      => fx3_pclk_pll_reset
    );

    -- Cross domain synchronizer chains
    U_usb_speed : entity work.synchronizer
      generic map (
        RESET_LEVEL         =>  '0'
      ) port map (
        reset               =>  '0',
        clock               =>  fx3_pclk_pll,
        async               =>  nios_gpio(7),
        sync                =>  usb_speed
      ) ;

    U_usb_speed_rx : entity work.synchronizer
      generic map (
        RESET_LEVEL         =>  '0'
      ) port map (
        reset               =>  '0',
        clock               =>  rx_clock,
        async               =>  nios_gpio(7),
        sync                =>  usb_speed_rx
      ) ;

    U_usb_speed_tx : entity work.synchronizer
      generic map (
        RESET_LEVEL         =>  '0'
      ) port map (
        reset               =>  '0',
        clock               =>  tx_clock,
        async               =>  nios_gpio(7),
        sync                =>  usb_speed_tx
      ) ;

    generate_mux_sel : for i in rx_mux_sel'range generate
        U_rx_source : entity work.synchronizer
          generic map (
            RESET_LEVEL         =>  '0'
          ) port map (
            reset               =>  '0',
            clock               =>  rx_clock,
            async               =>  nios_gpio(8+i),
            sync                =>  rx_mux_sel(i)
          ) ;
    end generate ;

    U_meta_sync_fx3 : entity work.synchronizer
      generic map (
        RESET_LEVEL         =>  '0'
      ) port map (
        reset               =>  '0',
        clock               =>  fx3_pclk_pll,
        async               =>  nios_gpio(16),
        sync                =>  meta_en_fx3
      ) ;

    U_meta_sync_tx : entity work.synchronizer
      generic map (
        RESET_LEVEL         =>  '0'
      ) port map (
        reset               =>  '0',
        clock               =>  tx_clock,
        async               =>  nios_gpio(16),
        sync                =>  meta_en_tx
      ) ;

    U_meta_sync_rx : entity work.synchronizer
      generic map (
        RESET_LEVEL         =>  '0'
      ) port map (
        reset               =>  '0',
        clock               =>  rx_clock,
        async               =>  nios_gpio(16),
        sync                =>  meta_en_rx
      ) ;

    xb_mode <= nios_gpio(31 downto 30);

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
        clock               =>  tx_clock,
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

    -- TX sample fifo
    tx_sample_fifo.aclr <= tx_reset ;
    tx_sample_fifo.wclock <= fx3_pclk_pll ;
    tx_sample_fifo.rclock <= tx_clock ;
    U_tx_sample_fifo : entity work.tx_fifo
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

    tx_channel_mux : process( all )
        variable ch     : integer range 0 to 1 := 0;
        variable fifo_i : std_logic_vector(11 downto 0) := (others => '0');
        variable fifo_q : std_logic_vector(11 downto 0) := (others => '0');
    begin
        if( rising_edge(tx_clock) ) then
            if( channel_sel = '0' ) then
                ch := 0;
            else
                ch := 1;
            end if;

            if( tx_sample_fifo.rempty = '0' ) then
                tx_sample_fifo.rreq <= ad9361.ch(ch).dac.i.valid;
                fifo_i := tx_sample_fifo.rdata(11 downto 0);
                fifo_q := tx_sample_fifo.rdata(27 downto 16);
            else
                tx_sample_fifo.rreq <= '0';
                fifo_i := (others => '0');
                fifo_q := (others => '0');
            end if;

            ad9361.ch(ch).dac.i.data <= fifo_i & "0000";
            ad9361.ch(ch).dac.q.data <= fifo_q & "0000";

            tx_sample_raw_i <= resize(signed(fifo_i), tx_sample_raw_i'length);
            tx_sample_raw_q <= resize(signed(fifo_q), tx_sample_raw_q'length);
        end if;
    end process;

    -- TX meta fifo
    tx_meta_fifo.aclr <= tx_reset ;
    tx_meta_fifo.wclock <= fx3_pclk_pll ;
    tx_meta_fifo.rclock <= tx_clock ;
    U_tx_meta_fifo : entity work.tx_meta_fifo
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

    U_rx_loopack_fifo : entity work.rx_fifo
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
      port map (
        clock               =>  rx_clock,
        reset               =>  rx_reset,
        enable              =>  rx_enable,

        usb_speed           =>  usb_speed_rx,
        meta_en             =>  meta_en_rx,
        timestamp           =>  rx_timestamp,

        fifo_clear          =>  rx_sample_fifo.aclr,
        fifo_full           =>  rx_sample_fifo.wfull,
        fifo_usedw          =>  rx_sample_fifo.wused,
        fifo_data           =>  rx_sample_fifo.wdata,
        fifo_write          =>  rx_sample_fifo.wreq,

        meta_fifo_full      =>  rx_meta_fifo.wfull,
        meta_fifo_usedw     =>  rx_meta_fifo.wused,
        meta_fifo_data      =>  rx_meta_fifo.wdata,
        meta_fifo_write     =>  rx_meta_fifo.wreq,

        in_i                =>  rx_sample_corrected_i,
        in_q                =>  rx_sample_corrected_q,
        in_valid            =>  rx_sample_corrected_valid,

        overflow_led        =>  rx_overflow_led,
        overflow_count      =>  rx_overflow_count,
        overflow_duration   =>  x"ffff"
      ) ;

    rx_sample_corrected_i     <= resize(rx_mux_i,16);
    rx_sample_corrected_q     <= resize(rx_mux_q,16);
    rx_sample_corrected_valid <= rx_mux_valid;

    --U_fifo_reader : entity work.fifo_reader
    --  port map (
    --    clock               =>  tx_clock,
    --    reset               =>  tx_reset,
    --    enable              =>  tx_enable,
    --
    --    usb_speed           =>  usb_speed_tx,
    --    meta_en             =>  meta_en_tx,
    --    timestamp           =>  tx_timestamp,
    --
    --    fifo_empty          =>  tx_sample_fifo.rempty,
    --    fifo_usedw          =>  tx_sample_fifo.rused,
    --    fifo_data           =>  tx_sample_fifo.rdata,
    --    fifo_read           =>  tx_sample_fifo.rreq,
    --
    --    meta_fifo_empty     =>  tx_meta_fifo.rempty,
    --    meta_fifo_usedw     =>  tx_meta_fifo.rused,
    --    meta_fifo_data      =>  tx_meta_fifo.rdata,
    --    meta_fifo_read      =>  tx_meta_fifo.rreq,
    --
    --    out_i               =>  tx_sample_raw_i,
    --    out_q               =>  tx_sample_raw_q,
    --    out_valid           =>  tx_sample_raw_valid,
    --
    --    underflow_led       =>  tx_underflow_led,
    --    underflow_count     =>  tx_underflow_count,
    --    underflow_duration  =>  x"ffff"
    --  ) ;

    tx_sample_i     <= tx_sample_raw_i;
    tx_sample_q     <= tx_sample_raw_q;
    tx_sample_valid <= tx_sample_raw_valid;

    lms_rx_enable_sig <= nios_gpio(1);

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

    rx_trigger_arm_rb    <= rx_trigger_arm;
    rx_trigger_fire_rb   <= rx_trigger_fire;
    rx_trigger_master_rb <= rx_trigger_master;
    rx_trigger_line_rb   <= rx_trigger_line;
    rx_trigger_unused_rb <= (others => '0');

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

    U_tx_trig_sync : entity work.synchronizer
      generic map (
        RESET_LEVEL =>  '0'
      ) port map (
        reset       =>  tx_reset,
        clock       =>  tx_clock,
        async       =>  tx_trigger_line,
        sync        =>  tx_trigger_line_sync
      ) ;

    txtrig : entity work.trigger(async)
      generic map (
        DEFAULT_OUTPUT  => '1'
      ) port map (
        armed           => tx_trigger_arm_sync,
        fired           => tx_trigger_fire,
        master          => tx_trigger_master,
        trigger_in      => tx_trigger_line_sync,
        trigger_out     => tx_trigger_line,
        signal_in       => tx_sample_fifo_rempty_untriggered,
        signal_out      => tx_sample_fifo.rempty
      );

    tx_trigger_arm_rb    <= tx_trigger_arm;
    tx_trigger_fire_rb   <= tx_trigger_fire;
    tx_trigger_master_rb <= tx_trigger_master;
    tx_trigger_line_rb   <= tx_trigger_line;
    tx_trigger_unused_rb <= (others => '0');

    U_sync_channel_sel : entity work.synchronizer
      generic map (
        RESET_LEVEL         =>  '0'
      ) port map (
        reset               =>  '0',
        clock               =>  rx_clock,
        async               =>  nios_gpio(17),
        sync                =>  channel_sel
      ) ;

    rx_channel_mux : process( all )
        variable ch : integer range 0 to 1 := 0;
    begin
        if( rising_edge(rx_clock) ) then
            if( channel_sel = '0' ) then
                ch := 0;
            else
                ch := 1;
            end if;

            rx_sample_i(15 downto 12) <= ( others => ad9361.ch(ch).adc.i.data(11) );
            rx_sample_i(11 downto 0)  <= signed(ad9361.ch(ch).adc.i.data(11 downto 0));
            rx_sample_q(15 downto 12) <= ( others => ad9361.ch(ch).adc.q.data(11) );
            rx_sample_q(11 downto 0)  <= signed(ad9361.ch(ch).adc.q.data(11 downto 0));
            rx_sample_valid           <= ad9361.ch(ch).adc.i.valid and
                                         ad9361.ch(ch).adc.q.valid;
        end if;
    end process;


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
                    if( lms_rx_enable_qualified = '1' ) then
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
                    rx_mux_i <= rx_entropy_i ;
                    rx_mux_q <= rx_entropy_q ;
                    rx_mux_valid <= rx_entropy_valid ;
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

    fx3_ctl_in <= fx3_ctl ;

    command_serial_in <= fx3_uart_txd when sys_rst_80M = '0' else '1' ;
    fx3_uart_rxd <= command_serial_out when sys_rst_80M = '0' else 'Z' ;

    -- NIOS control system for si5338, vctcxo trim and lms control
    U_nios_system : component nios_system
      port map (
        clk_clk                         => \80MHz\,
        reset_reset_n                   => '1',
        dac_MISO                        => nios_sdo,
        dac_MOSI                        => nios_sdio,
        dac_SCLK                        => nios_sclk,
        dac_SS_n                        => nios_ss_n,
        spi_MISO                        => adi_spi_sdo,
        spi_MOSI                        => adi_spi_sdi,
        spi_SCLK                        => adi_spi_sclk,
        spi_SS_n                        => adi_spi_csn,
        gpio_export                     => nios_gpio,
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
        xb_gpio_in_port                 => nios_xb_gpio_in,
        xb_gpio_out_port                => nios_xb_gpio_out,
        xb_gpio_dir_export              => nios_xb_gpio_dir,
        command_serial_in               => command_serial_in,
        command_serial_out              => command_serial_out,
        correction_tx_phase_gain_export => open,
        correction_rx_phase_gain_export => open,
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
        rx_trigger_ctl_out_port         => rx_trigger_ctl,
        tx_trigger_ctl_out_port         => tx_trigger_ctl,
        rx_trigger_ctl_in_port          => rx_trigger_ctl_rb,
        tx_trigger_ctl_in_port          => tx_trigger_ctl_rb
      ) ;

    -- DAC SPI (data latched on falling edge)
    dac_sclk <= not nios_sclk when nios_gpio(11) = '0' else '0';
    dac_sdi  <= nios_sdio     when nios_gpio(11) = '0' else '0';
    dac_csn  <= nios_ss_n(0)  when nios_gpio(11) = '0' else '1';

    -- ADF SPI (data latched on rising edge)
    adf_sclk <= nios_sclk    when nios_gpio(11) = '1' else '0';
    adf_sdi  <= nios_sdio    when nios_gpio(11) = '1' else '0';
    adf_csn  <= nios_ss_n(1) when nios_gpio(11) = '1' else '1';
    adf_ce   <= nios_gpio(11);

    nios_sdo <= adf_muxout when ((nios_ss_n(1) = '0') and (nios_gpio(11) = '1'))
                else '0';

    -- Expansion I2C
    exp_i2c_scl <= 'Z';
    exp_i2c_sda <= 'Z';

    -- Power monitor I2C
    pwr_scl     <= i2c_scl_out when i2c_scl_oen = '0' else 'Z';
    pwr_sda     <= i2c_sda_out when i2c_sda_oen = '0' else 'Z';

    i2c_scl_in  <= pwr_scl;
    i2c_sda_in  <= pwr_sda;

    -- temp assignments for initial bringup vv
    exp_gpio      <= (others => exp_blink);

    exp_clock_out <= exp_clock_in;

    exp_toggler : process(exp_clock_in)
        variable count : natural range 0 to 100_000_000 := 100_000_000 ;
    begin
        if( rising_edge(exp_clock_in) ) then
            count := count - 1 ;
            if( count = 0 ) then
                count := 100_000_00 ;
                exp_blink <= not exp_blink;
            end if ;
        end if ;
    end process ;

    -- end temp ^^

    -- Power supply synchronization
    -- ADP2384 sync freq must be +/- 10% of the Fsw set by RT.
    -- RT = 53.6k +/- 1%, so Fsw = 0.999768 MHz to 1.015514 MHz.
    -- Therefore, sync must fall between:
    --   -10% of 1.015514 MHz = 0.913964 MHz
    --   +10% of 0.999768 MHz = 1.099745 MHz
    -- Dividing 38.4 MHz by 38 yields 1.010526 MHz.
    ps_sync : process(c5_clock_1)
        variable count  : natural range 0 to 19 := 19;
        variable ps_clk : std_logic := '0';
    begin
        if( rising_edge(c5_clock_1) ) then
            ps_sync_1p1 <= ps_clk;
            ps_sync_1p8 <= ps_clk;
            count := count - 1;
            if( count = 0 ) then
                count  := 19;
                ps_clk := not ps_clk;
            end if;
        end if;
    end process;

    toggle_led1 : process(fx3_pclk_pll)
        variable count : natural range 0 to 100_000_000 := 100_000_000 ;
    begin
        if( rising_edge(fx3_pclk_pll) ) then
            count := count - 1 ;
            if( count = 0 ) then
                count := 100_000_00 ;
                led1_blink <= not led1_blink;
            end if ;
        end if ;
    end process ;

    led(1) <= led1_blink        when nios_gpio(15) = '0' else not nios_gpio(12);
    led(2) <= tx_underflow_led  when nios_gpio(15) = '0' else not nios_gpio(13);
    led(3) <= rx_overflow_led   when nios_gpio(15) = '0' else not nios_gpio(14);

    adi_ctrl_in    <= unpack(rffe_gpio.o).ctrl_in;
    adi_tx_spdt2_v <= unpack(rffe_gpio.o).tx_spdt2;
    adi_tx_spdt1_v <= unpack(rffe_gpio.o).tx_spdt1;
    tx_bias_en     <= unpack(rffe_gpio.o).tx_bias_en;
    adi_rx_spdt2_v <= unpack(rffe_gpio.o).rx_spdt2;
    adi_rx_spdt1_v <= unpack(rffe_gpio.o).rx_spdt1;
    rx_bias_en     <= unpack(rffe_gpio.o).rx_bias_en;
    --adi_sync_in    <= unpack(rffe_gpio.o).sync_in;
    adi_en_agc     <= unpack(rffe_gpio.o).en_agc;
    adi_txnrx      <= unpack(rffe_gpio.o).txnrx;
    adi_enable     <= unpack(rffe_gpio.o).enable;
    adi_reset_n    <= unpack(rffe_gpio.o).reset_n;

    -- Synchronize the AD9361 ctrl_out bus into the Nios clock domain
    gen_sync_adi_ctrl_out : for i in adi_ctrl_out'range generate
        U_sync_adi_ctrl_out : entity work.synchronizer
          generic map (
            RESET_LEVEL         =>  '0'
          ) port map (
            reset               =>  '0',
            clock               =>  \80MHz\,
            async               =>  adi_ctrl_out(i),
            sync                =>  rffe_gpio.i.ctrl_out(i)
          );
    end generate;

    -- CTS and the SPI CSx are tied to the same signal.  When we are in reset, allow for SPI accesses
    fx3_uart_cts            <= '1' when sys_rst_sync = '0' else 'Z'  ;

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

end architecture;
