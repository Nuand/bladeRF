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

library altera_mf ;
    use altera_mf.altera_mf_components.all ;


entity wishbone_master is
  generic (
    ADDR_BITS   : integer  := 32 ;
    DATA_BITS   : integer  := 32
  ) ;

  port (
    -- Control signals
    clock       :   in  std_logic ;
    reset       :   in  std_logic ;

    -- Memory mapped interface
    addr        :   in  std_logic_vector(ADDR_BITS-1 downto 0) ;
    din         :   in  std_logic_vector(DATA_BITS-1 downto 0) ;
    dout        :   out std_logic_vector(DATA_BITS-1 downto 0) ;
    write       :   in  std_logic ;
    read        :   in  std_logic ;
    waitreq     :   out std_logic ;
    readack     :   out std_logic ;
    intr        :   out std_logic ;

    -- Wishbone signals
    wb_clk_i    :   in  std_logic ;
    wb_rst_i    :   in  std_logic ;

    wb_adr_o    :   out std_logic_vector(ADDR_BITS-1 downto 0) ;
    wb_dat_o    :   out std_logic_vector(DATA_BITS-1 downto 0) ;
    wb_dat_i    :   in  std_logic_vector(DATA_BITS-1 downto 0) ;
    wb_we_o     :   out std_logic ;
    wb_sel_o    :   out std_logic ;
    wb_stb_o    :   out std_logic ;
    wb_ack_i    :   in  std_logic ;
    wb_cyc_o    :   out std_logic
  ) ;
end entity ;

architecture arch of wishbone_master is

    signal av2wb_wfull   : std_logic ;
    signal wb_empty      : std_logic ;
    signal wb_empty_r    : std_logic ;
    signal wb_bus        : std_logic_vector((ADDR_BITS + DATA_BITS) downto 0) ;

    signal wb2av_wfull   : std_logic ;
    signal av_empty      : std_logic ;
    signal av_bus        : std_logic_vector(DATA_BITS-1 downto 0) ;



    type av_fsm_t is (INIT, WAIT_FOR_ACK) ;

    type av_state_t is record
        state           : av_fsm_t;
        read_op         : std_logic ;
        waitreq         : std_logic ;
        readack         : std_logic ;
        adr             : std_logic_vector(ADDR_BITS-1 downto 0) ;
        din             : std_logic_vector(DATA_BITS-1 downto 0) ;
        dout            : std_logic_vector(DATA_BITS-1 downto 0) ;
        we              : std_logic ;
        wreq            : std_logic ;
        req_r           : std_logic ;
    end record;

    function AV_NULL_STATE return av_state_t is
        variable rv : av_state_t ;
    begin
        rv.state    := INIT;
        rv.read_op  := '0';
        rv.waitreq  := '0';
        rv.readack  := '0';

        rv.adr      := ( others => '0' );
        rv.din      := ( others => '0' );
        rv.dout     := ( others => '0' );
        rv.we       := '0';

        rv.wreq     := '0';
        rv.req_r    := '0';
        return rv ;
    end function;

    signal av_current : av_state_t ;
    signal av_future  : av_state_t ;



    type wb_fsm_t is (INIT, WAIT_FOR_REQ) ;

    type wb_state_t is record
        state           : wb_fsm_t;
        sel             : std_logic ;
    end record;

    function WB_NULL_STATE return wb_state_t is
        variable rv : wb_state_t ;
    begin
        rv.state := INIT ;
        rv.sel   := '0' ;
        return rv ;
    end function;


    signal wb_current : wb_state_t ;
    signal wb_future  : wb_state_t ;

