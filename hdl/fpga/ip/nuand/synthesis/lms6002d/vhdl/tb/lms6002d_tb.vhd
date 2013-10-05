library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;
    use ieee.math_real.all ;

library std ;
    use std.env.all ;

library nuand ;
    use nuand.util.all ;

entity lms6002d_tb is
end entity ;

architecture arch of lms6002d_tb is

    signal rx_clock                 :   std_logic                       := '1' ;
    signal rx_reset                 :   std_logic                       := '1' ;
    signal rx_enable                :   std_logic                       := '0' ;

    signal rx_sample_i              :   signed(11 downto 0) ;
    signal rx_sample_q              :   signed(11 downto 0) ;
    signal rx_sample_valid          :   std_logic ;

    signal rx_lms_data              :   signed(11 downto 0)             := (others =>'0') ;
    signal rx_lms_iq_sel            :   std_logic                       := '0' ;
    signal rx_lms_enable            :   std_logic ;

    signal tx_clock                 :   std_logic                       := '1' ;
    signal tx_reset                 :   std_logic                       := '1' ;
    signal tx_enable                :   std_logic                       := '0' ;

    signal tx_sample_i              :   signed(11 downto 0)             := (others =>'0') ;
    signal tx_sample_q              :   signed(11 downto 0)             := (others =>'0') ;
    signal tx_sample_valid          :   std_logic                       := '0' ;

    signal tx_lms_data              :   signed(11 downto 0) ;
    signal tx_lms_iq_sel            :   std_logic ;
    signal tx_lms_enable            :   std_logic ;

    signal lms_resetx               :   std_logic                       := '0' ;

    -- Simulation end signals
    signal rx_end                   :   boolean                         := false ;
    signal tx_end                   :   boolean                         := false ;
    signal tb_end                   :   boolean                         := false ;

begin

    -- Clock stimulus
    tx_clock <= not tx_clock after 1 ns ;
    rx_clock <= not rx_clock after 1 ns ;

    U_lms6002d : entity work.lms6002d(arch)
      port map (
        rx_clock                =>  rx_clock,
        rx_reset                =>  rx_reset,
        rx_enable               =>  rx_enable,

        rx_sample_i             =>  rx_sample_i,
        rx_sample_q             =>  rx_sample_q,
        rx_sample_valid         =>  rx_sample_valid,

        rx_lms_data             =>  rx_lms_data,
        rx_lms_iq_sel           =>  rx_lms_iq_sel,
        rx_lms_enable           =>  rx_lms_enable,

        tx_clock                =>  tx_clock,
        tx_reset                =>  tx_reset,
        tx_enable               =>  tx_enable,

        tx_sample_i             =>  tx_sample_i,
        tx_sample_q             =>  tx_sample_q,
        tx_sample_valid         =>  tx_sample_valid,

        tx_lms_data             =>  tx_lms_data,
        tx_lms_iq_sel           =>  tx_lms_iq_sel,
        tx_lms_enable           =>  tx_lms_enable
    ) ;

    lms_resetx <= '1' after 5 ns ;

    U_lms6002d_model : entity work.lms6002d_model(arch)
      port map (
        resetx                  =>  lms_resetx,
        rx_clock                =>  rx_clock,
        rx_clock_out            =>  open,
        rx_data                 =>  rx_lms_data,
        rx_enable               =>  rx_lms_enable,
        rx_iq_select            =>  rx_lms_iq_sel,

        tx_clock                =>  tx_clock,
        tx_data                 =>  tx_lms_data,
        tx_enable               =>  tx_lms_enable,
        tx_iq_select            =>  tx_lms_iq_sel,

        sclk                    =>  '0',
        sen                     =>  '0',
        sdio                    =>  '0',
        sdo                     =>  open,

        pll_out                 =>  open
      ) ;

    -- End testbench
    tb_end <= rx_end and tx_end ;

    rx_tb : process
    begin
        -- Setup reset input conditions
        rx_reset <= '1' ;
        nop( rx_clock, 10 ) ;

        -- Come out of reset
        rx_reset <= '0' ;
        nop( rx_clock, 10 ) ;

        -- Enable the RX side of things
        rx_enable <= '1' ;
        nop( rx_clock, 2000 ) ;

        -- Disable RX
        rx_enable <= '0' ;
        nop( rx_clock, 100 ) ;

        -- Enable the RX side of things
        rx_enable <= '1' ;
        nop( rx_clock, 2000 ) ;

        report "-- RX Finished --" ;
        rx_end <= true ;
        wait ;
    end process ;

    tx_tb : process
    begin
        -- Setup reset input conditions
        tx_enable <= '0' ;
        nop( tx_clock, 700 ) ;
        tx_reset <= '0' ;
        nop( tx_clock, 10 ) ;
        tx_enable <= '1' ;
        for i in 1 to 10000 loop
            tx_sample_i <= to_signed(i mod 2048,tx_sample_i'length) ;
            tx_sample_q <= to_signed(-(i mod 2048),tx_sample_q'length) ;
            tx_sample_valid <= '1' ;
            nop( tx_clock, 1 ) ;
            tx_sample_valid <= '0' ;
            nop( tx_clock, 1 ) ;
        end loop ;
        report "-- TX Finshed --" ;
        tx_end <= true ;
        wait ;
    end process ;

    finish_tb : process
    begin
        wait until tb_end = true ;
        report "-- End of Simulation --" ;
        stop(2) ;
    end process ;

end architecture ;

