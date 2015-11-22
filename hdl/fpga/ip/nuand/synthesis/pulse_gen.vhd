-- From Altera Solution ID rd05312011_49, need the following for Qsys to use VHDL 2008:
-- altera vhdl_input_version vhdl_2008

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

-- -----------------------------------------------------------------------------
-- Entity:      pulse_gen
-- Description: Takes an incoming (previously synchronized) signal that lasts
--              for one or more clock cycles, and outputs a single-cycle pulse.
--
-- Parameters:
--   EDGE_RISE    : '1' = generates a pulse when input transitions low-to-high
--   EDGE_FALL    : '1' = generates a pulse when input transitions high-to-low
--
--   It is possible to have both a rising and falling edge pulse, meaning
--   the output pulse will occur for one clock period on the rising edge
--   of the input, and once more for the falling edge of the input.
--
-- -----------------------------------------------------------------------------
entity pulse_gen is
    generic(
        EDGE_RISE       : std_logic := '1';
        EDGE_FALL       : std_logic := '0'
    );
    port(
        clock           : in  std_logic;
        reset           : in  std_logic;
        sync_in         : in  std_logic := '0';
        pulse_out       : out std_logic
    );
end entity;

architecture arch of pulse_gen is

    type fsm_t is (
        IDLE,
        RUNNING
    );

    type state_t is record
        state   : fsm_t;
        history : std_logic_vector(1 downto 0);
    end record;

    constant RESET_VALUES : state_t := (
        state   => IDLE,
        history => (others => '-')
    );

    signal current : state_t;
    signal future  : state_t;

begin

    sync_proc : process( clock, reset )
    begin
        if( reset = '1' ) then
            current <= RESET_VALUES;
        elsif( rising_edge(clock) ) then
            current <= future;
        end if;
    end process;

    comb_proc : process( all )
    begin

        -- Defaults
        future    <= current;
        pulse_out <= '0';

        case( current.state ) is
            when IDLE =>

                -- Read current value of sync_in and use it to populate
                -- input history to prevent initial false pulses.
                future.history <= (others => sync_in);
                future.state   <= RUNNING;

            when RUNNING =>

                future.history <= current.history(0) & sync_in;

                -- Output a pulse when input signal transitions from low to high
                if( EDGE_RISE = '1' ) then
                    if( current.history = "01" ) then
                        pulse_out <= '1';
                    end if;
                end if;

                -- Output a pulse when input signal transitions from high to low
                if( EDGE_FALL = '1' ) then
                    if( current.history = "10" ) then
                        pulse_out <= '1';
                    end if;
                end if;

        end case;

    end process;

end architecture;
