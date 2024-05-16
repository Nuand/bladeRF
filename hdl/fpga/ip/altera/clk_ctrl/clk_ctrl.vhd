-- Copyright (c) 2024 Nuand LLC
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
    use ieee.math_real.all;

entity clk_ctrl is
    port (
        inclk       :in  std_logic;
        ena         :in  std_logic;
        outclk      :out std_logic
    );
end entity clk_ctrl;

architecture rtl of clk_ctrl is

    component altclkctrl
    generic (
        clock_type          : string := "Auto";
        ena_register_mode   : string := "none";
        number_of_clocks    : integer := 1
    );
    port (
        inclk   : in  std_logic_vector(0 downto 0);
        ena     : in  std_logic;
        outclk  : out std_logic
    );
    end component;

    signal clk_ctrl_out : std_logic;

begin

    clkctrl : altclkctrl
    port map (
        inclk   => (0 => inclk),
        ena     => ena,
        outclk  => clk_ctrl_out
    );

    outclk <= clk_ctrl_out;

end architecture rtl;
