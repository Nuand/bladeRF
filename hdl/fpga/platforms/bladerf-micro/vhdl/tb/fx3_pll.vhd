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

library ieee ;
    use ieee.std_logic_1164.all ;

entity fx3_pll is
    port (
        refclk   : in  std_logic;
        rst      : in  std_logic;
        outclk_0 : out std_logic;
        locked   : out std_logic
    );
end entity ; -- fx3_pll

architecture simulation of fx3_pll is
    constant PHASE_OFFSET   : time  := 1 sec * (1.0/100.0e6) - 500 ps;
    signal outclk           : std_logic   := '1';
begin
    -- Generate 80 MHz clock
    outclk <= refclk after PHASE_OFFSET;

    -- Send it out...
    simulate_pll : process(rst, outclk) is
    begin
        if (rst = '1') then
            outclk_0    <= '0';
            locked      <= '0';
        else
            outclk_0    <= outclk;
            locked      <= '1';
        end if;
    end process ;
end architecture ; -- simulation
