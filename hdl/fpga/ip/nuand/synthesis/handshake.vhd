library ieee ;
    use ieee.std_logic_1164.all ;

entity handshake is
  generic (
    DATA_WIDTH      :       positive        := 32
  ) ;
  port (
    source_reset    :   in  std_logic ;
    source_clock    :   in  std_logic ;
    source_data     :   in  std_logic_vector(DATA_WIDTH-1 downto 0) ;

    dest_reset      :   in  std_logic ;
    dest_clock      :   in  std_logic ;
    dest_data       :   out std_logic_vector(DATA_WIDTH-1 downto 0) ;
    dest_req        :   in  std_logic ;
    dest_ack        :   out std_logic
  ) ;
end entity ;

architecture arch of handshake is

    -- Holding flops for bus at the request time
    signal source_holding   :   std_logic_vector(source_data'range) ;

    -- Source req and ack signals
    signal source_req       :   std_logic ;
    signal source_ack       :   std_logic ;

    -- Synchronization signal for destination
    signal dest_ack_sync    :   std_logic ;

begin

    U_sync_req : entity work.synchronizer
      generic map (
        RESET_LEVEL     =>  '0'
      ) port map (
        reset           =>  source_reset,
        clock           =>  source_clock,
        async           =>  dest_req,
        sync            =>  source_req
      ) ;

    dest_ack <= dest_ack_sync ;
    U_sync_ack : entity work.synchronizer
      generic map (
        RESET_LEVEL     =>  '0'
      ) port map (
        reset           =>  dest_reset,
        clock           =>  dest_clock,
        async           =>  source_ack,
        sync            =>  dest_ack_sync
      ) ;

    dest_data <= source_holding ;
    grab_source : process(source_reset, source_clock)
        variable prev_req : std_logic := '0' ;
    begin
        if( source_reset = '1' ) then
            source_holding <= (others =>'0') ;
            source_ack <= '0' ;
            prev_req := '0' ;
        elsif( rising_edge(source_clock) ) then
            if( prev_req = '0' and source_req = '1' and source_ack = '0' ) then
                source_holding <= source_data ;
                source_ack <= '1' ;
            elsif( source_req = '0' and source_ack = '1' ) then
                source_ack <= '0' ;
            end if ;
            prev_req := source_req ;
        end if ;
    end process ;

end architecture ;

