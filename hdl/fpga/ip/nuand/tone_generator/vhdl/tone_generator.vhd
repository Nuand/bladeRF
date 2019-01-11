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

library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;

package tone_generator_p is
    type tone_generator_input_t is record
        dphase      : natural;
        duration    : natural;
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
        rv.dphase   := 0;
        rv.duration := 0;
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
    signal countdown    : natural;
    signal nco_in       : nco_input_t;
    signal nco_out      : nco_output_t;
    signal tail         : natural;
begin
    U_nco : entity work.nco
      port map (
        clock   => clock,
        reset   => reset,
        inputs  => nco_in,
        outputs => nco_out
      );

    handle_input : process(clock, reset) is
        variable dphase     : integer;
    begin
        if (reset = '1') then
            dphase          := 0;
            countdown       <= 0;
            tail            <= 0;

            nco_in.dphase   <= to_signed(dphase, nco_in.dphase'length);
            nco_in.valid    <= '0';

            outputs.re      <= (others => '0');
            outputs.im      <= (others => '0');
            outputs.valid   <= '0';
            outputs.active  <= '0';

        elsif (rising_edge(clock)) then
            nco_in.valid    <= '0';
            outputs.active  <= '0';

            if (inputs.valid = '1') then
                countdown   <= inputs.duration;
                dphase      := inputs.dphase;
            end if;

            if (countdown > 0) then
                countdown       <= countdown - 1;
                nco_in.dphase   <= to_signed(dphase, nco_in.dphase'length);
                nco_in.valid    <= '1';
                outputs.active  <= '1';
                tail        <= HOLDOVER_CLOCKS;
            elsif (tail > 0) then
                -- Keep nco_in.valid high for a bit after countdown is done,
                -- to avoid a gap in outputs.valid between sequential tones.
                tail            <= tail - 1;
                nco_in.dphase   <= (others => '0');
                nco_in.valid    <= '1';
            end if;

            outputs.re      <= nco_out.re;
            outputs.im      <= nco_out.im;
            outputs.valid   <= nco_out.valid;
        end if;
    end process handle_input;

end architecture arch;
