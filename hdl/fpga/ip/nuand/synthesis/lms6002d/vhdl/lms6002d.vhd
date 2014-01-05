library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;

entity lms6002d is
  port (
    -- RX Controls
    rx_clock            :   in      std_logic ;
    rx_reset            :   in      std_logic ;
    rx_enable           :   in      std_logic ;

    -- RX Interface with LMS6002D
    rx_lms_data         :   in      signed(11 downto 0) ;
    rx_lms_iq_sel       :   in      std_logic ;
    rx_lms_enable       :   buffer  std_logic ;

    -- RX Sample Interface
    rx_sample_i         :   buffer  signed(11 downto 0) ;
    rx_sample_q         :   buffer  signed(11 downto 0) ;
    rx_sample_valid     :   buffer  std_logic ;

    -- TX Controls
    tx_clock            :   in      std_logic ;
    tx_reset            :   in      std_logic ;
    tx_enable           :   in      std_logic ;

    -- TX Sample Interface
    tx_sample_i         :   in      signed(11 downto 0) ;
    tx_sample_q         :   in      signed(11 downto 0) ;
    tx_sample_valid     :   in      std_logic ;

    -- TX Interface to the LMS6002D
    tx_lms_data         :   buffer  signed(11 downto 0) ;
    tx_lms_iq_sel       :   buffer  std_logic ;
    tx_lms_enable       :   buffer  std_logic
  ) ;
end entity ;

architecture arch of lms6002d is

begin

    -------------
    -- Receive --
    -------------
    rx_sample : process(rx_clock, rx_reset)
    begin
        if( rx_reset = '1' ) then
            rx_sample_i <= (others =>'0') ;
            rx_sample_q <= (others =>'0') ;
            rx_sample_valid <= '0' ;
            rx_lms_enable <= '0' ;
        elsif( rising_edge( rx_clock ) ) then
            if( rx_lms_iq_sel = '0' ) then
                rx_lms_enable <= rx_enable ;
            end if ;

            rx_sample_valid <= '0' ;
            if( rx_lms_enable = '1' ) then
                if(rx_lms_iq_sel = '1' ) then
                    rx_sample_i <= rx_lms_data ;
                else
                    rx_sample_q <= rx_lms_data ;
                    rx_sample_valid <= '1' ;
                end if ;
            end if ;
        end if ;
    end process ;

    --------------
    -- Transmit --
    --------------
    tx_sample : process(tx_clock, tx_reset)
        variable tx_q_reg   :   signed(11 downto 0) ;
    begin
        if( tx_reset = '1' ) then
            tx_lms_data <= (others =>'0') ;
            tx_lms_iq_sel <= '0' ;
            tx_lms_enable <= '0' ;
            tx_q_reg := (others =>'0') ;
        elsif( rising_edge( tx_clock ) ) then
            if( tx_lms_iq_sel = '0' ) then
                tx_lms_enable <= tx_enable ;
            end if ;

            if( tx_sample_valid = '1' ) then
                tx_lms_data <= tx_sample_i ;
                tx_q_reg := tx_sample_q ;
                tx_lms_iq_sel <= '0' ;
            elsif( tx_lms_enable = '1' ) then
                tx_lms_data <= tx_q_reg ;
                tx_lms_iq_sel <= '1' ;
            else
                tx_lms_data <= (others =>'0') ;
                tx_lms_iq_sel <= '0' ;
            end if ;
        end if ;
    end process ;

end architecture ;

