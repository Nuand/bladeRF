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
    port (
        clock       : in  std_logic;
        reset       : in  std_logic;
        inputs      : in  tone_generator_input_t;
        outputs     : out tone_generator_output_t
    );
end entity tone_generator;

architecture arch of tone_generator is
    constant HOLDCOUNT  : natural := 32;

    type fsm_t is (IDLE, WAIT_FOR_ACK, START_TONE, ACTIVE_TONE, FINISH_TONE,
                   WAIT_FOR_ZERO, HOLD);

    type state_t is record
        state           : fsm_t;
        is_active       : boolean;
        nco_reset       : std_logic;
        downcount       : natural;
        holdcount       : natural;
        tone_latch      : tone_generator_input_t;
        nco_in          : nco_input_t;
        last_i          : signed(15 downto 0);
        last_q          : signed(15 downto 0);
        zero_hold_i     : boolean;
        zero_hold_q     : boolean;
        zero_xing_i     : boolean;
        zero_xing_q     : boolean;
        tone_pending    : boolean;
    end record state_t;

    function NULL_STATE return state_t is
        variable rv : state_t;
    begin
        rv.state        := IDLE;
        rv.is_active    := false;
        rv.nco_reset    := '1';
        rv.downcount    := 0;
        rv.holdcount    := 0;
        rv.tone_latch   := NULL_TONE_GENERATOR_INPUT;
        rv.nco_in       := (dphase => (others => '0'), valid => '0');
        rv.last_i       := (others => '0');
        rv.last_q       := (others => '0');
        rv.zero_hold_i  := true;
        rv.zero_hold_q  := true;
        rv.zero_xing_i  := false;
        rv.zero_xing_q  := false;
        rv.tone_pending := false;
        return rv;
    end function NULL_STATE;

    function is_zero_cross(a : signed ; b : signed) return boolean is
    begin
        return (a >= 0 and b <= 0) or (a <= 0 and b >= 0);
    end function is_zero_cross;

    signal current  : state_t;
    signal future   : state_t;

    signal nco_out  : nco_output_t;
begin
    U_nco : entity work.nco
      port map (
        clock   => clock,
        reset   => current.nco_reset,
        inputs  => current.nco_in,
        outputs => nco_out
      );

    outputs.re     <= (others => '0') when current.zero_hold_i else nco_out.re;
    outputs.im     <= (others => '0') when current.zero_hold_q else nco_out.im;
    outputs.valid  <= nco_out.valid when (current.nco_reset = '0') else '0';
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

        -- Keep last sample, so we can determine if we find a zero-crossing
        if (nco_out.valid = '1') then
            future.last_i <= nco_out.re;
            future.last_q <= nco_out.im;

            future.zero_xing_i <= is_zero_cross(current.last_i, nco_out.re);
            future.zero_xing_q <= is_zero_cross(current.last_q, nco_out.im);
        end if;

        -- Latch valid inputs...
        if (inputs.valid = '1') then
            future.tone_latch   <= inputs;
            future.tone_pending <= true;
        end if;

        -- Defaults: active if tone pending, nco_in valid, nco not reset
        future.is_active    <= current.tone_pending;
        future.nco_in.valid <= '1';
        future.nco_reset    <= '0';

        case (current.state) is
            when IDLE =>
                -- Zero all outputs, reset the NCO while idle
                future.zero_hold_i  <= true;
                future.zero_hold_q  <= true;
                future.nco_reset    <= '1';
                future.nco_in.valid <= '0';

                -- We have a tone to do, folks
                if (current.tone_pending) then
                    future.nco_reset <= '0';
                    future.state     <= WAIT_FOR_ACK;
                end if;

            when WAIT_FOR_ACK =>    -- Wait until valid is deasserted
                if (inputs.valid = '0') then
                    future.state <= START_TONE;
                end if;

            when START_TONE =>  -- Set up NCO, counters, etc
                -- We are active
                future.is_active <= true;

                -- Set up our downcounts
                future.downcount <= to_integer(current.tone_latch.duration);
                future.holdcount <= HOLDCOUNT;

                -- Configure the NCO
                future.nco_in.dphase <= current.tone_latch.dphase;

                -- Update state
                future.tone_pending <= false;
                future.state        <= ACTIVE_TONE;

            when ACTIVE_TONE => -- Tone is coming out
                -- We are active
                future.is_active <= true;

                -- Wait for a zero-crossing to un-zero NCO output
                if (current.nco_in.dphase /= 0) then
                    if (current.zero_hold_i and current.zero_xing_i) then
                        future.zero_hold_i <= false;
                    end if;

                    if (current.zero_hold_q and current.zero_xing_q) then
                        future.zero_hold_q <= false;
                    end if;
                else
                    future.zero_hold_i <= true;
                    future.zero_hold_q <= true;
                end if;

                -- Countdown
                if (current.downcount > 0) then
                    future.downcount <= current.downcount - 1;
                else
                    future.state <= FINISH_TONE;
                end if;

            when FINISH_TONE => -- Determine next action
                -- Decrement hold count
                if (current.holdcount > 0) then
                    future.holdcount <= current.holdcount - 1;
                end if;

                -- We have a few situations to handle:
                --
                --   current dphase   next dphase     next state
                -- 1.      0               0          START_TONE
                -- 2.      0             != 0         START_TONE
                -- 3.      0             none         IDLE
                -- 4.    not 0             0          Zero-xing -> START_TONE
                -- 5.    not 0           != 0         START_TONE
                -- 6.    not 0           none         Zero-xing -> IDLE

                if (current.nco_in.dphase /= 0) then
                    -- Cases 4, 5, and 6: go to another state
                    future.state <= WAIT_FOR_ZERO;
                else
                    if (current.tone_pending) then
                        -- Cases 1 and 2
                        future.state <= WAIT_FOR_ACK;
                    else
                        -- Case 3
                        future.state <= HOLD;
                    end if;
                end if;

            when WAIT_FOR_ZERO =>   -- Wait for zero crossing
                -- Decrement hold count
                if (current.holdcount > 0) then
                    future.holdcount <= current.holdcount - 1;
                end if;

                -- Snatch the next zero-crossing
                if (not current.zero_hold_i and current.zero_xing_i) then
                    future.zero_hold_i <= true;
                end if;

                if (not current.zero_hold_q and current.zero_xing_q) then
                    future.zero_hold_q <= true;
                end if;

                -- We are going to handle these cases here:
                --
                --   current dphase   next dphase     next state
                -- 4.    not 0             0          Zero-xing -> START_TONE
                -- 5.    not 0           != 0         START_TONE
                -- 6.    not 0           none         Zero-xing -> IDLE

                if (current.tone_pending and current.tone_latch.dphase /= 0)
                then
                    -- Case 5
                    future.state <= WAIT_FOR_ACK;
                elsif (current.zero_hold_i and current.zero_hold_q) then
                    if (current.tone_pending) then
                        -- Case 4
                        future.state <= WAIT_FOR_ACK;
                    else
                        -- Case 6
                        future.state <= HOLD;
                    end if;
                end if;

            when HOLD =>    -- Keep the NCO spinning to avoid discontinuities
                -- Decrement hold count
                if (current.holdcount > 0) then
                    future.holdcount <= current.holdcount - 1;
                else
                    future.state <= IDLE;
                end if;

                -- We have a tone to do, folks
                if (current.tone_pending) then
                    future.state <= WAIT_FOR_ACK;
                end if;

        end case;
    end process comb_proc;
end architecture arch;
