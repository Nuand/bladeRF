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

entity async_fifo is
  port (
    -- Global clear
    clear       :   in  std_logic ;

    -- Write side
    w_clock     :   in  std_logic ;
    w_enable    :   in  std_logic ;
    w_data      :   in  std_logic_vector(7 downto 0) ;
    w_empty     :   out std_logic ;
    w_full      :   out std_logic ;

    -- Read side
    r_clock     :   in  std_logic ;
    r_enable    :   in  std_logic ;
    r_data      :   out std_logic_vector(7 downto 0) ;
    r_empty     :   out std_logic ;
    r_full      :   out std_logic
  ) ;
end entity ; -- async_fifo

architecture arch of async_fifo is

    -- Clear signals for their respective sides
    signal r_clear : std_logic ;
    signal w_clear : std_logic ;

    type status_t is record
        address : natural range 0 to 1023 ;
        count   : natural range 0 to 1023 ;
    end record ;

begin

end architecture ; -- arch
