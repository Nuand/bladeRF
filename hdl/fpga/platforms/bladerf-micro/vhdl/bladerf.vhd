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
    -- Main 38.4MHz system clock (3.3 V)
    c5_clock_1          :   in      std_logic ;
    c5_clock_2          :   in      std_logic ;

    -- VCTCXO DAC (3.3 V)
    dac_sclk            :   out     std_logic := '0' ;
    dac_sdi             :   out     std_logic := '0' ;
    dac_csn             :   out     std_logic := '1' ;

    -- LEDs (3.3 V)
    led                 :   buffer  std_logic_vector(3 downto 1) := (others =>'0') ;

    -- AD9361 RX Interface (2.5 V, LVDS)
    adi_rx_clock        :   in      std_logic ;
    adi_rx_data         :   in      std_logic_vector(5 downto 0) ;
    adi_rx_frame        :   in      std_logic ;

    -- RX RF Switching (3.3 V)
    adi_rx_sp4t1_v      :   out     std_logic_vector(2 downto 1) ;
    adi_rx_sp4t2_v      :   out     std_logic_vector(2 downto 1) ;

    -- AD9361 TX Interface (2.5 V, LVDS)
    adi_tx_clock        :   out     std_logic ;
    adi_tx_data         :   out     std_logic_vector(5 downto 0) ;
    adi_tx_frame        :   out     std_logic ;

    -- TX RF Switching (3.3 V)
    adi_tx_sp4t1_v      :   out     std_logic_vector(2 downto 1) ;
    adi_tx_sp4t2_v      :   out     std_logic_vector(2 downto 1) ;

    -- AD9361 SPI Interface (2.5 V)
    adi_spi_sclk        :   buffer  std_logic := '0' ;
    adi_spi_csn         :   out     std_logic := '1' ;
    adi_spi_sdi         :   out     std_logic := '0' ;
    adi_spi_sdo         :   in      std_logic := '0' ;

    -- AD9361 Control Interface (2.5 V)
    adi_reset_n         :   buffer  std_logic ;
    adi_enable          :   out     std_logic;
    adi_txnrx           :   out     std_logic := '0';
    adi_en_agc          :   out     std_logic := '0';
    adi_ctrl_in         :   out     std_logic_vector(3 downto 0) := (others => '0');
    adi_ctrl_out        :   in      std_logic_vector(7 downto 0);

    -- Misc I2C Interface (3.3 V)
    i2c1_scl            :   inout   std_logic ;
    i2c1_sda            :   inout   std_logic ;

    -- ADF4002 SPI Interface (3.3 V)
    adf_sclk            :   out     std_logic := '0' ;
    adf_csn             :   out     std_logic := '1' ;
    adf_sdi             :   out     std_logic := '0' ;
    adf_sdo             :   in      std_logic := '0' ;
    adf_ce              :   out     std_logic := '0';
    adf_muxout          :   in      std_logic;

    -- FX3 GPIF Interface - VIO1/2/3 (1.8 V)
    fx3_pclk            :   in      std_logic ;
    fx3_gpif            :   inout   std_logic_vector(31 downto 0) ;
    fx3_ctl             :   inout   std_logic_vector(12 downto 0) ;

    -- FX3 UART/Flash Interface - VIO4 (3.3 V)
    fx3_uart_rxd        :   out     std_logic ;
    fx3_uart_txd        :   in      std_logic ;
    fx3_uart_cts        :   out     std_logic ;

    -- Expansion Interface (3.3 V)
    exp_present_n       :   in      std_logic ;
    exp_i2c_sda         :   inout   std_logic;
    exp_i2c_scl         :   inout   std_logic;
    exp_clock_in        :   in      std_logic ;
    exp_clock_out       :   out     std_logic;
    exp_gpio            :   inout   std_logic_vector(27 downto 0)
  ) ;
end entity ; -- bladerf
--TODO: explain revisions
