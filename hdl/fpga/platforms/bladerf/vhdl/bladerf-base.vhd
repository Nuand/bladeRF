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
