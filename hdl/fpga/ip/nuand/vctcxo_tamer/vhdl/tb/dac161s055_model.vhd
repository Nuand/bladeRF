-- Copyright (c) 2015 Nuand LLC
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

entity dac161s055_model is
    generic (
        -- Defaults chosen to match bladerf.pdf
        DAC_WIDTH       : positive := 16;
        VREF            : real     := 2.5;
        MZB             : boolean  := true; -- power on mid-scale?
        DIRECT_CONTROL  : boolean  := true
    );
    port (
        -- SPI Control interface
        --sclk            : in  std_logic;
        --sen             : in  std_logic;
        --sdi             : in  std_logic;
        --sdo             : out std_logic;

        -- Direct control interface
        dac_count       : in  unsigned(DAC_WIDTH-1 downto 0);
        ldacb           : in  std_logic := '0';

        -- Analog output
        vout            : out real
    );
end entity;

architecture arch of dac161s055_model is

    constant DAC_MID_SCALE : unsigned(DAC_WIDTH-1 downto 0) :=
        to_unsigned( ((2**(DAC_WIDTH))/2)-1 , DAC_WIDTH );

    constant DAC_LSB_VOLTAGE : real := ( (VREF) / real(2**DAC_WIDTH) );

    -- Given a DAC value, get the analog output voltage
    function get_voltage( dac : unsigned ) return real is
    begin
        return ( real(to_integer(dac)) * DAC_LSB_VOLTAGE );
    end function;


    -- FSM States
    --type fsm_t is (     );

    -- State of internal signals
    --type state_t is record
    --    state           : fsm_t;
    --end record;

    --constant RESET_VALUE : state_t := (
    --    state           => RESET_COUNTERS,
    --);


    --signal   current           : state_t := RESET_VALUE;
    --signal   future            : state_t := RESET_VALUE;

    signal   powered_up        : boolean := false;

begin

    dac_guts : if( DIRECT_CONTROL = true ) generate
        comb_proc : process( all )
        begin

            if( powered_up = false ) then
                -- Power-up voltage output
                if( MZB ) then
                    vout <= get_voltage(DAC_MID_SCALE);
                else
                    vout <= 0.0;
                end if;
                powered_up <= true;
            end if;

            if( ldacb = '0' ) then
                vout <= get_voltage(dac_count);
            end if;

        end process;
    end generate;

end architecture;
