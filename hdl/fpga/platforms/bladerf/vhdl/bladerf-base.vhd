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

architecture base of blader is

begin

    -- This file is the basic skeleton of what the bladeRF FPGA
    -- should drive for outputs.  All of the outputs are defined
    -- here.
    --
    -- Use this file as a template for new targets.

    -- VCTCXO DAC
    dac_sclk        <= '1' ;
    dac_sdi         <= '1' ;
    dac_csx         <= '1' ;

    -- LEDs
    led             <= (others =>'0') ;

    -- LMS RX Interface
    lms_rx_enable   <=  '0' ;
    lms_rx_v        <= (others =>'0') ;

    -- LMS TX Interface
    lms_tx_data     <= (others =>'0') ;
    lms_iq_select   <= '0' ;
    lms_tx_v        <= (others =>'0') ;

    -- LMS SPI Interface
    lms_sclk        <= '1' ;
    lms_sen         <= '1' ;
    lms_sdio        <= '0' ;

    -- LMS Control Interface
    lms_reset       <= '0' ;

    -- Si5338 I2C Interface
    si_scl          <= '1' ;
    si_sda          <= '1' ;

    -- FX3 Interface
    fx3_gpif        <= (others =>'Z') ;
    fx3_ctl         <= (others =>'Z') ;
    fx3_uart_rxd    <= '1' ;

    -- Mini expansion
    mini_exp1       <= '0' ;
    mini_exp2       <= '0' ;

    -- Expansion Interface
    exp_spi_clock   <= '1' ;
    exp_spi_mosi    <= '0' ;
    exp_gpio        <= (others =>'0') ;

end architecture ;
