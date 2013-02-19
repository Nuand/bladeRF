library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;
    use ieee.math_real.all ;
    use ieee.math_complex.all ;

entity lms6002d_model is
  port (
    -- LMS RX Interface
    lms_rx_clock        :   in      std_logic ;
    lms_rx_clock_out    :   out     std_logic ;
    lms_rx_data         :   out     signed(11 downto 0) ;
    lms_rx_enable       :   in      std_logic ;
    lms_rx_iq_select    :   in      std_logic ;

    -- LMS TX Interface
    lms_tx_clock        :   in      std_logic ;
    lms_tx_data         :   in      signed(11 downto 0) ;
    lms_tx_enable       :   in      std_logic ;
    lms_tx_iq_select    :   in      std_logic ;

    -- LMS SPI Interface
    lms_sclk            :   in      std_logic ;
    lms_sen             :   in      std_logic ;
    lms_sdio            :   out     std_logic ;
    lms_sdo             :   in      std_logic ;

    -- LMS Control Interface
    lms_pll_out         :   out     std_logic ;
    lms_reset           :   in      std_logic
  ) ;
end entity ; -- lms6002d_model

architecture arch of lms6002d_model is

begin

    lms_pll_out <= '0' ;

    -- Write transmit samples to a file or drop them on the floor

    -- Read receive samples from a file or predetermined tone
    lms_rx_clock_out <= lms_rx_clock ;
    
    drive_rx_data : process( lms_reset, lms_rx_clock )
    begin
        if( lms_reset = '1' ) then
            lms_rx_data <= (others =>'0') ;
        elsif( rising_edge(lms_rx_clock) or falling_edge(lms_rx_clock) ) then
            lms_rx_data <= lms_rx_data + 1 ;
        end if ;
    end process ;

    -- Keep track of the SPI register set
    lms_sdio <= '0' ;

end architecture ; -- arch
