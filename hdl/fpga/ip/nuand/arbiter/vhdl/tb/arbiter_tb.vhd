-- Copyright (c) 2017 Nuand LLC
--
-- This program is free software: you can redistribute it and/or modify
-- it under the terms of the GNU Affero General Public License as
-- published by the Free Software Foundation, either version 3 of the
-- License, or (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU Affero General Public License for more details.
--
-- You should have received a copy of the GNU Affero General Public License
-- along with this program.  If not, see <http://www.gnu.org/licenses/>.

library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;

entity arbiter_tb is
end entity ;

architecture arch of arbiter_tb is

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

    signal arb_request     :   std_logic_vector(3 downto 0) ;
    signal arb_ack         :   std_logic_vector(3 downto 0) ;
    signal arb_granted     :   std_logic_vector(3 downto 0) ;

    procedure read_csr(
        signal clock    :   in  std_logic ;
        signal addr     :   out std_logic_vector(4 downto 0) ;
        signal dout     :   in std_logic_vector(7 downto 0) ;
        signal read     :   out std_logic ;
        signal readack  :   in  std_logic ;
               reg      :   in  integer ;
               rv       :   out std_logic_vector(7 downto 0)
    ) is
    begin
        addr <= std_logic_vector(to_unsigned(reg, addr'length)) ;
        read <= '1' ;
        wait until rising_edge(clock) and readack = '1' ;
        rv := std_logic_vector(dout) ;
        read <= '0' ;
        wait until rising_edge(clock) ;
        wait until rising_edge(clock) ;
    end procedure ;

    procedure write_csr(
        signal clock    :   in  std_logic ;
        signal addr     :   out std_logic_vector(4 downto 0) ;
        signal din      :   out std_logic_vector(7 downto 0) ;
        signal write    :   out std_logic ;
               reg      :   in  integer ;
               rv       :   std_logic_vector(7 downto 0)
    ) is
    begin
        addr <= std_logic_vector(to_unsigned(reg, addr'length)) ;
        din <= std_logic_vector(rv) ;
        write <= '1' ;
        wait until rising_edge(clock) ;
        write <= '0' ;
        wait until rising_edge(clock) ;
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

    U_arbiter : entity work.arbiter
      generic map(
        N => 4
      )
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

        request         =>  arb_request,
        ack             =>  arb_ack,
        granted         =>  arb_granted

      ) ;

    tb : process
        variable ts : std_logic_vector(7 downto 0) := (others =>'0') ;
    begin
        arb_request <= ( others => '0' ) ;
        arb_ack     <= ( others => '0' ) ;
        reset <= '1' ;
        nop( clock, 100 ) ;

        reset <= '0' ;
        nop( clock, 100 ) ;

        -- read CSR
        read_csr( clock, addr, dout, read, readack, 0, ts ) ;
        nop( clock, 1000 ) ;

        -- NiOS request
        write_csr( clock, addr, din, write, 0, x"01" ) ;

        -- NiOS should be granted
        nop( clock, 100 ) ;
        read_csr( clock, addr, dout, read, readack, 1, ts ) ;

        -- Contender #4 request
        arb_request <= x"4";

        -- NiOS ACK
        nop( clock, 1000 ) ;
        write_csr( clock, addr, din, write, 0, x"02" ) ;

        nop( clock, 1000 ) ;
        -- NiOS request #2
        write_csr( clock, addr, din, write, 8, x"00" ) ;
        write_csr( clock, addr, din, write, 0, x"01" ) ;
        nop( clock, 1000 ) ;

        -- Contender #4 ACK
        arb_request <= x"0";
        arb_ack <= x"4";
        nop( clock, 10) ;
        arb_ack <= x"0";


        -- NiOS ACK #2 (with intr)
        nop( clock, 1000 ) ;
        write_csr( clock, addr, din, write, 8, x"01" ) ;
        nop( clock, 1000 ) ;
        write_csr( clock, addr, din, write, 0, x"02" ) ;

        nop( clock, 1000) ;
        -- Read the time back
        report "-- End of Simulation" severity failure ;
    end process ;

end architecture ;

