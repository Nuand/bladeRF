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

    -- Exported signals
    synchronize :   out std_logic ;
    time_tx     :   in  std_logic_vector(63 downto 0) ;
    time_rx     :   in  std_logic_vector(63 downto 0)

  ) ;
end entity ;

architecture arch of time_tamer is

    signal time_tx_r, time_tx_rr, time_tx_snap : std_logic_vector(63 downto 0);
    signal time_rx_r, time_rx_rr, time_rx_snap : std_logic_vector(63 downto 0);

    signal sync_counter : signed(15 downto 0);
    -- Registers
    --  Address     Name            Description
    --  00 - 07     RXTIME          RX Time taken at the snapshot
    --  08 - 0f     TXTIME          TX Time taken at the snapshot
    --  00          SYNC            Writing this register will synchronize the timers

begin

    process (clock)
    begin
        if( rising_edge( clock )) then
            time_tx_r <= time_tx;
            time_tx_rr <= time_tx_r;
            if (addr = "1000" and read = '1') then
                time_tx_snap <= time_tx_rr;
            end if;

            time_rx_r <= time_rx;
            time_rx_rr <= time_rx_r;
            if (addr = "0000" and read = '1') then
                time_rx_snap <= time_rx_rr;
            end if;
        end if;
    end process;

    process (clock)
    begin
        if( rising_edge( clock )) then
            case addr is
                when "0000"  => dout <= time_rx_snap(7 downto 0);
                when "0001"  => dout <= time_rx_snap(15 downto 8);
                when "0010"  => dout <= time_rx_snap(23 downto 16);
                when "0011"  => dout <= time_rx_snap(31 downto 24);
                when "0100"  => dout <= time_rx_snap(39 downto 32);
                when "0101"  => dout <= time_rx_snap(47 downto 40);
                when "0110"  => dout <= time_rx_snap(55 downto 48);
                when "0111"  => dout <= time_rx_snap(63 downto 56);

                when "1000"  => dout <= time_tx_snap(7 downto 0);
                when "1001"  => dout <= time_tx_snap(15 downto 8);
                when "1010"  => dout <= time_tx_snap(23 downto 16);
                when "1011"  => dout <= time_tx_snap(31 downto 24);
                when "1100"  => dout <= time_tx_snap(39 downto 32);
                when "1101"  => dout <= time_tx_snap(47 downto 40);
                when "1110"  => dout <= time_tx_snap(55 downto 48);
                when "1111"  => dout <= time_tx_snap(63 downto 56);

                when others  => dout <= (others => 'X');
            end case;
            readack <= read;
        end if;
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
    waitreq <= '0';

    -- Using address, din, dout, etc - make a register space

    -- Double register the incoming timestamps for reading and put them in the local clock domain

    -- Output the synchronize signal on write of a register such that the rising edge
    -- can be caught by the counters and set to zero.  Make the long pulse into a single
    -- pulse in the timers clock domain

end architecture ;