begin

    av_sync_proc : process(clock, reset)
    begin
        if (reset = '1' ) then
            av_current <= AV_NULL_STATE ;
        elsif (rising_edge(clock) ) then
            av_current <= av_future;
        end if;
    end process;

    av_comb_proc : process( all )
    begin
        av_future <= av_current;

        av_future.din <= ( others => '0' );
        av_future.adr <= ( others => '0' );
        av_future.req_r <= '0';
        av_future.wreq  <= '0';
        av_future.readack <= '0';

        case(av_current.state) is
            when INIT =>
               if (read = '1' or write = '1' ) then
                  if (av_current.req_r = '0' ) then
                     av_future.din  <= din;
                     av_future.adr  <= addr;
                     av_future.we   <= write;
                     av_future.wreq <= '1';

                     av_future.waitreq <= '1';
                     av_future.state   <= WAIT_FOR_ACK;

                  end if;
                  av_future.req_r <= '1';
               end if;

            when WAIT_FOR_ACK =>
               if (av_empty = '0' ) then
                  av_future.req_r   <= '1';
                  av_future.state   <= INIT;
                  av_future.waitreq <= '0';
                  av_future.dout    <= av_bus;
                  if (av_current.we = '0' ) then
                     av_future.readack <= '1';
                  end if;
               end if;

            when others =>
               av_future <= AV_NULL_STATE;
        end case;
    end process;

    wb_sync_proc : process(clock, reset)
    begin
        if (reset = '1' ) then
            wb_current <= WB_NULL_STATE ;
        elsif (rising_edge(wb_clk_i) ) then
            wb_current <= wb_future;
        end if;
    end process;

    wb_comb_proc : process( all )
    begin
        wb_future <= wb_current;

        wb_future.sel <= '0';

        case(wb_current.state) is
            when INIT =>
               if (wb_empty = '0' ) then
                  wb_future.sel   <= '1';
                  wb_future.state <= WAIT_FOR_REQ;
               end if;
            when WAIT_FOR_REQ =>
               if (wb_ack_i = '1') then
                  wb_future.state <= INIT;
               else
                  wb_future.sel <= '1';
               end if;
            when others =>
               wb_future <= WB_NULL_STATE;
        end case;
    end process;

    intr <= '0';




    U_av2wb: dcfifo
      generic map (
        lpm_width       =>  (1+ADDR_BITS+DATA_BITS),
        lpm_widthu      =>  4,
        lpm_numwords    =>  16,
        lpm_showahead   =>  "ON"
      )
      port map (
        aclr            => reset,

        wrclk           => clock,
        wrreq           => av_current.wreq and not av2wb_wfull,
        data            => av_current.we & av_current.adr & av_current.din,

        wrfull          => av2wb_wfull,
        wrempty         => open,
        wrusedw         => open,

        rdclk           => wb_clk_i,
        rdreq           => not wb_empty,
        q               => wb_bus,

        rdfull          => open,
        rdempty         => wb_empty,
        rdusedw         => open
      );


    wb_adr_o <= wb_bus(ADDR_BITS+DATA_BITS-1 downto DATA_BITS);
    wb_dat_o <= wb_bus(DATA_BITS-1 downto 0);
    wb_we_o  <= wb_bus(wb_bus'high);
    wb_cyc_o <= not wb_empty;
    wb_sel_o <= wb_current.sel;
    wb_stb_o <= '1';

    U_wb2av: dcfifo
      generic map (
        lpm_width       =>  32,
        lpm_widthu      =>  4,
        lpm_numwords    =>  16,
        lpm_showahead   =>  "ON"
      )
      port map (
        aclr            => reset,

        wrclk           => wb_clk_i,
        wrreq           => wb_ack_i and not wb2av_wfull,
        data            => wb_dat_i,

        wrfull          => wb2av_wfull,
        wrempty         => open,
        wrusedw         => open,

        rdclk           => clock,
        rdreq           => not av_empty,
        q               => av_bus,

        rdfull          => open,
        rdempty         => av_empty,
        rdusedw         => open
      );
    dout    <= av_current.dout;
    readack <= av_current.readack;
    waitreq <= av_current.waitreq;

end architecture ;
