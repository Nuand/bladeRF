-- RFIC SPI Control
-- Copyright (c) 2020 Nuand LLC
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

library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;

entity bladerf_rfic_spi_ctrl is
  generic(
    CLOCK_DIV           : positive  range 2 to 16 := 2;
    ADDR_WIDTH          : positive                := 16;
    DATA_WIDTH          : positive                := 8
  );
  port (
    -- Physical Interface
    sclk                : out   std_logic;
    miso                : in    std_logic;
    mosi                : out   std_logic;
    cs_n                : out   std_logic;

    arbiter_req         : out   std_logic;
    arbiter_grant       : in    std_logic;
    arbiter_done        : out   std_logic;

    -- Control Interface
    clock               : in    std_logic;
    reset               : in    std_logic;
    tx_ota_req          : in    std_logic
  );
end entity; -- pll_reset

architecture arch of bladerf_rfic_spi_ctrl is
    type fsm_t is (
        INIT,
        GET_ARBITER,
        WAIT_END_TX,
        RELEASE_ARBITER,
        WAIT_TO_WR,
        WAIT_WR_DVALID,
        WAIT_WR_DVALID_2,
        WAIT_RD_DVALID,
        WAIT_RD_DVALID_2
    );

    type state_t is record
        state        : fsm_t;
        n_state      : fsm_t;
        mm_read      : std_logic;
        mm_write     : std_logic;
        mm_addr      : std_logic_vector(ADDR_WIDTH-1 downto 0);
        mm_din       : std_logic_vector(DATA_WIDTH-1 downto 0);
        mm_dout      : std_logic_vector(DATA_WIDTH-1 downto 0);
        counter      : natural range 0 to 50000;
        arbiter_req  : std_logic;
        arbiter_done : std_logic;
    end record;

    constant reset_value : state_t := (
        state        => INIT,
        n_state      => INIT,
        mm_read      => '0',
        mm_write     => '0',
        mm_addr      => ( others => '0' ),
        mm_din       => ( others => '0' ),
        mm_dout      => ( others => '0' ),
        counter      => 0,
        arbiter_req  => '0',
        arbiter_done => '0'
    );

    signal current  : state_t := reset_value;
    signal future   : state_t := reset_value;

    signal mm_dout    : std_logic_vector(DATA_WIDTH-1 downto 0);
    signal mm_dvalid  : std_logic ;
    signal mm_busy    : std_logic ;

begin
    arbiter_req  <= current.arbiter_req;
    arbiter_done <= current.arbiter_done;

    U_rfic_spi: entity work.rfic_spi_controller
       port map(
        -- Physical Interface
        sclk         =>  sclk,
        miso         =>  miso,
        mosi         =>  mosi,
        cs_n         =>  cs_n,

        -- Avalon-MM Interface
        mm_clock     => clock,
        mm_reset     => reset,
        mm_read      => current.mm_read,
        mm_write     => current.mm_write,
        mm_addr      => current.mm_addr,
        mm_din       => current.mm_din,
        mm_dout      => mm_dout,
        mm_dout_val  => mm_dvalid,
        mm_busy      => mm_busy
      );

    sync_proc : process(clock)
    begin
        if(reset = '1') then
            current <= reset_value;
        elsif(rising_edge(clock)) then
            current <= future;
        end if;
    end process sync_proc;

    comb_proc : process(all)
    begin
        -- defaults
        future              <= current;
        future.mm_read      <= '0';
        future.mm_write     <= '0';

        future.arbiter_req   <= '0';
        future.arbiter_done  <= '0';

        -- states
        case current.state is
            when INIT =>
                if( tx_ota_req = '1' ) then
                    future.arbiter_req <= '1';
                    future.state   <= GET_ARBITER;
                end if;

            when GET_ARBITER =>
                future.arbiter_req <= '1';
                if( arbiter_grant = '1' ) then
                    future.state   <= WAIT_WR_DVALID;
                    future.n_state <= WAIT_END_TX;

                    future.mm_addr  <= x"0051";
                    future.mm_din   <= x"00";
                    future.mm_write <= '1';
                end if;

            when WAIT_END_TX =>
                if( tx_ota_req = '0' ) then
                    future.state   <= WAIT_TO_WR;
                    future.counter <= 550;
                end if;


            when WAIT_TO_WR =>
                if( current.counter = 0) then
                    future.state   <= WAIT_WR_DVALID;
                    future.n_state <= RELEASE_ARBITER;

                    future.mm_addr  <= x"0051";
                    future.mm_din   <= x"10";
                    future.mm_write <= '1';
                else
                    future.counter  <= current.counter - 1;
                end if;


            when RELEASE_ARBITER =>
                future.arbiter_done <= '1';
                future.state        <= INIT;

            when WAIT_WR_DVALID =>
                future.state <= WAIT_WR_DVALID_2;

            when WAIT_WR_DVALID_2 =>
                if( mm_busy = '0' ) then
                    future.state <= WAIT_RD_DVALID;
                    future.mm_addr  <= x"0051";
                    future.mm_din   <= x"00";
                    future.mm_read  <= '1';
                end if;

            when WAIT_RD_DVALID =>
                future.state <= WAIT_RD_DVALID_2;

            when WAIT_RD_DVALID_2 =>
                if( mm_dvalid = '1' ) then
                    future.mm_dout  <= mm_dout;
                    future.state    <= current.n_state;
                end if;
        end case;

    end process comb_proc;

end architecture arch;
