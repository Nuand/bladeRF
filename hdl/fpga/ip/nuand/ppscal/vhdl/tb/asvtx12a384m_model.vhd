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
    use ieee.math_real.all;

library std;
    use std.env.all;

entity asvtx12a384m_model is
    port(
        vcon  : in  real;      -- control voltage
        clock : out std_logic; -- output clock
    );
end entity;

architecture arch of asvtx12a384m_model is

    constant FREQ_NOMINAL       : real    := 38.4e6;
    constant FREQ_STABILITY_MIN : real    := (-2.0 / (1e6));
    constant FREQ_STABILITY_MAX : real    := (+2.0 / (1e6));

    -- Control Voltage Tolerances
    constant VCON_MIN          : real    := 0.4;
    constant VCON_MAX          : real    := 2.4;

    -- Frequency Tuning Tolerances
    constant PPM_VCON_MIN_MIN  : real    := (-5.0 / (1e6));
    constant PPM_VCON_MIN_MAX  : real    := (-9.5 / (1e6));
    constant PPM_VCON_MAX_MIN  : real    := (5.0 / (1e6));
    constant PPM_VCON_MAX_MAX  : real    := (9.5 / (1e6));

    -- Given a frequency, compute the half period time
    function half_clk_per( freq : real ) return time is
    begin
        return ( (0.5 sec) / real(freq) );
    end function;

    -- Compute a random PPM in the given range
    function random_ppm( ppm_min : real; ppm_max : real ) return real is
        variable seed1 : positive;
        variable seed2 : positive;
        variable rand  : real;
    begin
        uniform(seed1, seed2, rand);
        return (rand*ppm_max + ppm_min);
    end function;

    -- Given a Vcon voltage, calculate the minimum possible PPM error
    function vcon_ppm_min_calc( vcon : real ) return real is
    begin
        return ( (5.0 * vcon) - 7.0 );
    end function;

    -- Given a Vcon voltage, calculate the maximum possible PPM error
    function vcon_ppm_max_calc( vcon : real ) return real is
    begin
        return ( (9.5 * vcon) - 13.3 );
    end function;

    -- Given a Vcon PPM and a frequency stability PPM, compute the freuqency
    function freq_calc( vcon_ppm : real; tol_ppm : real ) return real is
        variable rv : real;
    begin
        -- Frequency with Vcon PPM error applied
        rv := ( (FREQ_NOMINAL) + (vcon_ppm/1.0e6)*FREQ_NOMINAL);
        -- Apply frequency stability tolerance
        rv := ( (rv) + (tol_ppm/1.0e6)*rv);
        return rv;
    end function;

    -- Current outputs
    signal   freq     : real;
    signal   half_per : time;



