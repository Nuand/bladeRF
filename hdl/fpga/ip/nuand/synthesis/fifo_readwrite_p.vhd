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

package fifo_readwrite_p is

    type sample_control_t is record
        enable   : std_logic;
        data_req : std_logic;
    end record;

    type sample_controls_t is array( natural range <> ) of sample_control_t;

    type sample_stream_t is record
        data_i : signed(15 downto 0);
        data_q : signed(15 downto 0);
        data_v : std_logic;
    end record;

    type sample_streams_t is array( natural range <> ) of sample_stream_t;

    constant SAMPLE_CONTROL_ENABLE  : sample_control_t;
    constant SAMPLE_CONTROL_DISABLE : sample_control_t;

    constant ZERO_SAMPLE            : sample_stream_t;

    -- Count how many channels are enabled
    function count_enabled_channels( x : sample_controls_t ) return natural;

end package;

package body fifo_readwrite_p is

    constant SAMPLE_CONTROL_ENABLE  : sample_control_t := (
        enable   => '1',
        data_req => '1'
    );

    constant SAMPLE_CONTROL_DISABLE : sample_control_t := (
        enable   => '0',
        data_req => '0'
    );


    constant ZERO_SAMPLE            : sample_stream_t := (
        data_i   => (others => '0'),
        data_q   => (others => '0'),
        data_v   => '0'
    );

    -- Count how many channels are enabled
    function count_enabled_channels( x : sample_controls_t ) return natural is
        variable rv : natural := 0;
    begin
        rv := 0;
        for i in x'range loop
            if( x(i).enable = '1' ) then
                rv := rv + 1;
            end if;
        end loop;
        return rv;
    end function;

end package body;
