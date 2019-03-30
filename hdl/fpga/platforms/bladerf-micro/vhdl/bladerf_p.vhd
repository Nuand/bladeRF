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

library work;
    use work.common_dcfifo_p.all;

package bladerf_p is

    -- ========================================================================
    -- Component declarations for Verilog files
    -- ========================================================================

    component system_pll is
        port (
            refclk   : in  std_logic;
            rst      : in  std_logic;
            outclk_0 : out std_logic;
            locked   : out std_logic
        );
    end component;

    component fx3_pll is
        port (
            refclk   : in  std_logic;
            rst      : in  std_logic;
            outclk_0 : out std_logic;
            locked   : out std_logic
        );
    end component;

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
        rx_trigger_ctl_out_port         :   out std_logic_vector(7 downto 0)
      );
    end component;

    -- ========================================================================
    -- TYPEDEFS
    -- ========================================================================

    constant TX_FIFO_WWIDTH         : natural := 32;    -- write side data width
    constant TX_FIFO_RWIDTH         : natural := 64;    -- read side data width
    constant TX_FIFO_LENGTH         : natural := 16384; -- samples

    constant RX_FIFO_WWIDTH         : natural := 64;    -- write side data width
    constant RX_FIFO_RWIDTH         : natural := 32;    -- read side data width
    constant RX_FIFO_LENGTH         : natural := 4096;  -- samples

    constant ADSB_FIFO_WWIDTH       : natural := 128;   -- write side data width
    constant ADSB_FIFO_RWIDTH       : natural := 32;    -- read side data width
    constant ADSB_FIFO_LENGTH       : natural := 1024;  -- samples

    constant LOOPBACK_FIFO_WWIDTH   : natural := 64;    -- write side data width
    constant LOOPBACK_FIFO_RWIDTH   : natural := 64;    -- read side data width
    constant LOOPBACK_FIFO_LENGTH   : natural := 512;   -- samples

    constant META_FIFO_TX_WWIDTH    : natural := 32;    -- write side data width
    constant META_FIFO_TX_RWIDTH    : natural := 128;   -- read side data width
    constant META_FIFO_TX_LENGTH    : natural := 512;   -- 32-bit words

    constant META_FIFO_RX_WWIDTH    : natural := 128;   -- write side data width
    constant META_FIFO_RX_RWIDTH    : natural := 32;    -- read side data width
    constant META_FIFO_RX_LENGTH    : natural := 512;   -- 32-bit words

    type tx_fifo_t is record
        aclr    :   std_logic;

        wclock  :   std_logic;
        wdata   :   std_logic_vector(TX_FIFO_WWIDTH-1 downto 0);
        wreq    :   std_logic;
        wempty  :   std_logic;
        wfull   :   std_logic;
        wused   :   std_logic_vector(compute_wrusedw_high(TX_FIFO_LENGTH, "OFF") downto 0);

        rclock  :   std_logic;
        rdata   :   std_logic_vector(TX_FIFO_RWIDTH-1 downto 0);
        rreq    :   std_logic;
        rempty  :   std_logic;
        rfull   :   std_logic;
        rused   :   std_logic_vector(compute_rdusedw_high(TX_FIFO_LENGTH, TX_FIFO_WWIDTH,
                                                          TX_FIFO_RWIDTH, "OFF") downto 0);
    end record;

    type rx_fifo_t is record
        aclr    :   std_logic;

        wclock  :   std_logic;
        wdata   :   std_logic_vector(RX_FIFO_WWIDTH-1 downto 0);
        wreq    :   std_logic;
        wempty  :   std_logic;
        wfull   :   std_logic;
        wused   :   std_logic_vector(compute_wrusedw_high(RX_FIFO_LENGTH, "OFF") downto 0);

        rclock  :   std_logic;
        rdata   :   std_logic_vector(RX_FIFO_RWIDTH-1 downto 0);
        rreq    :   std_logic;
        rempty  :   std_logic;
        rfull   :   std_logic;
        rused   :   std_logic_vector(compute_rdusedw_high(RX_FIFO_LENGTH, RX_FIFO_WWIDTH,
                                                          RX_FIFO_RWIDTH, "OFF") downto 0);
    end record;

    type adsb_fifo_t is record
        aclr    :   std_logic;

        wclock  :   std_logic;
        wdata   :   std_logic_vector(ADSB_FIFO_WWIDTH-1 downto 0);
        wreq    :   std_logic;
        wempty  :   std_logic;
        wfull   :   std_logic;
        wused   :   std_logic_vector(compute_wrusedw_high(ADSB_FIFO_LENGTH, "OFF") downto 0);

        rclock  :   std_logic;
        rdata   :   std_logic_vector(ADSB_FIFO_RWIDTH-1 downto 0);
        rreq    :   std_logic;
        rempty  :   std_logic;
        rfull   :   std_logic;
        rused   :   std_logic_vector(compute_rdusedw_high(ADSB_FIFO_LENGTH, ADSB_FIFO_WWIDTH,
                                                          ADSB_FIFO_RWIDTH, "OFF") downto 0);
    end record;

    type loopback_fifo_t is record
        aclr    :   std_logic;

        wclock  :   std_logic;
        wdata   :   std_logic_vector(LOOPBACK_FIFO_WWIDTH-1 downto 0);
        wreq    :   std_logic;
        wempty  :   std_logic;
        wfull   :   std_logic;
        wused   :   std_logic_vector(compute_wrusedw_high(LOOPBACK_FIFO_LENGTH, "OFF") downto 0);

        rclock  :   std_logic;
        rdata   :   std_logic_vector(LOOPBACK_FIFO_RWIDTH-1 downto 0);
        rreq    :   std_logic;
        rempty  :   std_logic;
        rfull   :   std_logic;
        rused   :   std_logic_vector(compute_rdusedw_high(LOOPBACK_FIFO_LENGTH, LOOPBACK_FIFO_WWIDTH,
                                                          LOOPBACK_FIFO_RWIDTH, "OFF") downto 0);
    end record;

    type meta_fifo_tx_t is record
        aclr    :   std_logic;

        wclock  :   std_logic;
        wdata   :   std_logic_vector(META_FIFO_TX_WWIDTH-1 downto 0);
        wreq    :   std_logic;
        wempty  :   std_logic;
        wfull   :   std_logic;
        wused   :   std_logic_vector(compute_wrusedw_high(META_FIFO_TX_LENGTH, "OFF") downto 0);

        rclock  :   std_logic;
        rdata   :   std_logic_vector(META_FIFO_TX_RWIDTH-1 downto 0);
        rreq    :   std_logic;
        rempty  :   std_logic;
        rfull   :   std_logic;
        rused   :   std_logic_vector(compute_rdusedw_high(META_FIFO_TX_LENGTH, META_FIFO_TX_WWIDTH,
                                                          META_FIFO_TX_RWIDTH, "OFF") downto 0);
    end record;

    type meta_fifo_rx_t is record
        aclr    :   std_logic;

        wclock  :   std_logic;
        wdata   :   std_logic_vector(META_FIFO_RX_WWIDTH-1 downto 0);
        wreq    :   std_logic;
        wempty  :   std_logic;
        wfull   :   std_logic;
        wused   :   std_logic_vector(compute_wrusedw_high(META_FIFO_RX_LENGTH, "OFF") downto 0);

        rclock  :   std_logic;
        rdata   :   std_logic_vector(META_FIFO_RX_RWIDTH-1 downto 0);
        rreq    :   std_logic;
        rempty  :   std_logic;
        rfull   :   std_logic;
        rused   :   std_logic_vector(compute_rdusedw_high(META_FIFO_RX_LENGTH, META_FIFO_RX_WWIDTH,
                                                          META_FIFO_RX_RWIDTH, "OFF") downto 0);
    end record;

    type nios_gpo_t is record
        xb_mode         : std_logic_vector(1 downto 0);
        si_clock_sel    : std_logic;
        ufl_clock_oe    : std_logic;
        meta_sync       : std_logic;
        led_mode        : std_logic;
        leds            : std_logic_vector(3 downto 1);
        adf_chip_enable : std_logic;
        rx_mux_sel      : std_logic_vector(2 downto 0);
        usb_speed       : std_logic;
    end record;

    type nios_gpi_t is record
        -- Actual inputs
        pwr_status   : std_logic;
        -- Readback of Nios GPIO outputs
        gpo_readback : nios_gpo_t;
    end record;

    type nios_gpio_t is record
        i : nios_gpi_t;
        o : nios_gpo_t;
    end record;

    type sky13374_397lf_t is (
        DISABLED,
        RF1_TO_RF2,
        RF1_TO_RF3,
        UNDEFINED
    );

    type rffe_gpo_t is record
        reset_n      : std_logic;
        enable       : std_logic;
        txnrx        : std_logic;
        en_agc       : std_logic;
        sync_in      : std_logic;
        rx_bias_en   : std_logic;
        rx_spdt1     : std_logic_vector(2 downto 1);
        rx_spdt2     : std_logic_vector(2 downto 1);
        tx_bias_en   : std_logic;
        tx_spdt1     : std_logic_vector(2 downto 1);
        tx_spdt2     : std_logic_vector(2 downto 1);
        mimo_rx_en   : std_logic_vector(1 downto 0);
        mimo_tx_en   : std_logic_vector(1 downto 0);
        ctrl_in      : std_logic_vector(3 downto 0);
    end record;

    type rffe_gpi_t is record
        ctrl_out     : std_logic_vector(7 downto 0);
        adf_muxout   : std_logic;
    end record;

    type rffe_gpio_t is record
        i : rffe_gpi_t;
        o : std_logic_vector(31 downto 0);
    end record;

    type datastream_t is record
        enable : std_logic;
        valid  : std_logic;
        data   : std_logic_vector(15 downto 0);
    end record;

    type sample_t is record
        i      : datastream_t;
        q      : datastream_t;
    end record;

    type channel_t is record
        dac    : sample_t;
        adc    : sample_t;
    end record;

    type channels_t is array( natural range <> ) of channel_t;

    type mimo_2r2t_t is record
        clock         : std_logic;
        reset         : std_logic;
        adc_overflow  : std_logic;
        adc_underflow : std_logic;
        dac_overflow  : std_logic;
        dac_underflow : std_logic;
        ch            : channels_t(0 to 1);
    end record;

    type trigger_t is record
        arm       : std_logic;
        fire      : std_logic;
        master    : std_logic;
        trig_line : std_logic;
    end record;


    -- ========================================================================
    -- PACK FUNCTIONS -- pack a human-readable record/type into bits
    -- ========================================================================

    function pack( x : sky13374_397lf_t ) return std_logic_vector;
    function pack( x : rffe_gpo_t )       return std_logic_vector;
    function pack( x : rffe_gpio_t )      return std_logic_vector;
    function pack( x : trigger_t )        return std_logic_vector;
    function pack( x : nios_gpo_t )       return std_logic_vector;
    function pack( x : nios_gpi_t )       return std_logic_vector;


    -- ========================================================================
    -- UNPACK FUNCTIONS -- unpack bits into a human-readable record/type
    -- ========================================================================

    function unpack( x : std_logic_vector(1 downto 0)  ) return sky13374_397lf_t;
    function unpack( x : std_logic_vector(31 downto 0) ) return rffe_gpo_t;
    function unpack( trig_gpo  : std_logic_vector(7 downto 0);
                     trig_line : std_logic ) return trigger_t;
    function unpack( x : std_logic_vector(31 downto 0) ) return nios_gpo_t;


    -- ========================================================================
    -- Utility functions
    -- ========================================================================

    function adp2384_sync_divisors( constant REFCLK_HZ  : real := 38.4e6;
                                             n_divisors : natural
                                    ) return integer_vector;

    -- ========================================================================
    -- TYPEDEF RESET CONSTANTS -- deferred to permit use of pack/unpack
    -- ========================================================================

    constant RFFE_GPO_DEFAULT           : rffe_gpo_t;
    constant RFFE_GPI_DEFAULT           : rffe_gpi_t;
    constant TX_FIFO_T_DEFAULT          : tx_fifo_t;
    constant RX_FIFO_T_DEFAULT          : rx_fifo_t;
    constant ADSB_FIFO_T_DEFAULT        : adsb_fifo_t;
    constant LOOPBACK_FIFO_T_DEFAULT    : loopback_fifo_t;
    constant META_FIFO_TX_T_DEFAULT     : meta_fifo_tx_t;
    constant META_FIFO_RX_T_DEFAULT     : meta_fifo_rx_t;
    constant MIMO_2R2T_T_DEFAULT        : mimo_2r2t_t;
    constant TRIGGER_T_DEFAULT          : trigger_t;

