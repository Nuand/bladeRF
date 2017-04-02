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
    -- TYPEDEFS
    -- ========================================================================

    type sky13374_397lf_t is (
        DISABLED,
        RF1_TO_RF2,
        RF1_TO_RF3,
        UNDEFINED
    );

    --type gpio32_t is record
    --    i  : std_logic_vector(31 downto 0);
    --    o  : std_logic_vector(31 downto 0);
    --    oe : std_logic_vector(31 downto 0);
    --end record;

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

    type mimo_t is record
        clock         : std_logic;
        reset         : std_logic;
        adc_overflow  : std_logic;
        adc_underflow : std_logic;
        dac_overflow  : std_logic;
        dac_underflow : std_logic;
        -- Maybe some future version of Quartus will allow unconstrained
        -- arrays within a record that are then constrained when defining
        -- signals of this record type "signal s : mimo_t(ch(0 to 1));"
        -- Until then, define this record as a fixed 2x2 MIMO here.
        ch            : channels_t(0 to 1);
    end record;


    -- ========================================================================
    -- PACK FUNCTIONS -- pack a human-readable record/type into bits
    -- ========================================================================

    function pack( x : sky13374_397lf_t ) return std_logic_vector;
    function pack( x : rffe_gpo_t )       return std_logic_vector;
    function pack( x : rffe_gpio_t )      return std_logic_vector;


    -- ========================================================================
    -- UNPACK FUNCTIONS -- unpack bits into a human-readable record/type
    -- ========================================================================

    function unpack( x : std_logic_vector(1 downto 0)  ) return sky13374_397lf_t;
    function unpack( x : std_logic_vector(31 downto 0) ) return rffe_gpo_t;


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

end package body;
