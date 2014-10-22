library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;

entity handshake_tb is
end entity ;

architecture arch of handshake_tb is

    -- Constants
    constant DATA_WIDTH     :   positive            := 32 ;

    -- Source signals
    signal source_reset     :   std_logic           := '1' ;
    signal source_clock     :   std_logic           := '1' ;
    signal source_data      :   std_logic_vector(DATA_WIDTH-1 downto 0) := (others =>'0') ;

    signal dest_reset       :   std_logic           := '1' ;
    signal dest_clock       :   std_logic           := '1' ;
    signal dest_data        :   std_logic_vector(DATA_WIDTH-1 downto 0) ;
    signal dest_req         :   std_logic           := '0' ;
    signal dest_ack         :   std_logic ;

    procedure nop( signal clock : in std_logic ; count : natural ) is
    begin
        for i in 1 to count loop
            wait until rising_edge(clock) ;
        end loop ;
    end procedure ;

begin

    source_clock <= not source_clock after 100 ns ;
    dest_clock <= not dest_clock after 3 ns ;

    U_handshake : entity work.handshake
      generic map (
        DATA_WIDTH      =>  DATA_WIDTH
      ) port map (
        source_reset    =>  source_reset,
        source_clock    =>  source_clock,
        source_data     =>  source_data,

        dest_reset      =>  dest_reset,
        dest_clock      =>  dest_clock,
        dest_data       =>  dest_data,
        dest_req        =>  dest_req,
        dest_ack        =>  dest_ack
      ) ;

    increment_source_data : process(source_reset, source_clock)
    begin
        if( source_reset = '1' ) then
            source_data <= (others =>'0') ;
        elsif( rising_edge(source_clock) ) then
            source_data <= std_logic_vector(unsigned(source_data) + 1) ;
        end if ;
    end process ;

    source_tb : process
    begin
        source_reset <= '1' ;
        nop( source_clock, 100 ) ;
        source_reset <= '0' ;
        wait ;
    end process ;

    dest_tb : process
    begin
        dest_reset <= '1' ;
        nop( dest_clock, 100 ) ;
        dest_reset <= '0' ;
        nop( dest_clock, 100 ) ;

        for i in 1 to 100 loop
            dest_req <= '1' ;
            wait until rising_edge(dest_clock) and dest_ack = '1' ;
            dest_req <= '0' ;
            wait until rising_edge(dest_clock) and dest_ack = '0' ;
            for x in 1 to 10 loop
                wait until rising_edge(source_clock) ;
            end loop ;
        end loop ;

        report "-- End of Simulation --" severity failure ;
    end process ;

end architecture ;

