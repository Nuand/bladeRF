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

    --component system_pll is
    --    port (
    --        refclk   : in  std_logic;
    --        rst      : in  std_logic;
    --        outclk_0 : out std_logic;
    --        locked   : out std_logic
    --    );
    --end component;
    --
    --component fx3_pll is
    --    port (
    --        refclk   : in  std_logic;
    --        rst      : in  std_logic;
    --        outclk_0 : out std_logic;
    --        locked   : out std_logic
    --    );
    --end component;

    component nios_system is
      port (
        clk_clk                         :   in  std_logic := 'X'; -- clk
        reset_reset_n                   :   in  std_logic := 'X'; -- reset_n
        dac_MISO                        :   in  std_logic := 'X'; -- MISO
        dac_MOSI                        :   out std_logic;        -- MOSI
        dac_SCLK                        :   out std_logic;        -- SCLK
        dac_SS_n                        :   out std_logic_vector(1 downto 0);        -- SS_n
        spi_MISO                        :   in  std_logic := 'X'; -- MISO
        spi_MOSI                        :   out std_logic;        -- MOSI
        spi_SCLK                        :   out std_logic;        -- SCLK
        spi_SS_n                        :   out std_logic;        -- SS_n
        oc_i2c_scl_pad_o                :   out std_logic;
        oc_i2c_scl_padoen_o             :   out std_logic;
        oc_i2c_sda_pad_i                :   in  std_logic;
        oc_i2c_sda_pad_o                :   out std_logic;
        oc_i2c_sda_padoen_o             :   out std_logic;
        oc_i2c_arst_i                   :   in  std_logic;
        oc_i2c_scl_pad_i                :   in  std_logic;
        gpio_in_port                    :   in  std_logic_vector(31 downto 0);
        gpio_out_port                   :   out std_logic_vector(31 downto 0);
        xb_gpio_in_port                 :   in  std_logic_vector(31 downto 0) := (others => 'X');
        xb_gpio_out_port                :   out std_logic_vector(31 downto 0);
        xb_gpio_dir_export              :   out std_logic_vector(31 downto 0);
        command_serial_in               :   in  std_logic ;
        command_serial_out              :   out std_logic ;
        correction_rx_phase_gain_export :   out std_logic_vector(31 downto 0);
        correction_tx_phase_gain_export :   out std_logic_vector(31 downto 0);
        rx_tamer_ts_sync_in             :   in  std_logic;
        rx_tamer_ts_sync_out            :   out std_logic ;
        rx_tamer_ts_pps                 :   in  std_logic ;
        rx_tamer_ts_clock               :   in  std_logic ;
        rx_tamer_ts_reset               :   in  std_logic;
        rx_tamer_ts_time                :   out std_logic_vector(63 downto 0) ;
        tx_tamer_ts_sync_in             :   in  std_logic;
        tx_tamer_ts_sync_out            :   out std_logic ;
        tx_tamer_ts_pps                 :   in  std_logic ;
        tx_tamer_ts_clock               :   in  std_logic ;
        tx_tamer_ts_reset               :   in  std_logic;
        tx_tamer_ts_time                :   out std_logic_vector(63 downto 0);
        vctcxo_tamer_tune_ref           :   in  std_logic;
        vctcxo_tamer_vctcxo_clock       :   in  std_logic;
        tx_trigger_ctl_in_port          :   in std_logic_vector(7 downto 0) := (others => '0');
        tx_trigger_ctl_out_port         :   out std_logic_vector(7 downto 0);
        rx_trigger_ctl_in_port          :   in std_logic_vector(7 downto 0) := (others => '0');
        rx_trigger_ctl_out_port         :   out std_logic_vector(7 downto 0);
        arbiter_request                 :   in  std_logic_vector(1 downto 0) := (others => '0');
        arbiter_granted                 :   out std_logic_vector(1 downto 0) := (others => '0');
        arbiter_ack                     :   in  std_logic_vector(1 downto 0) := (others => '0');
        agc_dc_i_max_export             :   out std_logic_vector(15 downto 0);
        agc_dc_i_mid_export             :   out std_logic_vector(15 downto 0);
        agc_dc_i_min_export             :   out std_logic_vector(15 downto 0);
        agc_dc_q_max_export             :   out std_logic_vector(15 downto 0);
        agc_dc_q_mid_export             :   out std_logic_vector(15 downto 0);
        agc_dc_q_min_export             :   out std_logic_vector(15 downto 0)
      );
    end component;

    -- ========================================================================
    -- TYPEDEFS
    -- ========================================================================

    constant TX_FIFO_WWIDTH         : natural := 32;    -- write side data width
    constant TX_FIFO_RWIDTH         : natural := 32;    -- read side data width
    constant TX_FIFO_LENGTH         : natural := 4096;  -- samples

    constant RX_FIFO_WWIDTH         : natural := 32;    -- write side data width
    constant RX_FIFO_RWIDTH         : natural := 32;    -- read side data width
    constant RX_FIFO_LENGTH         : natural := 4096;  -- samples

    constant ADSB_FIFO_WWIDTH      : natural := 128;   -- write side data width
    constant ADSB_FIFO_RWIDTH      : natural := 32;    -- read side data width
    constant ADSB_FIFO_LENGTH      : natural := 1024;  -- samples

    constant LOOPBACK_FIFO_WWIDTH   : natural := 32;    -- write side data width
    constant LOOPBACK_FIFO_RWIDTH   : natural := 32;    -- read side data width
    constant LOOPBACK_FIFO_LENGTH   : natural := 512;   -- samples

    constant META_FIFO_TX_WWIDTH    : natural := 32;    -- write side data width
    constant META_FIFO_TX_RWIDTH    : natural := 128;   -- read side data width
    constant META_FIFO_TX_LENGTH    : natural := 32;    -- 32-bit words

    constant META_FIFO_RX_WWIDTH    : natural := 128;   -- write side data width
    constant META_FIFO_RX_RWIDTH    : natural := 32;    -- read side data width
    constant META_FIFO_RX_LENGTH    : natural := 32;    -- 32-bit words

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
        agc_en          : std_logic;
        agc_band_sel    : std_logic;
        ts_div2         : std_logic;  -- Not used in FPGA versions >= 0.3.0
        meta_en         : std_logic;
        led_mode        : std_logic;
        leds            : std_logic_vector(3 downto 1);
        rx_mux_sel      : std_logic_vector(2 downto 0);
        usb_speed       : std_logic;
        lms_rx_v        : std_logic_vector(2 downto 1);
        lms_tx_v        : std_logic_vector(2 downto 1);
        lms_tx_enable   : std_logic;
        lms_rx_enable   : std_logic;
        lms_reset       : std_logic;
    end record;

    type nios_gpi_t is record
        -- Why? Are these used by host at all?
        nios_ss_n       : std_logic_vector(1 downto 0);
        xb_mode2        : std_logic_vector(1 downto 0);

        -- Normal readback of Nios general-purpose outputs
        gpo_readback    : nios_gpo_t;
    end record;

    type nios_gpio_t is record
        i : nios_gpi_t;
        o : nios_gpo_t;
    end record;


    -- ========================================================================
    -- PACK FUNCTIONS -- pack a human-readable record/type into bits
    -- ========================================================================

    function pack( x : nios_gpo_t )       return std_logic_vector;
    function pack( x : nios_gpi_t )       return std_logic_vector;


    -- ========================================================================
    -- UNPACK FUNCTIONS -- unpack bits into a human-readable record/type
    -- ========================================================================

    function unpack( x : std_logic_vector(31 downto 0) ) return nios_gpo_t;


    -- ========================================================================
    -- TYPEDEF RESET CONSTANTS -- deferred to permit use of pack/unpack
    -- ========================================================================

    constant TX_FIFO_T_DEFAULT          : tx_fifo_t;
    constant RX_FIFO_T_DEFAULT          : rx_fifo_t;
    constant ADSB_FIFO_T_DEFAULT        : adsb_fifo_t;
    constant LOOPBACK_FIFO_T_DEFAULT    : loopback_fifo_t;
    constant META_FIFO_TX_T_DEFAULT     : meta_fifo_tx_t;
    constant META_FIFO_RX_T_DEFAULT     : meta_fifo_rx_t;

