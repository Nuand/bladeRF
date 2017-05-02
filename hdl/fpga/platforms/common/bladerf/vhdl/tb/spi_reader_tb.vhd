-- Copyright (c) 2013 Nuand LLC
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
    use ieee.numeric_std.all ;

entity spi_reader_tb is
end entity ;

architecture arch of spi_reader_tb is

    signal clock    :   std_logic   := '1' ;
    signal sclk     :   std_logic ;
    signal miso     :   std_logic := '0' ;
    signal mosi     :   std_logic ;
    signal enx      :   std_logic ;
    signal reset_out : std_logic ;

begin

    clock <= not clock after 1 ns ;

    U_spi_reader : entity work.spi_reader
      port map (
        clock   =>  clock,

        sclk    =>  sclk,
        miso    =>  miso,
        mosi    =>  mosi,
        enx     =>  enx,
        reset_out => reset_out
      ) ;

end architecture ;
