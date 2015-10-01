-- Copyright (c) 2015 Nuand LLC
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

entity ppscal_tb is
end entity;

architecture tb of ppscal_tb is

    function half_clk_per( freq : real ) return time is
    begin
        return ( (0.5 sec) / real(freq) );
    end function;

    constant    SYS_CLK_PER     : time      := half_clk_per( 80.0e6 );
    constant    REF_1PPS_PER    : time      := half_clk_per( 5.0e6 );

    signal      ref_1pps        : std_logic := '1';

    signal      sys_clk         : std_logic := '1';
    signal      sys_rst         : std_logic;
    signal      rd_req          : std_logic;
    signal      wr_req          : std_logic;
    signal      addr            : std_logic_vector(7 downto 0);
    signal      wr_data         : std_logic_vector(7 downto 0);
    signal      rd_data         : std_logic_vector(7 downto 0);
    signal      rd_data_val     : std_logic;
    signal      wait_req        : std_logic;
    signal      irq             : std_logic;
    signal      tcxo_clock      : std_logic;

begin

    sys_clk <= not sys_clk after SYS_CLK_PER;
    sys_rst <= '1', '0' after SYS_CLK_PER*5;

    ref_1pps <= not ref_1pps after REF_1PPS_PER;


    uut : entity work.ppscal(arch)
        port map(
            -- Physical Interface
            ref_1pps        => ref_1pps,
            tcxo_clock      => tcxo_clock,

            -- Avalon-MM Interface
            mm_clock        => sys_clk,         -- in  std_logic;
            mm_reset        => sys_rst,         -- in  std_logic;
            mm_rd_req       => rd_req,          -- in  std_logic;
            mm_wr_req       => wr_req,          -- in  std_logic;
            mm_addr         => addr,            -- in  std_logic_vector(ADDR_WIDTH-1 downto 0);
            mm_wr_data      => wr_data,         -- in  std_logic_vector(DATA_WIDTH-1 downto 0);
            mm_rd_data      => rd_data,         -- out std_logic_vector(DATA_WIDTH-1 downto 0);
            mm_rd_datav     => rd_data_val,     -- out std_logic;
            mm_wait_req     => wait_req,        -- out std_logic;

            -- Interrupt interface
            mm_irq          => irq
        );


    driver : entity work.mm_driver
        port map (
            mm_clock        => sys_clk,     -- in  std_logic;
            mm_reset        => sys_rst,     -- in  std_logic;
            mm_rd_req       => rd_req,      -- out std_logic;
            mm_wr_req       => wr_req,      -- out std_logic;
            mm_addr         => addr,        -- out std_logic_vector(7 downto 0);
            mm_wr_data      => wr_data,     -- out std_logic_vector(7 downto 0);
            mm_rd_data      => rd_data,     -- in  std_logic_vector(7 downto 0);
            mm_rd_datav     => rd_data_val, -- in  std_logic;
            mm_wait_req     => wait_req,    -- in  std_logic

            mm_irq          => irq,

            tcxo_clock      => tcxo_clock
        );

end architecture;
