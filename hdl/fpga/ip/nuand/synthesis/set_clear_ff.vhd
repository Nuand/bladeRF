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

-- set_clear_ff: provides a simple clocked set/clear flipflop

library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;

entity set_clear_ff is
  port (
    clock       : in    std_logic;
    reset       : in    std_logic;
    set         : in    std_logic;
    clear       : in    std_logic;
    q           : out   std_logic
  );
end entity; -- set_clear_ff

architecture arch of set_clear_ff is
    signal q_r      :   std_logic;
    signal set_r    :   std_logic;
    signal clr_r    :   std_logic;
begin
    process (reset, clock)
    begin
        if (reset = '1') then
            q_r     <= '0';
            set_r   <= '0';
            clr_r   <= '0';
        elsif (rising_edge(clock)) then
            set_r   <= set;
            clr_r   <= clear;

            if (set_r = '0' and set = '1') then
                q_r <= '1';
            elsif(clr_r = '0' and clear = '1') then
                q_r <= '0';
            end if;
        end if;
    end process;

    q <= q_r;

end architecture; -- arch
