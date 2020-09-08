-- Copyright (c) 2020 Nuand LLC
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

library nuand ;
    use nuand.util.all ;
    use nuand.fifo_readwrite_p.all;
    use nuand.common_dcfifo_p.all;
    use nuand.bladerf_p.all;


entity rfic_spi_tb is
end entity ;

architecture arch of rfic_spi_tb is
    signal  clock         : std_logic := '0' ;
    signal  reset         : std_logic ;
    signal  tx_ota_req    : std_logic ;
begin
    clock <= not clock after 6.25 ns;
    reset <= '1', '0' after 100 ns;
    tx_ota_req <= '0', '1' after 1 us, '0' after 20 us;


    uut: entity nuand.bladerf_rfic_spi_ctrl
       port map(
         sclk    => open,
         miso    => '0',
         mosi    => open,
         cs_n    => open,

         arbiter_req    => open,
         arbiter_grant  => '1',
         arbiter_done   => open,

         clock      => clock,
         reset      => reset,
         tx_ota_req => tx_ota_req
       );


end architecture ;