end package;

package body bladerf_p is

    -- ========================================================================
    -- PACK FUNCTIONS
    -- ========================================================================

    function pack( x : nios_gpo_t ) return std_logic_vector is
        variable rv : std_logic_vector(31 downto 0) := (others => '0');
    begin
        rv(31 downto 30) := x.xb_mode;
        -- AVAILABLE: x(29 downto 23);
        -- RESERVED:  rv(22 downto 21) := x.xb_mode2;  -- Why? Is this even needed?
        -- RESERVED:  rv(20 downto 19) := x.nios_ss_n; -- Why? Is this even needed?
        rv(18)           := x.agc_en;
        rv(17)           := x.ts_div2; -- Not used in FPGA versions >= 0.3.0
        rv(16)           := x.meta_en;
        rv(15)           := x.led_mode;
        rv(14 downto 12) := x.leds;
        -- AVAILABLE: rv(11)
        rv(10 downto 8)  := x.rx_mux_sel;
        rv(7)            := x.usb_speed;
        rv(6 downto 5)   := x.lms_rx_v;
        rv(4 downto 3)   := x.lms_tx_v;
        rv(2)            := x.lms_tx_enable;
        rv(1)            := x.lms_rx_enable;
        rv(0)            := x.lms_reset;
        return rv;
    end function;

    function pack( x : nios_gpi_t ) return std_logic_vector is
        variable rv : std_logic_vector(31 downto 0) := (others => '0');
    begin
        -- Readback of outputs
        rv               := pack(x.gpo_readback);
        -- Inputs
        rv(22 downto 21) := x.xb_mode2;  -- Why? Is this even needed?
        rv(20 downto 19) := x.nios_ss_n; -- Why? Is this even needed?
        return rv;
    end function;


    -- ========================================================================
    -- UNPACK FUNCTIONS
    -- ========================================================================

    function unpack( x : std_logic_vector(31 downto 0) ) return nios_gpo_t is
        variable rv : nios_gpo_t;
    begin
        rv.xb_mode         := x(31 downto 30);
        -- AVAILABLE: x(29 downto 23);
        --rv.xb_mode2      := x(22 downto 21); -- Why? Is this even needed?
        --rv.nios_ss_n     := x(20 downto 19); -- Why? Is this even needed?
        rv.agc_en          := x(18);
        rv.ts_div2         := x(17); -- Not used in FPGA versions >= 0.3.0
        rv.meta_en         := x(16);
        rv.led_mode        := x(15);
        rv.leds            := x(14 downto 12);
        -- Available: x(11)
        rv.rx_mux_sel      := x(10 downto 8);
        rv.usb_speed       := x(7);
        rv.lms_rx_v        := x(6 downto 5);
        rv.agc_band_sel    := rv.lms_rx_v(rv.lms_rx_v'low);
        rv.lms_tx_v        := x(4 downto 3);
        rv.lms_tx_enable   := x(2);
        rv.lms_rx_enable   := x(1);
        rv.lms_reset       := x(0);
        return rv;
    end function;


    -- ========================================================================
    -- TYPEDEF RESET CONSTANTS
    -- ========================================================================

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

end package body;
