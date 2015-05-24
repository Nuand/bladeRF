library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;

library std ;
    use std.textio.all ;

entity command_uart_tb is
end entity ;

architecture arch of command_uart_tb is

    signal clock        :   std_logic       := '1' ;
    signal reset        :   std_logic       := '1' ;

    signal host_sin     :   std_logic ;
    signal host_sout    :   std_logic ;

    signal host_addr    :   std_logic_vector(4 downto 0) ;
    signal host_dout    :   std_logic_vector(31 downto 0) ;
    signal host_din     :   std_logic_vector(31 downto 0) ;
    signal host_read    :   std_logic                       := '0' ;
    signal host_write   :   std_logic                       := '0' ;
    signal host_readack :   std_logic ;
    signal host_waitreq :   std_logic ;
    signal host_irq     :   std_logic ;

    signal dev_addr     :   std_logic_vector(4 downto 0) ;
    signal dev_dout     :   std_logic_vector(31 downto 0) ;
    signal dev_din      :   std_logic_vector(31 downto 0) ;
    signal dev_read     :   std_logic                        := '0' ;
    signal dev_write    :   std_logic                        := '0' ;
    signal dev_readack  :   std_logic ;
    signal dev_waitreq  :   std_logic ;
    signal dev_irq      :   std_logic ;

    procedure nop( signal clock : in std_logic ; count : in natural ) is
    begin
        for i in 1 to count loop
            wait until rising_edge(clock) ;
        end loop ;
    end procedure ;

    procedure write_command(
        signal clock : in std_logic ;
        signal addr : out std_logic_vector(4 downto 0) ;
        signal din : out std_logic_vector(31 downto 0) ;
        signal write : out std_logic ;
               command : in std_logic_vector(127 downto 0)
    ) is
    begin
        for i in 0 to 3 loop
            addr <= std_logic_vector(to_unsigned(4*i, addr'length)) ;
            din <= command(i*32+31 downto i*32) ;
            write <= '1' ;
            wait until rising_edge(clock) ;
            write <= '0' ;
            wait until rising_edge(clock) ;
        end loop ;
    end procedure ;

    procedure read_command(
        signal clock : in std_logic ;
        signal addr : out std_logic_vector(4 downto 0) ;
        signal dout : in std_logic_vector(31 downto 0) ;
        signal read : out std_logic ;
               command : out std_logic_vector(127 downto 0)
    ) is
    begin
        for i in 0 to 3 loop
            addr <= std_logic_vector(to_unsigned(4*i, addr'length)) ;
            read <= '1' ;
            wait until rising_edge(clock) ;
            read <= '1' ;
            wait until rising_edge(clock) ;
            read <= '0' ;
            command(i*32+31 downto i*32) := dout ;
            nop( clock, 10 ) ;
        end loop ;
    end procedure ;

    procedure send_retune(
        signal clock        :   in      std_logic ;
        signal addr         :   out     std_logic_vector(4 downto 0) ;
        signal din          :   out     std_logic_vector(31 downto 0) ;
        signal write        :   out     std_logic ;
               timestamp    :   in      std_logic_vector(63 downto 0) ;
               nint         :   in      positive ;
               nfrac        :   in      natural ;
               freqsel      :   in      natural range 0 to 63 ;
               vcocap       :   in      natural range 0 to 63 ;
               tx           :   in      std_logic ;
               rx           :   in      std_logic ;
               bandsel      :   in      std_logic
    ) is
        variable data : std_logic_vector(127 downto 0) ;
        constant nint_slv : std_logic_vector(8 downto 0) := std_logic_vector(to_unsigned(nint,9)) ;
        constant nfrac_slv : std_logic_vector(22 downto 0) := std_logic_vector(to_unsigned(nfrac,23)) ;
        constant freqsel_slv : std_logic_vector(5 downto 0) := std_logic_vector(to_unsigned(freqsel,6)) ;
        constant vcocap_slv : std_logic_vector(5 downto 0) := std_logic_vector(to_unsigned(vcocap,6)) ;
    begin
        data( 0*8+7 downto  0*8)    := std_logic_vector(to_unsigned(character'pos('T'),8)) ;
        data( 8*8+7 downto  1*8)    := timestamp ;
        data(12*8+7 downto  9*8)    := nfrac_slv(7 downto 0) & nfrac_slv(15 downto 8) & nint_slv(0) & nfrac_slv(22 downto 16) & nint_slv(8 downto 1) ;
        data(13*8+7 downto 13*8)    := tx & rx & freqsel_slv ;
        data(14*8+7 downto 14*8)    := bandsel & '0' & vcocap_slv ;
        data(15*8+7 downto 15*8)    := (others =>'0') ;
        write_command(clock, addr, din, write, data) ;
    end procedure ;

begin

    clock <= not clock after 1 ns ;

    U_host : entity work.command_uart
      port map (
        clock       =>  clock,
        reset       =>  reset,

        rs232_sin   =>  host_sin,
        rs232_sout  =>  host_sout,

        addr        =>  host_addr,
        din         =>  host_din,
        dout        =>  host_dout,
        read        =>  host_read,
        write       =>  host_write,
        readack     =>  host_readack,
        waitreq     =>  host_waitreq,
        irq         =>  host_irq
      ) ;

    U_device : entity work.command_uart
      port map (
        clock       =>  clock,
        reset       =>  reset,

        rs232_sin   =>  host_sout,
        rs232_sout  =>  host_sin,

        addr        =>  dev_addr,
        din         =>  dev_din,
        dout        =>  dev_dout,
        read        =>  dev_read,
        write       =>  dev_write,
        readack     =>  dev_readack,
        waitreq     =>  dev_waitreq,
        irq         =>  dev_irq
      ) ;

    tb : process
        variable response : std_logic_vector(127 downto 0) ;
    begin
        nop( clock, 100 ) ;
        reset <= '0' ;
        nop( clock, 100 ) ;

        -- Send a retune command for giggles
        report "HOST: Send retune command" ;
        send_retune(
            clock       => clock,
            addr        => host_addr,
            din         => host_din,
            write       => host_write,
            timestamp   => (others =>'0'),
            nint        => 131,
            nfrac       => 0,
            vcocap      => 32,
            freqsel     => 44,
            tx          => '0',
            rx          => '0',
            bandsel     => '0'
        ) ;

        wait until rising_edge(clock) and host_irq = '1' ;
        report "HOST: Received response" ;
        read_command(clock, host_addr, host_dout, host_read, response) ;

        nop( clock, 100 ) ;

        write_command(clock, host_addr, host_din, host_write, (others =>'0'));

        nop( clock, 10000 ) ;

        write_command(clock, host_addr, host_din, host_write, response ) ;

        wait until rising_edge(clock) and host_irq = '1' ;
        nop(clock, 500) ;
        report "HOST: Received response" ;
        read_command(clock, host_addr, host_dout, host_read, response ) ;

        nop( clock, 1000 ) ;

        report "-- End of Simulation --" severity failure ;

    end process ;

    -- Just turn it around and send it back
    receive_command : process
        variable request : std_logic_vector(127 downto 0) ;
    begin
        wait until rising_edge(clock) and dev_irq = '1' ;
        report "DEVICE: Received request" ;
        nop( clock, 100 ) ;
        read_command(clock, dev_addr, dev_dout, dev_read, request) ;
        nop( clock, 1000 ) ;
        report "DEVICE: Writing response" ;
        write_command(clock, dev_addr, dev_din, dev_write, request) ;
    end process ;

end architecture ;
