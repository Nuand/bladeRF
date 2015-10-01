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
        BIT_WIDTH       : positive := 16;
        VREF            : real     := 2.5;
        MZB             : boolean  := true -- power on mid-scale?
        --bypass_spi      : boolean  := true
    )
    port (
        -- Control interface
        --sclk            : in  std_logic;
        --sen             : in  std_logic;
        --sdi             : in  std_logic;
        --sdo             : out std_logic;

        -- Direct control interface (if bypassing SPI)
        dac_count       : in  unsigned(BIT_WIDTH-1 downto 0);
        ldacb           : in  std_logic := '0';

        -- Analog output
        vout            : out real
    );
end entity;

architecture arch of mm_driver is

    constant DAC_MID_SCALE : unsigned(BIT_WIDTH-1 downto 0) :=
        to_unsigned( ((2**(BIT_WIDTH))/2) , BIT_WIDTH );

    constant DAC_LSB_VOLTAGE : real := ( (VREF) / (2**BIT_WIDTH) );

    -- Given a DAC value, get the analog output voltage
    function get_voltage( dac : unsigned ) return real is
    begin
        return ( real(to_integer(dac)) * DAC_LSB_VOLTAGE;
    end function;


    -- FSM States
    type fsm_t is (
    );

    -- State of internal signals
    type state_t is record
        state           : fsm_t;

    end record;

    constant RESET_VALUE : state_t := (
        state           => RESET_COUNTERS,

    );


    signal   current           : state_t := RESET_VALUE;
    signal   future            : state_t := RESET_VALUE;

begin

    comb_proc : process( all )
    begin

        -- Power-up voltage output
        if( MZB ) then
            vout <= DAC_MID_SCALE;
        else
            vout <= 0.0;
        end if;

        -- Continuously load the DAC
        load_dac : while( ldacb = '0' ) loop
            vout <= get_voltage(dac_count);
        end loop;

        report "dac161s055 completed." severity warning;
        wait;

    end process;

end architecture;
