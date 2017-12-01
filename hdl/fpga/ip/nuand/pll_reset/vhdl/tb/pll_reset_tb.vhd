-- PLL Reset Controller Simulation Testbench
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

library nuand;

entity pll_reset_tb is
end entity; -- pll_reset_tb

architecture tb of pll_reset_tb is

    signal done             : boolean   := false;

    signal clock            : std_logic := '1';
    signal pll_locked       : std_logic := '0';

    signal pll_locked_out   : std_logic;
    signal pll_reset        : std_logic;

    constant DEVICE_FAMILY  : string := "Cyclone V";
    constant CLOCK_HZ       : natural := 10_000_000;

    constant EXPECTED_RESET_TIME    : time := 1.1 ms;
    constant ALLOWED_MARGIN         : time := 0.1 ms;

    type stage_t is (
        TEST_START,
        TEST_PAUSE,
        WAIT_FOR_RESET_0,
        WAIT_FOR_RESET_1,
        WAIT_FOR_PLL_LOCKED_OUT_0,
        WAIT_FOR_PLL_LOCKED_OUT_1,
        TEST_END
    );

    signal stage : stage_t := TEST_START;

    -- freq_to_period: convert from frequency in Hz to period in seconds
    function freq_to_period(freq : natural) return time is
    begin
        return 1 sec / freq;
    end function freq_to_period;

