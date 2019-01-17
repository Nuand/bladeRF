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

library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;
    use ieee.math_real.all;
    use ieee.math_complex.all;

library work;
    use work.bladerf;
    use work.bladerf_p.all;
    use work.fifo_readwrite_p.all;

architecture foxhunt_bladerf of bladerf is

    attribute noprune          : boolean;
    attribute keep             : boolean;

    alias  sys_reset_async     : std_logic is fx3_ctl(7);
    signal sys_reset_pclk      : std_logic;
    signal sys_reset           : std_logic;

    signal sys_clock           : std_logic;
    signal sys_pll_locked      : std_logic;
    signal sys_pll_reset       : std_logic;

    signal fx3_pclk_pll        : std_logic;
    signal fx3_pclk_pll_locked : std_logic;
    signal fx3_pclk_pll_reset  : std_logic;

    signal rx_mux_sel             : unsigned(2 downto 0);

    signal nios_xb_gpio_in        : std_logic_vector(31 downto 0) := (others => '0');
    signal nios_xb_gpio_out       : std_logic_vector(31 downto 0) := (others => '0');
    signal nios_xb_gpio_oe        : std_logic_vector(31 downto 0) := (others => '0');

    signal nios_gpio              : nios_gpio_t;
    signal nios_gpo_slv           : std_logic_vector(31 downto 0);

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

    signal rx_overflow_led        : std_logic := '1';

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

    signal   ps_sync              : std_logic_vector(0 downto 0)          := (others => '0');

    signal tonegen_streams        : sample_streams_t(dac_controls'range)  := (others => ZERO_SAMPLE);
    signal tonegen_sample_i       : std_logic_vector(15 downto 0);
    signal tonegen_sample_q       : std_logic_vector(15 downto 0);
    signal tonegen_sample_v       : std_logic;
    signal tonegen_running        : std_logic;

    -- override nios_system from bladerf_p
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
        gpio_in_port                    :   in  std_logic_vector(31 downto 0);
        gpio_out_port                   :   out std_logic_vector(31 downto 0);
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
        rx_trigger_ctl_out_port         :   out std_logic_vector(7 downto 0);
        tonegen_sample_valid            :   out std_logic;
        tonegen_sample_i                :   out std_logic_vector(15 downto 0);
        tonegen_sample_q                :   out std_logic_vector(15 downto 0);
        tonegen_sample_clk              :   in  std_logic := 'X';
        tonegen_status_empty            :   out std_logic;
        tonegen_status_full             :   out std_logic;
        tonegen_status_running          :   out std_logic
      );
    end component;

