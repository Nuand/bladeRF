-- Copyright (c) 2013-2017 Nuand LLC
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

library nuand ;

entity bladerf_tb is
end entity ; -- bladerf_tb

architecture arch of bladerf_tb is

    constant C5_CLOCK_HALF_PERIOD       : time  := 1 sec * (1.0/38.4e6/2.0) ;
    constant ADI_RX_CLOCK_HALF_PERIOD   : time  := 1 sec * (1.0/61.44e6/2.0);

    type fx3_gpif_t is record
        pclk            :   std_logic ;
        gpif            :   std_logic_vector(31 downto 0) ;
        ctl             :   std_logic_vector(12 downto 0) ;
    end record ;

    type fx3_uart_t is record
        rxd             :   std_logic ;
        txd             :   std_logic ;
        cts             :   std_logic ;
    end record ;

    procedure nop( signal clock : in std_logic ; count : in natural ) is
    begin
        for i in 1 to count loop
            wait until rising_edge(clock) ;
        end loop ;
    end procedure ;

    signal c5_clock2    :   std_logic   := '1';
    signal adi_rx_clock :   std_logic   := '1';
    signal fx3_gpif     :   fx3_gpif_t;
    signal fx3_uart     :   fx3_uart_t;

begin

    -- Main 38.4MHz clock input
    c5_clock2 <= not c5_clock2 after C5_CLOCK_HALF_PERIOD ;

    -- AD9361 clock input
    adi_rx_clock <= not adi_rx_clock after ADI_RX_CLOCK_HALF_PERIOD;

    -- Top level of the FPGA
    U_bladerf : entity nuand.bladerf
      port map (
        -- Main system clock
        c5_clock2           => c5_clock2,

        -- Si53304 clock controls
        si_clock_sel        => open,
        c5_clock2_oe        => open,
        ufl_clock_oe        => open,
        exp_clock_oe        => open,

        -- VCTCXO DAC
        dac_sclk            => open,
        dac_sdi             => open,
        dac_csn             => open,

        -- LEDs
        led                 => open,

        -- Power supply sync
        ps_sync_1p1         => open,
        ps_sync_1p8         => open,

        -- Power monitor
        pwr_sda             => open,
        pwr_scl             => open,
        pwr_status          => '0',

        -- AD9361 RX interface
        adi_rx_clock        => adi_rx_clock,
        adi_rx_data         => (others => '0'),
        adi_rx_frame        => '0',

        -- RX RF switching
        adi_rx_spdt1_v      => open,
        adi_rx_spdt2_v      => open,

        -- RX bias-tee enable
        rx_bias_en          => open,

        -- AD9361 TX interface
        adi_tx_clock        => open,
        adi_tx_data         => open,
        adi_tx_frame        => open,

        -- TX RF switching
        adi_tx_spdt1_v      => open,
        adi_tx_spdt2_v      => open,

        -- TX bias-tee enable
        tx_bias_en          => open,

        -- AD9361 SPI interface
        adi_spi_sclk        => open,
        adi_spi_csn         => open,
        adi_spi_sdi         => open,
        adi_spi_sdo         => '0',

        -- AD9361 control interface
        adi_reset_n         => open,
        adi_enable          => open,
        adi_txnrx           => open,
        adi_en_agc          => open,
        adi_ctrl_in         => open,
        adi_ctrl_out        => (others => '0'),
        adi_sync_in         => open,

        -- ADF4002 SPI interface
        adf_sclk            => open,
        adf_csn             => open,
        adf_sdi             => open,
        adf_ce              => open,
        adf_muxout          => '0',

        -- FX3 GPIF Interface
        fx3_pclk            => fx3_gpif.pclk,
        fx3_gpif            => fx3_gpif.gpif,
        fx3_ctl             => fx3_gpif.ctl,

        -- FX3 UART interface
        fx3_uart_rxd        => fx3_uart.rxd,
        fx3_uart_txd        => fx3_uart.txd,
        fx3_uart_cts        => fx3_uart.cts,

        -- Expansion Interface
        exp_present         => '0',
        exp_clock_req       => '0',
        exp_i2c_sda         => open,
        exp_i2c_scl         => open,
        exp_gpio            => open,

        -- Mini expansion
        mini_exp1           => open,
        mini_exp2           => open,

        -- HW rev
        hw_rev              => (others => '0')
      ) ;

    -- FX3 Model
    U_fx3 : entity nuand.fx3_model(dma)
      port map (
        -- GPIF
        fx3_pclk            => fx3_gpif.pclk,
        fx3_gpif            => fx3_gpif.gpif,
        fx3_ctl             => fx3_gpif.ctl,

        -- UART
        fx3_uart_rxd        => fx3_uart.rxd,
        fx3_uart_txd        => fx3_uart.txd,
        fx3_uart_cts        => fx3_uart.cts,
        fx3_rx_en           => '1',
        fx3_rx_meta_en      => '1',
        fx3_tx_en           => '1',
        fx3_tx_meta_en      => '1'
      ) ;

    -- Stimulus
    tb : process
    begin
        wait for 10 ms ;
        report "-- End of simulation --" severity failure ;
    end process ;

end architecture ; -- arch
