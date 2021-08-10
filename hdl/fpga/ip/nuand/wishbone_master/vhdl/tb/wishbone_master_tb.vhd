-- Copyright (c) 2021 Nuand LLC
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
-- copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in
-- all copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
-- THE SOFTWARE.

library ieee ;
    use ieee.std_logic_1164.all ;
    use ieee.numeric_std.all ;

entity wishbone_master_tb is
end entity ;

architecture arch of wishbone_master_tb is
    constant ADDR_BITS  :   integer   := 32;
    constant DATA_BITS  :   integer   := 32;

    signal clock        :   std_logic       := '1' ;
    signal reset        :   std_logic       := '1' ;

    signal addr         :   std_logic_vector(ADDR_BITS-1 downto 0) := (others => '0');
    signal din          :   std_logic_vector(DATA_BITS-1 downto 0) := (others => '0');
    signal dout         :   std_logic_vector(DATA_BITS-1 downto 0) ;
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
        signal addr     :   out std_logic_vector(ADDR_BITS-1 downto 0) ;
        signal dout     :   in std_logic_vector(DATA_BITS-1 downto 0) ;
        signal read     :   out std_logic ;
        signal readack  :   in  std_logic ;
               reg      :   in  integer ;
               rv       :   out std_logic_vector(DATA_BITS-1 downto 0)
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
        signal addr     :   out std_logic_vector(ADDR_BITS-1 downto 0) ;
        signal din      :   out std_logic_vector(DATA_BITS-1 downto 0) ;
        signal write    :   out std_logic ;
               reg      :   in  integer ;
               rv       :   std_logic_vector(DATA_BITS-1 downto 0)
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
        signal addr     :   out std_logic_vector(ADDR_BITS-1 downto 0) ;
        signal din      :   out std_logic_vector(DATA_BITS-1 downto 0) ;
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
        signal addr     :   out std_logic_vector(ADDR_BITS-1 downto 0) ;
        signal din      :   out std_logic_vector(DATA_BITS-1 downto 0) ;
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

    signal wb_clk_i    : std_logic := '0' ;
    signal wb_rst_i    : std_logic ;

    signal wb_adr_o    : std_logic_vector(ADDR_BITS-1 downto 0) ;
    signal wb_dat_o    : std_logic_vector(DATA_BITS-1 downto 0) ;
    signal wb_dat_i    : std_logic_vector(DATA_BITS-1 downto 0) ;
    signal wb_we_o     : std_logic ;
    signal wb_sel_o    : std_logic ;
    signal wb_stb_o    : std_logic ;
    signal wb_ack_i    : std_logic ;
    signal wb_cyc_o    : std_logic ;
begin

    wb_clk_i <= not wb_clk_i after 3 ns ;
    process(wb_clk_i)
        variable idx : integer := 0;
    begin
        if(rising_edge(wb_clk_i)) then
            wb_ack_i <= wb_cyc_o;
            if (wb_cyc_o = '1' and wb_we_o = '0' ) then
                idx := idx + 1;
                wb_dat_i <= wb_dat_o(wb_dat_o'high-1 downto 0) & '0';
            end if;
        end if;
    end process;


    clock <= not clock after 1 ns ;

    U_wishbone_master : entity work.wishbone_master
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

        wb_clk_i        =>  wb_clk_i,
        wb_rst_i        =>  wb_rst_i,

        wb_adr_o        =>  wb_adr_o,
        wb_dat_o        =>  wb_dat_o,
        wb_dat_i        =>  wb_dat_i,
        wb_we_o         =>  wb_we_o,
        wb_sel_o        =>  wb_sel_o,
        wb_stb_o        =>  wb_stb_o,
        wb_ack_i        =>  wb_ack_i,
        wb_cyc_o        =>  wb_cyc_o

      ) ;

    tb : process
        variable ts : std_logic_vector(DATA_BITS-1 downto 0) := (others =>'0') ;
    begin
        arb_request <= ( others => '0' ) ;
        arb_ack     <= ( others => '0' ) ;
        reset <= '1' ;
        nop( clock, 100 ) ;

        reset <= '0' ;
        nop( clock, 100 ) ;

        -- read CSR
        read_csr( clock, addr, dout, read, readack, 1000, ts ) ;
        nop( clock, 1000 ) ;

        -- NiOS request
        write_csr( clock, addr, din, write, 3, x"00000001" ) ;

        -- NiOS should be granted
        nop( clock, 100 ) ;
        read_csr( clock, addr, dout, read, readack, 1, ts ) ;

        -- NiOS ACK
        nop( clock, 1000 ) ;
        write_csr( clock, addr, din, write, 0, x"00000002" ) ;

        nop( clock, 1000 ) ;
        -- NiOS request #2
        write_csr( clock, addr, din, write, 8, x"00000000" ) ;
        write_csr( clock, addr, din, write, 0, x"00000001" ) ;
        nop( clock, 1000 ) ;

        -- NiOS ACK #2 (with intr)
        nop( clock, 1000 ) ;
        write_csr( clock, addr, din, write, 8, x"00000001" ) ;
        nop( clock, 1000 ) ;
        write_csr( clock, addr, din, write, 0, x"00000002" ) ;

        nop( clock, 1000) ;
        -- Read the time back
        report "-- End of Simulation" severity failure ;
    end process ;

end architecture ;

