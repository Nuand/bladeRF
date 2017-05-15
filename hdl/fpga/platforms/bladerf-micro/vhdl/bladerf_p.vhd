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

    type fifo_t is record
        aclr    :   std_logic;

        wclock  :   std_logic;
        wdata   :   std_logic_vector(31 downto 0);
        wreq    :   std_logic;
        wempty  :   std_logic;
        wfull   :   std_logic;
        wused   :   std_logic_vector(11 downto 0);

        rclock  :   std_logic;
        rdata   :   std_logic_vector(31 downto 0);
        rreq    :   std_logic;
        rempty  :   std_logic;
        rfull   :   std_logic;
        rused   :   std_logic_vector(11 downto 0);
    end record;

    type meta_fifo_tx_t is record
        aclr    :   std_logic;

        wclock  :   std_logic;
        wdata   :   std_logic_vector(31 downto 0);
        wreq    :   std_logic;
        wempty  :   std_logic;
        wfull   :   std_logic;
        wused   :   std_logic_vector(4 downto 0);

        rclock  :   std_logic;
        rdata   :   std_logic_vector(127 downto 0);
        rreq    :   std_logic;
        rempty  :   std_logic;
        rfull   :   std_logic;
        rused   :   std_logic_vector(2 downto 0);
    end record;

    type meta_fifo_rx_t is record
        aclr    :   std_logic;

        wclock  :   std_logic;
        wdata   :   std_logic_vector(127 downto 0);
        wreq    :   std_logic;
        wempty  :   std_logic;
        wfull   :   std_logic;
        wused   :   std_logic_vector(4 downto 0);

        rclock  :   std_logic;
        rdata   :   std_logic_vector(31 downto 0);
        rreq    :   std_logic;
        rempty  :   std_logic;
        rfull   :   std_logic;
        rused   :   std_logic_vector(6 downto 0);
    end record;

    type nios_gpio_t is record
        usb_speed   : std_logic;
        rx_mux_sel  : std_logic_vector(2 downto 0);
        spi_mux     : std_logic;
        leds        : std_logic_vector(3 downto 1);
        led_mode    : std_logic;
        meta_sync   : std_logic;
        channel_sel : std_logic;
        xb_mode     : std_logic_vector(1 downto 0);
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
        ctrl_in      : std_logic_vector(3 downto 0);
        sync_in      : std_logic;
        rx_spdt1     : std_logic_vector(2 downto 1);
        rx_spdt2     : std_logic_vector(2 downto 1);
        rx_bias_en   : std_logic;
        tx_spdt1     : std_logic_vector(2 downto 1);
        tx_spdt2     : std_logic_vector(2 downto 1);
        tx_bias_en   : std_logic;
    end record;

    type rffe_gpi_t is record
        ctrl_out     : std_logic_vector(7 downto 0);
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


    -- ========================================================================
    -- UNPACK FUNCTIONS -- unpack bits into a human-readable record/type
    -- ========================================================================

    function unpack( x : std_logic_vector(1 downto 0)  ) return sky13374_397lf_t;
    function unpack( x : std_logic_vector(31 downto 0) ) return rffe_gpo_t;
    function unpack( trig_gpo  : std_logic_vector(7 downto 0);
                     trig_line : std_logic ) return trigger_t;


    -- ========================================================================
    -- TYPEDEF RESET CONSTANTS
    -- ========================================================================

    constant RFFE_GPO_DEFAULT : rffe_gpo_t := (
        ctrl_in      => (others => '0'),
        tx_spdt2     => pack(DISABLED),
        tx_spdt1     => pack(DISABLED),
        tx_bias_en   => '0',
        rx_spdt2     => pack(DISABLED),
        rx_spdt1     => pack(DISABLED),
        rx_bias_en   => '0',
        sync_in      => '0',
        en_agc       => '0',
        txnrx        => '0',
        enable       => '0',
        reset_n      => '0'
    );

    constant RFFE_GPI_DEFAULT : rffe_gpi_t := (
        ctrl_out     => (others => '0')
    );

    constant FIFO_T_DEFAULT : fifo_t := (
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
        --rv(31 downto 24) := x.ctrl_out;    -- Reserved as inputs
        rv(23 downto 20) := x.ctrl_in;
        rv(19 downto 15) := (others => '0'); -- Available for future use
        rv(14 downto 13) := x.tx_spdt2;
        rv(12 downto 11) := x.tx_spdt1;
        rv(10)           := x.tx_bias_en;
        rv(9 downto 8)   := x.rx_spdt2;
        rv(7 downto 6)   := x.rx_spdt1;
        rv(5)            := x.rx_bias_en;
        rv(4)            := x.sync_in;
        rv(3)            := x.en_agc;
        rv(2)            := x.txnrx;
        rv(1)            := x.enable;
        rv(0)            := x.reset_n;
        return rv;
    end function;

    function pack( x : rffe_gpio_t ) return std_logic_vector is
        variable rv : std_logic_vector(31 downto 0);
    begin
        -- Physical inputs
        rv(31 downto 24) := x.i.ctrl_out;
        -- Output readback
        rv(23 downto 0)  := x.o(23 downto 0);
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
        --rv.ctrl_out      := x(31 downto 24);
        rv.ctrl_in       := x(23 downto 20);
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

end package body;
