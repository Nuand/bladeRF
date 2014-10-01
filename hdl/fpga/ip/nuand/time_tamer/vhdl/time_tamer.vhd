library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;

entity time_tamer is
  generic (
    RESET_LEVEL :   std_logic       := '1'
  ) ;
  port (
    -- Control signals
    clock       :   in  std_logic ;
    reset       :   in  std_logic ;

    -- Memory mapped interface
    addr        :   in  std_logic_vector(3 downto 0) ;
    din         :   in  std_logic_vector(7 downto 0) ;
    dout        :   out std_logic_vector(7 downto 0) ;
    write       :   in  std_logic ;
    read        :   in  std_logic ;
    waitreq     :   out std_logic ;
    readack     :   out std_logic ;
    intr        :   out std_logic ;

    -- Exported signalsa
    synchronize :   out std_logic ;
    tx_clock    :   in  std_logic ;
    tx_reset    :   in  std_logic ;
    tx_time     :   in  std_logic_vector(63 downto 0) ;

    rx_clock    :   in  std_logic ;
    rx_reset    :   in  std_logic ;
    rx_time     :   in  std_logic_vector(63 downto 0)
  ) ;
end entity ;

architecture arch of time_tamer is

    signal ack          : std_logic ;
    signal hold         : std_logic ;

    signal tx_snap_time : std_logic_vector(63 downto 0);
    signal rx_snap_time : std_logic_vector(63 downto 0);

    signal rx_snap_req  : std_logic ;
    signal tx_snap_req  : std_logic ;

    signal rx_snap_ack  : std_logic ;
    signal tx_snap_ack  : std_logic ;

    signal sync_counter : signed(15 downto 0);
    -- Registers
    --  Address     Name            Description
    --  00 - 07     RXTIME          RX Time taken at the snapshot
    --  08 - 0f     TXTIME          TX Time taken at the snapshot
    --  00          SYNC            Writing this register will synchronize the timers

begin

    readack <= ack ;
    waitreq <= not ack ;

    U_tx_handshake : entity work.handshake
      generic map (
        DATA_WIDTH      =>  tx_time'length
      ) port map (
        source_reset    =>  tx_reset,
        source_clock    =>  tx_clock,
        source_data     =>  tx_time,

        dest_reset      =>  reset,
        dest_clock      =>  clock,
        dest_data       =>  tx_snap_time,
        dest_req        =>  tx_snap_req,
        dest_ack        =>  tx_snap_ack
      ) ;

    U_rx_handshake : entity work.handshake
      generic map (
        DATA_WIDTH      =>  rx_time'length
      ) port map (
        source_reset    =>  rx_reset,
        source_clock    =>  rx_clock,
        source_data     =>  rx_time,

        dest_reset      =>  reset,
        dest_clock      =>  clock,
        dest_data       =>  rx_snap_time,
        dest_req        =>  rx_snap_req,
        dest_ack        =>  rx_snap_ack
      ) ;

    request : process(clock, reset)
    begin
        if( reset = '1' ) then
            rx_snap_req <= '0' ;
            tx_snap_req <= '0' ;
        elsif( rising_edge(clock) ) then
            if( addr = "1000" and read = '1' ) then
                tx_snap_req <= '1' ;
            else
                tx_snap_req <= '0' ;
            end if ;

            if( addr = "0000" and read = '1' ) then
                rx_snap_req <= '1' ;
            else
                rx_snap_req <= '0' ;
            end if ;
        end if ;
    end process ;

    generate_ack : process(clock, reset)
    begin
        if( reset = '1' ) then
            ack <= '0' ;
        elsif( rising_edge(clock) ) then
            if( addr = "1000" and read = '1' ) then
                ack <= tx_snap_ack ;
            elsif( addr = "0000" and read = '1' ) then
                ack <= rx_snap_ack ;
            else
                ack <= read ;
            end if ;
        end if ;
    end process ;

    datamux : process(all)
    begin
        case addr is
            when "0000"  => dout <= rx_snap_time(7 downto 0);
            when "0001"  => dout <= rx_snap_time(15 downto 8);
            when "0010"  => dout <= rx_snap_time(23 downto 16);
            when "0011"  => dout <= rx_snap_time(31 downto 24);
            when "0100"  => dout <= rx_snap_time(39 downto 32);
            when "0101"  => dout <= rx_snap_time(47 downto 40);
            when "0110"  => dout <= rx_snap_time(55 downto 48);
            when "0111"  => dout <= rx_snap_time(63 downto 56);

            when "1000"  => dout <= tx_snap_time(7 downto 0);
            when "1001"  => dout <= tx_snap_time(15 downto 8);
            when "1010"  => dout <= tx_snap_time(23 downto 16);
            when "1011"  => dout <= tx_snap_time(31 downto 24);
            when "1100"  => dout <= tx_snap_time(39 downto 32);
            when "1101"  => dout <= tx_snap_time(47 downto 40);
            when "1110"  => dout <= tx_snap_time(55 downto 48);
            when "1111"  => dout <= tx_snap_time(63 downto 56);

            when others  => dout <= (others => 'X');
        end case;
    end process;

    process (clock)
    begin
        if( rising_edge( clock )) then
            if( reset = '1' ) then
                sync_counter <= (others => '0');
            else
                if( write = '1' ) then
                    sync_counter <= to_signed(15000, sync_counter'length);
                else
                    if( sync_counter > 0 ) then
                        sync_counter <= sync_counter - 1;
                    end if;
                end if;
            end if;
        end if;
    end process;

    synchronize <= '1' when sync_counter > 0 else '0';
    intr <= '0';

end architecture ;

