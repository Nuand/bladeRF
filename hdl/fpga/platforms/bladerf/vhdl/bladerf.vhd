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

entity bladerf is
  port (
    -- Main 38.4MHz system clock
    c4_clock            :   in      std_logic ;

    -- VCTCXO DAC
    dac_sclk            :   out     std_logic := '0' ;
    dac_sdi             :   out     std_logic := '0' ;
    dac_sdo             :   in      std_logic ;
    dac_csx             :   out     std_logic := '1' ;

    -- LEDs
    led                 :   buffer  std_logic_vector(3 downto 1) := (others =>'0') ;

    -- LMS RX Interface
    lms_rx_clock_out    :   in      std_logic ;
    lms_rx_data         :   in      signed(11 downto 0) ;
    lms_rx_enable       :   out     std_logic ;
    lms_rx_iq_select    :   in      std_logic ;
    lms_rx_v            :   out     std_logic_vector(2 downto 1) ;

    -- LMS TX Interface
    c4_tx_clock         :   in      std_logic ;
    lms_tx_data         :   out     signed(11 downto 0) ;
    lms_tx_enable       :   out     std_logic ;
    lms_tx_iq_select    :   buffer  std_logic := '0' ;
    lms_tx_v            :   out     std_logic_vector(2 downto 1) ;

    -- LMS SPI Interface
    lms_sclk            :   buffer  std_logic := '0' ;
    lms_sen             :   out     std_logic := '1' ;
    lms_sdio            :   out     std_logic := '0' ;
    lms_sdo             :   in      std_logic := '0' ;

    -- LMS Control Interface
    lms_pll_out         :   in      std_logic ;
    lms_reset           :   buffer  std_logic ;

    -- Si5338 I2C Interface
    si_scl              :   inout   std_logic ;
    si_sda              :   inout   std_logic ;

    -- FX3 Interface
    fx3_pclk            :   in      std_logic ;
    fx3_gpif            :   inout   std_logic_vector(31 downto 0) ;
    fx3_ctl             :   inout   std_logic_vector(12 downto 0) ;
    fx3_uart_rxd        :   out     std_logic ;
    fx3_uart_txd        :   in      std_logic ;
    fx3_uart_cts        :   out     std_logic ;

    -- Reference signals
    ref_vctcxo_tune     :   in      std_logic ;
    ref_sma_clock       :   in      std_logic ;

    -- Mini expansion
    mini_exp1           :   inout   std_logic ;
    mini_exp2           :   inout   std_logic ;

    -- Expansion Interface
    exp_present         :   in      std_logic ;
    exp_spi_clock       :   out     std_logic ;
    exp_spi_miso        :   in      std_logic ;
    exp_spi_mosi        :   out     std_logic ;
    exp_clock_in        :   in      std_logic ;
    exp_gpio            :   inout   std_logic_vector(32 downto 2)
  ) ;
end entity ; -- bladerf
--TODO: explain revisions
