library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;

entity time_tamer_tb is
end entity ;

architecture arch of time_tamer_tb is

    signal clock        :   std_logic       := '1' ;
    signal reset        :   std_logic       := '1' ;

    signal addr         :   std_logic_vector(4 downto 0) := (others => '0');
    signal din          :   std_logic_vector(7 downto 0) ;
    signal dout         :   std_logic_vector(7 downto 0) ;
    signal write        :   std_logic                       := '0' ;
    signal read         :   std_logic                       := '0' ;
    signal waitreq      :   std_logic ;
    signal readack      :   std_logic ;
    signal intr         :   std_logic ;

    signal ts_clock     :   std_logic       := '1' ;
    signal ts_reset     :   std_logic       := '1' ;
    signal ts_time      :   std_logic_vector(63 downto 0) ;

    procedure read_time(
        signal clock    :   in  std_logic ;
        signal addr     :   out std_logic_vector(4 downto 0) ;
        signal dout     :   in std_logic_vector(7 downto 0) ;
        signal read     :   out std_logic ;
        signal readack  :   in  std_logic ;
               ts       :   out unsigned(63 downto 0)
    ) is
    begin
        for i in 0 to 7 loop
            addr <= std_logic_vector(to_unsigned(i, addr'length)) ;
            read <= '1' ;
            wait until rising_edge(clock) and readack = '1' ;
            ts(8*i+7 downto 8*i) := unsigned(dout) ;
            read <= '0' ;
            wait until rising_edge(clock) ;
            wait until rising_edge(clock) ;
        end loop ;
    end procedure ;

    procedure write_time(
        signal clock    :   in  std_logic ;
        signal addr     :   out std_logic_vector(4 downto 0) ;
        signal din      :   out std_logic_vector(7 downto 0) ;
        signal write    :   out std_logic ;
               ts       :   unsigned(63 downto 0)
    ) is
    begin
        for i in 0 to 7 loop
            addr <= std_logic_vector(to_unsigned(i, addr'length)) ;
            din <= std_logic_vector(ts(i*8+7 downto i*8)) ;
            write <= '1' ;
            wait until rising_edge(clock) ;
            write <= '0' ;
            wait until rising_edge(clock) ;
        end loop ;
    end procedure ;

    procedure nop( signal clock : in std_logic ; count : natural ) is
    begin
        for i in 1 to count loop
            wait until rising_edge(clock) ;
        end loop ;
    end procedure ;

    procedure clear_intr(
        signal clock    :   in  std_logic ;
        signal addr     :   out std_logic_vector(4 downto 0) ;
        signal din      :   out std_logic_vector(7 downto 0) ;
        signal write    :   out std_logic
    ) is
    begin
        addr <= std_logic_vector(to_unsigned(8, addr'length)) ;
        din <= std_logic_vector(to_unsigned(1, din'length)) ;
        write <= '1' ;
        wait until rising_edge(clock) ;
        write <= '0' ;
        wait until rising_edge(clock) ;
    end procedure ;

    procedure set_intr(
        signal clock    :   in  std_logic ;
        signal addr     :   out std_logic_vector(4 downto 0) ;
        signal din      :   out std_logic_vector(7 downto 0) ;
        signal write    :   out std_logic
    ) is
    begin
        addr <= std_logic_vector(to_unsigned(8, addr'length)) ;
        din <= std_logic_vector(to_unsigned(0, din'length)) ;
        write <= '1' ;
        wait until rising_edge(clock) ;
        write <= '0' ;
        wait until rising_edge(clock) ;
    end procedure ;

begin

    clock <= not clock after 1 ns ;

    ts_clock <= not ts_clock after 12 ns ;

    ts_reset <= '0' after 100 ns ;

    U_tamer : entity work.time_tamer
      port map (
        clock           =>  clock,
        reset           =>  reset,

        addr            =>  addr,
        din             =>  din,
        dout            =>  dout,
        write           =>  write,
        read            =>  read,
        waitreq         =>  waitreq,
        readack         =>  readack,
        intr            =>  intr,

        ts_sync_in      =>  '0',
        ts_sync_out     =>  open,

        ts_pps          =>  '0',
        ts_clock        =>  ts_clock,
        ts_reset        =>  ts_reset,
        ts_time         =>  ts_time
      ) ;

    tb : process
        variable ts : unsigned(63 downto 0) := (others =>'0') ;
    begin
        reset <= '1' ;
        nop( clock, 100 ) ;

        reset <= '0' ;
        nop( clock, 100 ) ;

        nop( clock, 1000 ) ;
        read_time( clock, addr, dout, read, readack, ts ) ;

        -- Write a comparison time in the future
        write_time( clock, addr, din, write, x"0000_0000_0000_0400" ) ;
        set_intr( clock, addr, din, write ) ;

        -- Wait for an interrupt
        wait until rising_edge(clock) and intr = '1' ;
        nop( clock, 1000 ) ;

        -- Clear the interrupt
        clear_intr( clock, addr, din, write ) ;

        -- Write a comparison time in the past
        write_time( clock, addr, din, write, x"0000_0000_0000_0400" ) ;
        set_intr( clock, addr, din, write ) ;

        -- Wait for an interrupt
        wait until rising_edge(clock) and intr = '1' ;
        nop( clock, 1000 ) ;

        -- Clear the interrupt
        clear_intr( clock, addr, din, write ) ;

        nop( clock, 1000 ) ;

        -- Read the time back
        read_time( clock, addr, dout, read, readack, ts ) ;

        nop( clock, 1000 ) ;

        report "-- End of Simulation" severity failure ;
    end process ;

end architecture ;
