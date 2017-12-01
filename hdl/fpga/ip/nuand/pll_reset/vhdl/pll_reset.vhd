-- From Altera Solution ID rd05312011_49, need the following for Qsys to use VHDL 2008:
-- altera vhdl_input_version vhdl_2008

-- PLL Reset Controller
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

entity pll_reset is
  generic (
    SYS_CLOCK_FREQ_HZ   : natural   := 10_000_000;
    DEVICE_FAMILY       : string    := "Cyclone V"
  );
  port (
    sys_clock           : in    std_logic;
    pll_locked          : in    std_logic;
    pll_locked_out      : out   std_logic;
    pll_reset           : out   std_logic
  );
end entity; -- pll_reset

architecture fsm of pll_reset is
    type fsm_t is (
        INIT,
        ASSERT_RESET,
        WAIT_FOR_LOCK,
        WAIT_FOR_UNLOCK
    );

    type state_t is record
        state       : fsm_t;
        downcount   : natural;
        pll_lock    : std_logic;
        pll_reset   : std_logic;
    end record;

    constant reset_value : state_t := (
        state       => INIT,
        downcount   => 0,
        pll_lock    => '0',
        pll_reset   => '0'
    );

    signal current  : state_t := reset_value;
    signal future   : state_t := reset_value;

    function reset_duration(device : string) return time is
        variable rv : time;
    begin
        if (device = "Cyclone IV") then
            rv := 1.1 ms;
        elsif (device = "Cyclone V") then
            rv := 1.1 ms;
        else
            report "unknown device family: " & device severity failure;
        end if;

        return rv;
    end function reset_duration;

    function time_to_ticks(duration : time; freq : natural) return natural is
        variable period : time;
        variable rv     : natural;
    begin
        period := 1 sec / freq;
        rv := duration / period;
        return rv;
    end function time_to_ticks;

begin

    sync_proc : process(sys_clock)
    begin
        if(rising_edge(sys_clock)) then
            current <= future;
        end if;
    end process sync_proc;

    comb_proc : process(all)
    begin
        -- outputs
        pll_locked_out      <= current.pll_lock;
        pll_reset           <= current.pll_reset;

        -- defaults
        future              <= current;
        future.pll_lock     <= pll_locked;
        future.pll_reset    <= '0';

        -- states
        case current.state is
            when INIT =>
                -- prepare countdown
                future.downcount    <= time_to_ticks(reset_duration(DEVICE_FAMILY), SYS_CLOCK_FREQ_HZ);
                future.state        <= ASSERT_RESET;

            when ASSERT_RESET =>
                -- keep reset asserted until countdown is done
                future.pll_reset    <= '1';

                if (current.downcount = 0) then
                    future.downcount    <= time_to_ticks(reset_duration(DEVICE_FAMILY), SYS_CLOCK_FREQ_HZ);
                    future.state        <= WAIT_FOR_LOCK;
                else
                    future.downcount    <= current.downcount - 1;
                end if;

            when WAIT_FOR_LOCK =>
                -- de-assert reset and hope it locks

                if (current.pll_lock = '1') then
                    -- we're good
                    future.state        <= WAIT_FOR_UNLOCK;
                end if;

                if (current.downcount = 0) then
                    -- it didn't lock, let's try again
                    future.downcount    <= time_to_ticks(reset_duration(DEVICE_FAMILY), SYS_CLOCK_FREQ_HZ);
                    future.state        <= INIT;
                else
                    future.downcount    <= current.downcount - 1;
                end if;

            when WAIT_FOR_UNLOCK =>
                -- monitor for PLL unlock

                if (current.pll_lock = '0') then
                    future.state    <= INIT;
                end if;

        end case;

    end process comb_proc;

end architecture fsm;
