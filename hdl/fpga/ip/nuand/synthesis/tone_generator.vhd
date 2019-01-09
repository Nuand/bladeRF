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
        period      : natural;
        duration    : natural;
        valid       : std_logic;
    end record;

    type tone_generator_output_t is record
        re      : signed(15 downto 0);
        im      : signed(15 downto 0);
        valid   : std_logic;
        idle    : std_logic;
    end record;
end package; -- tone_generator_p

library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;
    use ieee.math_real.all;

library work;
    use work.tone_generator_p.all;
    use work.nco_p.all;

entity tone_generator is
  port (
    clock   : in  std_logic;
    reset   : in  std_logic;
    inputs  : in  tone_generator_input_t;
    outputs : out tone_generator_output_t
  );
end entity; -- tone_generator

architecture arch of tone_generator is
    signal countdown    : natural;
    signal nco_input    : nco_input_t;
    signal nco_output   : nco_output_t;
begin
    U_nco : entity work.nco
      port map (
        clock   => clock,
        reset   => reset,
        inputs  => nco_input,
        outputs => nco_output
      );

    handle_input : process(clock, reset) is
        variable dphase : integer;
    begin
        if (reset = '1') then
            dphase := 0;

            nco_input.dphase <= to_signed(dphase, nco_input.dphase'length);
            nco_input.valid  <= '0';

            outputs.re    <= (others => '0');
            outputs.im    <= (others => '0');
            outputs.valid <= '0';
            outputs.idle  <= '0';

        elsif (rising_edge(clock)) then
            nco_input.valid <= '0';
            outputs.idle    <= '1';

            if (inputs.valid = '1') then
                countdown <= inputs.duration;

                if (inputs.period > 0) then
                    -- -4096 to 4096 is one rotation, and it should take
                    -- <period> clocks to do this
                    dphase := integer(round(8192.0 / real(inputs.period)));
                else
                    dphase := 0;
                end if;
            end if;

            -- TODO: gently glide down to zero-crossing?
            if (countdown > 0) then
                nco_input.dphase <= to_signed(dphase, nco_input.dphase'length);
                nco_input.valid  <= '1';
                countdown        <= countdown - 1;
                outputs.idle     <= '0';
            end if;

            outputs.re    <= nco_output.re;
            outputs.im    <= nco_output.im;
            outputs.valid <= nco_output.valid;
        end if;
    end process handle_input;

end architecture; -- arch
