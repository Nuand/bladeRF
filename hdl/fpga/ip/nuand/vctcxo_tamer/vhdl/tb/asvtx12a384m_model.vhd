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
        clock : out std_logic  -- output clock
    );
end entity;

architecture arch of asvtx12a384m_model is

    -- Control Voltage Tolerances
    constant VCON_MIN          : real    := 0.4;
    constant VCON_MAX          : real    := 2.4;

    -- Nominal output frequency
    constant FREQ_NOMINAL       : real    := 38.4e6;

    -- Frequency stability tolerance
    constant PPM_STABILITY_MIN : real    := -2.0;
    constant PPM_STABILITY_MAX : real    := 2.0;

    -- Frequency Tuning Tolerances
    constant PPM_VCON_MIN_MIN  : real    := -5.0;
    constant PPM_VCON_MIN_MAX  : real    := -9.5;
    constant PPM_VCON_MAX_MIN  : real    := 5.0;
    constant PPM_VCON_MAX_MAX  : real    := 9.5;

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
        return (rand*(ppm_max-ppm_min) + ppm_min);
    end function;

    -- Given a Vcon voltage, calculate the minimum possible PPM error
    function vcon_ppm_min_calc( vcon : real ) return real is
        variable slope     : real := 0.0;
        variable intercept : real := 0.0;
        variable rv        : real := 0.0;
    begin
        slope     := (PPM_VCON_MAX_MIN-PPM_VCON_MIN_MIN)/(VCON_MAX-VCON_MIN);
        intercept := PPM_VCON_MAX_MIN-(slope*VCON_MAX);
        if( (vcon >= VCON_MIN) and (vcon <= VCON_MAX) ) then
            rv := ( (slope * vcon) + intercept );
        else
            report "vcon_ppm_min_calc: vcon out of recommended range. Returning 0."
                severity warning;
            rv := 0.0;
        end if;

        return rv;
    end function;

    -- Given a Vcon voltage, calculate the maximum possible PPM error
    function vcon_ppm_max_calc( vcon : real ) return real is
        variable slope     : real := 0.0;
        variable intercept : real := 0.0;
        variable rv        : real := 0.0;
    begin
        slope     := (PPM_VCON_MAX_MAX-PPM_VCON_MIN_MAX)/(VCON_MAX-VCON_MIN);
        intercept := PPM_VCON_MAX_MAX-(slope*VCON_MAX);
        if( (vcon >= VCON_MIN) and (vcon <= VCON_MAX) ) then
            rv := ( (slope * vcon) + intercept );
        else
            report "vcon_ppm_max_calc: vcon out of recommended range. Returning 0."
                severity warning;
            rv := 0.0;
        end if;

        return rv;
    end function;

    -- Given a Vcon PPM and a frequency stability PPM, compute the freuqency
    function freq_calc( vcon_ppm : real; tol_ppm : real ) return real is
        variable rv : real;
    begin
        -- Frequency with Vcon PPM error applied
        rv := ( FREQ_NOMINAL * ( 1.0 + (vcon_ppm/1.0e6) ) );
        -- Apply frequency stability tolerance
        rv := ( rv * (1.0 + (tol_ppm/1.0e6)) );
        return rv;
    end function;

    signal   freq     : real;
    signal   half_per : time := 13.021 ns;
    signal   clock_i  : std_logic := '1';

begin

    -- i don't think the frequency is changing at all
    -- even though vcon is changing

    comb_proc : process( all )
        variable tuning_ppm    : real := 0.0;
        variable stability_ppm : real := 0.0;
        variable total_ppm     : real := 0.0;
    begin
        stability_ppm := random_ppm(PPM_STABILITY_MIN, PPM_STABILITY_MAX);
        report "Setting stability error to: " & real'image(stability_ppm) & " PPM.";
        tuning_ppm    := random_ppm(vcon_ppm_min_calc(vcon), vcon_ppm_max_calc(vcon));
        report "Setting tuning error to: " & real'image(tuning_ppm) & " PPM.";

        freq     <= freq_calc(tuning_ppm, stability_ppm);
        half_per <= half_clk_per(freq);
        report "freq_error=" & real'image(freq-FREQ_NOMINAL);
        report "freq=" & real'image(freq);
    end process;

    clock_i <= not clock_i after half_per;
    clock   <= clock_i;

end architecture;
