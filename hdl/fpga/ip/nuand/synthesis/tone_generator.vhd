-- Copyright (c) 2018 Nuand LLC
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

library work;
    use work.nco_p.all;

entity tone_generator is
  port (
    clock           :   in  std_logic;
    reset           :   in  std_logic;

    -- period of output wave, in clocks
    period          :   in  unsigned(15 downto 0);
    -- duration of output wave, in clocks
    duration        :   in  unsigned(31 downto 0);
    -- hold high until in_ack is asserted, then deassert
    in_valid        :   in  std_logic;

    -- deassert in_valid after in_ack
    in_ack          :   out std_logic;

    out_real        :   out signed(15 downto 0);
    out_imag        :   out signed(15 downto 0);
    out_valid       :   out std_logic
  );
end entity; -- tone_generator

architecture arch of tone_generator is

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

    out_real <= nco_output.re;
    out_imag <= nco_output.im;
    out_valid <= nco_output.valid;

end architecture; -- arch
