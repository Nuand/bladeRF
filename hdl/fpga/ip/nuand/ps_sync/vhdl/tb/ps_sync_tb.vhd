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
    use ieee.math_real.all;

entity ps_sync_tb is
end entity;

architecture arch of ps_sync_tb is

    signal clock : std_logic := '1';

begin

    clock <= not clock after 13.021 ns;

    u_uut0 : entity work.ps_sync
        generic map (
            OUTPUTS  => 1,
            USE_LFSR => true,
            HOP_LIST => (4, 5, 6, 7, 8),
            HOP_RATE => 1
        )
        port map (
            refclk   => clock,
            sync     => open
        );

    u_uut1 : entity work.ps_sync
        generic map (
            OUTPUTS  => 2,
            USE_LFSR => false,
            HOP_LIST => (4, 5, 6, 7, 8),
            HOP_RATE => 3
        )
        port map (
            refclk   => clock,
            sync     => open
        );

    u_uut2 : entity work.ps_sync
        generic map (
            OUTPUTS  => 1,
            USE_LFSR => false,
            HOP_LIST => (0 => 4),
            HOP_RATE => 2
        )
        port map (
            refclk   => clock,
            sync     => open
        );

end architecture;
