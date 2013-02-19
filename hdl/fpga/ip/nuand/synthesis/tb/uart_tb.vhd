library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;

entity uart_tb is
end entity ; -- uart_tb

architecture arch of uart_tb is

    signal clock        :   std_logic   := '1' ;
    signal reset        :   std_logic   := '1' ;
    signal enable       :   std_logic   := '0' ;

    signal rs232_rxd    :   std_logic   ;
    signal rs232_txd    :   std_logic   := '1' ;

    signal txd_we       :   std_logic ;
    signal txd_full     :   std_logic   := '0' ;
    signal txd_data     :   std_logic_vector(7 downto 0) ;

    signal rxd_re       :   std_logic ;
    signal rxd_empty    :   std_logic   := '1' ;
    signal rxd_data     :   std_logic_vector(7 downto 0) := (others =>'0') ;

    procedure nop( signal clock : in std_logic ; count : in natural ) is
    begin
        for i in 1 to count loop
            wait until rising_edge(clock) ;
        end loop ;
    end procedure ;

    procedure uart_send( signal clock : in std_logic ; signal txd : out std_logic ; data : in std_logic_vector(7 downto 0) ; cpb : in natural ) is
    begin
        wait until rising_edge(clock) ;
        -- Send start bit
        txd <= '0' ;
        nop( clock, cpb ) ;

        -- Send data
        for i in 0 to data'high loop
            txd <= data(i) ;
            nop( clock, cpb ) ;
        end loop ;

        -- Send stop bit
        txd <= '1' ;
        nop( clock, cpb ) ;
    end procedure ;

begin

    clock <= not clock after 1 ns ;

    U_uart : entity work.uart
      port map (
        clock       =>  clock,
        reset       =>  reset,
        enable      =>  enable,

        rs232_rxd   => rs232_rxd,
        rs232_txd   => rs232_txd,

        txd_we      => txd_we,
        txd_full    => txd_full,
        txd_data    => txd_data,

        rxd_re      => rxd_re,
        rxd_empty   => rxd_empty,
        rxd_data    => rxd_data
      ) ;

    stimulate : process
    begin
        -- Reset
        reset <= '1' ;
        nop( clock, 10 ) ;

        -- Done with reset ...
        reset <= '0' ;
        nop( clock, 10 ) ;

        -- Make sure we don't do anything when we aren't enabled ...
        enable <= '0' ;
        nop( clock, 10 ) ;

        -- ... and now lets go!
        enable <= '1' ;
        nop( clock, 100000 ) ;

        -- Ding!
        report "-- End of Simulation -- " severity failure ;

    end process ;

    -- Just keep sending data in from the UART
    simulate_uart_sending : process
        variable val : natural := 0 ;
    begin
        rs232_txd <= '1' ;
        wait until enable = '1' ;
        nop( clock, 100 ) ;
        while true loop
            uart_send( clock, rs232_txd, std_logic_vector(to_unsigned(val,8)), 10 ) ;
            val := (val + 1) mod 256 ;
            --nop( clock, 5 ) ;
        end loop ;
    end process ;

    -- Feedback data continuously
    simulate_s2u_fifo : process
    begin
        rxd_empty <= '1' ;
        wait until enable = '1' ;
        nop( clock, 100 ) ;
        rxd_empty <= '0' ;
        rxd_data <= (others =>'0') ;
        while true loop
            wait until rising_edge( clock ) and rxd_re = '1' ;
            rxd_data <= std_logic_vector(unsigned(rxd_data)+1) ;
        end loop ;
    end process ;

end architecture ; -- arch
