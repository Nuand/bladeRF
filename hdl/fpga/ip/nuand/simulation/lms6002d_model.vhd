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
    use ieee.math_real.all ;
    use ieee.math_complex.all ;

entity lms6002d_model is
  port (
    -- LMS RX Interface
    rx_clock        :   in      std_logic ;
    rx_clock_out    :   buffer  std_logic ;
    rx_data         :   buffer  signed(11 downto 0) ;
    rx_enable       :   in      std_logic ;
    rx_iq_select    :   buffer  std_logic ;

    -- LMS TX Interface
    tx_clock        :   in      std_logic ;
    tx_data         :   in      signed(11 downto 0) ;
    tx_enable       :   in      std_logic ;
    tx_iq_select    :   in      std_logic ;

    -- LMS SPI Interface
    sclk            :   in      std_logic ;
    sen             :   in      std_logic ;
    sdio            :   in      std_logic ;
    sdo             :   buffer  std_logic ;

    -- LMS Control Interface
    pll_out         :   buffer  std_logic ;
    resetx          :   in      std_logic
  ) ;
end entity ; -- lms6002d_model

architecture arch of lms6002d_model is

begin

    pll_out <= '0' ;

    -- Write transmit samples to a file or drop them on the floor

    -- Read receive samples from a file or predetermined tone
    rx_clock_out <= rx_clock ;

    drive_rx_data : process( resetx, rx_clock )
    begin
        if( resetx = '0' ) then
            rx_data <= (others =>'0') ;
            rx_iq_select <= '0' ;
        elsif( rising_edge(rx_clock) ) then
            if( rx_enable = '1' ) then
                rx_data <= rx_data + 1 ;
                rx_iq_select <= not rx_iq_select ;
            else
                rx_data <= (others =>'0') ;
                rx_iq_select <= '0' ;
            end if ;
        end if ;
    end process ;

    -- Keep track of the SPI register set
    sdo <= '0' ;

end architecture ; -- arch