begin




    tcxo_clock_i <= not tcxo_clock_i after current.tcxo_half_per;
    tcxo_clock   <= tcxo_clock_i;

    sync_proc : process( mm_clock, mm_reset )
    begin
        if( mm_reset = '1' ) then
            current <= RESET_VALUE;
        elsif( rising_edge(mm_clock) ) then
            current <= future;
        end if;
    end process;

    comb_proc : process( all )
    begin
        mm_rd_req   <= '0';
        mm_wr_req   <= '0';
        mm_addr     <= (others => '0');
        mm_wr_data  <= (others => '0');

        case (current.state) is
            when RESET_COUNTERS =>
                mm_wr_req  <= '1';
                mm_wr_data <= x"07";
                mm_addr    <= std_logic_vector(to_unsigned(CONTROL_ADDR,mm_addr'length));
                future.holdoff_count <= 0;
                future.state <= START_COUNTERS;

            when START_COUNTERS =>
                future.holdoff_count <= current.holdoff_count + 1;
                if( current.holdoff_count = RESET_TIME ) then
                    future.holdoff_count <= 0;
                    mm_wr_req  <= '1';
                    mm_wr_data <= x"00";
                    mm_addr    <= std_logic_vector(to_unsigned(CONTROL_ADDR,mm_addr'length));
                    future.state <= HOLDOFF;
                end if;

            when ENABLE_IRQS =>
                mm_wr_req <= '1';
                mm_wr_data <= x"01";
                mm_addr <= std_logic_vector(to_unsigned(INTERRUPT_ADDR,mm_addr'length));
                future.state <= WAIT_FOR_IRQ;

            when WAIT_FOR_IRQ =>
                if( mm_irq = '1' ) then
                    future.holdoff_count <= 0;
                    mm_wr_req <= '1';
                    mm_wr_data <= x"10"; -- clear and disable interrupts
                    mm_addr <= std_logic_vector(to_unsigned(INTERRUPT_ADDR,mm_addr'length));
                    --future.state <= READ_PPS_1S;
                    future.state <= READ_COUNTS;
                else
                    future.holdoff_count <= current.holdoff_count + 1;
                    if( current.holdoff_count = IRQ_TIMEOUT ) then
                        future.holdoff_count <= 0;
                        future.state <= TERMINATE_SIMULATION;
                    end if;
                end if;

            when READ_COUNTS =>
                future.holdoff_count <= current.holdoff_count + 1;
                mm_rd_req <= '1';
                if( current.holdoff_count < 8 ) then
                    mm_addr <= std_logic_vector(to_unsigned(PPS_CNT_1S_ADDR+(current.holdoff_count mod 8),mm_addr'length));
                    future.pps_1s_count <= mm_rd_data &
                                           current.pps_1s_count(current.pps_1s_count'left downto
                                                                current.pps_1s_count'right+mm_rd_data'length);
                elsif( current.holdoff_count < 16 ) then
                    -- Need to shift in the last byte of data from previous count
                    if( current.holdoff_count = 8 ) then
                        future.pps_1s_count <= mm_rd_data &
                                               current.pps_1s_count(current.pps_1s_count'left downto
                                                                    current.pps_1s_count'right+mm_rd_data'length);
                    end if;
                    -- Also start capturing the next counter value
                    mm_addr <= std_logic_vector(to_unsigned(PPS_CNT_10S_ADDR+(current.holdoff_count mod 8),mm_addr'length));
                    future.pps_10s_count <= mm_rd_data &
                                            current.pps_10s_count(current.pps_10s_count'left downto
                                                                  current.pps_10s_count'right+mm_rd_data'length);
                elsif( current.holdoff_count < 24 ) then
                    -- Need to shift in the last byte of data from previous count
                    if( current.holdoff_count = 16 ) then
                        future.pps_10s_count <= mm_rd_data &
                                                current.pps_10s_count(current.pps_10s_count'left downto
                                                                      current.pps_10s_count'right+mm_rd_data'length);
                    end if;
                    -- Also start capturing the next counter value
                    mm_addr <= std_logic_vector(to_unsigned(PPS_CNT_100S_ADDR+(current.holdoff_count mod 8),mm_addr'length));
                    future.pps_100s_count <= mm_rd_data &
                                             current.pps_100s_count(current.pps_100s_count'left downto
                                                                    current.pps_100s_count'right+mm_rd_data'length);
                else
                    -- Need to shift in the last byte of data from previous count
                    if( current.holdoff_count = 24 ) then
                        future.pps_100s_count <= mm_rd_data &
                                                 current.pps_100s_count(current.pps_100s_count'left downto
                                                                        current.pps_100s_count'right+mm_rd_data'length);
                    end if;
                    mm_rd_req <= '0';
                    future.holdoff_count <= 0;
                    future.state <= FREQ_ADJUST;
                end if;

            when FREQ_ADJUST =>
                future.tcxo_half_per <= half_clk_per(current.tcxo_freq+TCXO_INCR_FREQ);
                future.tcxo_freq     <= current.tcxo_freq + TCXO_INCR_FREQ;
                future.state <= HOLDOFF;

            when HOLDOFF =>
                future.holdoff_count <= current.holdoff_count + 1;
                if( current.holdoff_count = DEAD_TIME ) then
                    future.holdoff_count <= 0;
                    future.state <= ENABLE_IRQS;
                end if;

            when TERMINATE_SIMULATION =>
                --stop(0);
                null;

        end case;
    end process;

end architecture;