begin

    -- ========================================================================
    -- PLLs
    -- ========================================================================

    -- Create 80 MHz system clock from 38.4 MHz
    U_system_pll : component system_pll
        port map (
            refclk   => c5_clock2,
            rst      => sys_pll_reset,
            outclk_0 => sys_clock,
            locked   => sys_pll_locked
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
            outclk_0 =>  fx3_pclk_pll,
            locked   =>  fx3_pclk_pll_locked
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
            rx_enable           =>  rx_enable_pclk,
            tx_enable           =>  tx_enable_pclk,

            gpif_in             =>  fx3_gpif_in,
            gpif_out            =>  fx3_gpif_out,
            gpif_oe             =>  fx3_gpif_oe,
            ctl_in              =>  fx3_ctl_in,
            ctl_out             =>  fx3_ctl_out,
            ctl_oe              =>  fx3_ctl_oe,

            tx_fifo_write       =>  open,
            tx_fifo_full        =>  '0',
            tx_fifo_empty       =>  '0',
            tx_fifo_usedw       =>  tx_sample_fifo.wused,
            tx_fifo_data        =>  open,

            tx_timestamp        =>  fx3_timestamp,
            tx_meta_fifo_write  =>  open,
            tx_meta_fifo_full   =>  '0',
            tx_meta_fifo_empty  =>  '0',
            tx_meta_fifo_usedw  =>  tx_meta_fifo.wused,
            tx_meta_fifo_data   =>  open,

            rx_fifo_read        =>  open,
            rx_fifo_full        =>  '0',
            rx_fifo_empty       =>  '0',
            rx_fifo_usedw       =>  rx_sample_fifo.rused,
            rx_fifo_data        =>  (others => '0'),

            rx_meta_fifo_read   =>  open,
            rx_meta_fifo_full   =>  '0',
            rx_meta_fifo_empty  =>  '0',
            rx_meta_fifo_usedr  =>  rx_meta_fifo.rused,
            rx_meta_fifo_data   =>  (others => '0')
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
            gpio_in_port                    => pack(nios_gpio.i),
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

            tonegen_sample_clk              => tx_clock,
            tonegen_sample_valid            => tonegen_sample_v,
            tonegen_sample_i                => tonegen_sample_i,
            tonegen_sample_q                => tonegen_sample_q,
            tonegen_status_empty            => open,
            tonegen_status_full             => open,
            tonegen_status_running          => tonegen_running
        );

    tonegen_streams(0).data_i <= signed(tonegen_sample_i);
    tonegen_streams(0).data_q <= signed(tonegen_sample_q);
    tonegen_streams(0).data_v <= tonegen_sample_v;

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
    adi_en_agc     <= unpack(rffe_gpio.o).en_agc;
    adi_txnrx      <= unpack(rffe_gpio.o).txnrx;
    adi_enable     <= unpack(rffe_gpio.o).enable;
    adi_reset_n    <= unpack(rffe_gpio.o).reset_n;

    -- Unpack trigger GPIO bits into records
    rx_trigger_ctl <= unpack(rx_trigger_ctl_i, rx_trigger_line);
    tx_trigger_ctl <= unpack(tx_trigger_ctl_i, tx_trigger_line);

    -- LEDs
    led(1) <= led1_blink            when nios_gpio.o.led_mode = '0' else not nios_gpio.o.leds(1);
    led(2) <= not tonegen_running   when nios_gpio.o.led_mode = '0' else not nios_gpio.o.leds(2);
    led(3) <= rx_overflow_led       when nios_gpio.o.led_mode = '0' else not nios_gpio.o.leds(3);

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

    -- SI53304 controls / clock output enables
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
            tx_underflow_led     => open,
            tx_timestamp         => tx_timestamp,

            -- Triggering
            trigger_arm          => tx_trigger_ctl.arm,
            trigger_fire         => tx_trigger_ctl.fire,
            trigger_master       => tx_trigger_ctl.master,
            trigger_line         => tx_trigger_line,

            -- Samples from host via FX3
            sample_fifo_wclock   => fx3_pclk_pll,
            sample_fifo_wreq     => '0',
            sample_fifo_wdata    => (others => '0'),
            sample_fifo_wempty   => open,
            sample_fifo_wfull    => open,
            sample_fifo_wused    => open,

            -- Metadata from host via FX3
            meta_fifo_wclock     => fx3_pclk_pll,
            meta_fifo_wreq       => '0',
            meta_fifo_wdata      => (others => '0'),
            meta_fifo_wempty     => open,
            meta_fifo_wfull      => open,
            meta_fifo_wused      => open,

            -- Digital Loopback Interface
            loopback_enabled     => '0',
            loopback_fifo_wclock => tx_loopback_fifo.wclock,
            loopback_fifo_wdata  => open,
            loopback_fifo_wreq   => open,
            loopback_fifo_wfull  => '0',
            loopback_fifo_wused  => (others => '0'),

            -- RFFE Interface
            dac_controls         => dac_controls,
            dac_streams          => open
        );

    dac_streams(0) <= tonegen_streams(0);
    dac_streams(1) <= tonegen_streams(0);

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
            rx_timestamp           => rx_timestamp,

            -- Triggering
            trigger_arm            => rx_trigger_ctl.arm,
            trigger_fire           => rx_trigger_ctl.fire,
            trigger_master         => rx_trigger_ctl.master,
            trigger_line           => rx_trigger_line,

            -- Samples to host via FX3
            sample_fifo_rclock     => fx3_pclk_pll,
            sample_fifo_raclr      => '1',
            sample_fifo_rreq       => '0',
            sample_fifo_rdata      => open,
            sample_fifo_rempty     => open,
            sample_fifo_rfull      => open,
            sample_fifo_rused      => open,

            -- Mini expansion signals
            mini_exp               => mini_exp2 & mini_exp1,

            -- Metadata to host via FX3
            meta_fifo_rclock       => fx3_pclk_pll,
            meta_fifo_raclr        => '1',
            meta_fifo_rreq         => '0',
            meta_fifo_rdata        => open,
            meta_fifo_rempty       => open,
            meta_fifo_rfull        => open,
            meta_fifo_rused        => open,

            -- Digital Loopback Interface
            loopback_fifo_wenabled => tx_loopback_enabled,
            loopback_fifo_wreset   => tx_reset,
            loopback_fifo_wclock   => tx_loopback_fifo.wclock,
            loopback_fifo_wdata    => (others => '0'),
            loopback_fifo_wreq     => '0',
            loopback_fifo_wfull    => open,
            loopback_fifo_wused    => open,

            -- RFFE Interface
            adc_controls           => adc_controls,
            adc_streams            => adc_streams
        );

    adc_assignment_proc : process( all )
    begin
        for i in adc_controls'range loop
            adc_controls(i).enable   <= '0';
            adc_controls(i).data_req <= '0';
            adc_streams(i).data_i    <= (others => '0');
            adc_streams(i).data_q    <= (others => '0');
            adc_streams(i).data_v    <= '0';
        end loop;
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

    U_sync_usb_speed_rx : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  '0',
            clock               =>  rx_clock,
            async               =>  nios_gpio.o.usb_speed,
            sync                =>  usb_speed_rx
        );

    U_sync_usb_speed_tx : entity work.synchronizer
        generic map (
            RESET_LEVEL         =>  '0'
        )
        port map (
            reset               =>  '0',
            clock               =>  tx_clock,
            async               =>  nios_gpio.o.usb_speed,
            sync                =>  usb_speed_tx
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

    --generate_sync_mimo_rx_en : for i in mimo_rx_enables'range generate
    --    U_sync_mimo_rx_en : entity work.synchronizer
    --        generic map (
    --            RESET_LEVEL         =>  '0'
    --            )
    --        port map (
    --            reset               =>  '0',
    --            clock               =>  rx_clock,
    --            async               =>  unpack(rffe_gpio.o).mimo_rx_en(i),
    --            sync                =>  mimo_rx_enables(i)
    --        );
    --end generate;

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

end architecture;
