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
    c5_clock2           :   in      std_logic ;

    -- SI53304 clock controls (3.3 V)
    si_clock_sel        :   out     std_logic := '0';
    c5_clock2_oe        :   out     std_logic := '0';
    ufl_clock_oe        :   out     std_logic := '0';
    exp_clock_oe        :   out     std_logic := '0';

    -- VCTCXO DAC (3.3 V)
    dac_sclk            :   out     std_logic := '0' ;
    dac_sdi             :   out     std_logic := '0' ;
    dac_csn             :   out     std_logic := '1' ;

    -- LEDs (3.3 V)
    led                 :   buffer  std_logic_vector(3 downto 1) := (others =>'0') ;

    -- Power supply sync (2.5 V)
    ps_sync_1p1         :   out     std_logic := '0';
    ps_sync_1p8         :   out     std_logic := '0';

    -- INA219 power monitor (3.3 V)
    pwr_sda             :   inout   std_logic := 'Z';
    pwr_scl             :   out     std_logic := 'Z';

    -- TPS2115A power mux status
    pwr_status          :   in      std_logic;

    -- AD9361 RX Interface (2.5 V, LVDS)
    adi_rx_clock        :   in      std_logic ;
    adi_rx_data         :   in      std_logic_vector(5 downto 0) ;
    adi_rx_frame        :   in      std_logic ;

    -- RX RF Switching (3.3 V)
    adi_rx_spdt1_v      :   out     std_logic_vector(2 downto 1) ;
    adi_rx_spdt2_v      :   out     std_logic_vector(2 downto 1) ;

    -- RX Bias-T Enable
    rx_bias_en          :   out     std_logic := '0';

    -- AD9361 TX Interface (2.5 V, LVDS)
    adi_tx_clock        :   out     std_logic ;
    adi_tx_data         :   out     std_logic_vector(5 downto 0) ;
    adi_tx_frame        :   out     std_logic ;

    -- TX RF Switching (3.3 V)
    adi_tx_spdt1_v      :   out     std_logic_vector(2 downto 1) ;
    adi_tx_spdt2_v      :   out     std_logic_vector(2 downto 1) ;

    -- TX Bias-T Enable
    tx_bias_en          :   out     std_logic := '0';

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
    adi_sync_in         :   out     std_logic := '0';

    -- ADF4002 SPI Interface (3.3 V)
    adf_sclk            :   out     std_logic := '0' ;
    adf_csn             :   out     std_logic := '1' ;
    adf_sdi             :   out     std_logic := '0' ;
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

    -- Expansion Interface (3.3 V / 2.5 V / 1.8 V)
    exp_present         :   in      std_logic;
    exp_clock_req       :   in      std_logic;
    exp_i2c_sda         :   inout   std_logic;
    exp_i2c_scl         :   inout   std_logic;
    exp_gpio            :   inout   std_logic_vector(31 downto 0);

    -- Mini expansion interface (3.3 V / 2.5 V / 1.8 V)
    mini_exp1           :   inout   std_logic := 'Z';
    mini_exp2           :   inout   std_logic := 'Z';

    -- Hardware revision resistors (3.3 V / 2.5 V / 1.8 V)
    hw_rev              :   in      std_logic_vector(1 downto 0)

  ) ;
end entity ; -- bladerf
--TODO: explain revisions
