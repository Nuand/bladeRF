library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;

entity time_tamer is
  port (
    -- Control signals
    clock       :   in  std_logic ;
    reset       :   in  std_logic ;

    -- Memory mapped interface
    addr        :   in  std_logic_vector(4 downto 0) ;
    din         :   in  std_logic_vector(7 downto 0) ;
    dout        :   out std_logic_vector(7 downto 0) ;
    write       :   in  std_logic ;
    read        :   in  std_logic ;
    waitreq     :   out std_logic ;
    readack     :   out std_logic ;
    intr        :   out std_logic ;

    -- Sync signals
    ts_sync_in  :   in  std_logic ;
    ts_sync_out :   out std_logic ;

    -- Exported signals
    ts_pps      :   in  std_logic ;
    ts_clock    :   in  std_logic ;
    ts_reset    :   in  std_logic ;
    ts_time     :   out std_logic_vector(63 downto 0)
  ) ;
end entity ;

architecture arch of time_tamer is

    signal ack          : std_logic ;

    signal snap_time : unsigned(63 downto 0);

    signal snap_req  : std_logic ;

    signal snap_ack  : std_logic ;

    signal hold_time    :   unsigned(63 downto 0) ;

    signal uaddr    :   unsigned(addr'range) ;

    signal status   :   std_logic_vector(7 downto 0) ;

    signal intr_clear   :   std_logic ;
    signal intr_set     :   std_logic ;

    signal timestamp    :   unsigned(63 downto 0) ;

    signal compare_time         :   unsigned(63 downto 0) := (others => '0');

    signal mm_clear_compare     :   std_logic ;
    signal mm_compare_cleared   :   std_logic ;

    signal ts_clear_compare     :   std_logic ;
    signal ts_compare_cleared   :   std_logic ;

    signal ts_time_trigger      :   std_logic ;
    signal mm_time_trigger      :   std_logic ;

    signal current_time         :   unsigned(63 downto 0) ;
    signal current_req          :   std_logic ;
    signal current_ack          :   std_logic ;

    signal status_past          :   std_logic ;

    signal mm_compare_load      :   std_logic ;
    signal ts_compare_load      :   std_logic ;

    signal mm_compare_loaded    :   std_logic ;
    signal ts_compare_loaded    :   std_logic ;

    signal mm_ts_reset          :   std_logic ;

begin

    uaddr <= unsigned(addr) ;

    readack <= ack ;
    waitreq <= not(ack or write) ;

    ts_sync_out <= '0' ;

    status <= (0 => status_past, others =>'0') ;

    U_sync_ts_reset : entity work.synchronizer
      generic map (
        RESET_LEVEL =>  '1'
      ) port map (
        reset       =>  reset,
        clock       =>  clock,
        async       =>  ts_reset,
        sync        =>  mm_ts_reset
      ) ;

    U_sync_load : entity work.synchronizer
      generic map (
        RESET_LEVEL =>  '0'
      ) port map (
        reset       =>  ts_reset,
        clock       =>  ts_clock,
        async       =>  mm_compare_load,
        sync        =>  ts_compare_load
      ) ;

    U_sync_loaded : entity work.synchronizer
      generic map (
        RESET_LEVEL =>  '0'
      ) port map (
        reset       =>  reset,
        clock       =>  clock,
        async       =>  ts_compare_loaded,
        sync        =>  mm_compare_loaded
      ) ;

    U_sync_clear : entity work.synchronizer
      generic map (
        RESET_LEVEL =>  '0'
      ) port map (
        reset       =>  ts_reset,
        clock       =>  ts_clock,
        async       =>  mm_clear_compare,
        sync        =>  ts_clear_compare
      ) ;

    U_sync_cleared : entity work.synchronizer
      generic map (
        RESET_LEVEL =>  '0'
      ) port map (
        reset       =>  reset,
        clock       =>  clock,
        async       =>  ts_compare_cleared,
        sync        =>  mm_compare_cleared
      ) ;

    U_sync_trigger : entity work.synchronizer
      generic map (
        RESET_LEVEL =>  '0'
      ) port map (
        reset       =>  reset,
        clock       =>  clock,
        async       =>  ts_time_trigger,
        sync        =>  mm_time_trigger
      ) ;

    ts_time <= std_logic_vector(timestamp) ;

    increment_time : process(ts_clock, ts_reset)
        variable tick : std_logic := '1' ;
    begin
        if( ts_reset = '1' ) then
            tick := '1' ;
            timestamp <= (others =>'0') ;
        elsif( rising_edge(ts_clock) ) then
            if( tick = '0' ) then
                timestamp <= timestamp + 1 ;
            end if ;
            tick := not tick ;
        end if ;
    end process ;

    U_snap : entity work.handshake
      generic map (
        DATA_WIDTH          =>  ts_time'length
      ) port map (
        source_reset        =>  ts_reset,
        source_clock        =>  ts_clock,
        source_data         =>  std_logic_vector(timestamp),

        dest_reset          =>  reset,
        dest_clock          =>  clock,
        unsigned(dest_data) =>  snap_time,
        dest_req            =>  snap_req,
        dest_ack            =>  snap_ack
      ) ;

    U_current : entity work.handshake
      generic map (
        DATA_WIDTH          =>  ts_time'length
      ) port map (
        source_reset        =>  ts_reset,
        source_clock        =>  ts_clock,
        source_data         =>  std_logic_vector(timestamp),

        dest_reset          =>  reset,
        dest_clock          =>  clock,
        unsigned(dest_data) =>  current_time,
        dest_req            =>  current_req,
        dest_ack            =>  current_ack
      ) ;

    request : process(clock, reset)
    begin
        if( reset = '1' ) then
            snap_req <= '0' ;
        elsif( rising_edge(clock) ) then
            if( uaddr = 0 and read = '1' ) then
                snap_req <= '1' ;
            else
                snap_req <= '0' ;
            end if ;
        end if ;
    end process ;

    generate_ack : process(clock, reset)
    begin
        if( reset = '1' ) then
            ack <= '0' ;
        elsif( rising_edge(clock) ) then
            if( uaddr = 0 and read = '1' and mm_ts_reset = '0' ) then
                ack <= snap_ack ;
            else
                ack <= read ;
            end if ;
        end if ;
    end process ;

    mm_read : process(clock)
    begin
        if( rising_edge(clock) ) then
            case to_integer(uaddr) is
                when 0  => dout <= std_logic_vector(snap_time(7 downto 0));
                when 1  => dout <= std_logic_vector(snap_time(15 downto 8));
                when 2  => dout <= std_logic_vector(snap_time(23 downto 16));
                when 3  => dout <= std_logic_vector(snap_time(31 downto 24));
                when 4  => dout <= std_logic_vector(snap_time(39 downto 32));
                when 5  => dout <= std_logic_vector(snap_time(47 downto 40));
                when 6  => dout <= std_logic_vector(snap_time(55 downto 48));
                when 7  => dout <= std_logic_vector(snap_time(63 downto 56));

                when 9  => dout <= status ;

                when others  => dout <= x"13";
            end case;
        end if ;
    end process;

    mm_write : process(clock)
    begin
        if( rising_edge(clock) ) then
            intr_set <= '0' ;
            intr_clear <= '0' ;
            if( write = '1' ) then
                case to_integer(uaddr) is
                    when 0 => hold_time( 7 downto  0) <= unsigned(din) ;
                    when 1 => hold_time(15 downto  8) <= unsigned(din) ;
                    when 2 => hold_time(23 downto 16) <= unsigned(din) ;
                    when 3 => hold_time(31 downto 24) <= unsigned(din) ;
                    when 4 => hold_time(39 downto 32) <= unsigned(din) ;
                    when 5 => hold_time(47 downto 40) <= unsigned(din) ;
                    when 6 => hold_time(55 downto 48) <= unsigned(din) ;
                    when 7 => hold_time(63 downto 56) <= unsigned(din) ;

                    when 8 =>
                        -- Command coming in!
                        case to_integer(unsigned(din)) is
                            when 0 =>
                                -- Set interrupt
                                intr_set <= '1' ;

                            when 1 =>
                                -- Clear interrupt
                                intr_clear <= '1' ;

                            when 2 =>
                                -- Latch time manual

                            when 3 =>
                                -- Latch time next PPS

                            when others =>
                                null ;

                        end case ;

                    when others => null ;
                end case ;
            end if ;
        end if ;
    end process ;

    check_compare : process(ts_clock, ts_reset)
        type fsm_t is (WAIT_FOR_LOAD, WAIT_FOR_COMPARE, WAIT_FOR_CLEAR) ;
        variable fsm : fsm_t := WAIT_FOR_LOAD ;
    begin
        if( ts_reset = '1' ) then
            fsm := WAIT_FOR_LOAD ;
            ts_compare_loaded <= '0' ;
            ts_compare_cleared <= '0' ;
            ts_time_trigger <= '0' ;
        elsif(rising_edge(ts_clock)) then
            case fsm is

                when WAIT_FOR_LOAD =>
                    if( ts_compare_load = '1' ) then
                        ts_compare_loaded <= '1' ;
                        compare_time <= hold_time ;
                        ts_compare_cleared <= '0' ;
                        fsm := WAIT_FOR_COMPARE ;
                    end if ;

                when WAIT_FOR_COMPARE =>
                    if( compare_time = timestamp ) then
                        ts_time_trigger <= '1' ;
                        ts_compare_loaded <= '0' ;
                        fsm := WAIT_FOR_CLEAR ;
                    end if ;

                when WAIT_FOR_CLEAR =>
                    if( ts_clear_compare = '1' ) then
                        ts_time_trigger <= '0' ;
                        ts_compare_cleared <= '1' ;
                        fsm := WAIT_FOR_LOAD ;
                    end if ;

                when others =>
                    null ;

            end case ;
        end if ;
    end process ;

    interrupt : process(clock, reset)
        type fsm_t is (WAIT_FOR_ARM, CHECK_CURRENT_TIME, SET_COMPARE_TIME, PAST_TIME, WAIT_FOR_COMPARE, WAIT_FOR_INTR_CLEAR, WAIT_FOR_COMPARE_CLEAR) ;
        variable fsm : fsm_t := WAIT_FOR_ARM ;
    begin
        if( reset = '1' ) then
            intr <= '0' ;
            fsm := WAIT_FOR_ARM ;
            mm_clear_compare <= '0' ;
            mm_compare_load <= '0' ;
            current_req <= '0' ;
            status_past <= '0' ;
        elsif( rising_edge(clock) ) then
            if( mm_ts_reset = '1' ) then
                intr <= '0' ;
                fsm := WAIT_FOR_ARM ;
                mm_clear_compare <= '0' ;
                mm_compare_load <= '0' ;
                current_req <= '0' ;
                status_past <= '0' ;
            else
                case fsm is
                    when WAIT_FOR_ARM =>
                        current_req <= '0' ;
                        status_past <= '0' ;
                        mm_clear_compare <= '0' ;
                        mm_compare_load <= '0' ;
                        if( intr_set = '1' ) then
                            fsm := CHECK_CURRENT_TIME ;
                        end if ;

                    when CHECK_CURRENT_TIME =>
                        current_req <= '1' ;
                        if( current_ack = '1' ) then
                            current_req <= '0' ;
                            if( hold_time < current_time ) then
                                fsm := PAST_TIME ;
                                status_past <= '1' ;
                            else
                                fsm := SET_COMPARE_TIME ;
                            end if ;
                        end if ;

                    when SET_COMPARE_TIME =>
                            mm_compare_load <= '1' ;
                            if( mm_compare_loaded = '1' ) then
                                mm_compare_load <= '0' ;
                                fsm := WAIT_FOR_COMPARE ;
                            end if ;

                    when PAST_TIME =>
                        intr <= '1' ;
                        fsm := WAIT_FOR_INTR_CLEAR ;

                    when WAIT_FOR_COMPARE =>
                        if( mm_time_trigger = '1' ) then
                            intr <= '1' ;
                            fsm := WAIT_FOR_INTR_CLEAR ;
                        end if ;

                    when WAIT_FOR_INTR_CLEAR =>
                        mm_clear_compare <= '1' ;
                        if( intr_clear = '1' ) then
                            intr <= '0' ;
                            if( status_past = '1' ) then
                                fsm := WAIT_FOR_ARM ;
                            else
                                fsm := WAIT_FOR_COMPARE_CLEAR ;
                            end if ;
                        end if ;

                    when WAIT_FOR_COMPARE_CLEAR =>
                        if( mm_compare_cleared = '1' ) then
                            mm_clear_compare <= '0' ;
                            fsm := WAIT_FOR_ARM ;
                        end if ;

                    when others => null ;
                end case ;
            end if ;
        end if ;
    end process ;

end architecture ;

