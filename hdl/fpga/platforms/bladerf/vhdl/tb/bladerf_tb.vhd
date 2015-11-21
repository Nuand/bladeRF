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

library nuand ;

entity bladerf_tb is
end entity ; -- bladerf_tb

architecture arch of bladerf_tb is

    constant C4_CLOCK_HALF_PERIOD   : time  := 1 sec * (1.0/38.4e6/2.0) ;

    type lms_rx_t is record
        clock           :   std_logic ;
        clock_out       :   std_logic ;
        data            :   signed(11 downto 0) ;
        enable          :   std_logic ;
        iq_select       :   std_logic ;
    end record ;

    type lms_tx_t is record
        clock           :   std_logic ;
        data            :   signed(11 downto 0) ;
        enable          :   std_logic ;
        iq_select       :   std_logic ;
    end record ;

    type lms_spi_t is record
        sclk            :   std_logic ;
        sen             :   std_logic ;
        sdio            :   std_logic ;
        sdo             :   std_logic ;
    end record ;

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

    signal c4_clock     :   std_logic   := '1' ;
    signal lms_rx       :   lms_rx_t := ( clock => '1', data => (others =>'0'), clock_out => '1', iq_select => '1', enable => '0' ) ;
    signal lms_tx       :   lms_tx_t := ( clock => '1', data => (others =>'0'), enable => '1', iq_select => '1' ) ;
    signal lms_spi      :   lms_spi_t ;
    signal lms_pll_out  :   std_logic ;
    signal lms_reset    :   std_logic ;
    signal fx3_gpif     :   fx3_gpif_t ;
    signal fx3_uart     :   fx3_uart_t ;
    signal refexp_1pps  :   std_logic ;

begin

    -- Main 38.4MHz clock input
    c4_clock <= not c4_clock after C4_CLOCK_HALF_PERIOD ;

    -- LMS 80MHz RX and TX clocks
    lms_tx.clock <= not lms_tx.clock after (1.0/80.0e6) * 1 sec ;
    lms_rx.clock <= not lms_rx.clock after (1.0/80.0e6) * 1 sec ;

    -- Top level of the FPGA
    U_bladerf : entity nuand.bladerf
      port map (
        -- Main system clock
        c4_clock            => c4_clock,

        -- VCTCXO DAC
        dac_sclk            => open,
        dac_sdi             => open,
        dac_sdo             => '0',
        dac_csx             => open,

        -- LEDs
        led                 => open,

        -- LMS RX Interface
        lms_rx_clock_out    => lms_rx.clock_out,
        lms_rx_data         => lms_rx.data,
        lms_rx_enable       => lms_rx.enable,
        lms_rx_iq_select    => lms_rx.iq_select,
        lms_rx_v            => open,

        -- LMS TX Interface
        c4_tx_clock         => lms_tx.clock,
        lms_tx_data         => lms_tx.data,
        lms_tx_enable       => lms_tx.enable,
        lms_tx_iq_select    => lms_tx.iq_select,
        lms_tx_v            => open,

        -- LMS SPI Interface
        lms_sclk            => lms_spi.sclk,
        lms_sen             => lms_spi.sen,
        lms_sdio            => lms_spi.sdio,
        lms_sdo             => lms_spi.sdo,

        -- LMS Control Interface
        lms_pll_out         => lms_pll_out,
        lms_reset           => lms_reset,

        -- FX3 Interface
        fx3_pclk            => fx3_gpif.pclk,
        fx3_gpif            => fx3_gpif.gpif,
        fx3_ctl             => fx3_gpif.ctl,
        fx3_uart_rxd        => fx3_uart.rxd,
        fx3_uart_txd        => fx3_uart.txd,
        fx3_uart_cts        => fx3_uart.cts,

        -- Reference signals
        ref_vctcxo_tune     => refexp_1pps,
        ref_sma_clock       => '0',

        -- Mini expansion
        mini_exp1           => open,
        mini_exp2           => open,

        -- Expansion Interface
        exp_present         => '0',
        exp_spi_clock       => open,
        exp_spi_miso        => '0',
        exp_spi_mosi        => open,
        exp_clock_in        => '0',
        exp_gpio            => open
      ) ;

    -- LMS6002D Model
    U_lms6002d : entity nuand.lms6002d_model
      port map (
        -- LMS RX Interface
        rx_clock        => lms_rx.clock,
        rx_clock_out    => lms_rx.clock_out,
        rx_data         => lms_rx.data,
        rx_enable       => lms_rx.enable,
        rx_iq_select    => lms_rx.iq_select,

        -- LMS TX Interface
        tx_clock        => lms_tx.clock,
        tx_data         => lms_tx.data,
        tx_enable       => lms_tx.enable,
        tx_iq_select    => lms_tx.iq_select,

        -- LMS SPI Interface
        sclk            => lms_spi.sclk,
        sen             => lms_spi.sen,
        sdio            => lms_spi.sdio,
        sdo             => lms_spi.sdo,

        -- LMS Control Interface
        pll_out         => lms_pll_out,
        resetx          => lms_reset
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

    -- Create an accurate 1pps signal that is 1 ms wide
    create_1pps : process
        constant PULSE_PERIOD   : time := 1 sec ;
        constant PULSE_WIDTH    : time := 1 ms ;
    begin
        if( now = 0 ps ) then
            refexp_1pps <= '0' ;
        else
            refexp_1pps <= '1' ;
        end if ;
        wait for PULSE_WIDTH ;
        refexp_1pps <= '0' ;
        wait for PULSE_PERIOD - PULSE_WIDTH ;
    end process ;

    -- Stimulus
    tb : process
    begin
        wait for 10 ms ;
        report "-- End of simulation --" severity failure ;
    end process ;

end architecture ; -- arch

