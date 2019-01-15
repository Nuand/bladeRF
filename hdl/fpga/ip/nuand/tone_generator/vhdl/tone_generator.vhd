-- Copyright (c) 2019 Nuand LLC
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

-- From Altera Solution ID rd05312011_49, need the following for Qsys to use VHDL 2008:
-- altera vhdl_input_version vhdl_2008

library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;

package tone_generator_p is
    type tone_generator_input_t is record
        dphase      : signed(15 downto 0);
        duration    : unsigned(31 downto 0);
        valid       : std_logic;
    end record;

    type tone_generator_output_t is record
        re          : signed(15 downto 0);
        im          : signed(15 downto 0);
        valid       : std_logic;
        active      : std_logic;
    end record;

    function NULL_TONE_GENERATOR_INPUT return tone_generator_input_t;
    function NULL_TONE_GENERATOR_OUTPUT return tone_generator_output_t;
end package tone_generator_p;

package body tone_generator_p is
    function NULL_TONE_GENERATOR_INPUT return tone_generator_input_t is
        variable rv : tone_generator_input_t;
    begin
        rv.dphase   := (others => '0');
        rv.duration := (others => '0');
        rv.valid    := '0';
        return rv;
    end function NULL_TONE_GENERATOR_INPUT;

    function NULL_TONE_GENERATOR_OUTPUT return tone_generator_output_t is
        variable rv : tone_generator_output_t;
    begin
        rv.re       := (others => '0');
        rv.im       := (others => '0');
        rv.valid    := '0';
        rv.active   := '0';
        return rv;
    end function NULL_TONE_GENERATOR_OUTPUT;
end package body tone_generator_p;

library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;
    use ieee.math_real.all;

library work;
    use work.nco_p.all;
    use work.tone_generator_p.all;

entity tone_generator is
    generic (
        HOLDOVER_CLOCKS : natural := 16
    );
    port (
        clock       : in  std_logic;
        reset       : in  std_logic;
        inputs      : in  tone_generator_input_t;
        outputs     : out tone_generator_output_t
    );
end entity tone_generator;

architecture arch of tone_generator is
    type fsm_t is (IDLE, START_TONE, WAIT_FOR_COUNTDOWN, TAIL);

    type state_t is record
        state           : fsm_t;
        is_active       : boolean;
        countdown       : natural;
        tailcount       : natural;
        tone_in         : tone_generator_input_t;
        nco_in          : nco_input_t;
    end record state_t;

    function NULL_STATE return state_t is
        variable rv : state_t;
    begin
        rv.state        := IDLE;
        rv.is_active    := false;
        rv.countdown    := 0;
        rv.tailcount    := 0;
        rv.tone_in      := NULL_TONE_GENERATOR_INPUT;
        rv.nco_in       := (dphase => (others => '0'), valid => '0');
        return rv;
    end function NULL_STATE;

    signal current  : state_t;
    signal future   : state_t;

    signal nco_out  : nco_output_t;
begin
    U_nco : entity work.nco
      port map (
        clock   => clock,
        reset   => reset,
        inputs  => current.nco_in,
        outputs => nco_out
      );

    outputs.re     <= nco_out.re;
    outputs.im     <= nco_out.im;
    outputs.valid  <= nco_out.valid;
    outputs.active <= '1' when current.is_active else '0';

    -- State machinery
    sync_proc : process(clock, reset)
    begin
        if (reset = '1') then
            current <= NULL_STATE;
        elsif (rising_edge(clock)) then
            current <= future;
        end if;
    end process sync_proc;

    comb_proc : process(all)
    begin
        future <= current;

        future.is_active    <= false;
        future.nco_in.valid <= '0';

        case (current.state) is
            when IDLE =>
                if (inputs.valid = '1') then
                    future.tone_in <= inputs;
                    future.state   <= START_TONE;
                end if;

            when START_TONE =>
                future.countdown     <= to_integer(current.tone_in.duration);
                future.tailcount     <= HOLDOVER_CLOCKS;
                future.nco_in.dphase <= current.tone_in.dphase;
                future.nco_in.valid  <= '1';
                future.is_active     <= true;
                future.state         <= WAIT_FOR_COUNTDOWN;

            when WAIT_FOR_COUNTDOWN =>
                future.nco_in.valid <= '1';
                future.is_active    <= true;

                if (current.countdown > 0) then
                    future.countdown <= current.countdown - 1;
                end if;

                if (current.countdown = 0) then
                    future.state <= TAIL;
                end if;

            when TAIL =>
                future.nco_in.dphase <= (others => '0');
                future.nco_in.valid  <= '1';
                future.is_active     <= true;

                if (current.tailcount > 0) then
                    future.tailcount <= current.tailcount - 1;
                end if;

                if (current.tailcount = 0) then
                    future.state <= IDLE;
                end if;

                if (inputs.valid = '1') then
                    future.tone_in <= inputs;
                    future.state   <= START_TONE;
                end if;

        end case;
    end process comb_proc;
end architecture arch;