begin

    clock <= not clock after freq_to_period(CLOCK_HZ)/2 when not done else '0';

    U_pll_reset : entity nuand.pll_reset
    generic map (
        SYS_CLOCK_FREQ_HZ   => CLOCK_HZ,
        DEVICE_FAMILY       => DEVICE_FAMILY
    )
    port map (
        sys_clock           => clock,

        pll_locked          => pll_locked,
        pll_locked_out      => pll_locked_out,
        pll_reset           => pll_reset
    );

    tb : process
        variable start_time : time;
    begin
        -- Make sure we're in reset
        report "TEST: START";
        stage       <= TEST_START;
        pll_locked  <= '0';
        start_time  := now;

        wait for 1 us;

        assert (pll_reset = '1')
            report "pll_reset not asserted"
            severity failure;

        -- PLL Reset state machine is holding the PLL in reset
        -- Wait for reset to drop
        stage <= WAIT_FOR_RESET_0;

        wait until pll_reset = '0';

        assert (now - start_time) > (EXPECTED_RESET_TIME - ALLOWED_MARGIN)
            report "FAIL: pll_reset = 0 early " & to_string(now - start_time)
            severity failure;
        assert (now - start_time) < (EXPECTED_RESET_TIME + ALLOWED_MARGIN)
            report "FAIL: pll_reset = 0 late " & to_string(now - start_time)
            severity failure;

        report "GOOD: pll_reset = 0 after " & to_string(now - start_time);

        -- Simulate a reasonable length of time to lock
        wait for 100 us;

        -- Lock the PLL
        report "TEST: simulating PLL lock";
        stage       <= WAIT_FOR_PLL_LOCKED_OUT_1;
        pll_locked  <= '1';
        start_time  := now;

        wait until pll_locked_out = '1';

        report "INFO: pll_locked_out = 1 after " & to_string(now - start_time);

        -- maintain lock for awhile
        report "TEST: pausing with PLL locked";
        stage <= TEST_PAUSE;

        while (now - start_time < 5 ms) loop
            assert (pll_reset = '0')
                report "FAIL: pll_reset unexpectedly asserted"
                severity failure;
            wait for 1 us;
        end loop;

        -- simulate lock failure
        report "TEST: simulating lock failure";
        stage       <= WAIT_FOR_RESET_1;
        pll_locked  <= '0';
        start_time  := now;

        wait until pll_reset = '1';

        report "INFO: pll_reset = 1 after " & to_string(now - start_time);

        -- Wait for reset to drop
        stage <= WAIT_FOR_RESET_0;

        wait until pll_reset = '0';

        assert (now - start_time) > (EXPECTED_RESET_TIME - ALLOWED_MARGIN)
            report "FAIL: pll_reset = 0 early " & to_string(now - start_time)
            severity failure;
        assert (now - start_time) < (EXPECTED_RESET_TIME + ALLOWED_MARGIN)
            report "FAIL: pll_reset = 0 late " & to_string(now - start_time)
            severity failure;

        report "GOOD: pll_reset = 0 after " & to_string(now - start_time);

        -- Simulate a reasonable length of time to lock
        wait for 100 us;

        -- Lock the PLL
        report "TEST: simulating PLL lock";
        stage       <= WAIT_FOR_PLL_LOCKED_OUT_1;
        pll_locked  <= '1';
        start_time  := now;

        wait until pll_locked_out = '1';

        report "INFO: pll_locked_out = 1 after " & to_string(now - start_time);

        -- maintain lock for awhile
        report "TEST: pausing with PLL locked";
        stage <= TEST_PAUSE;

        while (now - start_time < 5 ms) loop
            assert (pll_reset = '0')
                report "FAIL: pll_reset unexpectedly asserted"
                severity failure;
            wait for 1 us;
        end loop;

        -- simulate lock failure
        report "TEST: simulating lock failure";
        stage       <= WAIT_FOR_RESET_1;
        pll_locked  <= '0';
        start_time  := now;

        wait until pll_reset = '1';

        report "INFO: pll_reset = 1 after " & to_string(now - start_time);

        -- Wait for reset to drop
        stage <= WAIT_FOR_RESET_0;

        wait until pll_reset = '0';

        assert (now - start_time) > (EXPECTED_RESET_TIME - ALLOWED_MARGIN)
            report "FAIL: pll_reset = 0 early " & to_string(now - start_time)
            severity failure;
        assert (now - start_time) < (EXPECTED_RESET_TIME + ALLOWED_MARGIN)
            report "FAIL: pll_reset = 0 late " & to_string(now - start_time)
            severity failure;

        report "GOOD: pll_reset = 0 after " & to_string(now - start_time);

        -- simulate an unreasonable length of time to lock
        report "TEST: simulating failure to lock PLL";
        stage       <= WAIT_FOR_RESET_1;
        start_time  := now;

        wait until pll_reset = '1';

        assert (now - start_time) > (EXPECTED_RESET_TIME - ALLOWED_MARGIN)
            report "FAIL: pll_reset = 1 early " & to_string(now - start_time)
            severity failure;
        assert (now - start_time) < (EXPECTED_RESET_TIME + ALLOWED_MARGIN)
            report "FAIL: pll_reset = 1 late " & to_string(now - start_time)
            severity failure;

        report "GOOD: pll_reset = 1 after " & to_string(now - start_time);

        start_time  := now;
        stage       <= WAIT_FOR_RESET_0;

        wait until pll_reset = '0';

        assert (now - start_time) > (EXPECTED_RESET_TIME - ALLOWED_MARGIN)
            report "FAIL: pll_reset = 0 early " & to_string(now - start_time)
            severity failure;
        assert (now - start_time) < (EXPECTED_RESET_TIME + ALLOWED_MARGIN)
            report "FAIL: pll_reset = 0 late " & to_string(now - start_time)
            severity failure;

        report "GOOD: pll_reset = 0 after " & to_string(now - start_time);

        -- re-lock and finish
        wait for 100 us;
        report "TEST: relocking to clean up";
        pll_locked  <= '1';
        start_time  := now;
        stage       <= WAIT_FOR_PLL_LOCKED_OUT_1;

        wait until pll_locked_out = '1';

        report "INFO: pll_locked_out = 1 after " & to_string(now - start_time);

        -- done
        stage <= TEST_END;
        wait for 100 us;

        done <= true;
        report "TEST: completed";
        wait;

    end process;

end architecture tb;