end package;

package body bladerf_p is

    -- ========================================================================
    -- PACK FUNCTIONS
    -- ========================================================================

    function pack( x : sky13374_397lf_t ) return std_logic_vector is
        variable rv : unsigned(2 downto 1) := (others => '0');
    begin
        case x is
            when DISABLED   => rv := "00";
            when RF1_TO_RF2 => rv := "01";
            when RF1_TO_RF3 => rv := "10";
            when others     => rv := "00";
        end case;
        return std_logic_vector(rv);
    end function;

    function pack( x : rffe_gpo_t ) return std_logic_vector is
        variable rv : std_logic_vector(31 downto 0) := (others => '0');
    begin
        --rv(31 downto 24) := x.ctrl_out;      -- Reserved as inputs
        rv(23 downto 20)   := x.ctrl_in;
        --rv(19)           := x.adf_muxout;    -- Reserved as input
        rv(18)             := x.mimo_tx_en(1);
        rv(17)             := x.mimo_rx_en(1);
        rv(16)             := x.mimo_tx_en(0);
        rv(15)             := x.mimo_rx_en(0);
        rv(14 downto 13)   := x.tx_spdt2;
        rv(12 downto 11)   := x.tx_spdt1;
        rv(10)             := x.tx_bias_en;
        rv(9 downto 8)     := x.rx_spdt2;
        rv(7 downto 6)     := x.rx_spdt1;
        rv(5)              := x.rx_bias_en;
        rv(4)              := x.sync_in;
        rv(3)              := x.en_agc;
        rv(2)              := x.txnrx;
        rv(1)              := x.enable;
        rv(0)              := x.reset_n;
        return rv;
    end function;

    function pack( x : rffe_gpio_t ) return std_logic_vector is
        variable rv : std_logic_vector(31 downto 0);
    begin
        -- Physical inputs
        rv(31 downto 24)  := x.i.ctrl_out;
        rv(19)            := x.i.adf_muxout;
        -- Output readback
        rv(23 downto 20)  := x.o(23 downto 20);
        rv(18 downto 0)   := x.o(18 downto 0);
        return rv;
    end function;

    function pack( x : trigger_t ) return std_logic_vector is
        variable rv : std_logic_vector(7 downto 0);
    begin
        rv(7 downto 4) := (others => '0');
        rv(3)          := x.trig_line;
        rv(2)          := x.master;
        rv(1)          := x.fire;
        rv(0)          := x.arm;
        return rv;
    end function;

    function pack( x : nios_gpo_t ) return std_logic_vector is
        variable rv : std_logic_vector(31 downto 0) := (others => 'U');
    begin
        rv(31 downto 30) := x.xb_mode;
        rv(18)           := x.si_clock_sel;
        rv(17)           := x.ufl_clock_oe;
        rv(16)           := x.meta_sync;
        rv(15)           := x.led_mode;
        rv(14 downto 12) := x.leds;
        rv(11)           := x.adf_chip_enable;
        rv(10 downto 8)  := x.rx_mux_sel;
        rv(7)            := x.usb_speed;
        --rv(0)          := x.pwr_status;  -- Reserved as input
        return rv;
    end function;

    function pack( x : nios_gpi_t ) return std_logic_vector is
        variable rv : std_logic_vector(31 downto 0) := (others => 'U');
    begin
        -- Readback of outputs
        rv    := pack(x.gpo_readback);
        -- Physical inputs
        rv(0) := x.pwr_status;
        return rv;
    end function;


    -- ========================================================================
    -- UNPACK FUNCTIONS
    -- ========================================================================

    function unpack( x : std_logic_vector(1 downto 0) ) return sky13374_397lf_t is
        variable rv : sky13374_397lf_t;
    begin
        case x is
            when "00"   => rv := DISABLED;
            when "01"   => rv := RF1_TO_RF2;
            when "10"   => rv := RF1_TO_RF3;
            when others => rv := UNDEFINED;
        end case;
        return rv;
    end function;

    function unpack( x : std_logic_vector(31 downto 0) ) return rffe_gpo_t is
        variable rv : rffe_gpo_t := RFFE_GPO_DEFAULT;
    begin
        --rv.ctrl_out    := x(31 downto 24); -- Reserved as input
        rv.ctrl_in       := x(23 downto 20);
        --rv.adf_muxout  := x(19);           -- Reserved as input
        rv.mimo_tx_en    := x(18) & x(16);   -- channel 1 & channel 0
        rv.mimo_rx_en    := x(17) & x(15);   -- channel 1 & channel 0
        rv.tx_spdt2      := x(14 downto 13);
        rv.tx_spdt1      := x(12 downto 11);
        rv.tx_bias_en    := x(10);
        rv.rx_spdt2      := x(9 downto 8);
        rv.rx_spdt1      := x(7 downto 6);
        rv.rx_bias_en    := x(5);
        rv.sync_in       := x(4);
        rv.en_agc        := x(3);
        rv.txnrx         := x(2);
        rv.enable        := x(1);
        rv.reset_n       := x(0);
        return rv;
    end function;

    function unpack( trig_gpo  : std_logic_vector(7 downto 0);
                     trig_line : std_logic ) return trigger_t is
        variable rv : trigger_t := TRIGGER_T_DEFAULT;
    begin
        rv.trig_line := trig_line;
        rv.master    := trig_gpo(2);
        rv.fire      := trig_gpo(1);
        rv.arm       := trig_gpo(0);
        return rv;
    end function;

    function unpack( x : std_logic_vector(31 downto 0) ) return nios_gpo_t is
        variable rv : nios_gpo_t;
    begin
        rv.xb_mode         := x(31 downto 30);
        rv.si_clock_sel    := x(18);
        rv.ufl_clock_oe    := x(17);
        rv.meta_sync       := x(16);
        rv.led_mode        := x(15);
        rv.leds            := x(14 downto 12);
        rv.adf_chip_enable := x(11);
        rv.rx_mux_sel      := x(10 downto 8);
        rv.usb_speed       := x(7);
        --rv.pwr_status    := x(0);            -- Reserved as input
        return rv;
    end function;


    -- ========================================================================
    -- Utility functions
    -- ========================================================================

    -- ADP2384 SYNC divisor calculator
    --   The ADP2384 supports a synchronization frequency within +/-10% of the
    --   switching frequency set by the RT resistor. Given the frequency of the
    ---  reference clock (the clock that will be divided down), and the number
    --   of divisors requested, this function will compute and return an array
    --   of equally-spaced divisors producing valid SYNC frequencies.
    --
    -- Parameters:
    --   refclk_hz:    Frequency of the reference clock to be divided down
    --   n_sync_freqs: Number of divisors to compute
    function adp2384_sync_divisors( constant REFCLK_HZ  : real := 38.4e6;
                                             n_divisors : natural
                                    ) return integer_vector is

        -- Because Quartus 16.0 doesn't support this function from VHDL 2008
        function minimum( x : real; y : real ) return real is
            variable rv : real;
        begin
            if( x < y ) then
                rv := x;
            else
                rv := y;
            end if;
            return rv;
        end function;

        -- Because Quartus 16.0 doesn't support this function from VHDL 2008
        function maximum( x : real; y : real ) return real is
            variable rv : real;
        begin
            if( x > y ) then
                rv := x;
            else
                rv := y;
            end if;
            return rv;
        end function;

        -- Convert to kHz for ease of calculations
        constant REFCLK_KHZ       : real    := REFCLK_HZ / 1.0e3;

        -- Min/max switching frequency of ADP2384
        constant FSW_KHZ_MIN      : real    := 200.0;
        constant FSW_KHZ_MAX      : real    := 1400.0;

        -- RT resistor value and tolerance
        constant RT_KOHM          : real    := 332.0;
        constant RT_TOL           : real    := 0.01;


        -- Given the RT resistor value and tolerance, compute the min/max
        -- and convert to kohm.
        constant RT_KOHM_MIN      : real    := RT_KOHM * (1.0 - RT_TOL);
        constant RT_KOHM_MAX      : real    := RT_KOHM * (1.0 + RT_TOL);

        -- Compute the min/max switching frequencies (kHz) dictated by RT
        constant RT_FSW_KHZ_MIN    : real    := 69120.0 / (RT_KOHM_MAX + 15.0);
        constant RT_FSW_KHZ_MAX    : real    := 69120.0 / (RT_KOHM_MIN + 15.0);

        -- The SYNC pin frequency can be +/- 10% of the frequency set by RT.
        -- Due to tolerances, use -10% of fsw_max, and +10% of fsw_min. This
        -- will guarantee a compatible fsync across all board variations.
        constant FSYNC_TOL        : real    := 0.10;
        constant FSYNC_KHZ_MIN    : real    := (1.0 - FSYNC_TOL) * RT_FSW_KHZ_MAX;
        constant FSYNC_KHZ_MAX    : real    := (1.0 + FSYNC_TOL) * RT_FSW_KHZ_MIN;

        -- Compute the max/min divisors, keeping the sync frequency within the valid
        -- window of the ADP2384. Divide by two due to the 50% duty cycle.
        constant DIVISOR_MIN      : real    := ceil(  (REFCLK_KHZ / minimum(FSYNC_KHZ_MAX, FSW_KHZ_MAX)) / 2.0 );
        constant DIVISOR_MAX      : real    := floor( (REFCLK_KHZ / maximum(FSYNC_KHZ_MIN, FSW_KHZ_MIN)) / 2.0 );

        -- Compute the number of cycles between divisors to create an equally
        -- spaced array of divisors of the requested length.
        constant DIVISOR_INCR     : real    := (DIVISOR_MAX - DIVISOR_MIN) / real(n_divisors - 1);

        variable rv               : integer_vector(0 to n_divisors-1);
        variable fsync_khz        : real;

    begin

        assert (DIVISOR_INCR >= 1.0)
            report "ERROR: Too many divisors requested for range " &
            real'image(DIVISOR_MIN) & " to " & real'image(DIVISOR_MAX) & "."
            severity failure;

        for i in 0 to (n_divisors - 1) loop
            rv(i)     := integer(floor(DIVISOR_MAX - (real(i) * DIVISOR_INCR)));
            fsync_khz := REFCLK_KHZ / real( rv(i) * 2 );

            report "ADP2384 SYNC frequency " & integer'image(i) & " = " &
                real'image(fsync_khz) & " kHz."
                severity note;

            assert ( fsync_khz >= FSW_KHZ_MIN ) and ( fsync_khz <= FSW_KHZ_MAX )
                report "ERROR: ADP2384 SYNC frequency " & real'image(fsync_khz) &
                " is outside of valid range (" & real'image(FSW_KHZ_MIN) & " kHz to " &
                real'image(FSW_KHZ_MAX) & "kHz."
                severity failure;
        end loop;

        return rv;

    end function;

    -- ========================================================================
    -- TYPEDEF RESET CONSTANTS
    -- ========================================================================

    constant RFFE_GPO_DEFAULT : rffe_gpo_t := (
        reset_n      => '0',
        enable       => '0',
        txnrx        => '0',
        en_agc       => '0',
        sync_in      => '0',
        rx_bias_en   => '0',
        rx_spdt1     => pack(DISABLED),
        rx_spdt2     => pack(DISABLED),
        tx_bias_en   => '0',
        tx_spdt1     => pack(DISABLED),
        tx_spdt2     => pack(DISABLED),
        mimo_rx_en   => (others => '0'),
        mimo_tx_en   => (others => '0'),
        ctrl_in      => (others => '0')
    );

    constant RFFE_GPI_DEFAULT : rffe_gpi_t := (
        ctrl_out    => (others => '0'),
        adf_muxout  => '0'
    );

    constant TX_FIFO_T_DEFAULT : tx_fifo_t := (
        aclr    => '1',
        wclock  => '0',
        wdata   => (others => '0'),
        wreq    => '0',
        wempty  => '1',
        wfull   => '0',
        wused   => (others => '0'),
        rclock  => '0',
        rdata   => (others => '0'),
        rreq    => '0',
        rempty  => '1',
        rfull   => '0',
        rused   => (others => '0')
    );

    constant RX_FIFO_T_DEFAULT : rx_fifo_t := (
        aclr    => '1',
        wclock  => '0',
        wdata   => (others => '0'),
        wreq    => '0',
        wempty  => '1',
        wfull   => '0',
        wused   => (others => '0'),
        rclock  => '0',
        rdata   => (others => '0'),
        rreq    => '0',
        rempty  => '1',
        rfull   => '0',
        rused   => (others => '0')
    );

    constant ADSB_FIFO_T_DEFAULT : adsb_fifo_t := (
        aclr    => '1',
        wclock  => '0',
        wdata   => (others => '0'),
        wreq    => '0',
        wempty  => '1',
        wfull   => '0',
        wused   => (others => '0'),
        rclock  => '0',
        rdata   => (others => '0'),
        rreq    => '0',
        rempty  => '1',
        rfull   => '0',
        rused   => (others => '0')
    );

    constant LOOPBACK_FIFO_T_DEFAULT : loopback_fifo_t := (
        aclr    => '1',
        wclock  => '0',
        wdata   => (others => '0'),
        wreq    => '0',
        wempty  => '1',
        wfull   => '0',
        wused   => (others => '0'),
        rclock  => '0',
        rdata   => (others => '0'),
        rreq    => '0',
        rempty  => '1',
        rfull   => '0',
        rused   => (others => '0')
    );

    constant META_FIFO_TX_T_DEFAULT : meta_fifo_tx_t := (
        aclr    => '1',
        wclock  => '0',
        wdata   => (others => '0'),
        wreq    => '0',
        wempty  => '1',
        wfull   => '0',
        wused   => (others => '0'),
        rclock  => '0',
        rdata   => (others => '0'),
        rreq    => '0',
        rempty  => '1',
        rfull   => '0',
        rused   => (others => '0')
    );

    constant META_FIFO_RX_T_DEFAULT : meta_fifo_rx_t := (
        aclr    => '1',
        wclock  => '0',
        wdata   => (others => '0'),
        wreq    => '0',
        wempty  => '1',
        wfull   => '0',
        wused   => (others => '0'),
        rclock  => '0',
        rdata   => (others => '0'),
        rreq    => '0',
        rempty  => '1',
        rfull   => '0',
        rused   => (others => '0')
    );

    constant MIMO_2R2T_T_DEFAULT : mimo_2r2t_t := (
        clock         => '0',
        reset         => '1',
        adc_overflow  => '0',
        adc_underflow => '0',
        dac_overflow  => '0',
        dac_underflow => '0',
        ch            => (others => (
            dac => (i => (enable => '0',
                          valid  => '0',
                          data   => (others => '0')),
                    q => (enable => '0',
                          valid  => '0',
                          data   => (others => '0'))),
            adc => (i => (enable => '0',
                          valid  => '0',
                          data   => (others => '0')),
                    q => (enable => '0',
                          valid  => '0',
                          data   => (others => '0')))
            ))
    );

    constant TRIGGER_T_DEFAULT : trigger_t := (
        arm       => '0',
        fire      => '0',
        master    => '0',
        trig_line => '0'
    );

end package body;
